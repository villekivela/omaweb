#include "SpaceProject.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace omaweb {

QString nearestProjectSpace(const QString &directory, const QHash<QString, SpaceProject> &projects)
{
    if (directory.isEmpty()) {
        return {};
    }
    auto folder = QDir::cleanPath(directory);
    while (true) {
        for (auto it = projects.cbegin(); it != projects.cend(); ++it) {
            if (QDir::cleanPath(it->directory) == folder) {
                return it.key();
            }
        }
        const auto above = QFileInfo(folder).path();
        if (above == folder) {
            return {};
        }
        folder = above;
    }
}

QString projectSpaceName(const QString &directory, const QStringList &takenNames)
{
    auto name = QFileInfo(QDir::cleanPath(directory)).fileName();
    if (name.isEmpty()) {
        name = QDir::cleanPath(directory);
    }
    auto candidate = name;
    for (auto number = 2; takenNames.contains(candidate, Qt::CaseInsensitive); ++number) {
        candidate = QStringLiteral("%1 %2").arg(name).arg(number);
    }
    return candidate;
}

QStringList projectAgentArguments(const QString &command, const QString &directory)
{
    auto arguments = QProcess::splitCommand(command);
    for (auto &argument : arguments) {
        argument.replace(QStringLiteral("{dir}"), directory);
    }
    return arguments;
}

} // namespace omaweb
