#include "TouchpadHold.h"

#include <QGuiApplication>
#include <QLoggingCategory>

#include <wayland-client.h>

#include <cstring>

namespace omaweb {

namespace {

    // The page glide's log, which a hold stops.
    Q_LOGGING_CATEGORY(glideLog, "omaweb.glide", QtWarningMsg)

    // The two interfaces of `pointer-gestures-unstable-v1` this uses, written out as
    // wayland-scanner would from the protocol's XML. Its generated code is Qt Wayland's
    // private one, and a request's or an event's number and signature are part of the stable
    // protocol and never move. Only the requests this sends and the events it reads carry their
    // argument types; libwayland reads the others' only to marshal them.
    extern const wl_interface holdInterface;

    const wl_interface *getHoldTypes[] = {&holdInterface, &wl_pointer_interface};
    const wl_message gesturesRequests[] = {
        {"get_swipe_gesture", "no", nullptr},
        {"get_pinch_gesture", "no", nullptr},
        {"release", "2", nullptr},
        {"get_hold_gesture", "3no", getHoldTypes},
    };
    const wl_interface gesturesInterface
        = {"zwp_pointer_gestures_v1", 3, 4, gesturesRequests, 0, nullptr};

    const wl_interface *holdBeginTypes[] = {nullptr, nullptr, &wl_surface_interface, nullptr};
    const wl_interface *holdEndTypes[] = {nullptr, nullptr, nullptr};
    const wl_message holdRequests[] = {{"destroy", "3", nullptr}};
    const wl_message holdEvents[] = {
        {"begin", "3uuou", holdBeginTypes},
        {"end", "3uui", holdEndTypes},
    };
    const wl_interface holdInterface
        = {"zwp_pointer_gesture_hold_v1", 3, 1, holdRequests, 2, holdEvents};

    constexpr uint32_t holdVersion = 3;
    constexpr uint32_t releaseRequest = 2;
    constexpr uint32_t getHoldRequest = 3;
    constexpr uint32_t destroyRequest = 0;

} // namespace

struct TouchpadHold::Native {
    struct HoldListener {
        void (*begin)(void *, wl_proxy *, uint32_t, uint32_t, wl_surface *, uint32_t);
        void (*end)(void *, wl_proxy *, uint32_t, uint32_t, int32_t);
    };

    TouchpadHold *owner = nullptr;
    wl_display *display = nullptr;
    wl_pointer *pointer = nullptr;
    wl_registry *registry = nullptr;
    wl_proxy *gestures = nullptr;
    wl_proxy *hold = nullptr;

    static void global(
        void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
    {
        auto *self = static_cast<Native *>(data);
        if (self->gestures != nullptr || version < holdVersion
            || std::strcmp(interface, gesturesInterface.name) != 0) {
            return;
        }
        self->gestures = static_cast<wl_proxy *>(
            wl_registry_bind(registry, name, &gesturesInterface, holdVersion));
        self->hold = wl_proxy_marshal_flags(
            self->gestures, getHoldRequest, &holdInterface, holdVersion, 0, nullptr, self->pointer);
        static const HoldListener listener {held, ended};
        wl_proxy_add_listener(self->hold,
            reinterpret_cast<void (**)(void)>(const_cast<HoldListener *>(&listener)), self);
        wl_display_flush(self->display);
        qCInfo(glideLog) << "listening for touchpad holds";
    }

    static void globalRemoved(void *, wl_registry *, uint32_t) { }

    static void held(
        void *data, wl_proxy *, uint32_t, uint32_t time, wl_surface *, uint32_t fingers)
    {
        qCInfo(glideLog) << "hold began at" << time << "with" << fingers << "fingers";
        emit static_cast<Native *>(data)->owner->held();
    }

    static void ended(void *, wl_proxy *, uint32_t, uint32_t time, int32_t cancelled)
    {
        qCInfo(glideLog) << "hold ended at" << time << (cancelled != 0 ? "cancelled" : "");
    }

    ~Native()
    {
        if (hold != nullptr) {
            wl_proxy_marshal_flags(
                hold, destroyRequest, nullptr, holdVersion, WL_MARSHAL_FLAG_DESTROY);
        }
        if (gestures != nullptr) {
            wl_proxy_marshal_flags(
                gestures, releaseRequest, nullptr, holdVersion, WL_MARSHAL_FLAG_DESTROY);
        }
        if (registry != nullptr) {
            wl_registry_destroy(registry);
        }
    }
};

TouchpadHold::TouchpadHold(QObject *parent)
    : QObject(parent)
{
    auto *application = qGuiApp;
    auto *wayland = application != nullptr
        ? application->nativeInterface<QNativeInterface::QWaylandApplication>()
        : nullptr;
    if (wayland == nullptr || wayland->display() == nullptr || wayland->pointer() == nullptr) {
        qCInfo(glideLog) << "no touchpad holds: no Wayland pointer";
        return;
    }
    m_native = std::make_unique<Native>();
    m_native->owner = this;
    m_native->display = wayland->display();
    m_native->pointer = wayland->pointer();
    // On the display's default queue, which Qt dispatches on the main thread, so `held` is
    // emitted there too. The globals arrive the next time Qt reads the display.
    m_native->registry = wl_display_get_registry(m_native->display);
    static const wl_registry_listener listener {Native::global, Native::globalRemoved};
    wl_registry_add_listener(m_native->registry, &listener, m_native.get());
    wl_display_flush(m_native->display);
}

TouchpadHold::~TouchpadHold() = default;

bool TouchpadHold::listening() const { return m_native != nullptr && m_native->hold != nullptr; }

} // namespace omaweb
