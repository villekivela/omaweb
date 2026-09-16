#include "GlobalPrivacyControl.h"

#include "PrivacyFile.h"

#include <utility>

namespace omaweb {
namespace {

    constexpr QLatin1StringView enabledKey("global-privacy-control");

} // namespace

GlobalPrivacyControl::GlobalPrivacyControl(QString configRoot, QObject *parent)
    : QObject(parent)
    , m_configRoot(std::move(configRoot))
{
    load();
}

bool GlobalPrivacyControl::enabled() const { return m_enabled; }

void GlobalPrivacyControl::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    save();
    emit enabledChanged();
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
    const auto value = PrivacyFile::read(m_configRoot, enabledKey);
    m_enabled = value.isBool() ? value.toBool() : true;
}

void GlobalPrivacyControl::save() const { PrivacyFile::write(m_configRoot, enabledKey, m_enabled); }

} // namespace omaweb
