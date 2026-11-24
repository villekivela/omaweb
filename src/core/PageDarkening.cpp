#include "PageDarkening.h"

#include "SettingsFile.h"

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView enabledKey("darken-pages");

} // namespace

PageDarkening::PageDarkening(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_settings(std::move(configRoot))
{
    connect(&m_settings, &SettingsFile::changed, this, [this](const QStringList &keys) {
        if (keys.contains(enabledKey)) {
            load();
        }
    });
    load();
}

bool PageDarkening::enabled() const { return m_enabled; }

void PageDarkening::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    // Applied when the file says it was written, as an edit made there is. A
    // write the file refuses changes nothing, and saying so redraws a switch
    // the reader flipped back to where it stands.
    if (!m_settings.set(enabledKey, enabled)) {
        emit enabledChanged();
    }
}

// A file that cannot be read the way it is written darkens nothing: only an
// explicit `true` does.
void PageDarkening::load()
{
    const auto value = m_settings.value(enabledKey).toBool(false);
    if (value != m_enabled) {
        m_enabled = value;
        emit enabledChanged();
    }
}

QStringList pageDarkeningEngineFlags(const QStringList &engineFlags)
{
    const auto switchName = QStringLiteral("--dark-mode-settings");
    QStringList settings;
    for (const auto &flag : engineFlags) {
        if (flag.startsWith(switchName + u'=')) {
            settings = flag.sliced(switchName.size() + 1).split(u',', Qt::SkipEmptyParts);
        }
    }
    // Chromium reads the names without regard to case.
    for (const auto &setting : settings) {
        if (setting.section(u'=', 0, 0)
                .trimmed()
                .compare(QStringLiteral("InversionAlgorithm"), Qt::CaseInsensitive)
            == 0) {
            return {};
        }
    }
    settings.append(QStringLiteral("InversionAlgorithm=4"));
    return {switchName + u'=' + settings.join(u',')};
}

} // namespace omaweb
