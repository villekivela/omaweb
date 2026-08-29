#include "ThemeController.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace tanto {

ThemeController::ThemeController(QString themePath, QObject *parent)
    : QObject(parent)
    , m_themePath(std::move(themePath))
    , m_themeDirectory(QFileInfo(m_themePath).absolutePath())
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        reload();
        refreshWatchPaths();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        reload();
        refreshWatchPaths();
    });
    refreshWatchPaths();
    reload();
}

QVariantMap ThemeController::palette() const
{
    return m_palette;
}

void ThemeController::reload()
{
    QFile file(m_themePath);
    if (!file.open(QIODevice::ReadOnly)) {
        const auto fallback = fallbackPalette();
        if (m_palette != fallback) {
            m_palette = fallback;
            emit paletteChanged();
        }
        return;
    }

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        return;
    }
    const auto next = document.object().toVariantMap();
    if (next != m_palette) {
        m_palette = next;
        emit paletteChanged();
    }
}

QVariantMap ThemeController::fallbackPalette() const
{
    return {
        {QStringLiteral("window"), QStringLiteral("#16151d")},
        {QStringLiteral("sidebar"), QStringLiteral("#d91d1b29")},
        {QStringLiteral("overlay"), QStringLiteral("#f5282634")},
        {QStringLiteral("surface"), QStringLiteral("#302e3d")},
        {QStringLiteral("surfaceHover"), QStringLiteral("#3d394e")},
        {QStringLiteral("text"), QStringLiteral("#f3f1fa")},
        {QStringLiteral("mutedText"), QStringLiteral("#aaa5b7")},
        {QStringLiteral("accent"), QStringLiteral("#9b87ff")},
        {QStringLiteral("border"), QStringLiteral("#4a4658")},
        {QStringLiteral("privateAccent"), QStringLiteral("#dc6bce")},
    };
}

void ThemeController::refreshWatchPaths()
{
    if (QFile::exists(m_themePath) && !m_watcher.files().contains(m_themePath)) {
        m_watcher.addPath(m_themePath);
    }
    if (QFileInfo::exists(m_themeDirectory) && !m_watcher.directories().contains(m_themeDirectory)) {
        m_watcher.addPath(m_themeDirectory);
    }
}

} // namespace tanto
