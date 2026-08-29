#pragma once

#include <QFlags>
#include <QObject>

namespace tanto {

class EngineCapabilities final {
    Q_GADGET

public:
    enum Capability {
        Navigation = 1 << 0,
        PersistentProfiles = 1 << 1,
        PrivateProfiles = 1 << 2,
        ContentBlocking = 1 << 3,
        KeyboardPageCommands = 1 << 4,
        Diagnostics = 1 << 5,
        RendererRecovery = 1 << 6,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)
    Q_FLAG(Capabilities)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EngineCapabilities::Capabilities)

} // namespace tanto
