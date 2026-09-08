#pragma once

#include <QString>

namespace omaweb {

// Where one engine's Spaces live on disk (ADR 0008). A data root and an engine
// name are all it takes to say where a Space keeps its browser database and
// where that engine keeps its Engine profile, so a caller asking where
// something belongs asks here rather than spelling the layout out again.
//
// Nothing here touches the filesystem. Asking where something belongs is not
// the same as asking for it to exist, and a reader that created directories
// left every "is it there" test unable to fail.
//
// A window that keeps nothing holds no SpaceStorage at all rather than one
// built on an empty root: absence is the honest answer for a Private window,
// whose engine uses the shared temporary profile instead (ADR 0012).
class SpaceStorage {
public:
    SpaceStorage(QString dataRoot, QString engineName);

    QString dataRoot() const;
    QString engineName() const;

    // Where this engine keeps its own state for one Space: cookies, site
    // storage, cache, and whatever else it writes for itself.
    QString profilePathFor(const QString &spaceId) const;
    // Where one Space keeps its tabs, history and Site permissions. The
    // history search reads the same file from its own thread, so the layout is
    // named here rather than spelled out twice.
    QString databasePathFor(const QString &spaceId) const;

private:
    QString m_dataRoot;
    QString m_engineName;
};

} // namespace omaweb
