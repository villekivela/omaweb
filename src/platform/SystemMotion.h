#pragma once

#include <QByteArrayView>
#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QVariant>

#include <array>

namespace omaweb {

// Whether the desktop asks applications not to move things. The chrome moves
// what the pointer did so the eye can follow it; a reader who has asked the
// desktop for no movement gets none from Omaweb either, whatever started it.
//
// Several places answer, and any one of them asking stills the chrome:
//
// - the desktop portal's `org.freedesktop.appearance` `reduced-motion`, the
//   freedesktop setting for exactly this;
// - Hyprland's `animations:enabled`, which is how a reader on Omarchy turns
//   motion off, read when the browser starts and again on each config reload;
// - GNOME's `org.gnome.desktop.interface` `enable-animations`, through the
//   same portal, which GTK desktops set and the GTK portal passes on;
// - on macOS, the system's Reduce motion accessibility setting.
//
// Each is read off the session as it changes, so nothing here costs anything
// while the chrome is still.
class SystemMotion final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool reduced READ reduced NOTIFY reducedChanged)

public:
    explicit SystemMotion(QObject *parent = nullptr);
    ~SystemMotion() override;

    bool reduced() const;

signals:
    void reducedChanged();

private:
    enum class Source { Portal, Hyprland, Gnome, MacAccessibility, Count };

    void setAsks(Source source, bool asks);

    std::array<bool, static_cast<std::size_t>(Source::Count)> m_asks {};
    // The macOS notification token, held as the header is read by C++ too.
    void *m_macObserver = nullptr;
};

// The portal's `reduced-motion` answer: 1 asks for reduced motion, 0 is no
// preference, and anything else is read as no preference, as the portal's
// documentation asks.
bool portalAsksToReduceMotion(const QVariant &reducedMotion);

// Hyprland's answer to `j/getoption animations:enabled`: its `int` is 0 while
// the compositor's own animations are off. Anything that is not that option's
// answer says nothing.
bool hyprlandAsksToReduceMotion(QByteArrayView getoptionJson);

// Whether a read from Hyprland's event socket holds the line announcing a
// config reload.
bool hyprlandConfigurationReloaded(QByteArrayView events);

enum class HyprlandSocket { Requests, Events };

// Where the running Hyprland instance listens, or empty when the session is
// not Hyprland's.
QString hyprlandSocketPath(const QProcessEnvironment &environment, HyprlandSocket socket);

// GNOME's `enable-animations`: false asks for no animation.
bool gnomeAsksToReduceMotion(const QVariant &enableAnimations);

// Makes `SystemMotion` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerSystemMotion();

} // namespace omaweb
