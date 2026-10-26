#pragma once

class QGuiApplication;
class QObject;

namespace omaweb {

// Logs each key the application hands an object, what it would type, and each move of the focus,
// so a key a script sent that did nothing can be traced to where it went. Because it writes what
// the reader typed, only a browser configured with OMAWEB_KEY_EVENT_LOG has it, as the runtime
// budget's is. Silent unless `omaweb.keys` is enabled, as with
// `QT_LOGGING_RULES=omaweb.keys.info=true`, and then it returns the filter it installed; otherwise
// it installs nothing and returns null.
QObject *installKeyEventLog(QGuiApplication *application);

} // namespace omaweb
