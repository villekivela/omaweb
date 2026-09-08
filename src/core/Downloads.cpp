#include "Downloads.h"

#include "BrowserController.h"
#include "DownloadPolicy.h"
#include "SessionStore.h"

#include <QDir>
#include <QFileInfo>
#include <QQmlEngine>
#include <QUuid>

#include <algorithm>

namespace omaweb {

namespace {

    bool isRunning(const QString &state)
    {
        return state == QStringLiteral("in-progress") || state == QStringLiteral("requested");
    }

} // namespace

qreal Downloads::Row::fraction() const
{
    return totalBytes > 0 ? std::min(1.0, static_cast<qreal>(receivedBytes) / totalBytes) : -1.0;
}

Downloads::Downloads(SessionStore *store, DownloadPermissions *permissions, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_permissions(permissions)
{
    const auto history = m_store ? m_store->downloadHistory() : QVariantList {};
    m_rows.reserve(history.size());
    for (const auto &entry : history) {
        const auto record = entry.toMap();
        Row row;
        row.rowId = QString::number(++m_nextRowId);
        row.recordId = record.value(QStringLiteral("id")).toString();
        row.url = record.value(QStringLiteral("url")).toUrl();
        row.path = record.value(QStringLiteral("path")).toString();
        row.state = record.value(QStringLiteral("state")).toString();
        row.error = record.value(QStringLiteral("error")).toString();
        row.receivedBytes = record.value(QStringLiteral("receivedBytes")).toLongLong();
        row.totalBytes = record.value(QStringLiteral("totalBytes")).toLongLong();
        m_rows.append(std::move(row));
    }
}

int Downloads::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant Downloads::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case RowIdRole:
        return row.rowId;
    case RecordIdRole:
        return row.recordId;
    case RuntimeIdRole:
        return row.runtimeId;
    case UrlRole:
        return row.url;
    case PathRole:
        return row.path;
    case FileNameRole:
        return fileNameOf(row.path);
    case StateRole:
        return row.state;
    case ErrorRole:
        return row.error;
    case ReceivedBytesRole:
        return row.receivedBytes;
    case TotalBytesRole:
        return row.totalBytes;
    case FractionRole:
        return row.fraction();
    case RunningRole:
        return row.running;
    default:
        return {};
    }
}

QHash<int, QByteArray> Downloads::roleNames() const
{
    return {{RowIdRole, "rowId"}, {RecordIdRole, "recordId"}, {RuntimeIdRole, "runtimeId"},
        {UrlRole, "url"}, {PathRole, "path"}, {FileNameRole, "fileName"}, {StateRole, "state"},
        {ErrorRole, "error"}, {ReceivedBytesRole, "receivedBytes"}, {TotalBytesRole, "totalBytes"},
        {FractionRole, "fraction"}, {RunningRole, "running"}};
}

int Downloads::running() const
{
    return static_cast<int>(
        std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &row) { return row.running; }));
}

qreal Downloads::fraction() const
{
    qint64 received = 0;
    qint64 total = 0;
    for (const auto &row : m_rows) {
        if (!row.running) {
            continue;
        }
        received += row.receivedBytes;
        // One download of unknown size makes the whole of it unknown: a
        // fraction of a total that is missing a term would be a lie.
        if (total < 0 || row.totalBytes <= 0) {
            total = -1;
        } else {
            total += row.totalBytes;
        }
    }
    return total > 0 ? std::min(1.0, static_cast<qreal>(received) / total) : -1.0;
}

int Downloads::finished() const { return m_finished; }

QVariantList Downloads::runningDownloads() const
{
    QVariantList downloads;
    for (const auto &row : m_rows) {
        if (!row.running) {
            continue;
        }
        downloads.append(QVariantMap {{QStringLiteral("fileName"), fileNameOf(row.path)},
            {QStringLiteral("fraction"), row.fraction()}});
    }
    return downloads;
}

QVariantMap Downloads::question() const
{
    if (!m_questionOpen) {
        return {};
    }
    return {{QStringLiteral("token"), m_question.token},
        {QStringLiteral("disposition"), m_question.disposition},
        {QStringLiteral("origin"), m_question.origin},
        {QStringLiteral("sourceUrl"), m_question.sourceUrl},
        {QStringLiteral("fileName"), m_question.fileName},
        {QStringLiteral("risk"), m_question.risk}};
}

