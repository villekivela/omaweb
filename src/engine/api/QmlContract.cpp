#include "QmlContract.h"

#include <QMetaMethod>
#include <QMetaObject>
#include <QObject>

namespace omaweb::contract {

QStringList missing(
    const QObject &object, std::span<const Property> properties, std::span<const Method> methods)
{
    QStringList absent;
    const auto *metaObject = object.metaObject();
    for (const auto &required : properties) {
        const auto index = metaObject->indexOfProperty(required.name);
        if (index < 0) {
            absent.append(QStringLiteral("property %1").arg(QString::fromLatin1(required.name)));
            continue;
        }
        const auto property = metaObject->property(index);
        if (property.metaType().id() != required.type) {
            absent.append(QStringLiteral("property %1 has type %2, expected %3")
                    .arg(QString::fromLatin1(required.name),
                        QString::fromLatin1(property.typeName()),
                        QString::fromLatin1(QMetaType(required.type).name())));
        }
    }

    for (const auto &required : methods) {
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
            absent.append(QStringLiteral("method or signal %1 has the wrong signature")
                    .arg(QString::fromLatin1(required.name)));
        }
    }
    return absent;
}

} // namespace omaweb::contract
