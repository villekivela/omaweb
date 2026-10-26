#pragma once

#include <QObject>

#include <memory>

namespace omaweb {

// Fingers coming to rest on the touchpad, which is what stops a page's glide in GTK 4 (#808).
// Qt hears a touchpad only once the fingers move, as a scroll or a swipe, and its Wayland
// plugin does not bind the hold gesture of `zwp_pointer_gestures_v1`, so this binds it itself.
//
// It listens from construction where the compositor offers version 3 of the protocol, as
// Hyprland and wlroots do, and the seat has a pointer. Elsewhere, macOS and Qt's offscreen
// platform among them, it never fires. Destroy it before the application.
class TouchpadHold final : public QObject {
    Q_OBJECT

public:
    explicit TouchpadHold(QObject *parent = nullptr);
    ~TouchpadHold() override;

    // False off Wayland, before the compositor has listed its protocols, and where it offers no
    // hold gesture.
    bool listening() const;

signals:
    // Fingers came down on the touchpad and are not moving.
    void held();

private:
    struct Native;
    std::unique_ptr<Native> m_native;
};

} // namespace omaweb
