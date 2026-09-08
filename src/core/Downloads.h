#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace omaweb {

class SessionStore;

// What deciding a Download disposition needs to know about the window that is
// asking. Site permissions and the reader's interactions are the window's, not
// the download list's, so the window answers these rather than being reached
// into.
class DownloadPermissions {
public:
    virtual ~DownloadPermissions() = default;

    // The origin permissions are keyed by, empty where the address has none.
    virtual QString permissionOrigin(const QUrl &url) const = 0;
    virtual bool originInteracted(const QUrl &url) const = 0;
    // The standing answer to "may this origin download by itself", in the
    // controller's PermissionDecision values.
    virtual int automaticDownloadDecision(const QString &origin) const = 0;
    virtual bool rememberAutomaticDownloadDecision(const QString &origin, int decision) = 0;
};

// One window's downloads: the ones running now and the Download records its
// Space kept, in one list with a role saying which. The type owns its own
// mutations, so a caller starts, answers or forgets a download and binds to
// what comes out; nothing has to be refreshed by hand.
//
// A row's identity is the type's own. A Private window keeps no Download
// record, so its running downloads have no record id and would otherwise be
// indistinguishable.
class Downloads final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    // Download activity: what the window's running downloads add up to.
    Q_PROPERTY(int running READ running NOTIFY activityChanged)
    Q_PROPERTY(qreal fraction READ fraction NOTIFY activityChanged)
    Q_PROPERTY(int finished READ finished NOTIFY activityChanged)
    // Read only where something binds to it, so progress does not rebuild a
    // list nobody is reading. That is why the model does not keep one.
    Q_PROPERTY(QVariantList runningDownloads READ runningDownloads NOTIFY activityChanged)
    // The held question on screen, empty when there is none. One at a time.
    Q_PROPERTY(QVariantMap question READ question NOTIFY questionChanged)

public:
    enum Role {
        RowIdRole = Qt::UserRole + 1,
        RecordIdRole,
        RuntimeIdRole,
        UrlRole,
        PathRole,
        FileNameRole,
        StateRole,
        ErrorRole,
        ReceivedBytesRole,
        TotalBytesRole,
        FractionRole,
        RunningRole,
    };
    Q_ENUM(Role)

    Downloads(SessionStore *store, DownloadPermissions *permissions, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int running() const;
    qreal fraction() const;
    int finished() const;
    QVariantList runningDownloads() const;
    QVariantMap question() const;

    // How many downloads this origin has running, which is what makes a second
    // one automatic rather than asked for.
    int activeCount(const QUrl &origin) const;

    // What Omaweb decides about a request before it starts. A file name that
    // is already taken still returns SaveDownloadAs.
    Q_INVOKABLE QVariantMap disposition(const QUrl &origin, const QString &fileName,
        const QString &mimeType, const QString &directory, bool answered = false) const;

    // Intake. The runtime id is `namespace + ":" + id`, and the namespace is
    // how a cancel or a retry finds the engine profile that started it.
    Q_INVOKABLE void started(const QString &runtimeId, const QUrl &sourceUrl, const QUrl &pageUrl,
        const QString &path, const QString &state, qint64 receivedBytes, qint64 totalBytes);
    Q_INVOKABLE void updated(const QString &runtimeId, const QString &state, qint64 receivedBytes,
        qint64 totalBytes, const QString &error);

    // A download the engine is holding, waiting for the reader. The queue spans
    // engine profiles, so it carries the namespace with the engine's token.
    Q_INVOKABLE void hold(const QString &downloadNamespace, const QString &token, int disposition,
        const QString &origin, const QUrl &sourceUrl, const QString &fileName, const QString &risk);
    // Answers the question on screen and presents the next one, if any.
    Q_INVOKABLE void answer(bool keep, const QString &path, int permissionDecision);

    Q_INVOKABLE void cancel(int row);
    Q_INVOKABLE void retry(int row);
    // A no-op where the row has no Download record to forget.
    Q_INVOKABLE bool forget(int row);

signals:
    void countChanged();
    void activityChanged();
    void questionChanged();

    // Reaching an engine is QML's to do: it holds the table from download
    // namespace to engine profile.
    void cancelRequested(const QString &downloadNamespace, const QString &runtimeId);
    void retryRequested(
        const QString &downloadNamespace, const QString &runtimeId, const QUrl &sourceUrl);
    void releaseRequested(
        const QString &downloadNamespace, const QString &token, const QString &path);
    void discardRequested(
        const QString &downloadNamespace, const QString &token, const QString &fileName);

    // What the window says about a download that has settled.
    void downloadCompleted(
        const QString &path, const QUrl &sourceUrl, const QUrl &pageUrl, const QString &fileName);
    void downloadFailed(const QString &fileName, const QString &error);

private:
    struct Row {
        QString rowId;
        QString recordId;
        QString runtimeId;
        QUrl url;
        QUrl pageUrl;
        QString path;
        QString state;
        QString error;
        qint64 receivedBytes = 0;
        qint64 totalBytes = 0;
        bool running = false;

        // How far along, or -1 where the size is not known.
        qreal fraction() const;
    };

    struct HeldDownload {
        QString downloadNamespace;
        QString token;
        int disposition = 0;
        QString origin;
        QUrl sourceUrl;
        QString fileName;
        QString risk;
    };

    static QString fileNameOf(const QString &path);
    static QString namespaceOf(const QString &runtimeId);

    int indexOfRuntimeId(const QString &runtimeId) const;
    void rowChanged(int row);
    void present();

    SessionStore *m_store = nullptr;
    DownloadPermissions *m_permissions = nullptr;
    QVector<Row> m_rows;
    QVector<HeldDownload> m_queue;
    // The question on screen, and whether there is one. A question is presented
    // only once the one before it has been answered.
    HeldDownload m_question;
    bool m_questionOpen = false;
    int m_finished = 0;
    quint64 m_nextRowId = 0;
};

// Makes `Downloads` available to QML as `import Omaweb`. The type is
// uncreatable: a window is given the one its controller holds. Call once per
// process, before loading QML.
void registerDownloads();

} // namespace omaweb
