#pragma once

// Stand-ins for the Quickshell types the vendored Omarchy component kit
// imports (see third_party/omarchy-shell/README.md). Shimming the dependency
// is what lets the kit stay a byte-for-byte copy: no vendored file is
// patched, so a sync is a review of upstream's diff rather than a merge.
//
// The scope is the subset `qs.Commons` and the Quickshell-free components in
// `qs.Ui` actually touch: an environment lookup, a watched config file, and a
// short-lived command run for its output. Layer-shell and Hyprland surfaces
// are out of scope, and so are the kit components that need them.

#include <QObject>
#include <QString>
#include <QStringList>

namespace omaweb::quickshell {

// Registers the shim's types under the `Quickshell` and `Quickshell.Io` module
// URIs, and picks the Qt Quick Controls style the kit needs. Call once per QML
// engine, before loading anything that imports the vendored kit.
void installShim();

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

} // namespace omaweb::quickshell
