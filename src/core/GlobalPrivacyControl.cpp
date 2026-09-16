#include "GlobalPrivacyControl.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

namespace omaweb {
namespace {

    // The file is named for the Settings section it belongs to rather than
    // for its one key, so the next privacy decision has somewhere to go.
    constexpr auto fileName = "privacy.json";
    constexpr auto enabledKey = "global-privacy-control";

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
    m_enabled = true;
    if (m_configRoot.isEmpty()) {
        return;
    }
    QFile file(QDir(m_configRoot).filePath(QLatin1String(fileName)));
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto value
        = QJsonDocument::fromJson(file.readAll()).object().value(QLatin1String(enabledKey));
    if (value.isBool()) {
        m_enabled = value.toBool();
    }
}

void GlobalPrivacyControl::save() const
{
    if (m_configRoot.isEmpty() || !QDir().mkpath(m_configRoot)) {
        return;
    }
    QSaveFile file(QDir(m_configRoot).filePath(QLatin1String(fileName)));
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject {{QLatin1String(enabledKey), m_enabled}})
            .toJson(QJsonDocument::Indented));
    file.commit();
}

} // namespace omaweb
