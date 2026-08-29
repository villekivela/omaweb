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
    Q_PROPERTY(bool ready READ ready CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)

public:
    explicit BrowserController(QString dataRoot, QObject *parent = nullptr);

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
    bool ready() const;
    QString errorMessage() const;

    Q_INVOKABLE void activateTab(const QString &tabId);
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
    void backRequested();
    void forwardRequested();
    void reloadRequested();

private:
    struct ClosedTab {
        TabState tab;
        bool valid = false;
    };

    void initialize();
    void ensureDefaultSpace();
    void ensureActiveTab();
    void persistTabs();
    void setActiveTab(const QString &tabId);
    static QUrl resolveInput(const QString &input);

    SessionStore m_store;
    SpaceListModel m_spaces;
    TabListModel m_tabs;
    QString m_activeSpaceId;
    QString m_activeSpaceName;
    QString m_activeTabId;
    QString m_errorMessage;
    ClosedTab m_closedTab;
    bool m_ready = false;
};

} // namespace tanto
