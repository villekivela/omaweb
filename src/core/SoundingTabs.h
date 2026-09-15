#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <QList>

namespace omaweb {

// Which tab is making sound, and what its page says about what is playing.
//
// One process, one answer. A reader with three Spaces open has one pair of
// media keys and one media widget on the bar, so the browser owes the desktop
// one player rather than one per tab or one per window. This is where the
// windows put what they hear from their pages, and where the platform layer
// reads the single tab the desktop is told about.
//
// The tab that answers is the last one to have started, which is what makes a
// media key predictable: a second page starting takes the keys, and when it
// stops they fall back to the one that was playing before it.
//
// A tab whose page declares it is paused is still the player. Pausing from the
// desktop would otherwise withdraw the player that the next key press needs,
// and the reader would have to find the tab to start it again. A tab that goes
// silent while declaring nothing, and one whose page declares that its media
// session is over, both leave.
class SoundingTabs final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap announcement READ announcement NOTIFY announcementChanged)

public:
    explicit SoundingTabs(QObject *parent = nullptr);

    // Whether this tab is making sound, with the title its own row carries and
    // whether it belongs to a Private window. The title is what the desktop is
    // told when the page declares nothing of its own.
    Q_INVOKABLE void reportSound(
        const QString &tabId, bool sounding, const QString &tabTitle, bool privateTab);
    // What the page declares through its own media session: "state" as one of
    // "none", "playing" or "paused", and the optional "title", "artist",
    // "album", "artwork", "canGoNext" and "canGoPrevious". An empty map is a
    // page that declares nothing, which is not the same as one declaring that
    // nothing is playing.
    Q_INVOKABLE void reportDeclared(const QString &tabId, const QVariantMap &declared);
    // The tab is gone: closed, or its engine discarded. Nothing left to play.
    Q_INVOKABLE void forget(const QString &tabId);

    // Empty when no tab is making sound, which is what withdrawing the player
    // means. Otherwise "tabId" and "playing", and, for a tab outside a Private
    // window, whatever of "title", "artist", "album" and "artwork" is known.
    QVariantMap announcement() const;
    // The tab a key the desktop sends is for, which is the tab the desktop was
    // told about. Empty where no tab is making sound.
    Q_INVOKABLE QString soundingTabId() const;

signals:
    void announcementChanged();

private:
    struct TabSound {
        QString tabId;
        QString tabTitle;
        bool privateTab = false;
        bool audible = false;
        QVariantMap declared;
    };

    TabSound &entryFor(const QString &tabId);
    const TabSound *player() const;
    static bool answers(const TabSound &tab);
    void republish();

    // In the order the tabs started, so the last one to start is the last one
    // here and the one the desktop reaches.
    QList<TabSound> m_tabs;
    QVariantMap m_announcement;
};

// Makes `SoundingTabs` available to QML as `import Omaweb`. One instance per
// process, because one desktop hears one player.
void registerSoundingTabs();

} // namespace omaweb
