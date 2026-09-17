#include "QtPageFonts.h"

#include "FontSettings.h"
#include "QtProfileSettings.h"

#include <QWebEngineSettings>
#include <QtGlobal>

#include <algorithm>

namespace omaweb {

QtPageFonts::QtPageFonts(FontSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
    if (!available()) {
        qWarning("Omaweb was built against Qt %s and is running on Qt %s, so a page's fonts "
                 "stay the engine's own.",
            QT_VERSION_STR, qVersion());
        return;
    }
    connect(m_settings, &FontSettings::pageFontsChanged, this, &QtPageFonts::apply);
}

bool QtPageFonts::available() const { return m_settings && QtProfileSettings::reachable(); }

void QtPageFonts::attachToProfile(QObject *profile)
{
    if (!available()) {
        return;
    }
    auto *settings = QtProfileSettings::of(profile);
    if (!settings) {
        qWarning("A page's fonts could not reach the profile's settings.");
        return;
    }
    const auto attached = std::ranges::any_of(
        m_profiles, [profile](const QPointer<QObject> &known) { return known == profile; });
    if (attached) {
        return;
    }
    if (!m_engineFontsRead) {
        readEngineFonts(settings);
    }
    m_profiles.emplace_back(profile);
    applyTo(settings);
}

void QtPageFonts::apply()
{
    std::erase_if(m_profiles, [](const QPointer<QObject> &profile) { return profile.isNull(); });
    for (const auto &profile : m_profiles) {
        if (auto *settings = QtProfileSettings::of(profile.data())) {
            applyTo(settings);
        }
    }
}

// A value the reader has not set is reset rather than written back as the
// default it reads as, so the engine's own answer stands in its own right.
void QtPageFonts::applyTo(QWebEngineSettings *settings) const
{
    using PageFamily = FontSettings::PageFamily;
    using PageSize = FontSettings::PageSize;
    const auto fonts = m_settings->pageFonts();
    if (m_settings->pageFamilyOverridden(PageFamily::Standard)) {
        settings->setFontFamily(QWebEngineSettings::StandardFont, fonts.standardFamily);
    } else {
        settings->resetFontFamily(QWebEngineSettings::StandardFont);
    }
    if (m_settings->pageFamilyOverridden(PageFamily::Fixed)) {
        settings->setFontFamily(QWebEngineSettings::FixedFont, fonts.fixedFamily);
    } else {
        settings->resetFontFamily(QWebEngineSettings::FixedFont);
    }
    if (m_settings->pageSizeOverridden(PageSize::Default)) {
        settings->setFontSize(QWebEngineSettings::DefaultFontSize, fonts.fontSize);
        // Code stays the engine's own step below prose: the reader named one
        // size, and the fixed size follows it by the distance the engine
        // keeps between its two defaults.
        settings->setFontSize(QWebEngineSettings::DefaultFixedFontSize,
            std::max(1, fonts.fontSize - (m_engineFontSize - m_engineFixedFontSize)));
    } else {
        settings->resetFontSize(QWebEngineSettings::DefaultFontSize);
        settings->resetFontSize(QWebEngineSettings::DefaultFixedFontSize);
    }
    if (m_settings->pageSizeOverridden(PageSize::Minimum)) {
        settings->setFontSize(QWebEngineSettings::MinimumFontSize, fonts.minimumFontSize);
    } else {
        settings->resetFontSize(QWebEngineSettings::MinimumFontSize);
    }
}

void QtPageFonts::readEngineFonts(const QWebEngineSettings *settings)
{
    m_engineFontsRead = true;
    m_engineFontSize = settings->fontSize(QWebEngineSettings::DefaultFontSize);
    m_engineFixedFontSize = settings->fontSize(QWebEngineSettings::DefaultFixedFontSize);
    m_settings->setEngineFonts({
        settings->fontFamily(QWebEngineSettings::StandardFont),
        settings->fontFamily(QWebEngineSettings::FixedFont),
        m_engineFontSize,
        settings->fontSize(QWebEngineSettings::MinimumFontSize),
    });
}

} // namespace omaweb
