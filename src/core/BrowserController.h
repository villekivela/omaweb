#pragma once

#include "SessionStore.h"
#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QObject>
#include <QUrl>

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

public:
    BrowserController(QString dataRoot, QString engineName, QObject *parent = nullptr);
    BrowserController(QString dataRoot, QString engineName, bool privateBrowsing,
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
};

} // namespace tanto
