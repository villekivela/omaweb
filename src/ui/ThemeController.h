#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

namespace tanto {

class ThemeController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap palette READ palette NOTIFY paletteChanged)

public:
    explicit ThemeController(QString themePath, QObject *parent = nullptr);

    QVariantMap palette() const;
    Q_INVOKABLE void reload();

signals:
    void paletteChanged();

private:
    static QVariantMap defaultOpacity();
    static QVariantMap defaultFont();
    // The first candidate family the host actually has installed. A theme
    // names the families it prefers; Qt has to be handed one that exists,
    // because a missing family costs a full font-alias sweep and then draws
    // in whatever face Qt substitutes.
    static QString installedFamily(const QStringList &candidates);
    QVariantMap fallbackPalette() const;
    QVariantMap normalizedPalette(QVariantMap palette) const;
    void refreshWatchPaths();

    QString m_themePath;
    QString m_themeDirectory;
    QFileSystemWatcher m_watcher;
    QVariantMap m_palette;
};

} // namespace tanto
