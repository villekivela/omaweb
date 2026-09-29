#include "AgentActivityLog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace omaweb {
namespace {

    constexpr auto fileName = "agent-activity.jsonl";
    constexpr auto onlyTheReader = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

    QByteArray line(const AgentActivityLog::Entry &entry)
    {
        const QJsonObject object {
            {QStringLiteral("time"), static_cast<double>(entry.time)},
            {QStringLiteral("agent"), entry.agent},
            {QStringLiteral("spaceId"), entry.spaceId},
            {QStringLiteral("space"), entry.space},
            {QStringLiteral("tabId"), entry.tabId},
            {QStringLiteral("address"), entry.address},
            {QStringLiteral("verb"), entry.verb},
            {QStringLiteral("target"), entry.target},
            {QStringLiteral("outcome"), entry.outcome},
        };
        return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    }

    QVariantMap row(const AgentActivityLog::Entry &entry)
    {
        return {
            {QStringLiteral("time"), static_cast<double>(entry.time)},
            {QStringLiteral("agent"), entry.agent},
            {QStringLiteral("spaceId"), entry.spaceId},
            {QStringLiteral("space"), entry.space},
            {QStringLiteral("tabId"), entry.tabId},
            {QStringLiteral("address"), entry.address},
            {QStringLiteral("verb"), entry.verb},
            {QStringLiteral("target"), entry.target},
            {QStringLiteral("outcome"), entry.outcome},
        };
    }

} // namespace

AgentActivityLog::AgentActivityLog(QString directory, QObject *parent)
    : QObject(parent)
    , m_path(directory.isEmpty() ? QString {} : QDir(directory).filePath(QLatin1String(fileName)))
{
    if (!m_path.isEmpty()) {
        QDir().mkpath(directory);
        load();
    }
}

QString AgentActivityLog::path() const { return m_path; }

const QList<AgentActivityLog::Entry> &AgentActivityLog::entries() const { return m_entries; }

void AgentActivityLog::load()
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    auto unreadable = false;
    while (!file.atEnd()) {
        const auto text = file.readLine().trimmed();
        if (text.isEmpty()) {
            continue;
        }
        const auto object = QJsonDocument::fromJson(text).object();
        if (object.isEmpty()) {
            unreadable = true;
            continue;
        }
        m_entries.append(Entry {
            .time = static_cast<qint64>(object.value(QStringLiteral("time")).toDouble()),
            .agent = object.value(QStringLiteral("agent")).toString(),
            .spaceId = object.value(QStringLiteral("spaceId")).toString(),
            .space = object.value(QStringLiteral("space")).toString(),
            .tabId = object.value(QStringLiteral("tabId")).toString(),
            .address = object.value(QStringLiteral("address")).toString(),
            .verb = object.value(QStringLiteral("verb")).toString(),
            .target = object.value(QStringLiteral("target")).toString(),
            .outcome = object.value(QStringLiteral("outcome")).toString(),
        });
    }
    file.close();
    // A line cut short by a crash is dropped with the expired ones.
    std::stable_sort(m_entries.begin(), m_entries.end(),
        [](const Entry &left, const Entry &right) { return left.time < right.time; });
    prune(unreadable);
}

void AgentActivityLog::prune(bool rewriteAnyway)
{
    const auto oldest = QDateTime::currentMSecsSinceEpoch() - retentionMs;
    const auto expired = std::find_if(m_entries.cbegin(), m_entries.cend(),
                             [oldest](const Entry &entry) { return entry.time >= oldest; })
        - m_entries.cbegin();
    auto dropped = expired;
    // Past the bound, a tenth more goes, so the file is not written again at
    // every line that follows.
    if (m_entries.size() - dropped > maximumEntries) {
        dropped = m_entries.size() - (maximumEntries - maximumEntries / 10);
    }
    if (dropped > 0) {
        m_entries.remove(0, dropped);
    }
    if (dropped > 0 || rewriteAnyway) {
        rewrite();
    }
}

void AgentActivityLog::rewrite() const
{
    if (m_path.isEmpty()) {
        return;
    }
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.setPermissions(onlyTheReader);
    for (const auto &entry : m_entries) {
        file.write(line(entry));
    }
    file.commit();
}

void AgentActivityLog::append(const Entry &entry) const
{
    if (m_path.isEmpty()) {
        return;
    }
    QFile file(m_path);
    const auto created = !file.exists();
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return;
    }
    if (created) {
        file.setPermissions(onlyTheReader);
    }
    file.write(line(entry));
}

void AgentActivityLog::record(Entry entry)
{
    if (entry.time == 0) {
        entry.time = QDateTime::currentMSecsSinceEpoch();
    }
    m_entries.append(entry);
    append(entry);
    // The oldest line is the first, so a week-old one is noticed at the next
    // line rather than only at the next start.
    if (m_entries.size() > maximumEntries
        || m_entries.constFirst().time < QDateTime::currentMSecsSinceEpoch() - retentionMs) {
        prune(false);
    }
    emit recorded();
}

QVariantList AgentActivityLog::rows(const QString &agent, const QString &spaceId) const
{
    const auto oldest = QDateTime::currentMSecsSinceEpoch() - retentionMs;
    QVariantList result;
    for (auto it = m_entries.crbegin(); it != m_entries.crend(); ++it) {
        if (it->time < oldest) {
            break;
        }
        if ((!agent.isEmpty() && it->agent != agent)
            || (!spaceId.isEmpty() && it->spaceId != spaceId)) {
            continue;
        }
        result.append(row(*it));
    }
    return result;
}

QStringList AgentActivityLog::agents() const
{
    QStringList names;
    for (const auto &entry : m_entries) {
        if (!names.contains(entry.agent)) {
            names.append(entry.agent);
        }
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QVariantList AgentActivityLog::spaces() const
{
    QHash<QString, QString> names;
    QStringList order;
    for (const auto &entry : m_entries) {
        if (entry.spaceId.isEmpty()) {
            continue;
        }
        if (!names.contains(entry.spaceId)) {
            order.append(entry.spaceId);
        }
        names.insert(entry.spaceId, entry.space.isEmpty() ? entry.spaceId : entry.space);
    }
    std::sort(order.begin(), order.end(), [&names](const QString &left, const QString &right) {
        return names.value(left).compare(names.value(right), Qt::CaseInsensitive) < 0;
    });
    QVariantList result;
    for (const auto &id : std::as_const(order)) {
        result.append(
            QVariantMap {{QStringLiteral("id"), id}, {QStringLiteral("name"), names.value(id)}});
    }
    return result;
}

} // namespace omaweb
