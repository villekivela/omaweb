#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace omaweb {

// What the desktop asked Qt for by way of an input method, and what Qt has
// installed to answer it with. A desktop that names a plugin it did not install
// leaves Qt with no input context at all, and Qt then binds no Wayland
// text-input protocol, so composing silently does nothing and nothing is
// printed to say why.
struct InputMethodHost {
    // The modules the environment names, in the order Qt would try them.
    QStringList requestedModules {};
    // The keys the installed platform input context plugins answer to.
    QStringList installedModules {};

    static InputMethodHost fromEnvironment();
};

// False only when the desktop asked for a module and none of the ones it named
// are installed. A desktop that asks for nothing is not misconfigured.
bool inputMethodAvailable(const InputMethodHost &host);

// Empty when the desktop named no input method, because there is nothing to
// report about a reader who is not using one.
QString inputMethodDiagnostic(const InputMethodHost &host);

// The reader-facing answer, read by Settings and warned about once at startup.
class InputMethodReport final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString diagnostic READ diagnostic CONSTANT)

public:
    explicit InputMethodReport(InputMethodHost host, QObject *parent = nullptr);

    bool available() const;
    QString diagnostic() const;

private:
    bool m_available = true;
    QString m_diagnostic;
};

// Makes one report available to QML as `import Omaweb`. Call once per process,
// before loading QML that reads it.
void registerInputMethodReport(InputMethodReport *report);

} // namespace omaweb
