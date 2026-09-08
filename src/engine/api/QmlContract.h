#pragma once

#include <QMetaType>
#include <QStringList>

#include <span>

class QObject;

namespace omaweb::contract {

// One entry a QML-facing object owes its callers, and the check that says
// which entries an object is missing.
//
// Nothing here knows what the entries mean. It exists so that each contract —
// the engine view's, the blocker's — is the list it declares and nothing else,
// and so that a fake is held to exactly the list its real counterpart is.

struct Property {
    const char *name;
    QMetaType::Type type;
};

struct Method {
    const char *name;
    bool signal;
    int parameterCount;
    // Checked only where an entry names it. A signal's first argument is
    // usually what tells two overloads apart; the rest of the arity is the
    // signature.
    QMetaType::Type firstParameterType = QMetaType::UnknownType;
};

QStringList missing(
    const QObject &object, std::span<const Property> properties, std::span<const Method> methods);

} // namespace omaweb::contract
