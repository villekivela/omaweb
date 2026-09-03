#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
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

private:
    static QVariantMap defaultOpacity();
    static QVariantMap defaultFont();
    static QVariantMap defaultSyntax();
    // The first candidate family the host actually has installed. A theme
    // names the families it prefers; Qt has to be handed one that exists,
    // because a missing family costs a full font-alias sweep and then draws
    // in whatever face Qt substitutes.
    static QString installedFamily(const QStringList &candidates);
    QVariantMap fallbackPalette() const;
    void apply(QVariantMap palette);
    QVariantMap normalizedPalette(QVariantMap palette) const;
    void refreshWatchPaths();

    QStringList m_themePaths;
    QFileSystemWatcher m_watcher;
    QVariantMap m_palette;
};

} // namespace omaweb
