#pragma once

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <optional>

namespace omaweb {

// A theme's colours by name, as `omarchy-theme-color --all` prints them once
// Omarchy has resolved legacy names and derived the missing shades.
using OmarchyColours = QMap<QString, QString>;

// Fills an Omarchy template the way `omarchy-theme-set-templates` does:
// `{{ key }}`, `{{ key_strip }}` and `{{ key_rgb }}` for every colour, and
// `{{ mix a b amount }}` with its `_strip` and `_rgb` forms. A token it cannot
// fill is left as it is, as Omarchy leaves it.
QByteArray renderOmarchyTemplate(const QByteArray &templateText, const OmarchyColours &colours);

// Asks Omarchy for a theme's colours, read-only, from the `colors.toml` given.
// None where Omarchy is not installed or the file does not read.
OmarchyColours readOmarchyColours(const QString &coloursFile);

// The name Omarchy's theme menu shows for a theme's folder:
// `catppuccin-latte` reads "Catppuccin Latte".
QString omarchyThemeName(const QString &folder);

// The palette sources in the order they outrank each other (ADR 0016, ADR
// 0066): an `OMAWEB_THEME_FILE` alone, else the reader's `theme.json`, the
// browser theme they picked, the desktop's render and the built-in palette.
QStringList paletteSources(const QString &environmentOverride, const QString &readerTheme,
    const QString &pickedTheme, const QString &desktopTheme, const QString &builtInTheme);

// Where the browser theme is read from and written to, passed in so a test
// can point all of it at a temporary home.
struct BrowserThemePaths {
    // `~/.config/omarchy/themes`, which outranks the system's copy of a theme
    // of the same name, as `omarchy theme dir` resolves it.
    QString userThemes;
    // `$OMARCHY_PATH/themes`.
    QString systemThemes;
    // The reader's `~/.config/omarchy/themed/omaweb.json.tpl`, which Omarchy
    // renders the desktop's palette through.
    QString userTemplate;
    // The template Omaweb ships, for a machine where the reader's is absent.
    QString shippedTemplate;
    // `~/.local/state/omarchy/current`: the desktop theme's name and colours.
    QString desktopTheme;
    // The palette Omaweb renders for a picked theme, in its own state.
    QString rendered;

    static BrowserThemePaths fromEnvironment(
        const QString &dataRoot, const QString &shippedTemplate);
};

// The browser theme (GLOSSARY.md): the desktop's, or an installed Omarchy
// theme the reader picked for the browser alone (ADR 0066).
//
// A picked theme is rendered through the same template Omarchy renders the
// desktop's palette through, into `BrowserThemePaths::rendered`, which the
// `ThemeController` reads between the reader's `theme.json` and the desktop's
// render. Following the desktop, or a picked theme that is not installed,
// leaves no render there, so the desktop's palette is the one read. The
// picked theme's colours are watched, and so are the theme folders, so an
// edit renders again and a theme removed and installed again is followed.
class BrowserTheme final : public QObject {
    Q_OBJECT
    // The picked theme's folder, or empty to follow the desktop.
    Q_PROPERTY(QString choice READ choice NOTIFY changed)
    Q_PROPERTY(QString choiceName READ choiceName NOTIFY changed)
    // A theme is picked and is not installed, so the desktop's is drawn.
    Q_PROPERTY(bool missing READ missing NOTIFY changed)
    Q_PROPERTY(QString desktopName READ desktopName NOTIFY changed)
    // Every installed theme, by name, each a map of `folder`, `name`,
    // `background`, `foreground` and `accent`. The colours arrive after the
    // list does, and are empty until then.
    Q_PROPERTY(QVariantList themes READ themes NOTIFY themesChanged)
    // The desktop theme's `background`, `foreground` and `accent`.
    Q_PROPERTY(QVariantMap desktopColours READ desktopColours NOTIFY themesChanged)
    // The file that sets the palette over any browser theme, as the reader
    // would name it, or empty where none does.
    Q_PROPERTY(QString overrideSource READ overrideSource NOTIFY changed)
    // A picked theme is what the browser draws: it is installed and no file
    // sets the palette over it.
    Q_PROPERTY(bool drawn READ drawn NOTIFY changed)

public:
    using ColourSource = std::function<OmarchyColours(const QString &coloursFile)>;

    // `readerTheme` is the reader's `theme.json`, and `environmentOverride`
    // what `OMAWEB_THEME_FILE` names; either one sets the palette instead.
    explicit BrowserTheme(BrowserThemePaths paths, ColourSource colours = readOmarchyColours,
        QString readerTheme = {}, QString environmentOverride = {}, QObject *parent = nullptr);
    ~BrowserTheme() override;

    QString choice() const;
    QString choiceName() const;
    bool missing() const;
    QString desktopName() const;
    QVariantList themes() const;
    QVariantMap desktopColours() const;
    QString overrideSource() const;
    bool drawn() const;
    QString renderedPath() const;

    // The reader's `browser-theme`: a folder name, or empty to follow the
    // desktop. Rendered before this returns.
    void setChoice(const QString &folder);

    // Lists the installed themes again and reads their colours off the event
    // loop, as Settings asks when it shows them.
    Q_INVOKABLE void refreshThemes();

signals:
    void changed();
    void themesChanged();

private:
    QString themeDirectory(const QString &folder) const;
    void update();
    void applySwatches();
    void watch();
    QString renderState() const;
    void render();
    QString readOverride() const;

    BrowserThemePaths m_paths;
    ColourSource m_colours;
    QString m_readerTheme;
    QString m_environmentOverride;
    QString m_choice;
    bool m_missing = false;
    QString m_override;
    QString m_desktopName;
    // The choice the last update announced, so a change of it is announced.
    QString m_announcedChoice;
    // What the render on disk was made from, or none before the first, which
    // also clears one an earlier run left for a choice that no longer stands.
    std::optional<QString> m_renderState;
    QVariantList m_themes;
    QVariantMap m_desktopColours;
    QFileSystemWatcher m_watcher;
    QTimer m_poll;
    QFutureWatcher<QVariantList> m_swatches;
};

} // namespace omaweb
