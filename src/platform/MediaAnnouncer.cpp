#include "MediaAnnouncer.h"

#include <QQmlEngine>

namespace omaweb {

#ifdef Q_OS_MACOS

// macOS carries this over Now Playing rather than over a bus, and Omaweb
// distributes no macOS build (ADR 0029). The development build says so rather
// than announcing to nothing.
MediaAnnouncer::MediaAnnouncer(QObject *parent)
    : QObject(parent)
{
}

MediaAnnouncer::~MediaAnnouncer() = default;

bool MediaAnnouncer::available() const { return false; }

void MediaAnnouncer::announce(const QVariantMap &announcement) { Q_UNUSED(announcement) }

#endif

void registerMediaAnnouncer()
{
    qmlRegisterSingletonType<MediaAnnouncer>("Omaweb", 1, 0, "MediaAnnouncer",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new MediaAnnouncer; });
}

} // namespace omaweb
