#include "ContentBlocker.h"

#include "ContentMatcher.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrentRun>

namespace tanto {

ContentBlocker::ContentBlocker(QString dataRoot, QObject *parent)
    : QObject(parent)
    , m_dataRoot(std::move(dataRoot))
{
    std::atomic_store(&m_runtime, std::make_shared<const Runtime>());
    load();
    recompile();
    QMetaObject::invokeMethod(this, &ContentBlocker::updateAllSubscriptions,
        Qt::QueuedConnection);
}

QString ContentBlocker::userRules() const
{
    return m_userRules;
}

void ContentBlocker::setUserRules(const QString &rules)
{
    if (rules == m_userRules) {
        return;
    }
    m_userRules = rules;
    save();
    emit configurationChanged();
    recompile();
}

QVariantList ContentBlocker::subscriptions() const
{
    QVariantList result;
    for (const auto &subscription : m_subscriptions) {
        result.append(QVariantMap{
            {QStringLiteral("id"), subscription.id},
            {QStringLiteral("title"), subscription.title},
            {QStringLiteral("source"), subscription.source},
            {QStringLiteral("license"), subscription.license},
            {QStringLiteral("updateAddress"), subscription.updateAddress},
            {QStringLiteral("updateStatus"), subscription.updateStatus},
            {QStringLiteral("lastUpdated"), subscription.lastUpdated},
            {QStringLiteral("enabled"), subscription.enabled},
        });
    }
    return result;
}

bool ContentBlocker::compiling() const
{
    return m_activeCompilations > 0;
}

QVariantMap ContentBlocker::compilationReport() const
{
    return m_compilationReport;
}

QString ContentBlocker::addSubscription(const QString &title, const QUrl &source,
    const QString &license, const QUrl &updateAddress)
{
    if (title.trimmed().isEmpty() || !source.isValid() || license.trimmed().isEmpty()
        || !updateAddress.isValid()) {
        return {};
    }
    Subscription subscription;
    subscription.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    subscription.title = title.trimmed();
    subscription.source = source;
    subscription.license = license.trimmed();
    subscription.updateAddress = updateAddress;
    subscription.updateStatus = QStringLiteral("not updated");
    const auto id = subscription.id;
    m_subscriptions.append(std::move(subscription));
    save();
    emit subscriptionsChanged();
    updateSubscription(id);
    return id;
}

void ContentBlocker::setSubscriptionEnabled(const QString &id, bool enabled)
{
    auto *subscription = findSubscription(id);
    if (!subscription || subscription->enabled == enabled) {
        return;
    }
    subscription->enabled = enabled;
    save();
    emit subscriptionsChanged();
    recompile();
}

void ContentBlocker::updateSubscription(const QString &id)
{
    auto *subscription = findSubscription(id);
    if (!subscription || !subscription->enabled) {
        return;
    }
    subscription->updateStatus = QStringLiteral("updating");
    emit subscriptionsChanged();
    auto *reply = m_network.get(QNetworkRequest(subscription->updateAddress));
    connect(reply, &QNetworkReply::finished, this, [this, reply, id] {
        auto *current = findSubscription(id);
        if (!current) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            current->updateStatus = QStringLiteral("failed: %1").arg(reply->errorString());
        } else {
            const auto candidate = reply->readAll();
            current->updateStatus = QStringLiteral("validating");
            ++m_activeCompilations;
            emit compilingChanged();
            auto *watcher = new QFutureWatcher<MatcherCompilation>(this);
            connect(watcher, &QFutureWatcher<MatcherCompilation>::finished, this,
                [this, watcher, id, candidate] {
                    const auto validation = watcher->result();
                    watcher->deleteLater();
                    --m_activeCompilations;
                    emit compilingChanged();
                    auto *subscription = findSubscription(id);
                    if (!subscription) {
                        return;
                    }
                    const auto accepted = validation.report
                                              .value(QStringLiteral("acceptedRuleCount"))
                                              .toInt();
                    const auto invalid = validation.report
                                             .value(QStringLiteral("invalidRuleCount"))
                                             .toInt();
                    if (!validation.matcher || accepted == 0 || invalid > 0) {
                        subscription->updateStatus = QStringLiteral(
                            "failed: list has no usable rules or contains invalid rules");
                        save();
                        emit subscriptionsChanged();
                        return;
                    }
                    QDir().mkpath(QFileInfo(listPath(id)).absolutePath());
                    QSaveFile file(listPath(id));
                    if (!file.open(QIODevice::WriteOnly) || file.write(candidate) < 0
                        || !file.commit()) {
                        subscription->updateStatus = QStringLiteral(
                            "failed: could not store list");
                        save();
                        emit subscriptionsChanged();
                        return;
                    }
                    subscription->updateStatus = QStringLiteral("compiling");
                    subscription->lastUpdated = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                    m_pendingCurrent.append(id);
                    save();
                    emit subscriptionsChanged();
                    recompile();
                });
            watcher->setFuture(QtConcurrent::run([candidate] {
                return ContentMatcher::compile(QString::fromUtf8(candidate));
            }));
        }
        save();
        emit subscriptionsChanged();
        reply->deleteLater();
    });
}

