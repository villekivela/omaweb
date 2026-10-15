#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>

#include <functional>
#include <utility>

namespace omaweb {

// One setting the desktop's settings portal holds, by namespace and key.
using PortalSetting = std::pair<QString, QString>;
using PortalSettingAnswer
    = std::function<void(const QString &nameSpace, const QString &key, const QVariant &value)>;

// Asks the settings portal once for each setting, without waiting, and hears
// every change to any setting from then on, for as long as `owner` lives.
// `answer` gets the value unwrapped from the variants the portal hands it in.
// A desktop with no portal, or no session bus, answers nothing, and `answer`
// is never called.
void followPortalSettings(
    QObject *owner, const QList<PortalSetting> &settings, PortalSettingAnswer answer);

} // namespace omaweb
