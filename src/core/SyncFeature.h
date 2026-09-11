#pragma once

#include <QObject>
#include <QString>
#include <QtPlugin>

namespace omaweb {

class BrowserController;
class ContentBlocker;
class KeyboardNavigation;

class SyncFeature {
public:
    virtual ~SyncFeature() = default;

    virtual int contractVersion() const = 0;
    virtual QObject *createController(BrowserController *browser, ContentBlocker *blocker,
        KeyboardNavigation *keyboardNavigation, const QString &dataRoot, const QString &configRoot,
        QObject *parent) = 0;
};

} // namespace omaweb

#define OmawebSyncFeature_iid "dev.omaweb.browser.SyncFeature/1"
Q_DECLARE_INTERFACE(omaweb::SyncFeature, OmawebSyncFeature_iid)
