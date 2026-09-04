#include "SavedDownload.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
#include <QByteArray>

#include <sys/stat.h>
#include <sys/xattr.h>
#endif

namespace omaweb {

namespace {

#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)

bool writeExtendedAttribute(const QByteArray &path, const char *name, const QByteArray &value)
{
    if (value.isEmpty()) {
        return true;
    }
#if defined(Q_OS_MACOS)
    return ::setxattr(path.constData(), name, value.constData(),
               static_cast<size_t>(value.size()), 0, 0)
        == 0;
#else
    return ::setxattr(path.constData(), name, value.constData(),
               static_cast<size_t>(value.size()), 0)
        == 0;
#endif
}

// Apple's quarantine value is four semicolon-separated fields: the flags, when
// it arrived, what put it there, and an event identifier the browser has none
// of. Gatekeeper reads the first three, which is what makes the file one the
// desktop asks about before opening.
QByteArray macQuarantineValue()
{
    return QByteArrayLiteral("0083;")
        + QByteArray::number(QDateTime::currentSecsSinceEpoch(), 16) + ";Omaweb;";
}

#endif

} // namespace

SavedDownload::SavedDownload(QObject *parent)
    : QObject(parent)
{
}

bool SavedDownload::quarantineAvailable() const
{
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

bool SavedDownload::quarantine(const QString &path, const QUrl &sourceUrl,
    const QUrl &pageUrl) const
{
    const QFileInfo file(path);
    if (path.isEmpty() || !file.isFile()) {
        return false;
    }
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    const auto native = QFile::encodeName(file.absoluteFilePath());
    bool marked = true;
#if defined(Q_OS_MACOS)
    marked = writeExtendedAttribute(native, "com.apple.quarantine", macQuarantineValue());
#endif
    // freedesktop's convention, and the one a Linux archive manager or file
    // manager already looks for. Written on macOS too: it costs nothing and a
    // reader's own tools may read it.
    marked = writeExtendedAttribute(native, "user.xdg.origin.url",
                 sourceUrl.isValid() ? sourceUrl.toString().toUtf8() : QByteArray())
        && marked;
    marked = writeExtendedAttribute(native, "user.xdg.referrer.url",
                 pageUrl.isValid() ? pageUrl.toString().toUtf8() : QByteArray())
        && marked;
    // Whether the metadata landed or not, the file is not left runnable. A
    // filesystem that carries no extended attributes is exactly the one where
    // this matters most.
    struct stat state {};
    if (::stat(native.constData(), &state) == 0) {
        const auto stripped = state.st_mode & ~static_cast<mode_t>(S_IXUSR | S_IXGRP | S_IXOTH);
        if (stripped != state.st_mode && ::chmod(native.constData(), stripped) != 0) {
            return false;
        }
    }
    return marked;
#else
    Q_UNUSED(sourceUrl)
    Q_UNUSED(pageUrl)
    return false;
#endif
}

bool SavedDownload::reveal(const QString &path) const
{
    const QFileInfo file(path);
    if (path.isEmpty() || !file.exists()) {
        return false;
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(file.absolutePath()));
}

void registerSavedDownload()
{
    qmlRegisterSingletonType<SavedDownload>("Omaweb", 1, 0, "SavedDownload",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SavedDownload; });
}

} // namespace omaweb
