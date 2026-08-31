#include "KitTheme.h"

#include "ThemeController.h"

#include <QColor>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QVariantMap>

namespace tanto {
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
    return color.isValid() ? QVariant::fromValue(color) : QVariant{};
}

} // namespace

KitTheme::KitTheme(QQmlEngine *engine, const ThemeController *theme, QObject *parent)
    : QObject(parent)
    , m_theme(theme)
{
    m_color = engine->singletonInstance<QObject *>(QStringLiteral("qs.Commons"),
        QStringLiteral("Color"));
    m_style = engine->singletonInstance<QObject *>(QStringLiteral("qs.Commons"),
        QStringLiteral("Style"));
    if (!m_color || !m_style) {
        qWarning("The Omarchy kit's qs.Commons singletons are unavailable, so kit "
                 "components will draw with the kit's own colour and type.");
        return;
    }

    connect(m_theme, &ThemeController::paletteChanged, this, &KitTheme::apply);
    // Every one of the kit's own sources — a watched colours.toml, a shell.toml
    // that resets Style wholesale when it fails to load, an `hyprctl` run — is
    // asynchronous, so being the first writer is not the same as being the
    // authority. Re-applying whenever one of them lands is.
    followResets(m_color,
        {QStringLiteral("foreground"), QStringLiteral("background"), QStringLiteral("accent"),
            QStringLiteral("muted")});
    followResets(m_style,
        {QStringLiteral("fontFamily"), QStringLiteral("resolvedFontFamily"),
            QStringLiteral("fontBaseSize")});
    apply();
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
    // window rather than the alpha Tanto's own chrome lets the desktop through.
    assign(m_color, QStringLiteral("background"),
        colorFrom(palette, QStringLiteral("windowOpaque")));
    // A private window's accent differs from the main window's, and the two
    // share this process. The singleton carries the ordinary palette; the
    // adapters in src/ui carry the per-window difference.
    assign(m_color, QStringLiteral("accent"), colorFrom(palette, QStringLiteral("accent")));
    assign(m_color, QStringLiteral("muted"), colorFrom(palette, QStringLiteral("mutedText")));

    const auto font = palette.value(QStringLiteral("font")).toMap();
    const auto family = font.value(QStringLiteral("family"));
    // Upstream keeps a fontconfig alias in `fontFamily` and the name it
    // resolves to in `resolvedFontFamily`, so that `omarchy font set` changes
    // the shell's type by rewriting fonts.conf. Tanto's theme names the family
    // instead, which is what gives up that path: both properties get the same
    // concrete name, and the desktop's own family reaches Tanto through
    // `integrations/omarchy/tanto.json.tpl` rendering it into the palette.
    assign(m_style, QStringLiteral("fontFamily"), family);
    assign(m_style, QStringLiteral("resolvedFontFamily"), family);
    assign(m_style, QStringLiteral("fontBaseSize"), font.value(QStringLiteral("size")));
    m_applying = false;
}

void KitTheme::followResets(QObject *target, const QStringList &properties)
{
    for (const auto &name : properties) {
        if (!QQmlProperty(target, name).connectNotifySignal(this, SLOT(apply()))) {
            qWarning("The Omarchy kit's %s no longer reports changes; Tanto's palette "
                     "may lose to the kit's own theme lookups.",
                qPrintable(name));
        }
    }
}

} // namespace tanto
