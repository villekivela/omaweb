#include "KitTheme.h"

#include "ThemeController.h"

#include <QColor>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QVariantMap>

namespace omaweb {
namespace {

    // Writing only on a difference is what stops the re-apply from chasing its own
    // change signals.
    void assign(QObject *target, const QString &name, const QVariant &value)
    {
        const auto key = name.toUtf8();
        if (!value.isValid() || target->property(key.constData()) == value) {
            return;
        }
        target->setProperty(key.constData(), value);
    }

    QVariant colorFrom(const QVariantMap &palette, const QString &key)
    {
        const QColor color(palette.value(key).toString());
        return color.isValid() ? QVariant::fromValue(color) : QVariant {};
    }

} // namespace

KitTheme::KitTheme(QQmlEngine *engine, const ThemeController *theme, QObject *parent)
    : QObject(parent)
    , m_theme(theme)
{
    m_color = engine->singletonInstance<QObject *>(
        QStringLiteral("qs.Commons"), QStringLiteral("Color"));
    m_style = engine->singletonInstance<QObject *>(
        QStringLiteral("qs.Commons"), QStringLiteral("Style"));
    if (!m_color || !m_style) {
        qWarning("The Omarchy kit's qs.Commons singletons are unavailable, so kit "
                 "components will draw with the kit's own colour and type.");
        return;
    }

    // The kit reads a theme's `colors.toml` and `shell.toml` once and expects a
    // running shell to be handed new values over IPC when the desktop switches
    // theme -- `Color.qml` says so, and sets `watchChanges: false` on both.
    // Omaweb is not that shell and has no such channel, so a completed theme
    // reload is the signal: the kit is asked to read them again before
    // Omaweb's own values go back on top. A reload is deliberately distinct
    // from paletteChanged because two desktop palettes can normalize to the
    // same colours while carrying different `[controls]` values. Without
    // this, a Pinned tile's border, the current tab's fill, the address field's
    // edge, and profile buttons can stay on the theme the reader has left.
    if (!m_color->property("colorsFile").value<QObject *>()
        || !m_color->property("shellFile").value<QObject *>()) {
        qWarning("The Omarchy kit no longer exposes colorsFile and shellFile, so its control "
                 "chrome will not follow a theme switch until the browser restarts.");
    }
    connect(m_theme, &ThemeController::paletteChanged, this, &KitTheme::apply);
    connect(m_theme, &ThemeController::themeReloaded, this, [this] {
        rereadKitSources();
        apply();
    });
    // Every one of the kit's own sources — a watched colours.toml, a shell.toml
    // that resets Style wholesale when it fails to load, an `hyprctl` run — is
    // asynchronous, so being the first writer is not the same as being the
    // authority. Re-applying whenever one of them lands is.
    followResets(m_color,
        { QStringLiteral("foreground"), QStringLiteral("background"), QStringLiteral("accent"),
            QStringLiteral("muted"), QStringLiteral("urgent") });
    followResets(m_style,
        { QStringLiteral("fontFamily"), QStringLiteral("resolvedFontFamily"),
            QStringLiteral("fontBaseSize") });
    apply();
}

void KitTheme::rereadKitSources()
{
    for (const auto *name : { "colorsFile", "shellFile" }) {
        if (auto *view = m_color->property(name).value<QObject *>()) {
            // Synchronous in the shim, so the kit has re-parsed both files by
            // the time this returns and `apply()` can put Omaweb's palette
            // back over whatever they reset.
            QMetaObject::invokeMethod(view, "reload");
        }
    }
}

void KitTheme::apply()
{
    if (m_applying) {
        return;
    }
    m_applying = true;
    const auto palette = m_theme->palette();
    assign(m_color, QStringLiteral("foreground"), colorFrom(palette, QStringLiteral("text")));
    // The kit paints `background` as a solid surface, so it takes the opaque
    // window rather than the alpha Omaweb's own chrome lets the desktop through.
    assign(
        m_color, QStringLiteral("background"), colorFrom(palette, QStringLiteral("windowOpaque")));
    // A private window's accent differs from the main window's, and the two
    // share this process. The singleton carries the ordinary palette; the
    // adapters in src/ui carry the per-window difference.
    assign(m_color, QStringLiteral("accent"), colorFrom(palette, QStringLiteral("accent")));
    assign(m_color, QStringLiteral("muted"), colorFrom(palette, QStringLiteral("mutedText")));
    // The kit reaches for `urgent` in its own destructive states, so the theme
    // owns that colour too rather than leaving the kit's default beside
    // Omaweb's own notices in the same window.
    assign(m_color, QStringLiteral("urgent"), colorFrom(palette, QStringLiteral("urgent")));

    const auto font = palette.value(QStringLiteral("font")).toMap();
    const auto family = font.value(QStringLiteral("family"));
    // Upstream keeps a fontconfig alias in `fontFamily` and the name it
    // resolves to in `resolvedFontFamily`, so that `omarchy font set` changes
    // the shell's type by rewriting fonts.conf. Omaweb's theme names the family
    // instead, which is what gives up that path: both properties get the same
    // concrete name, and the desktop's own family reaches Omaweb through
    // `integrations/omarchy/omaweb.json.tpl` rendering it into the palette.
    assign(m_style, QStringLiteral("fontFamily"), family);
    assign(m_style, QStringLiteral("resolvedFontFamily"), family);
    assign(m_style, QStringLiteral("fontBaseSize"), font.value(QStringLiteral("size")));
    m_applying = false;
}

void KitTheme::followResets(QObject *target, const QStringList &properties)
{
    for (const auto &name : properties) {
        if (!QQmlProperty(target, name).connectNotifySignal(this, SLOT(apply()))) {
            qWarning("The Omarchy kit's %s no longer reports changes; Omaweb's palette "
                     "may lose to the kit's own theme lookups.",
                qPrintable(name));
        }
    }
}

} // namespace omaweb
