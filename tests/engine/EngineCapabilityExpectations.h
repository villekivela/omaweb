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
    return expectedMockCapabilities() | EngineCapabilities::PersistentProfiles;
}

} // namespace omaweb::test
