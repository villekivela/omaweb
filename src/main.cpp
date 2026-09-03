#include "BrowserController.h"
#include "ContentBlocker.h"
#include "DevelopmentLaunch.h"
#include "ExternalProtocolHandler.h"
#include "FaviconTint.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "OmarchyTheme.h"
#include "PagePrinter.h"
#include "ProcessResources.h"
#include "QtContentBlocker.h"
#include "QtCookiePolicy.h"
#include "Quickshell.h"
#include "SystemClipboard.h"
#include "SystemNotifier.h"
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
    const auto override = qEnvironmentVariable("OMAWEB_DATA_ROOT");
    if (!override.isEmpty()) {
        return override;
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

// User-editable configuration lives beside every other tool's, under
// XDG_CONFIG_HOME (~/.config/omaweb), rather than in the application data
// directory that holds profiles, caches and blocklists.
QString configRoot()
{
    const auto override = qEnvironmentVariable("OMAWEB_CONFIG_ROOT");
    if (!override.isEmpty()) {
        return override;
    }
    const auto xdg = qEnvironmentVariable("XDG_CONFIG_HOME");
    const auto base = xdg.isEmpty() ? QDir::home().filePath(QStringLiteral(".config")) : xdg;
    return QDir(base).filePath(QStringLiteral("omaweb"));
}

// Every place a palette may come from, in the order they outrank each other.
// The files are not tested for here: the controller reads the first one that is
// there and watches the rest, so a desktop theme rendered a moment after
// startup takes over without a restart.
QStringList themePaths()
{
    const auto override = qEnvironmentVariable("OMAWEB_THEME_FILE");
    if (!override.isEmpty()) {
        // An override names one file, and nothing overtakes it.
        return {override};
    }
    // A theme the user dropped in their own config directory outranks a
    // desktop-managed one; both outrank the built-in.
    QStringList paths{QDir(configRoot()).filePath(QStringLiteral("theme.json"))};
#if defined(Q_OS_LINUX)
    paths.append(omaweb::OmarchyThemePaths::fromEnvironment().renderedTheme());
#endif
    paths.append(QStringLiteral(OMAWEB_THEME_PATH));
    return paths;
}

QString keybindingsPath()
{
    const auto override = qEnvironmentVariable("OMAWEB_KEYBINDINGS_FILE");
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
            omaweb::KeyboardNavigation::adoptDefaults(path,
                QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH));
            return path;
        }
        QFile::copy(QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH), path);
        return path;
    }
    omaweb::KeyboardNavigation::adoptDefaults(path,
        QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH));
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
        qCritical("Omaweb refuses to start with browser sandbox-disabling or single-process flags.");
        return 2;
    }

    // Chromium reads the listener out of the environment at initialization, so
    // the decision is made here and nowhere later. An ordinary launch clears
    // the variable rather than trusting it: whatever set it, Omaweb opens no
    // listener it was not asked for on its own command line.
    const auto launch = omaweb::readDevelopmentLaunch(arguments,
        QProcess::splitCommand(qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS")));
    if (!launch.refusal.isEmpty()) {
        qCritical("Omaweb refuses to start: %s", qPrintable(launch.refusal));
        return 2;
    }
    if (launch.remoteDebugging) {
        qputenv("QTWEBENGINE_REMOTE_DEBUGGING", launch.listenAddress.toLocal8Bit());
        qWarning("Omaweb is listening for remote debugging on %s. Anything running as this "
                 "user can read and drive every page in this session, and Private windows "
                 "are unavailable for it.",
            qPrintable(launch.listenAddress));
    } else {
        qunsetenv("QTWEBENGINE_REMOTE_DEBUGGING");
    }

    // Chromium learns its schemes before it starts, and content blocking
    // serves its substitute resources under one of Omaweb's own.
    omaweb::QtContentBlocker::registerSubstituteScheme();
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    omaweb::installWindowChrome(&application);
    QCoreApplication::setOrganizationName(QStringLiteral("Omaweb"));
    QCoreApplication::setApplicationName(QStringLiteral("Omaweb"));
    QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));
    // The Wayland app id and the X11 window class, which is what a desktop's
    // window rules key on. Left unset, Qt derives one from the executable or
    // the application name, and which of those it picks is Qt's business
    // rather than a name a reader can write a rule against. Omarchy washes
    // every window to 0.985 opacity by default and exempts browsers by class,
    // so this being ours to state is the difference between a page that is
    // opaque and one that is nearly so.
    QGuiApplication::setDesktopFileName(QStringLiteral("omaweb"));

    omaweb::BrowserController browser(dataRoot(), QStringLiteral("qt"), configRoot());
    omaweb::ContentBlocker contentBlocker(dataRoot());
    omaweb::KeyboardNavigation keyboardNavigation(
        keybindingsPath(), QStringLiteral(OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    omaweb::QtContentBlocker engineContentBlocker(&contentBlocker);
    // One filter for the process, attached to every Space's profile as it is
    // built. Third-party cookies are blocked by it; whether an origin has been
    // given an allowance is the core's answer, read per Space.
    omaweb::QtCookiePolicy engineCookiePolicy;
#if defined(Q_OS_LINUX)
    // Omarchy is the desktop Omaweb is built for, so following its theme is
    // the default rather than a template the reader has to install by hand.
    omaweb::followOmarchyTheme(omaweb::OmarchyThemePaths::fromEnvironment(),
        QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH));
#endif
    omaweb::ThemeController theme(themePaths());
    omaweb::WindowManager windowManager(
        QStringLiteral("qt"), configRoot(), launch.privateWindowsAvailable);

    omaweb::registerFaviconTint();
    omaweb::registerSystemClipboard();
    omaweb::registerExternalProtocolHandler();
    omaweb::registerPagePrinter();
    omaweb::registerSystemNotifier();
    omaweb::registerProcessResources();
    QQmlApplicationEngine engine;
    omaweb::quickshell::installShim(engine);
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("contentBlocker"), &contentBlocker);
    engine.rootContext()->setContextProperty(
        QStringLiteral("keyboardNavigation"), &keyboardNavigation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineContentBlocker"), &engineContentBlocker);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineCookiePolicy"), &engineCookiePolicy);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_PROFILE_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
    engine.addImportPath(QStringLiteral(OMAWEB_UI_DIRECTORY));
    // The vendored Omarchy component kit: qs.Ui and qs.Commons.
    engine.addImportPath(QStringLiteral(OMAWEB_OMARCHY_IMPORT_PATH));
    // The kit's own colour and type come from an Omarchy theme on disk. Omaweb's
    // palette is the source of truth, so it is pushed into the kit's singletons
    // once the engine can resolve them.
    omaweb::KitTheme kitTheme(&engine, &theme);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral(OMAWEB_MAIN_QML_URL)));

    if (arguments.contains(QStringLiteral("--validate-qml"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
