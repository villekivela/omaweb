#pragma once

#include "EngineCapabilities.h"

namespace omaweb::test {

inline int expectedMockCapabilities()
{
    return EngineCapabilities::Navigation | EngineCapabilities::PrivateProfiles
        | EngineCapabilities::ContentBlocking | EngineCapabilities::KeyboardPageCommands
        | EngineCapabilities::DeveloperTools | EngineCapabilities::RendererRecovery
        | EngineCapabilities::PageFind | EngineCapabilities::PageZoom | EngineCapabilities::Printing
        | EngineCapabilities::SiteFullscreen | EngineCapabilities::InlinePdfViewing
        | EngineCapabilities::CertificateDecisions | EngineCapabilities::ThirdPartyCookieControl;
}

inline int expectedQtCapabilities()
{
    int capabilities = expectedMockCapabilities() | EngineCapabilities::PersistentProfiles
        | EngineCapabilities::ProceduralCosmeticFiltering;
    // Only a build against the patched engine can host a Known extension, and
    // the adapter reports it from the same build option this reads.
#if OMAWEB_KNOWN_EXTENSIONS
    capabilities |= EngineCapabilities::KnownExtensions;
#endif
    // CNAME uncloaking is the Qt engine's once it carries the DNS alias
    // patch, and the build reads that from the engine it compiled against.
#if OMAWEB_CNAME_UNCLOAKING
    capabilities |= EngineCapabilities::CnameUncloaking;
#endif
    return capabilities;
}

} // namespace omaweb::test
