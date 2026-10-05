#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace omaweb {

// What makes a Space a project's (`omaweb dev`): the folder on this machine it
// is for, the address its app is served at, and the agent command `:ask` runs
// there instead of the global one, or nothing for the global one. It is a
// property of an ordinary Space and is kept beside the Space records, never
// in them, so it stays on this machine and outside the Sync projection.
struct SpaceProject {
    QString directory;
    QString address;
    QString agentCommand;

    bool operator==(const SpaceProject &) const = default;
};

// The Space whose project directory is `directory` or the nearest folder above
// it, or nothing. Folders are compared whole, so `/src/ap` is not above
// `/src/app`, and neither path is resolved: a folder recorded from inside a
// container may not exist here.
QString nearestProjectSpace(const QString &directory, const QHash<QString, SpaceProject> &projects);

// The name of a new Space for `directory`: the folder's own name, with a
// number added when a Space already has it.
QString projectSpaceName(const QString &directory, const QStringList &takenNames);

// An agent command as `:ask` runs it in a project's Space: split as a shell
// splits a line, then `{dir}` replaced by `directory` inside each argument, so
// a path with a space stays one argument and no shell reads it.
QStringList projectAgentArguments(const QString &command, const QString &directory);

} // namespace omaweb
