#include "SystemClipboard.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUuid>

namespace omaweb {

SystemClipboard::SystemClipboard(QObject *parent)
    : QObject(parent)
{
}

bool SystemClipboard::copyText(const QString &text)
{
    auto *clipboard = QGuiApplication::clipboard();
    if (!clipboard || text.isEmpty()) {
        return false;
    }
    clipboard->setText(text);
    return true;
}

QString SystemClipboard::text() const
{
    auto *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString {};
}

QString SystemClipboard::reserveImage() const
{
    const QDir temporary(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    return temporary.filePath(QStringLiteral("omaweb-capture-%1.png")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

bool SystemClipboard::copyImage(const QString &path)
{
    auto *clipboard = QGuiApplication::clipboard();
    const QImage image(path);
    QFile::remove(path);
    if (!clipboard || image.isNull()) {
        return false;
    }
    clipboard->setImage(image);
    return true;
}

QSize SystemClipboard::imageSize() const
{
    auto *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->image().size() : QSize {};
}

void registerSystemClipboard()
{
    qmlRegisterSingletonType<SystemClipboard>("Omaweb", 1, 0, "SystemClipboard",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemClipboard; });
}

} // namespace omaweb