int Downloads::activeCount(const QUrl &origin) const
{
    if (!m_permissions) {
        return 0;
    }
    const auto normalized = m_permissions->permissionOrigin(origin);
    if (normalized.isEmpty()) {
        return 0;
    }
    return static_cast<int>(
        std::count_if(m_rows.cbegin(), m_rows.cend(), [this, &normalized](const Row &row) {
            return row.running && m_permissions->permissionOrigin(row.pageUrl) == normalized;
        }));
}

QVariantMap Downloads::disposition(const QUrl &origin, const QString &fileName,
    const QString &mimeType, const QString &directory, bool answered) const
{
    const auto normalized = m_permissions ? m_permissions->permissionOrigin(origin) : QString {};
    const auto kind = DownloadPolicy::riskKind(fileName, mimeType);
    const auto automatic = !answered && !normalized.isEmpty()
        && (!m_permissions->originInteracted(origin) || activeCount(origin) > 0);
    QVariantMap answer {
        {QStringLiteral("risk"), kind},
        {QStringLiteral("fileName"), fileName},
        {QStringLiteral("origin"), normalized},
        {QStringLiteral("automatic"), automatic},
    };
    const auto decide = [&answer](BrowserController::DownloadDisposition disposition) {
        answer.insert(QStringLiteral("disposition"), static_cast<int>(disposition));
        return answer;
    };
    if (automatic) {
        const auto decision = m_permissions->automaticDownloadDecision(normalized);
        if (decision == BrowserController::Block) {
            return decide(BrowserController::RefuseDownload);
        }
        if (decision == BrowserController::Ask) {
            return decide(BrowserController::AskDownloadPermission);
        }
    }
    if (!kind.isEmpty() && !answered) {
        return decide(BrowserController::ConfirmDownload);
    }
    if (!directory.isEmpty() && !fileName.isEmpty()
        && QFileInfo::exists(QDir(directory).filePath(fileName))) {
        return decide(BrowserController::SaveDownloadAs);
    }
    return decide(BrowserController::AcceptDownload);
}

void Downloads::started(const QString &runtimeId, const QUrl &sourceUrl, const QUrl &pageUrl,
    const QString &path, const QString &state, qint64 receivedBytes, qint64 totalBytes)
{
    // A runtime id names one download, and a second row under the same name
    // would be a row nothing could reach again.
    if (runtimeId.isEmpty() || !sourceUrl.isValid() || indexOfRuntimeId(runtimeId) >= 0) {
        return;
    }
    // The mark reports what has finished since the window was last idle, so a
    // download starting into an idle window starts the count again.
    if (running() == 0) {
        m_finished = 0;
    }
    Row row;
    row.rowId = QString::number(++m_nextRowId);
    row.runtimeId = runtimeId;
    row.url = sourceUrl;
    row.pageUrl = pageUrl;
    row.path = path;
    row.state = state;
    row.receivedBytes = receivedBytes;
    row.totalBytes = totalBytes;
    row.running = isRunning(state);
    if (m_store) {
        const auto recordId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        // A Private window keeps no Download record, so the row carries none
        // and stands on its own identity.
        if (m_store->recordDownload(recordId, sourceUrl, path, state, receivedBytes, totalBytes)) {
            row.recordId = recordId;
        }
    }
    beginInsertRows({}, 0, 0);
    m_rows.prepend(std::move(row));
    endInsertRows();
    emit countChanged();
    emit activityChanged();
}

