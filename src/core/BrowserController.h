#pragma once

#include "SessionStore.h"
#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QObject>
#include <QHash>
#include <QSharedPointer>
#include <QUrl>
#include <QVariantList>

namespace tanto {

class BrowserController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *spaces READ spaces CONSTANT)
    Q_PROPERTY(QAbstractItemModel *tabs READ tabs CONSTANT)
    Q_PROPERTY(QString activeSpaceId READ activeSpaceId NOTIFY activeSpaceChanged)
    Q_PROPERTY(QString activeSpaceName READ activeSpaceName NOTIFY activeSpaceChanged)
    Q_PROPERTY(QString activeTabId READ activeTabId NOTIFY activeTabChanged)
    Q_PROPERTY(QUrl activeUrl READ activeUrl NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeProfilePath READ activeProfilePath NOTIFY activeSpaceChanged)
    Q_PROPERTY(bool activeTabPinned READ activeTabPinned NOTIFY activeTabChanged)
    Q_PROPERTY(bool activeRendererFailed READ activeRendererFailed NOTIFY activeTabChanged)
    Q_PROPERTY(QString activeRendererFailureReason READ activeRendererFailureReason NOTIFY activeTabChanged)
    Q_PROPERTY(bool privateBrowsing READ privateBrowsing CONSTANT)
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory CONSTANT)
    Q_PROPERTY(bool acceptDownloads READ acceptDownloads CONSTANT)

public:
    enum PermissionDecision {
        Ask = 0,
        AllowOnce = 1,
        AllowPersistently = 2,
        Block = 3,
    };
    Q_ENUM(PermissionDecision)

    BrowserController(QString dataRoot, QString engineName, QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
        QSharedPointer<QHash<QString, int>> sessionPermissionDecisions,
        QObject *parent = nullptr);

    QAbstractItemModel *spaces();
    QAbstractItemModel *tabs();
    QString activeSpaceId() const;
    QString activeSpaceName() const;
    QString activeTabId() const;
    QUrl activeUrl() const;
    QString activeTitle() const;
    QString activeProfilePath() const;
    bool activeTabPinned() const;
    bool activeRendererFailed() const;
    QString activeRendererFailureReason() const;
    bool privateBrowsing() const;
    bool ready() const;
    QString errorMessage() const;
    QString downloadDirectory() const;
    bool acceptDownloads() const;

    Q_INVOKABLE void activateTab(const QString &tabId);
    Q_INVOKABLE QString createSpace(const QString &name);
    Q_INVOKABLE bool switchSpace(const QString &spaceId);
    Q_INVOKABLE bool renameSpace(const QString &spaceId, const QString &name);
    Q_INVOKABLE bool deleteSpace(const QString &spaceId, const QString &confirmationName);
    Q_INVOKABLE bool requestTabMoveToSpace(const QString &tabId, const QString &destinationSpaceId,
        bool hasEditedFormState);
    Q_INVOKABLE bool confirmTabMoveToSpace(const QString &tabId, const QString &destinationSpaceId);
    Q_INVOKABLE void openInput(const QString &input, bool inNewTab);
    Q_INVOKABLE void closeActiveTab();
    Q_INVOKABLE void reopenClosedTab();
    Q_INVOKABLE void toggleActivePinned();
    Q_INVOKABLE void updateActiveTab(const QUrl &url, const QString &title);
    Q_INVOKABLE void setActiveLoading(bool loading);
    Q_INVOKABLE void reportRendererFailure(const QString &reason);
    Q_INVOKABLE void recoverActiveTab();
    Q_INVOKABLE void requestBack();
    Q_INVOKABLE void requestForward();
    Q_INVOKABLE void requestReload();
    Q_INVOKABLE void recordVisit(const QUrl &url, const QString &title);
    Q_INVOKABLE QVariantList historySuggestions(const QString &query, int limit = 8) const;
    Q_INVOKABLE int permissionDecision(const QUrl &url, const QString &permission);
    Q_INVOKABLE bool setPermissionDecision(const QUrl &url, const QString &permission,
        int decision);
    Q_INVOKABLE QString recordDownload(const QString &runtimeId, const QUrl &url, const QString &path,
        const QString &state, qint64 receivedBytes, qint64 totalBytes);
    Q_INVOKABLE bool updateDownload(const QString &id, const QString &state,
        qint64 receivedBytes, qint64 totalBytes, const QString &error);
    Q_INVOKABLE QVariantList downloadHistory() const;

signals:
    void activeSpaceChanged();
    void activeTabChanged();
    void spaceSuspended(const QString &spaceId);
    void spaceRestored(const QString &spaceId);
    void tabMoveConfirmationRequested(const QString &tabId, const QString &destinationSpaceId);
    void backRequested();
    void forwardRequested();
    void reloadRequested();
    void closeWindowRequested();

private:
    struct ClosedTab {
        TabState tab;
        bool valid = false;
    };

    void initialize();
    void ensureDefaultSpace();
    void ensureActiveTab();
    bool persistTabs();
    void setActiveTab(const QString &tabId);
    static TabState makeBlankTab(const QString &spaceId);
    static QUrl resolveInput(const QString &input);
    static QString normalizedOrigin(const QUrl &url);
    QString sessionPermissionKey(const QString &origin, const QString &permission) const;

    SessionStore m_store;
    SpaceListModel m_spaces;
    TabListModel m_tabs;
    QString m_activeSpaceId;
    QString m_activeSpaceName;
    QString m_activeTabId;
    QString m_engineName;
    QString m_errorMessage;
    ClosedTab m_closedTab;
    bool m_ready = false;
    bool m_privateBrowsing = false;
    QSharedPointer<QHash<QString, int>> m_sessionPermissionDecisions;
};

} // namespace tanto
