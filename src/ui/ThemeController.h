#pragma once

#include <QFileSystemWatcher>
#include <QObject>
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
    QVariantMap fallbackPalette() const;
    QVariantMap normalizedPalette(QVariantMap palette) const;
    void refreshWatchPaths();

    QString m_themePath;
    QString m_themeDirectory;
    QFileSystemWatcher m_watcher;
    QVariantMap m_palette;
};

} // namespace tanto
