#pragma once

#include <QObject>
#include <QSet>
#include <QHash>
#include <QSharedPointer>

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
    explicit WindowManager(QString engineName, QObject *parent = nullptr);
    WindowManager(QString engineName, bool privateWindowsAvailable, QObject *parent = nullptr);
    WindowManager(QString engineName, QString configRoot, bool privateWindowsAvailable,
        QObject *parent = nullptr);
    ~WindowManager() override;

    BrowserController *createPrivateWindow();
    Q_INVOKABLE void openPrivateWindow();
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
    bool ensurePrivateSession();

    QString m_engineName;
    QString m_configRoot;
    bool m_privateWindowsAvailable = true;
    std::unique_ptr<QTemporaryDir> m_privateRoot;
    QSet<BrowserController *> m_privateWindows;
    QSharedPointer<QHash<QString, int>> m_privatePermissionDecisions;
    // The site state the private windows share, alongside the Site permissions
    // they already do: third-party allowances and granted certificate
    // exceptions. All of it goes when the last of them closes.
    QSharedPointer<SessionSiteState> m_privateSiteState;
};

} // namespace omaweb
