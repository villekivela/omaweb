#include "StartupPhases.h"

#include <QDateTime>
#include <QLoggingCategory>

namespace omaweb {

namespace {

    Q_LOGGING_CATEGORY(startupLog, "omaweb.startup", QtWarningMsg)

} // namespace

void markStartupPhase(const char *phase)
{
    qCInfo(startupLog, "phase %s at %lld", phase, QDateTime::currentMSecsSinceEpoch());
}

} // namespace omaweb
