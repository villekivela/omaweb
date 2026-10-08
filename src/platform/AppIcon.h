#pragma once

#include "OmarchyTheme.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

namespace omaweb {

// Which icon the launcher shows Omaweb under: the black and white mark the
// package installs, or that mark in the Omarchy theme's colours.
enum class AppIcon {
    BlackAndWhite,
    Theme,
};

// Every file the app icon is made of, passed in so a test can point them all
// at a temporary home.
struct AppIconPaths {
    OmarchyThemePaths omarchy;
    // The desktop entry the package installed, or empty where there is none,
    // as in a build run from its tree.
    QString installedEntry;
    // `$XDG_DATA_HOME/applications/omaweb.desktop`, which outranks the
    // installed entry wherever both are.
    QString userEntry;
    // Where the copies of the rendered icon the user entry names are kept.
    QString iconDirectory;

    static AppIconPaths fromEnvironment(const QString &dataRoot);

    // The template Omaweb installs for the Theme icon, and the icon Omarchy
    // renders it into for whichever theme is active.
    QString iconTemplate() const;
    QString renderedIcon() const;
};

// Puts the launcher's icon where the reader's choice says, and keeps it there
// while Omaweb runs (ADR 0031: a setting, so the launcher changes now).
//
// The Theme icon is Omarchy's to render: Omaweb installs
// `omaweb-icon.svg.tpl` beside `omaweb.json.tpl`, under the same rules, and
// Omarchy renders it on every theme switch. The launcher learns of it through
// a per-user copy of the installed desktop entry whose `Icon=` names a copy of
// the rendered icon. The copy is named by its contents because Omarchy's menu
// caches an icon by its path, and the rendered icon keeps one path across
// theme switches.
class AppIconLauncher final : public QObject {
    Q_OBJECT

public:
    AppIconLauncher(AppIconPaths paths, QString shippedTemplate, QObject *parent = nullptr);

    // Applies the choice now, and again whenever Omarchy renders a theme
    // while it is Theme.
    void setChoice(AppIcon choice);

private:
    void installTemplate();
    void removeTemplate();
    bool ownsUserEntry() const;
    void apply();
    void watch();
    void removeCopiesBut(const QString &kept);

    AppIconPaths m_paths;
    QString m_shippedTemplate;
    AppIcon m_choice = AppIcon::BlackAndWhite;
    QFileSystemWatcher m_watcher;
};

} // namespace omaweb
