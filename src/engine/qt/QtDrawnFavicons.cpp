#include "QtDrawnFavicons.h"

#include <QDir>
#include <QFile>

namespace omaweb {

void forgetDrawnFavicons(const SpaceStorage &storage)
{
    const QDir spaces(storage.spacesDirectory());
    for (const auto &spaceId : spaces.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDir profile(storage.profilePathFor(spaceId));
        // Chromium's favicon database and its rollback journal.
        QFile::remove(profile.filePath(QStringLiteral("Favicons")));
        QFile::remove(profile.filePath(QStringLiteral("Favicons-journal")));
    }
}

} // namespace omaweb
