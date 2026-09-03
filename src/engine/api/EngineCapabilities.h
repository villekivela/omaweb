#pragma once

#include <QFlags>
#include <QObject>

namespace omaweb {

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
        // engine can supply one and not the next, and a command Omaweb cannot
        // carry out has to say so rather than do nothing.
        PageFind = 1 << 7,
        PageZoom = 1 << 8,
        Printing = 1 << 9,
        SiteFullscreen = 1 << 10,
        // A PDF drawn inside the engine's own sandbox. Without it a PDF is a
        // download, which is what an adapter that cannot show one does instead.
        InlinePdfViewing = 1 << 11,
        // The two parts of a site's security contract an engine can be
        // missing. Each is named on its own because Site information has to
        // say the engine cannot answer rather than show a reassuring blank:
        // whether a certificate failure can be reported and decided about at
        // all, and whether a third-party cookie can be refused. How much site
        // data a Space holds is not one of these — it is on disk wherever
        // PersistentProfiles says the engine keeps it.
        CertificateDecisions = 1 << 12,
        ThirdPartyCookieControl = 1 << 13,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)
    Q_FLAG(Capabilities)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EngineCapabilities::Capabilities)

} // namespace omaweb
