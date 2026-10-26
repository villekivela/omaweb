#pragma once

class QGuiApplication;
class QObject;

namespace omaweb {

// Logs each key the application hands an object, and each move of the focus, so a key a script
// sent that did nothing can be traced to where it went. Silent unless `omaweb.keys` is enabled, as
// with `QT_LOGGING_RULES=omaweb.keys.info=true`, and then it returns the filter it installed;
// otherwise it installs nothing and returns null, so a browser nobody is diagnosing pays nothing.
QObject *installKeyEventLog(QGuiApplication *application);

} // namespace omaweb
