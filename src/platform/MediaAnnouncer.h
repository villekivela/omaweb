#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace omaweb {

// What the desktop is told about the tab that is making sound, and what it
// sends back.
//
// Every other browser on the desktop answers the media keys and fills the bar's
// media widget, and the desktop reaches all of them the same way. This is the
// one place Omaweb talks that protocol: core says which tab sounds and what its
// page declares, and this puts it where the desktop looks.
//
// One player stands for the browser rather than one per tab. A bar showing a
// row per open tab is a bar nobody can read, and a media key needs one target
// rather than a choice.
//
// Where the running platform has no such protocol, `available` is false and
// `announce` does nothing, which costs the browser nothing on a desktop that
// reads none of this.
class MediaAnnouncer final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit MediaAnnouncer(QObject *parent = nullptr);
    ~MediaAnnouncer() override;

    bool available() const;

    // What `SoundingTabs` assembled: "tabId" and "playing", and whatever of
    // "title", "artist", "album", "artwork", "canGoNext" and "canGoPrevious"
    // the sounding tab is willing to say. An empty map withdraws the player.
    Q_INVOKABLE void announce(const QVariantMap &announcement);

signals:
    // One of "play", "pause", "playpause", "stop", "next" or "previous".
    void commanded(const QString &name);
};

// Makes `MediaAnnouncer` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerMediaAnnouncer();

} // namespace omaweb
