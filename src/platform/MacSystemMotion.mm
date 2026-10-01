#include "SystemMotion.h"

#import <AppKit/AppKit.h>

namespace omaweb {

namespace {

    bool macAsksToReduceMotion()
    {
        return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
    }

} // namespace

// The system's Reduce motion switch, read at start and again whenever the
// accessibility display options change.
SystemMotion::SystemMotion(QObject *parent)
    : QObject(parent)
{
    setAsks(Source::MacAccessibility, macAsksToReduceMotion());
    // This file is compiled without ARC, as the rest of Omaweb's AppKit code
    // is, so the observer's token is held here and let go with the object.
    id token = [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *) {
                    setAsks(Source::MacAccessibility, macAsksToReduceMotion());
                }];
    m_macObserver = [token retain];
}

SystemMotion::~SystemMotion()
{
    if (m_macObserver != nullptr) {
        id token = static_cast<id>(m_macObserver);
        [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:token];
        [token release];
    }
}

} // namespace omaweb
