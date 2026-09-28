#include "probe.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QStringLiteral("omaweb-376-probe"));
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    Probe probe;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("probe"), &probe);
    engine.rootContext()->setContextProperty(
        QStringLiteral("outputDirectory"), qEnvironmentVariable("PROBE_OUT", QStringLiteral(".")));
    engine.rootContext()->setContextProperty(QStringLiteral("rounds"),
        qEnvironmentVariableIntValue("PROBE_ROUNDS") > 0
            ? qEnvironmentVariableIntValue("PROBE_ROUNDS")
            : 3);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/AgentTab/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
