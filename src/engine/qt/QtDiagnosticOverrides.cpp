#include "QtDiagnosticOverrides.h"

#include <QQuickWebEngineProfile>
#include <QWebEngineClientHints>
#include <QWebEngineProfile>

namespace omaweb {

QtDiagnosticOverrides::QtDiagnosticOverrides(DiagnosticOverrides overrides, QObject *parent)
    : QObject(parent)
    , m_overrides(std::move(overrides))
{
}

// QML's profile and the widgets' one are separate classes carrying the same
// calls, as Content blocking finds (QtContentBlocker::attachToProfile).
void QtDiagnosticOverrides::attachToProfile(QObject *profileObject)
{
    if (m_overrides.empty()) {
        return;
    }
    const auto attach = [this](auto *profile) {
        profile->setHttpUserAgent(overriddenUserAgent(profile->httpUserAgent(), m_overrides));
        auto *hints = profile->clientHints();
        hints->setFullVersionList(overriddenFullVersionList(hints->fullVersionList(), m_overrides));
        hints->setFullVersion(overriddenFullVersion(hints->fullVersion(), m_overrides));
        profile->setHttpAcceptLanguage(
            overriddenAcceptLanguage(profile->httpAcceptLanguage(), m_overrides));
    };
    if (auto *profile = qobject_cast<QWebEngineProfile *>(profileObject)) {
        attach(profile);
    } else if (auto *profile = qobject_cast<QQuickWebEngineProfile *>(profileObject)) {
        attach(profile);
    }
}

} // namespace omaweb
