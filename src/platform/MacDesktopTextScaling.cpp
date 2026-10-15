#include "DesktopTextScaling.h"

namespace omaweb {

// macOS has no settings portal and no text scaling of its own for Omaweb to
// follow, so the factor stays 1.
DesktopTextScaling::DesktopTextScaling(QObject *parent)
    : QObject(parent)
{
}

DesktopTextScaling::~DesktopTextScaling() = default;

} // namespace omaweb
