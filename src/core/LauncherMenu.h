#pragma once

#include <QObject>
#include <QString>

namespace omaweb {

class BrowserController;

// Where the launcher's menu comes from and goes to (ADR 0057).
struct LauncherMenuPaths {
    // The menu the package installed.
    QString source;
    // Elephant's directory for menus, which is the only place it reads them from.
    QString directory;
    // Linking is for a machine that has Elephant, and nothing is written on one that has not.
    bool elephantInstalled = false;

    // The installed menu beside this executable, Elephant's directory under the reader's
    // configuration, and whether `elephant` is on the path.
    static LauncherMenuPaths forThisMachine();
};

// Makes, keeps or removes the link that puts Omaweb's menu where Elephant reads it.
//
// With `enabled` it links the menu unless something is already at the path: a file the reader put
// there, a link of theirs, even a dangling one, is left as it is. A link of Omaweb's own is kept,
// and pointed at the menu installed now when it names another or a menu that is gone, as after an
// upgrade. Without `enabled` it removes Omaweb's own link and nothing else. Omaweb's own link is a
// symbolic link to a file named `omaweb/elephant/menus/omawebtabs.lua`.
void updateLauncherLink(const LauncherMenuPaths &paths, bool enabled);

// "Show tabs in the launcher": Walker lists Omaweb's tabs behind the `@` prefix, through a menu
// Elephant reads. The setting is on by default, is kept as a preference the Sync projection does
// not name, so it stays on this machine, and is shown only where Elephant is installed. The link
// is brought up to date when the browser starts and whenever the setting changes.
class LauncherMenu final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    LauncherMenu(BrowserController *browser, LauncherMenuPaths paths, QObject *parent = nullptr);

    bool available() const;
    bool enabled() const;
    void setEnabled(bool enabled);

signals:
    void enabledChanged();

private:
    BrowserController *m_browser;
    LauncherMenuPaths m_paths;
};

} // namespace omaweb