void Downloads::updated(const QString &runtimeId, const QString &state, qint64 receivedBytes,
    qint64 totalBytes, const QString &error)
{
    const auto index = indexOfRuntimeId(runtimeId);
    if (index < 0) {
        return;
    }
    auto &row = m_rows[index];
    const auto wasRunning = row.running;
    row.state = state;
    row.error = error;
    row.receivedBytes = receivedBytes;
    row.totalBytes = totalBytes;
    row.running = isRunning(state);
    if (m_store && !row.recordId.isEmpty()) {
        m_store->updateDownload(row.recordId, state, receivedBytes, totalBytes, error);
    }
    if (state == QStringLiteral("completed")) {
        m_finished += 1;
        emit downloadCompleted(row.path, row.url, row.pageUrl, fileNameOf(row.path));
    } else if (state == QStringLiteral("interrupted")) {
        emit downloadFailed(fileNameOf(row.path), error);
    }
    // A settled download a Space did not record is not a Download record and
    // cannot become one, so nothing is left to show once it stops running.
    if (!row.running && row.recordId.isEmpty()) {
        beginRemoveRows({}, index, index);
        m_rows.removeAt(index);
        endRemoveRows();
        emit countChanged();
    } else {
        rowChanged(index);
    }
    if (wasRunning || isRunning(state)) {
        emit activityChanged();
    }
}

void Downloads::hold(const QString &downloadNamespace, const QString &token, int disposition,
    const QString &origin, const QUrl &sourceUrl, const QString &fileName, const QString &risk)
{
    if (token.isEmpty()) {
        return;
    }
    m_queue.append(
        HeldDownload {downloadNamespace, token, disposition, origin, sourceUrl, fileName, risk});
    present();
}

void Downloads::answer(bool keep, const QString &path, int permissionDecision)
{
    if (!m_questionOpen) {
        return;
    }
    const auto held = m_question;
    m_question = {};
    m_questionOpen = false;
    if (m_permissions && permissionDecision > BrowserController::Ask) {
        m_permissions->rememberAutomaticDownloadDecision(held.origin, permissionDecision);
    }
    if (keep) {
        emit releaseRequested(held.downloadNamespace, held.token, path);
    } else {
        emit discardRequested(held.downloadNamespace, held.token, held.fileName);
    }
    present();
    if (!m_questionOpen) {
        emit questionChanged();
    }
}

void Downloads::cancel(int row)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const auto &target = m_rows.at(row);
    if (target.runtimeId.isEmpty()) {
        return;
    }
    emit cancelRequested(namespaceOf(target.runtimeId), target.runtimeId);
}

// Unlike a cancel, a retry is worth asking for on a row with no engine left
// behind it: the namespace comes out empty and the window asks the page for the
// address again.
void Downloads::retry(int row)
{
    if (row < 0 || row >= m_rows.size()) {
        return;
    }
    const auto &target = m_rows.at(row);
    emit retryRequested(namespaceOf(target.runtimeId), target.runtimeId, target.url);
}

bool Downloads::forget(int row)
{
    if (row < 0 || row >= m_rows.size()) {
        return false;
    }
    const auto recordId = m_rows.at(row).recordId;
    if (recordId.isEmpty() || !m_store || !m_store->forgetDownload(recordId)) {
        return false;
    }
    beginRemoveRows({}, row, row);
    m_rows.removeAt(row);
    endRemoveRows();
    emit countChanged();
    emit activityChanged();
    return true;
}

QString Downloads::fileNameOf(const QString &path)
{
    const auto separator
        = std::max(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    return separator >= 0 ? path.mid(separator + 1) : path;
}

QString Downloads::namespaceOf(const QString &runtimeId)
{
    const auto separator = runtimeId.indexOf(QLatin1Char(':'));
    return separator > 0 ? runtimeId.left(separator) : QString {};
}

int Downloads::indexOfRuntimeId(const QString &runtimeId) const
{
    if (runtimeId.isEmpty()) {
        return -1;
    }
    for (int index = 0; index < m_rows.size(); ++index) {
        if (m_rows.at(index).runtimeId == runtimeId) {
            return index;
        }
    }
    return -1;
}

void Downloads::rowChanged(int row)
{
    const auto position = index(row);
    emit dataChanged(position, position);
}

// One question at a time is the type's invariant, not a caller's early return:
// the next held download is presented only once the one on screen is answered.
void Downloads::present()
{
    if (m_questionOpen || m_queue.isEmpty()) {
        return;
    }
    m_question = m_queue.takeFirst();
    m_questionOpen = true;
    emit questionChanged();
}

void registerDownloads()
{
    qmlRegisterUncreatableType<Downloads>("Omaweb", 1, 0, "Downloads",
        QStringLiteral("A window is given the downloads its controller holds."));
}

} // namespace omaweb
