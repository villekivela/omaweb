#include "FileChooser.h"

namespace omaweb {

// macOS has no portal, and Qt's dialog there is already the system's own panel.
bool FileChooser::ask(const QVariantMap &, QWindow *) { return false; }

void FileChooser::close() { forget(); }

void FileChooser::answered(uint, const QVariantMap &) { }

} // namespace omaweb
