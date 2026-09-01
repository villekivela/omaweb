#include "BrowserController.h"
#include "ContentBlocker.h"
#include "DevelopmentLaunch.h"
#include "FaviconTint.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "QtContentBlocker.h"
#include "Quickshell.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
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

// User-editable configuration lives beside every other tool's, under
// XDG_CONFIG_HOME (~/.config/tanto), rather than in the application data
// directory that holds profiles, caches and blocklists.
QString configRoot()
{
    const auto override = qEnvironmentVariable("TANTO_CONFIG_ROOT");
    if (!override.isEmpty()) {
        return override;
    }
    const auto xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    const auto base = xdg.isEmpty() ? QDir::home().filePath(QStringLiteral(".config")) : xdg;
    return QDir(base).filePath(QStringLiteral("tanto"));
}

QString themePath()
{
    const auto override = qEnvironmentVariable("TANTO_THEME_FILE");
    if (!override.isEmpty()) {
        return override;
    }
    // A theme the user dropped in their own config directory outranks a
    // desktop-managed one; both outrank the built-in.
    const auto configured = QDir(configRoot()).filePath(QStringLiteral("theme.json"));
    if (QFileInfo::exists(configured)) {
        return configured;
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

QString keybindingsPath()
{
    const auto override = qEnvironmentVariable("TANTO_KEYBINDINGS_FILE");
    if (!override.isEmpty()) {
        return override;
    }
    const auto directory = configRoot();
    QDir().mkpath(directory);
    const auto path = QDir(directory).filePath(QStringLiteral("keybindings.json"));
    if (!QFileInfo::exists(path)) {
        // Earlier versions kept the file under the data directory. Carry an
        // existing one over so a user's edited bindings survive the move.
        const auto legacy = QDir(QDir(dataRoot()).filePath(QStringLiteral("settings")))
            .filePath(QStringLiteral("keybindings.json"));
        if (QFileInfo::exists(legacy) && QFile::rename(legacy, path)) {
            tanto::KeyboardNavigation::adoptDefaults(path,
                QStringLiteral(TANTO_DEFAULT_KEYBINDINGS_PATH));
            return path;
        }
        QFile::copy(QStringLiteral(TANTO_DEFAULT_KEYBINDINGS_PATH), path);
        return path;
    }
    tanto::KeyboardNavigation::adoptDefaults(path,
        QStringLiteral(TANTO_DEFAULT_KEYBINDINGS_PATH));
    return path;
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

    // Chromium reads the listener out of the environment at initialization, so
    // the decision is made here and nowhere later. An ordinary launch clears
    // the variable rather than trusting it: whatever set it, Tanto opens no
    // listener it was not asked for on its own command line.
    const auto launch = tanto::readDevelopmentLaunch(arguments,
        QProcess::splitCommand(qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS")));
    if (!launch.refusal.isEmpty()) {
        qCritical("Tanto refuses to start: %s", qPrintable(launch.refusal));
        return 2;
    }
    if (launch.remoteDebugging) {
        qputenv("QTWEBENGINE_REMOTE_DEBUGGING", launch.listenAddress.toLocal8Bit());
        qWarning("Tanto is listening for remote debugging on %s. Anything running as this "
                 "user can read and drive every page in this session, and Private windows "
                 "are unavailable for it.",
            qPrintable(launch.listenAddress));
    } else {
        qunsetenv("QTWEBENGINE_REMOTE_DEBUGGING");
    }

    // Chromium learns its schemes before it starts, and content blocking
    // serves its substitute resources under one of Tanto's own.
    tanto::QtContentBlocker::registerSubstituteScheme();
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    tanto::installWindowChrome(&application);
    QCoreApplication::setOrganizationName(QStringLiteral("Tanto"));
    QCoreApplication::setApplicationName(QStringLiteral("Tanto"));
    QCoreApplication::setApplicationVersion(QStringLiteral(TANTO_VERSION));

    tanto::BrowserController browser(dataRoot(), QStringLiteral("qt"));
    tanto::ContentBlocker contentBlocker(dataRoot());
    tanto::KeyboardNavigation keyboardNavigation(
        keybindingsPath(), QStringLiteral(TANTO_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    tanto::QtContentBlocker engineContentBlocker(&contentBlocker);
    tanto::ThemeController theme(themePath());
    tanto::WindowManager windowManager(QStringLiteral("qt"), launch.privateWindowsAvailable);

    tanto::quickshell::installShim();
    tanto::registerFaviconTint();
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("contentBlocker"), &contentBlocker);
    engine.rootContext()->setContextProperty(
        QStringLiteral("keyboardNavigation"), &keyboardNavigation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineContentBlocker"), &engineContentBlocker);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(TANTO_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(TANTO_ENGINE_PROFILE_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(TANTO_ICON_FONT_URL)));
    engine.addImportPath(QStringLiteral(TANTO_UI_DIRECTORY));
    // The vendored Omarchy component kit: qs.Ui and qs.Commons.
    engine.addImportPath(QStringLiteral(TANTO_OMARCHY_IMPORT_PATH));
    // The kit's own colour and type come from an Omarchy theme on disk. Tanto's
    // palette is the source of truth, so it is pushed into the kit's singletons
    // once the engine can resolve them.
    tanto::KitTheme kitTheme(&engine, &theme);

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
