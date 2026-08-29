#include "BrowserController.h"
#include "ThemeController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTimer>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

namespace {

bool hasUnsafeEngineFlag(const QStringList &arguments)
{
    static const QStringList blocked = {
        QStringLiteral("--no-sandbox"),
        QStringLiteral("--single-process"),
        QStringLiteral("--in-process-gpu"),
        QStringLiteral("--in-process-network-service"),
    };
    for (const auto &argument : arguments) {
        for (const auto &flag : blocked) {
            if (argument == flag || argument.contains(flag)) {
                return true;
            }
        }
    }
    return false;
}

QString dataRoot()
{
    const auto override = qEnvironmentVariable("TANTO_DATA_ROOT");
    if (!override.isEmpty()) {
        return override;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString themePath()
{
    const auto override = qEnvironmentVariable("TANTO_THEME_FILE");
    if (!override.isEmpty()) {
        return override;
    }
#if defined(Q_OS_LINUX)
    const auto omarchyTheme = QDir::home().filePath(
        QStringLiteral(".local/state/omarchy/current/theme/tanto.json"));
    if (QFileInfo::exists(omarchyTheme)) {
        return omarchyTheme;
    }
#endif
    return QStringLiteral(TANTO_THEME_PATH);
}

} // namespace

int main(int argc, char *argv[])
{
    QStringList arguments;
    for (int index = 0; index < argc; ++index) {
        arguments.append(QString::fromLocal8Bit(argv[index]));
    }
    if (qEnvironmentVariableIsSet("QTWEBENGINE_DISABLE_SANDBOX") || hasUnsafeEngineFlag(arguments)) {
        qCritical("Tanto refuses to start with browser sandbox-disabling or single-process flags.");
        return 2;
    }

    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Tanto"));
    QCoreApplication::setApplicationName(QStringLiteral("Tanto"));
    QCoreApplication::setApplicationVersion(QStringLiteral(TANTO_VERSION));

    tanto::BrowserController browser(dataRoot(), QStringLiteral("qt"));
    tanto::ThemeController theme(themePath());

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

    if (arguments.contains(QStringLiteral("--validate-qml"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
