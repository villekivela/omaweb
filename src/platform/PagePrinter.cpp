#include "PagePrinter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUuid>

namespace omaweb {
namespace {

    QString printSpoolDirectory()
    {
        const auto root = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (root.isEmpty()) {
            return {};
        }
        const auto spool = QDir(root).filePath(QStringLiteral("omaweb-print"));
        return QDir().mkpath(spool) ? spool : QString {};
    }

    // A page title becomes a file name, and a title is whatever the site says it
    // is. Only the characters that make a readable name survive; the uniqueness is
    // the identifier beside them, not the title.
    QString spoolFileName(const QString &name)
    {
        QString safe;
        for (const auto character : name) {
            if (character.isLetterOrNumber()) {
                safe.append(character);
            } else if (!safe.isEmpty() && !safe.endsWith(QLatin1Char('-'))) {
                safe.append(QLatin1Char('-'));
            }
            if (safe.size() >= 48) {
                break;
            }
        }
        while (safe.endsWith(QLatin1Char('-'))) {
            safe.chop(1);
        }
        if (safe.isEmpty()) {
            safe = QStringLiteral("page");
        }
        return QStringLiteral("%1-%2.pdf")
            .arg(safe, QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    }

} // namespace

PagePrinter::PagePrinter(QObject *parent)
    : QObject(parent)
{
}

QString PagePrinter::reserveDestination(const QString &name) const
{
    const auto spool = printSpoolDirectory();
    if (spool.isEmpty()) {
        return {};
    }
    return QDir(spool).filePath(spoolFileName(name));
}

void PagePrinter::discard(const QString &path)
{
    const auto spool = printSpoolDirectory();
    // Only files this class handed out are its to remove.
    if (spool.isEmpty() || path.isEmpty() || QDir(spool) != QFileInfo(path).absoluteDir()) {
        return;
    }
    QFile::remove(path);
}

void registerPagePrinter()
{
    qmlRegisterSingletonType<PagePrinter>("Omaweb", 1, 0, "PagePrinter",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new PagePrinter; });
}

} // namespace omaweb
