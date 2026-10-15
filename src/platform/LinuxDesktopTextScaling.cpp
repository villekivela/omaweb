#include "DesktopTextScaling.h"

#include "LinuxPortalSettings.h"

namespace omaweb {

namespace {

    const auto kGnomeInterface = QStringLiteral("org.gnome.desktop.interface");
    const auto kTextScalingFactor = QStringLiteral("text-scaling-factor");

} // namespace

DesktopTextScaling::DesktopTextScaling(QObject *parent)
    : QObject(parent)
{
    followPortalSettings(this, {PortalSetting {kGnomeInterface, kTextScalingFactor}},
        [this](const QString &nameSpace, const QString &key, const QVariant &value) {
            if (nameSpace == kGnomeInterface && key == kTextScalingFactor) {
                setFactor(portalTextScalingFactor(value));
            }
        });
}

DesktopTextScaling::~DesktopTextScaling() = default;

} // namespace omaweb