void ContentBlocker::updateAllSubscriptions()
{
    for (const auto &subscription : std::as_const(m_subscriptions)) {
        if (subscription.enabled) {
            updateSubscription(subscription.id);
        }
    }
}

bool ContentBlocker::siteEnabled(const QUrl &url) const
{
    return !std::atomic_load(&m_runtime)->disabledSites.contains(siteKey(url));
}

void ContentBlocker::setSiteEnabled(const QUrl &url, bool enabled)
{
    const auto key = siteKey(url);
    if (key.isEmpty()) {
        return;
    }
    if (enabled) {
        m_disabledSites.remove(key);
    } else {
        m_disabledSites.insert(key);
    }
    replaceDisabledSites();
    save();
    emit configurationChanged();
}

int ContentBlocker::blockedRequestCount(const QUrl &url) const
{
    return m_blockedCounts.value(siteKey(url));
}

QString ContentBlocker::cosmeticStyleSheet(const QUrl &url) const
{
    const auto runtime = std::atomic_load(&m_runtime);
    if (!runtime->matcher || runtime->disabledSites.contains(siteKey(url))) {
        return {};
    }
    return runtime->matcher->cosmeticStyleSheet(url);
}

bool ContentBlocker::shouldBlock(const QUrl &requestUrl, const QUrl &sourceUrl,
    const QString &resourceType) const
{
    const auto runtime = std::atomic_load(&m_runtime);
    const auto sourceKey = siteKey(sourceUrl);
    if (!runtime->matcher || runtime->disabledSites.contains(sourceKey)
        || !runtime->matcher->shouldBlock(requestUrl, sourceUrl, resourceType)) {
        return false;
    }
    QPointer<ContentBlocker> guard(const_cast<ContentBlocker *>(this));
    QMetaObject::invokeMethod(const_cast<ContentBlocker *>(this), [guard, sourceUrl, sourceKey] {
        if (guard) {
            ++guard->m_blockedCounts[sourceKey];
            emit guard->blockedRequestCountChanged(sourceUrl);
        }
    }, Qt::QueuedConnection);
    return true;
}

QString ContentBlocker::siteKey(const QUrl &url)
{
    return url.host().toLower();
}

QString ContentBlocker::settingsPath() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("content-blocking/settings.json"));
}

QString ContentBlocker::listPath(const QString &id) const
{
    return QDir(m_dataRoot).filePath(
        QStringLiteral("content-blocking/lists/%1.txt").arg(id));
}

