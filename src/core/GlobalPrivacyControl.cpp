#include "GlobalPrivacyControl.h"

#include "SettingsFile.h"

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView enabledKey("global-privacy-control");

} // namespace

GlobalPrivacyControl::GlobalPrivacyControl(QString configRoot, QObject *parent)
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

bool GlobalPrivacyControl::enabled() const { return m_enabled; }

void GlobalPrivacyControl::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    // Applied when the file says it was written, the way an edit made there is.
    // A write the file refuses changes nothing, and saying so draws a switch
    // the reader flipped back to where it stands.
    if (!m_settings.set(enabledKey, enabled)) {
        emit enabledChanged();
    }
}

QByteArray GlobalPrivacyControl::headerName() { return QByteArrayLiteral("Sec-GPC"); }

QByteArray GlobalPrivacyControl::headerValue() { return QByteArrayLiteral("1"); }

// A getter on the prototype rather than a value on the instance, which is
// where the specification puts the attribute and what a page that inspects
// `Navigator.prototype` expects to find. Configurable, so a later definition
// by the engine itself replaces this one rather than throwing.
QString GlobalPrivacyControl::scriptSource()
{
    return QStringLiteral(R"JS((() => {
    Object.defineProperty(Navigator.prototype, 'globalPrivacyControl', {
        get() { return true; }, configurable: true, enumerable: true
    });
})();
)JS");
}

// A file that cannot be read the way it is written turns the signal off for
// nobody: only an explicit `false` does.
void GlobalPrivacyControl::load()
{
    const auto value = m_settings.value(enabledKey).toBool(true);
    if (value != m_enabled) {
        m_enabled = value;
        emit enabledChanged();
    }
}

} // namespace omaweb
