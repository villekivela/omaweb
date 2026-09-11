#include "SyncPlugin.h"

#include "BrowserController.h"
#include "SyncController.h"

int SyncPlugin::contractVersion() const { return 1; }

QObject *SyncPlugin::createController(omaweb::BrowserController *browser,
    omaweb::ContentBlocker *blocker, omaweb::KeyboardNavigation *keyboardNavigation,
    const QString &dataRoot, const QString &configRoot, QObject *parent)
{
    if (!browser || browser->privateBrowsing() || !browser->sessionStore()->recordsState()) {
        return nullptr;
    }
    return new omaweb::SyncController(
        browser, blocker, keyboardNavigation, dataRoot, configRoot, parent);
}
