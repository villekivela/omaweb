#include "Quickshell.h"

#include "QuickshellIo.h"

#include <QQmlEngine>
#include <QtGlobal>

namespace tanto::quickshell {

void installShim()
{
    qmlRegisterSingletonType<Quickshell>("Quickshell", 1, 0, "Quickshell",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new Quickshell; });
    qmlRegisterType<FileView>("Quickshell.Io", 1, 0, "FileView");
    qmlRegisterType<Process>("Quickshell.Io", 1, 0, "Process");
    qmlRegisterType<StdioCollector>("Quickshell.Io", 1, 0, "StdioCollector");
}

Quickshell::Quickshell(QObject *parent)
    : QObject(parent)
{
}

QString Quickshell::env(const QString &name) const
{
    return qEnvironmentVariable(name.toLocal8Bit().constData());
}

void Quickshell::execDetached(const QStringList &command)
{
    qWarning("Quickshell.execDetached is not available in Tanto: %s",
        qPrintable(command.join(QLatin1Char(' '))));
}

} // namespace tanto::quickshell
