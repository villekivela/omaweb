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

bool SoundingTabs::answers(const TabSound &tab)
{
    const auto state = tab.declared.value(kState).toString();
    return tab.audible || state == kPlaying || state == kPaused;
}

const SoundingTabs::TabSound *SoundingTabs::player() const
{
    for (auto index = m_tabs.size() - 1; index >= 0; --index) {
        if (answers(m_tabs.at(index))) {
            return &m_tabs.at(index);
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
    m_tabs.removeIf([](const TabSound &tab) { return !answers(tab); });

    QVariantMap announcement;
    if (const auto *sounding = player()) {
        announcement.insert(QStringLiteral("tabId"), sounding->tabId);
        // A page making sound is playing, whatever it declares. What it
        // declares decides only for a page that has gone quiet, which is where
        // a paused video and a finished one differ.
        const auto state = sounding->declared.value(kState).toString();
        announcement.insert(QStringLiteral("playing"), sounding->audible || state == kPlaying);
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

void SoundingTabs::reportSound(
    const QString &tabId, bool sounding, const QString &tabTitle, bool privateTab)
{
    if (tabId.isEmpty()) {
        return;
    }
    auto &tab = entryFor(tabId);
    tab.audible = sounding;
    tab.tabTitle = tabTitle;
    tab.privateTab = privateTab;
    republish();
}

void SoundingTabs::reportDeclared(const QString &tabId, const QVariantMap &declared)
{
    if (tabId.isEmpty()) {
        return;
    }
    entryFor(tabId).declared = declared;
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
