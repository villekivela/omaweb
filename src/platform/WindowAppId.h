#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QWindow>

#include <functional>

namespace omaweb {

// The name a compositor knows one window by, other than the application's.
// A Tab window opens as `omaweb-tab` (ADR 0062), so a Hyprland rule can send it
// to another monitor, while every other window keeps the application's
// `omaweb`.
//
// Qt sends the application's name for every window and has no public way to
// give one window another, so this goes through Qt's private Wayland window
// interface, and only on the Qt the browser was compiled against. Anywhere
// else, on another platform or another Qt, nothing is asked: `applied` stays
// false and the window keeps the application's name. The window is meant to
// carry a first title a rule can match instead.
class WindowAppId : public QObject {
    Q_OBJECT
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QString appId READ appId WRITE setAppId NOTIFY appIdChanged)
    // Whether the compositor was given the name.
    Q_PROPERTY(bool applied READ applied NOTIFY appliedChanged)

public:
    explicit WindowAppId(QObject *parent = nullptr);

    QWindow *window() const;
    void setWindow(QWindow *window);
    QString appId() const;
    void setAppId(const QString &appId);
    bool applied() const;

    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void windowChanged();
    void appIdChanged();
    void appliedChanged();

private:
    void watchSurface();
    void setApplied(bool applied);

    QPointer<QWindow> m_window;
    // The platform window already being watched, so it is watched once.
    const void *m_watched = nullptr;
    QString m_appId;
    bool m_applied = false;
};

// Asks the platform to send `appId` for the window's surface as soon as its
// role exists, which is before the compositor first maps it, and again for each
// role it is given later, calling `done` each time; for as long as `context`
// lives. Asked once per platform window. Does nothing where that cannot be asked safely.
// Implemented per platform.
void watchWindowRole(
    QWindow *window, const QString &appId, QObject *context, const std::function<void()> &done);

void registerWindowAppId();

} // namespace omaweb
