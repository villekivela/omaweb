#include "EngineViewContract.h"

#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>

namespace tanto {

QStringList validateEngineViewContract(const QObject &adapter)
{
    struct RequiredProperty {
        const char *name;
        QMetaType::Type type;
    };
    static constexpr RequiredProperty requiredProperties[] = {
        {"currentUrl", QMetaType::QUrl},
        {"pageTitle", QMetaType::QString},
        {"pageIconUrl", QMetaType::QUrl},
        {"loading", QMetaType::Bool},
        {"pageAudible", QMetaType::Bool},
        {"audioMuted", QMetaType::Bool},
        {"canGoBack", QMetaType::Bool},
        {"canGoForward", QMetaType::Bool},
        {"profilePath", QMetaType::QString},
        {"sharedProfile", QMetaType::QVariant},
        {"browserProfile", QMetaType::QVariant},
        {"pageHasFocus", QMetaType::Bool},
        {"capabilities", QMetaType::Int},
        {"contentBlocker", QMetaType::QVariant},
        {"blockedRequestCount", QMetaType::Int},
        {"keyboardNavigationConfiguration", QMetaType::QVariant},
        {"keyboardNavigationHintModeActive", QMetaType::Bool},
        {"keyboardNavigationScriptSource", QMetaType::QString},
        // The inspector the engine supplies, if it has one, and the palette it
        // is drawn in. The view is an opaque item the shell docks: no protocol,
        // no frontend type, nothing an engine without an inspector has to
        // pretend to have beyond reporting the capability off.
        {"developerToolsAttached", QMetaType::Bool},
        {"developerToolsView", QMetaType::QVariant},
        {"developerToolsColors", QMetaType::QVariant},
        // Find belongs to one tab, and a tab is one adapter, so the query and
        // where it has reached live here rather than in a table the shell keeps
        // beside the tabs.
        {"findQuery", QMetaType::QString},
        {"findMatchCount", QMetaType::Int},
        {"findActiveMatch", QMetaType::Int},
        {"zoomFactor", QMetaType::Double},
        // Fullscreen the site asked for, which is not fullscreen the reader
        // asked for: the origin is named so the notice can say who is holding
        // the screen.
        {"siteFullscreenActive", QMetaType::Bool},
        {"siteFullscreenOrigin", QMetaType::QString},
    };
    struct RequiredMethod {
        const char *name;
        bool signal;
        int parameterCount;
        QMetaType::Type firstParameterType = QMetaType::UnknownType;
    };
    static constexpr RequiredMethod requiredMethods[] = {
        {"goBack", false, 0},
        {"goForward", false, 0},
        {"reloadPage", false, 0},
        {"reloadPageBypassingCache", false, 0},
        {"stopLoading", false, 0},
        {"findText", false, 2},
        {"clearFind", false, 0},
        {"setZoomFactor", false, 1},
        // The adapter renders the page; presenting it belongs to the platform's
        // own print dialog, so the destination is named by the shell.
        {"printPage", false, 1},
        {"exitSiteFullscreen", false, 0},
        {"focusPage", false, 0},
        {"checkForEditedFormState", false, 1},
        {"acceptNewWindowRequest", false, 1},
        {"configureKeyboardNavigation", false, 1},
        {"attachDeveloperTools", false, 0},
        {"detachDeveloperTools", false, 0},
        {"inspectElement", false, 0},
        {"developerToolsClosed", true, 0},
        {"printFinished", true, 2, QMetaType::QString},
        {"pageContextRequested", true, 1},
        {"rendererFailed", true, 1, QMetaType::QString},
        {"newTabRequested", true, 2},
        {"auxiliaryWindowRequested", true, 2},
        {"windowCloseRequested", true, 0},
        {"backgroundTabRequested", true, 1, QMetaType::QUrl},
    };

    QStringList missing;
    const auto *metaObject = adapter.metaObject();
    for (const auto &required : requiredProperties) {
        const auto index = metaObject->indexOfProperty(required.name);
        if (index < 0) {
            missing.append(QStringLiteral("property %1").arg(QString::fromLatin1(required.name)));
            continue;
        }
        const auto property = metaObject->property(index);
        if (property.metaType().id() != required.type) {
            missing.append(QStringLiteral("property %1 has type %2, expected %3")
                .arg(QString::fromLatin1(required.name),
                    QString::fromLatin1(property.typeName()),
                    QString::fromLatin1(QMetaType(required.type).name())));
        }
    }

    for (const auto &required : requiredMethods) {
        bool found = false;
        for (int index = 0; index < metaObject->methodCount(); ++index) {
            const auto method = metaObject->method(index);
            if (method.name() != required.name) {
                continue;
            }
            const auto hasExpectedSignature = required.signal
                ? method.methodType() == QMetaMethod::Signal
                    && method.parameterCount() == required.parameterCount
                    && (required.firstParameterType == QMetaType::UnknownType
                        || method.parameterMetaType(0).id() == required.firstParameterType)
                : method.methodType() != QMetaMethod::Signal
                    && method.parameterCount() == required.parameterCount;
            if (hasExpectedSignature) {
                found = true;
                break;
            }
        }
        if (!found) {
            missing.append(QStringLiteral("method or signal %1 has the wrong signature")
                .arg(QString::fromLatin1(required.name)));
        }
    }
    return missing;
}

} // namespace tanto
