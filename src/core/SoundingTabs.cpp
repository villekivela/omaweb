#include "SoundingTabs.h"

#include <QQmlEngine>

#include <algorithm>

namespace omaweb {

namespace {

    const auto kState = QStringLiteral("state");
    const auto kPlaying = QStringLiteral("playing");
    const auto kPaused = QStringLiteral("paused");

    // What a page declares about itself and the desktop is willing to show. The
    // rest of the declaration is about what the controls can do, which a Private
    // window exports like any other window.
    const QStringList kDescriptiveKeys = {
        QStringLiteral("title"),
        QStringLiteral("artist"),
        QStringLiteral("album"),
        QStringLiteral("artwork"),
    };

} // namespace

SoundingTabs::SoundingTabs(QObject *parent)
    : QObject(parent)
{
}

SoundingTabs::TabSound &SoundingTabs::entryFor(const QString &tabId)
{
    const auto existing = std::find_if(
        m_tabs.begin(), m_tabs.end(), [&tabId](const TabSound &tab) { return tab.tabId == tabId; });
    if (existing != m_tabs.end()) {
        return *existing;
    }
    m_tabs.append(TabSound {tabId, {}, false, false, {}});
    return m_tabs.last();
}

bool SoundingTabs::isPlaying(const TabSound &tab)
{
    // A page making sound is playing, whatever it declares. What it declares
    // decides only for a page that has gone quiet, which is where a paused
    // video and a finished one differ.
    return tab.audible || tab.declared.value(kState).toString() == kPlaying;
}

bool SoundingTabs::hasMedia(const TabSound &tab)
{
    return isPlaying(tab) || tab.declared.value(kState).toString() == kPaused;
}

const SoundingTabs::TabSound *SoundingTabs::soundingTab() const
{
    // A tab that is playing outranks one that is paused however long ago each
    // started: a reader who pauses a video and leaves music running in another
    // tab means the music. Among equals, the one that started last.
    for (const auto playing : {true, false}) {
        for (auto index = m_tabs.size() - 1; index >= 0; --index) {
            const auto &tab = m_tabs.at(index);
            if (isPlaying(tab) == playing) {
                return &tab;
            }
        }
    }
    return nullptr;
}

void SoundingTabs::republish()
{
    // A tab with nothing playing holds nothing worth keeping: every report
    // carries the page's whole declaration, so the tab arrives complete when it
    // starts again. Dropping it is also what keeps this list in the order tabs
    // started in, so a tab that starts again is last rather than back where it
    // used to be.
    m_tabs.removeIf([](const TabSound &tab) { return !hasMedia(tab); });

    QVariantMap announcement;
    if (const auto *sounding = soundingTab()) {
        announcement.insert(QStringLiteral("tabId"), sounding->tabId);
        announcement.insert(QStringLiteral("playing"), isPlaying(*sounding));
        announcement.insert(QStringLiteral("canGoNext"),
            sounding->declared.value(QStringLiteral("canGoNext")).toBool());
        announcement.insert(QStringLiteral("canGoPrevious"),
            sounding->declared.value(QStringLiteral("canGoPrevious")).toBool());
        // ADR 0012 keeps a Private window out of everything the Spaces can see,
        // and the session bus is read by every application on the desktop. The
        // controls answer there like anywhere else; what is playing does not.
        if (!sounding->privateTab) {
            for (const auto &key : kDescriptiveKeys) {
                const auto value = sounding->declared.value(key).toString();
                if (!value.isEmpty()) {
                    announcement.insert(key, value);
                }
            }
            if (!announcement.contains(QStringLiteral("title")) && !sounding->tabTitle.isEmpty()) {
                announcement.insert(QStringLiteral("title"), sounding->tabTitle);
            }
        }
    }

    if (announcement == m_announcement) {
        return;
    }
    m_announcement = announcement;
    emit announcementChanged();
}

void SoundingTabs::reportSound(const QString &tabId, bool sounding, const QString &tabTitle,
    bool privateTab, const QVariantMap &declared)
{
    if (tabId.isEmpty()) {
        return;
    }
    auto &tab = entryFor(tabId);
    tab.audible = sounding;
    tab.tabTitle = tabTitle;
    tab.privateTab = privateTab;
    tab.declared = declared;
    republish();
}

void SoundingTabs::forget(const QString &tabId)
{
    if (m_tabs.removeIf([&tabId](const TabSound &tab) { return tab.tabId == tabId; }) == 0) {
        return;
    }
    republish();
}

QVariantMap SoundingTabs::announcement() const { return m_announcement; }

QString SoundingTabs::soundingTabId() const
{
    return m_announcement.value(QStringLiteral("tabId")).toString();
}

void registerSoundingTabs()
{
    qmlRegisterSingletonType<SoundingTabs>("Omaweb", 1, 0, "SoundingTabs",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SoundingTabs; });
}

} // namespace omaweb
