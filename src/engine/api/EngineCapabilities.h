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
        // Whether this engine can host a Known extension. An engine that
        // cannot is not missing a setting, it is missing the runtime: Omaweb
        // hides the surfaces rather than offering a control that would load
        // an extension into a browser that hangs on its first message
        // (ADR 0049).
        KnownExtensions = 1 << 14,
        // Whether Content blocking can check a request again under the names
        // in its host's CNAME chain. It needs the engine
        // to resolve the host for the interceptor, which the patched Qt engine
        // does and Ladybird does not (ADR 0050).
        CnameUncloaking = 1 << 15,
        // Whether the engine applies the procedural cosmetic rules Content
        // blocking hands it: the Qt engine runs the vendored matcher in each
        // frame, and Ladybird has no adapter to run it yet (ADR 0052).
        ProceduralCosmeticFiltering = 1 << 16,
        // Whether the adapter reports the certificate chain a page arrived
        // over, which Site information's certificate view shows. The chain a
        // certificate failure was raised for is reported wherever
        // CertificateDecisions is; the one a page loaded over needs the patched
        // Qt engine.
        PageCertificates = 1 << 17,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)
    Q_FLAG(Capabilities)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(EngineCapabilities::Capabilities)

void registerEngineCapabilities();

} // namespace omaweb
