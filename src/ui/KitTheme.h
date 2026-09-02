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
// after startup, and a value written once would simply lose to them.

#include <QObject>
#include <QString>
#include <QStringList>

class QQmlEngine;

namespace omaweb {

class ThemeController;

class KitTheme final : public QObject {
    Q_OBJECT

public:
    KitTheme(QQmlEngine *engine, const ThemeController *theme, QObject *parent = nullptr);

private slots:
    void apply();

private:
    void followResets(QObject *target, const QStringList &properties);

    const ThemeController *m_theme = nullptr;
    QObject *m_color = nullptr;
    QObject *m_style = nullptr;
    bool m_applying = false;
};

} // namespace omaweb
