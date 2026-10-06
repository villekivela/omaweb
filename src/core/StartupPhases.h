#pragma once

namespace omaweb {

// Marks a point on the way from `main` to the window's first frame, so a launch can be taken
// apart phase by phase. Silent unless `omaweb.startup` is enabled, as with
// `QT_LOGGING_RULES=omaweb.startup.info=true`. Each mark is logged as
// `phase <name> at <milliseconds since the epoch>`, on the wall clock, so a script that started the
// process can set it against its own.
void markStartupPhase(const char *phase);

} // namespace omaweb
