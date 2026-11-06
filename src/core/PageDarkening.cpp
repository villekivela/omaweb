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

} // namespace omaweb
