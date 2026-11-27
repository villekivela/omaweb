#pragma once

#include <QString>

namespace omaweb {

// Which engine a browser was built around, and the names that keep the Qt and
// Ladybird browsers apart on disk. Each variant has its own settings, Spaces,
// bookmarks and history, so the two never write the same file and an alpha
// engine cannot reach the reader's real profile (ADR 0073). Sync can carry
// bookmarks between them if the reader turns it on.
//
// Nothing here touches the filesystem.
class EngineVariant {
public:
    enum Kind { Qt, Ladybird };

    // Qt derives the application data directory from this name, so the two
    // variants' data roots are siblings, not one inside the other.
    static QString applicationName(Kind kind);
    // The variant's configuration directory under `configHome`, which is
    // XDG_CONFIG_HOME or ~/.config.
    static QString configRoot(Kind kind, const QString &configHome);
};

} // namespace omaweb
