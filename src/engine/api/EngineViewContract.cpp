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
        {"loading", QMetaType::Bool},
        {"canGoBack", QMetaType::Bool},
        {"canGoForward", QMetaType::Bool},
        {"profilePath", QMetaType::QString},
        {"pageHasFocus", QMetaType::Bool},
        {"capabilities", QMetaType::Int},
    };
    struct RequiredMethod {
        const char *name;
        bool signal;
    };
    static constexpr RequiredMethod requiredMethods[] = {
        {"goBack", false},
        {"goForward", false},
        {"reloadPage", false},
        {"focusPage", false},
        {"rendererFailed", true},
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
                    && method.parameterCount() == 1
                    && method.parameterMetaType(0).id() == QMetaType::QString
                : method.methodType() != QMetaMethod::Signal && method.parameterCount() == 0;
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
