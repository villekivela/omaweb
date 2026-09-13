#include "SyncPlugin.h"

#include "BrowserStateExchange.h"
#include "SyncController.h"

int SyncPlugin::contractVersion() const { return 2; }

QObject *SyncPlugin::createController(omaweb::BrowserStateExchange *state, const QString &dataRoot,
    const QString &configRoot, QObject *parent)
{
    if (!state || !state->eligible()) {
        return nullptr;
    }
    return new omaweb::SyncController(state, dataRoot, configRoot, parent);
}
