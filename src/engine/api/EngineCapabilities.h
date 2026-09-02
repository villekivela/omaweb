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
        DeveloperTools = 1 << 5,
        RendererRecovery = 1 << 6,
        // The everyday page operations. Each one is named separately because an
        // engine can supply one and not the next, and a command Tanto cannot
        // carry out has to say so rather than do nothing.
        PageFind = 1 << 7,
        PageZoom = 1 << 8,
        Printing = 1 << 9,
        SiteFullscreen = 1 << 10,
        // A PDF drawn inside the engine's own sandbox. Without it a PDF is a
        // download, which is what an adapter that cannot show one does instead.
        InlinePdfViewing = 1 << 11,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)
    Q_FLAG(Capabilities)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EngineCapabilities::Capabilities)

} // namespace tanto
