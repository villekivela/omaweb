#include "BrowserController.h"
#include "ContentBlocker.h"
#include "KeyboardNavigation.h"
#include "ThemeController.h"
#include "WindowManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
    tanto::ContentBlocker contentBlocker(dataRoot.path());
    const auto keybindingsPath = dataRoot.filePath(QStringLiteral("keybindings.json"));
    QFile::copy(QStringLiteral(TANTO_DEFAULT_KEYBINDINGS_PATH), keybindingsPath);
    tanto::KeyboardNavigation keyboardNavigation(
        keybindingsPath, QStringLiteral(TANTO_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    tanto::ThemeController theme(QStringLiteral(TANTO_THEME_PATH));
    tanto::WindowManager windowManager(QStringLiteral("mock"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("contentBlocker"), &contentBlocker);
    engine.rootContext()->setContextProperty(
        QStringLiteral("keyboardNavigation"), &keyboardNavigation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(TANTO_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(TANTO_ENGINE_PROFILE_URL)));
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
