#pragma once

#include <QObject>
#include <QList>
#include <QHash>
#include <QSharedPointer>
#include <QUrl>

#include "SessionSiteState.h"

#include <memory>

class QTemporaryDir;

namespace omaweb {

class BrowserController;

class WindowManager final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int privateWindowCount READ privateWindowCount NOTIFY privateWindowCountChanged)
    Q_PROPERTY(QString privateProfilePath READ privateProfilePath NOTIFY privateSessionChanged)
    // A session launched with a debugging listener has no Private windows to
    // offer: everything in one would be readable through the listener.
    Q_PROPERTY(bool privateWindowsAvailable READ privateWindowsAvailable CONSTANT)

public:
    explicit WindowManager(QObject *parent = nullptr);
    explicit WindowManager(bool privateWindowsAvailable, QObject *parent = nullptr);
    WindowManager(QString configRoot, bool privateWindowsAvailable, QObject *parent = nullptr);
    ~WindowManager() override;

    BrowserController *createPrivateWindow();
    Q_INVOKABLE void openPrivateWindow();
    // A Private window for a launch that asked for one: the one shown most
    // recently, with the address as a new tab, or a new one with the address
    // in its first tab. Null when this session has no Private windows.
    Q_INVOKABLE omaweb::BrowserController *showPrivateWindow(const QUrl &url);
    // Opens a link another app handed over where the first Link rule that
    // takes it says, or in the Space on show where none does. Answers the
    // window that shows it, which is the one to bring forward: the browser, a
    // Private window, or none when a rule asks for a Private window and this
    // session has none.
    BrowserController *openFromOutside(BrowserController *browser, const QUrl &url);
    // The window reports itself when it comes forward, so the next launch
    // finds the Private window the reader saw last.
    Q_INVOKABLE void notePrivateWindowShown(QObject *controller);
    Q_INVOKABLE void releasePrivateWindow(QObject *controller);
    int privateWindowCount() const;
    QString privateProfilePath() const;
    bool privateWindowsAvailable() const;

signals:
    void privateWindowRequested(BrowserController *controller, const QString &profilePath);
    void privateWindowCountChanged();
    void privateSessionEnding();
    void privateSessionChanged();

private:
    BrowserController *openNewPrivateWindow(const QUrl &url);
    bool ensurePrivateSession();

    QString m_configRoot;
    bool m_privateWindowsAvailable = true;
    std::unique_ptr<QTemporaryDir> m_privateRoot;
    // The one shown most recently first.
    QList<BrowserController *> m_privateWindows;
    // The Site permissions every Private window has agreed to, one answer for
    // all of them, handed to each window's store.
    QSharedPointer<QHash<QString, int>> m_privatePermissionDecisions;
    // The site state the private windows share, alongside the Site permissions
    // they already do: third-party allowances and granted certificate
    // exceptions. All of it goes when the last of them closes.
    QSharedPointer<SessionSiteState> m_privateSiteState;
};

} // namespace omaweb
