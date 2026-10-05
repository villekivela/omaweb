#include "Quickshell.h"

#include "QuickshellIo.h"

#include <QQmlEngine>
#include <QQuickStyle>
#include <QtGlobal>

namespace omaweb::quickshell {

void installShim(QQmlEngine &engine)
{
    // Omarchy installs the real Quickshell into Qt's own qml directory, and a
    // module found on the import path beats C++ registration -- the engine
    // would resolve `import Quickshell` there and then fail to load a plugin
    // Omaweb has no use for. Prepending the shim's own qmldir files shadows it.
    engine.addImportPath(QStringLiteral("qrc:/omaweb/quickshell-shim"));

    // The kit's `TextField` is a Qt Quick Controls `TextField` that replaces
    // its own `background`, and a native style refuses that customization —
    // on macOS the field silently renders as an Aqua box instead. Basic is
    // what the Omarchy shell itself draws on, so it is also what makes the
    // vendored components look like themselves here.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    qmlRegisterSingletonType<Quickshell>("Quickshell", 1, 0, "Quickshell",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new Quickshell; });
    qmlRegisterType<Singleton>("Quickshell", 1, 0, "Singleton");
    qmlRegisterType<FileView>("Quickshell.Io", 1, 0, "FileView");
    qmlRegisterType<IpcHandler>("Quickshell.Io", 1, 0, "IpcHandler");
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
    qWarning("Quickshell.execDetached is not available in Omaweb: %s",
        qPrintable(command.join(QLatin1Char(' '))));
}

Singleton::Singleton(QObject *parent)
    : QObject(parent)
{
}

QQmlListProperty<QObject> Singleton::data()
{
    return QQmlListProperty<QObject>(
        this, nullptr,
        [](QQmlListProperty<QObject> *list, QObject *child) { child->setParent(list->object); },
        nullptr, nullptr, nullptr);
}

} // namespace omaweb::quickshell
