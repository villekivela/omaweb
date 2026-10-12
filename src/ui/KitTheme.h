#pragma once

// The third seam of the Omarchy adaptation, after the Quickshell shim and the
// adapters in src/ui (see docs/adr/0017-vendor-the-omarchy-component-kit.md
// and 0018-drive-the-kit-from-the-theme-palette.md).
//
// Kit components read colour and type from the `qs.Commons` `Color` and `Style`
// singletons, which resolve an Omarchy theme from `~/.local/state/omarchy`,
// `~/.config/omarchy/shell.toml` and `hyprctl`. Omaweb's theme palette is its
// source of truth instead, so this pushes the palette into those singletons
// and keeps pushing: the kit's own lookups are files and processes that land
// after startup, and a value written once would simply lose to them. The
// desktop's motion preference goes in the same way, from `SystemMotion`.

#include <QObject>
#include <QString>
#include <QStringList>

class QQmlEngine;

namespace omaweb {

class FontSettings;
class ThemeController;

class KitTheme final : public QObject {
    Q_OBJECT
    // Whether Omaweb's palette also drives the colours the desktop's
    // `shell.toml` names, which it does while the reader has picked a browser
    // theme the desktop is not on (ADR 0066).
    Q_PROPERTY(bool drivesSurfaces READ drivesSurfaces WRITE setDrivesSurfaces NOTIFY
            drivesSurfacesChanged)

public:
    // The reader's font settings outrank the theme's base size (#170) and
    // are told the theme's, so that reset has something to return to. Without
    // them, the theme's size is the size.
    KitTheme(QQmlEngine *engine, const ThemeController *theme, FontSettings *fonts = nullptr,
        QObject *parent = nullptr);

    bool drivesSurfaces() const;
    void setDrivesSurfaces(bool drives);

signals:
    void drivesSurfacesChanged();

private slots:
    void apply();

private:
    // Ask the kit to re-read the theme files it only reads at startup.
    void rereadKitSources();
    void followResets(QObject *target, const QStringList &properties);

    const ThemeController *m_theme = nullptr;
    FontSettings *m_fonts = nullptr;
    QObject *m_color = nullptr;
    QObject *m_style = nullptr;
    QObject *m_systemMotion = nullptr;
    bool m_applying = false;
    bool m_drivesSurfaces = false;
};

} // namespace omaweb
