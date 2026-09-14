#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtWebEngineQuick>

int main(int argc, char *argv[])
{
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    engine.load(QUrl::fromLocalFile(QStringLiteral(SOURCE_DIR "/main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