void ContentBlocker::load()
{
    QFile file(settingsPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    m_userRules = root.value(QStringLiteral("userRules")).toString();
    for (const auto &value : root.value(QStringLiteral("disabledSites")).toArray()) {
        m_disabledSites.insert(value.toString());
    }
    for (const auto &value : root.value(QStringLiteral("subscriptions")).toArray()) {
        const auto object = value.toObject();
        Subscription subscription;
        subscription.id = object.value(QStringLiteral("id")).toString();
        subscription.title = object.value(QStringLiteral("title")).toString();
        subscription.source = QUrl(object.value(QStringLiteral("source")).toString());
        subscription.license = object.value(QStringLiteral("license")).toString();
        subscription.updateAddress = QUrl(object.value(QStringLiteral("updateAddress")).toString());
        subscription.updateStatus = object.value(QStringLiteral("updateStatus")).toString();
        subscription.lastUpdated = object.value(QStringLiteral("lastUpdated")).toString();
        subscription.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        if (!subscription.id.isEmpty()) {
            m_subscriptions.append(std::move(subscription));
        }
    }
}

void ContentBlocker::save() const
{
    QJsonArray subscriptions;
    for (const auto &subscription : m_subscriptions) {
        subscriptions.append(QJsonObject{
            {QStringLiteral("id"), subscription.id},
            {QStringLiteral("title"), subscription.title},
            {QStringLiteral("source"), subscription.source.toString()},
            {QStringLiteral("license"), subscription.license},
            {QStringLiteral("updateAddress"), subscription.updateAddress.toString()},
            {QStringLiteral("updateStatus"), subscription.updateStatus},
            {QStringLiteral("lastUpdated"), subscription.lastUpdated},
            {QStringLiteral("enabled"), subscription.enabled},
        });
    }
    QJsonArray disabledSites;
    for (const auto &site : m_disabledSites) {
        disabledSites.append(site);
    }
    QDir().mkpath(QFileInfo(settingsPath()).absolutePath());
    QSaveFile file(settingsPath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("userRules"), m_userRules},
        {QStringLiteral("disabledSites"), disabledSites},
        {QStringLiteral("subscriptions"), subscriptions},
    }).toJson(QJsonDocument::Indented));
    file.commit();
}

void ContentBlocker::recompile()
{
    QString rules = m_userRules;
    for (const auto &subscription : std::as_const(m_subscriptions)) {
        if (!subscription.enabled) {
            continue;
        }
        QFile file(listPath(subscription.id));
        if (file.open(QIODevice::ReadOnly)) {
            rules += QLatin1Char('\n') + QString::fromUtf8(file.readAll());
        }
    }
    const auto generation = ++m_compileGeneration;
    ++m_activeCompilations;
    emit compilingChanged();
    auto *watcher = new QFutureWatcher<MatcherCompilation>(this);
    connect(watcher, &QFutureWatcher<MatcherCompilation>::finished, this,
        [this, watcher, generation] {
            const auto compilation = watcher->result();
            watcher->deleteLater();
            --m_activeCompilations;
            emit compilingChanged();
            if (generation != m_compileGeneration || !compilation.matcher) {
                return;
            }
            auto runtime = std::make_shared<Runtime>();
            runtime->matcher = compilation.matcher;
            runtime->disabledSites = m_disabledSites;
            std::atomic_store(&m_runtime,
                std::shared_ptr<const Runtime>(std::move(runtime)));
            m_compilationReport = compilation.report.toVariantMap();
            for (const auto &id : std::as_const(m_pendingCurrent)) {
                if (auto *subscription = findSubscription(id)) {
                    subscription->updateStatus = QStringLiteral("current");
                }
            }
            m_pendingCurrent.clear();
            save();
            emit subscriptionsChanged();
            emit rulesChanged();
        });
    watcher->setFuture(QtConcurrent::run([rules = std::move(rules)] {
        return ContentMatcher::compile(rules);
    }));
}

void ContentBlocker::replaceDisabledSites()
{
    const auto current = std::atomic_load(&m_runtime);
    auto replacement = std::make_shared<Runtime>();
    replacement->matcher = current->matcher;
    replacement->disabledSites = m_disabledSites;
    std::atomic_store(&m_runtime,
        std::shared_ptr<const Runtime>(std::move(replacement)));
}

ContentBlocker::Subscription *ContentBlocker::findSubscription(const QString &id)
{
    for (auto &subscription : m_subscriptions) {
        if (subscription.id == id) {
            return &subscription;
        }
    }
    return nullptr;
}

} // namespace tanto
