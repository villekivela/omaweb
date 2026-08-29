#include "BrowserController.h"
#include "ThemeController.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Tanto"));
    QCoreApplication::setApplicationName(QStringLiteral("Tanto UI Lab"));

    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        qCritical("Could not create temporary UI-lab data directory.");
        return 1;
    }

    tanto::BrowserController browser(dataRoot.path(), QStringLiteral("mock"));
    tanto::ThemeController theme(QStringLiteral(TANTO_THEME_PATH));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(TANTO_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(TANTO_ICON_FONT_URL)));
    engine.addImportPath(QStringLiteral(TANTO_UI_DIRECTORY));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral(TANTO_MAIN_QML_URL)));

    if (application.arguments().contains(QStringLiteral("--validate-qml"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
