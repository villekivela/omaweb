#pragma once

#include "SyncFeature.h"

#include <QObject>

class SyncPlugin final : public QObject, public omaweb::SyncFeature {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OmawebSyncFeature_iid)
    Q_INTERFACES(omaweb::SyncFeature)

public:
    int contractVersion() const override;
    QObject *createController(omaweb::BrowserStateExchange *state, const QString &dataRoot,
        const QString &configRoot, QObject *parent) override;
};
