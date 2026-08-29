#include "EngineViewContract.h"

#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>

namespace tanto {

QStringList validateEngineViewContract(const QObject &adapter)
{
    static constexpr const char *requiredProperties[] = {
        "currentUrl",
        "pageTitle",
        "loading",
        "canGoBack",
        "canGoForward",
        "profilePath",
        "pageHasFocus",
    };
    static constexpr const char *requiredMethods[] = {
        "goBack",
        "goForward",
        "reloadPage",
        "focusPage",
        "rendererFailed",
    };

    QStringList missing;
    const auto *metaObject = adapter.metaObject();
    for (const auto *property : requiredProperties) {
        if (metaObject->indexOfProperty(property) < 0) {
            missing.append(QStringLiteral("property %1").arg(QString::fromLatin1(property)));
        }
    }

    for (const auto *method : requiredMethods) {
        bool found = false;
        for (int index = 0; index < metaObject->methodCount(); ++index) {
            if (metaObject->method(index).name() == method) {
                found = true;
                break;
            }
        }
        if (!found) {
            missing.append(QStringLiteral("method or signal %1").arg(QString::fromLatin1(method)));
        }
    }
    return missing;
}

} // namespace tanto
