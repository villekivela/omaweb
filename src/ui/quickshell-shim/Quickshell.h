#pragma once

// Stand-ins for the Quickshell types the vendored Omarchy component kit
// imports (see third_party/omarchy-shell/README.md). Shimming the dependency
// is what lets the kit stay a byte-for-byte copy: no vendored file is
// patched, so a sync is a review of upstream's diff rather than a merge.
//
// The scope is the subset `qs.Commons` and the Quickshell-free components in
// `qs.Ui` actually touch: an environment lookup, a watched config file, a
// short-lived command run for its output, the `Singleton` and `IpcHandler`
// types the kit's IPC registry declares, and the `Hyprland` event signal
// `Style` connects to. Layer-shell and Hyprland surfaces are out of scope, and
// so are the kit components that need them.

#include <QObject>
#include <QQmlListProperty>
#include <QString>
#include <QStringList>

class QQmlEngine;

namespace omaweb::quickshell {

// Registers the shim's types under the `Quickshell`, `Quickshell.Hyprland` and
// `Quickshell.Io` module URIs, puts the qmldir files that claim those URIs on
// the engine's import path, and picks the Qt Quick Controls style the kit
// needs. Call once per QML engine, before loading anything that imports the
// vendored kit.
void installShim(QQmlEngine &engine);

// The `Quickshell` singleton: `import Quickshell` then `Quickshell.env(...)`.
class Quickshell : public QObject {
    Q_OBJECT

public:
    explicit Quickshell(QObject *parent = nullptr);

    Q_INVOKABLE QString env(const QString &name) const;

    // API parity with upstream `Util.execDetached` / `Util.execArgv`. Browser
    // chrome has no business launching desktop helpers, so the call is
    // refused and logged instead of spawning anything.
    Q_INVOKABLE void execDetached(const QStringList &command);
};

// The `Singleton` root type a kit singleton such as `IpcRegistry` declares under `pragma
// Singleton`. Upstream's is a QObject whose default property takes the objects declared inside it,
// as `IpcHandler { id: bareHandler }` does there.
class Singleton : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<QObject> data READ data)
    Q_CLASSINFO("DefaultProperty", "data")

public:
    explicit Singleton(QObject *parent = nullptr);

    QQmlListProperty<QObject> data();
};

} // namespace omaweb::quickshell
