#pragma once

#include <QObject>

namespace omaweb {

// What this build of Omaweb was compiled against, where that decides whether a
// capability can be reported at all.
//
// A Known extension needs the patched engine ADR 0049 ships rather than the
// distribution's QtWebEngine. The difference is not visible at runtime until
// something hangs, so the package that carries the patched engine says so at
// build time and the adapter reports the capability only then. An Omaweb built
// against a stock engine reports it absent, and every surface that would load
// an extension is missing rather than dead.
class EngineBuild final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool knownExtensions READ knownExtensions CONSTANT)
    // Whether the engine this build compiled against can hand a request
    // interceptor the host's DNS aliases, which CNAME uncloaking needs
    // (ADR 0050). Read from the engine's own headers when Omaweb is built.
    Q_PROPERTY(bool cnameUncloaking READ cnameUncloaking CONSTANT)

public:
    explicit EngineBuild(QObject *parent = nullptr);

    bool knownExtensions() const;
    bool cnameUncloaking() const;
};

// Makes `EngineBuild` available to QML as `import Omaweb.Engine`. Call once per
// process, before loading QML that uses it.
void registerEngineBuild();

} // namespace omaweb
