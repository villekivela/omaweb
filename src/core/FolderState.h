#pragma once

#include <QString>
#include <QStringList>

namespace omaweb {

// A named row of the sidebar that holds tabs. It belongs to one Space, lasts
// until the reader removes it, and its place among the Space's Folders is its
// place in the list the store hands back. What it holds is a list of tab ids
// here rather than a field on each tab, because an older build rebuilds a tab's
// Sync record from a fixed list of fields and would lose one. An id may name a
// tab this machine does not have, which is ignored.
struct FolderState {
    QString id {};
    QString spaceId {};
    QString name {};
    QStringList tabIds {};
    // This machine's alone, and Sync never reads it.
    bool collapsed = false;

    bool operator==(const FolderState &) const = default;
};

} // namespace omaweb
