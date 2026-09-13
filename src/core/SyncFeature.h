#pragma once

#include <QObject>
#include <QString>
#include <QtPlugin>

namespace omaweb {

class BrowserStateExchange;

class SyncFeature {
public:
    virtual ~SyncFeature() = default;

    virtual int contractVersion() const = 0;
    virtual QObject *createController(BrowserStateExchange *state, const QString &dataRoot,
        const QString &configRoot, QObject *parent) = 0;
};

} // namespace omaweb

#define OmawebSyncFeature_iid "dev.omaweb.browser.SyncFeature/2"
Q_DECLARE_INTERFACE(omaweb::SyncFeature, OmawebSyncFeature_iid)
