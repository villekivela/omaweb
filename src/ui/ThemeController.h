#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

namespace omaweb {

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap palette READ palette NOTIFY paletteChanged)

public:
    explicit ThemeController(QString themePath, QObject *parent = nullptr);
    // The places a palette may come from, in the order they outrank each
    // other. The first one that reads is the palette; the rest are watched, so
    // a higher-ranked file appearing later takes over without a restart.
    explicit ThemeController(QStringList themePaths, QObject *parent = nullptr);

    QVariantMap palette() const;
    Q_INVOKABLE void reload();

signals:
    void paletteChanged();
    // A complete reload selected a theme source (or the built-in fallback).
    // Unlike paletteChanged, this is emitted even when normalization leaves
    // the visible palette unchanged.
    void themeReloaded();

private:
    static QVariantMap defaultOpacity();
    static QVariantMap defaultFont();
    static QVariantMap defaultSyntax();
    // The face the host would actually draw for the first candidate it has. A
    // theme names the families it prefers; Qt has to be handed one that
    // exists, because a missing family costs a full font-alias sweep and then
    // draws in whatever face Qt substitutes. A candidate may also be a
    // fontconfig alias rather than a family, and the answer is then the family
    // the alias stands for rather than the alias.
    static QString installedFamily(const QStringList &candidates);
    QVariantMap fallbackPalette() const;
    QStringList themeSourceState() const;
    void apply(QVariantMap palette);
    QVariantMap normalizedPalette(QVariantMap palette) const;
    void refreshWatchPaths();

    QStringList m_themePaths;
    // What each watched candidate resolved to when its watch was added, so a
    // relinked directory can be noticed and the watch re-pointed.
    QHash<QString, QString> m_watchedTargets;
    QFileSystemWatcher m_watcher;
    QTimer m_sourcePoll;
    QStringList m_sourceState;
    QVariantMap m_palette;
};

} // namespace omaweb
