#include "BrowserController.h"
#include "ContentBlocker.h"
#include "ReleaseWatch.h"
#include "EngineBuild.h"
#include "EngineCapabilities.h"
#include "PageImages.h"
#include "EnginePaths.h"
#include "DefaultBrowser.h"
#include "DevelopmentLaunch.h"
#include "ExternalProtocolHandler.h"
#include "FaviconTint.h"
#include "FontSettings.h"
#include "GlobalPrivacyControl.h"
#include "HttpsOnly.h"
#include "SecureDns.h"
#include "WebRtcPolicy.h"
#include "HardwareVideoDecode.h"
#include "InputMethod.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "LaunchRequest.h"
#include "MediaAnnouncer.h"
#include "OmarchyTheme.h"
#include "PagePrinter.h"
#include "ProcessResources.h"
#include "QtContentBlocker.h"
#include "QtCookiePolicy.h"
#include "QtHeldDownloads.h"
#include "QtPageFonts.h"
#include "QtSecureDns.h"
#include "QtWebRtcPolicy.h"
#include "Quickshell.h"
#include "RunningBrowser.h"
#include "RuntimeSecurity.h"
#include "SavedDownload.h"
#include "SoundingTabs.h"
#include "SystemClipboard.h"
#include "SystemNotifier.h"
#include "SyncLauncher.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QColor>
#include <QLibraryInfo>
#include <QStyleHints>
#include <QProcess>

#if defined(Q_OS_LINUX)
#include <dlfcn.h>
#endif
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QTimer>
#include <QtWebEngineCore/qtwebenginecoreglobal.h>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <cstdio>

namespace {

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
    QStringList paths {QDir(configRoot()).filePath(QStringLiteral("theme.json"))};
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
            omaweb::KeyboardNavigation::adoptDefaults(
                path, QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH));
            return path;
        }
        QFile::copy(QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH), path);
        return path;
    }
    omaweb::KeyboardNavigation::adoptDefaults(
        path, QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH));
    return path;
}

} // namespace

int main(int argc, char *argv[])
{
    QStringList arguments;
    for (int index = 0; index < argc; ++index) {
        arguments.append(QString::fromLocal8Bit(argv[index]));
    }
    // First, and before anything that can refuse to start or hand over to a
    // browser already running. A build has to be able to say what it is
    // exactly when it will not run, because that refusal is what a bug report
    // is about, and asking a running browser would answer for that one instead.
    if (omaweb::readVersionRequest(arguments)) {
        const auto report = omaweb::versionReport(QStringLiteral(OMAWEB_VERSION),
            QString::fromLatin1(qWebEngineVersion()),
            QString::fromLatin1(qWebEngineChromiumVersion()));
        std::fprintf(stdout, "%s\n", qPrintable(report));
        return 0;
    }
    if (qEnvironmentVariableIsSet("QTWEBENGINE_DISABLE_SANDBOX")) {
        qCritical("Omaweb refuses to start with QTWEBENGINE_DISABLE_SANDBOX set. There is no "
                  "Omaweb that runs page code outside a sandbox.");
        return 2;
    }

    const auto environmentCommandLine = qEnvironmentVariable("QTWEBENGINE_CHROMIUM_FLAGS");
    const auto environmentFlags = QProcess::splitCommand(environmentCommandLine);
#if defined(Q_OS_LINUX)
    // VA-API is the Linux interface. macOS decodes on the GPU without being
    // asked, and no other platform is distributed for.
    const auto addedFlags = omaweb::hardwareVideoDecodeFlags(environmentFlags);
#else
    const QStringList addedFlags;
    Q_UNUSED(environmentFlags);
#endif
    // What Omaweb adds goes after what the reader set, because Chromium reads
    // the last occurrence of a switch: an added feature list carries the
    // reader's own features forward, so nothing they asked for is dropped by
    // Omaweb asking for one more. Their value is carried over as they wrote it
    // rather than as it split, so a flag that quotes a space survives being
    // added to.
    const auto engineCommandLine
        = (environmentCommandLine + u' ' + addedFlags.join(u' ')).trimmed();
    // Audited as the engine will read it, so the rule about what a launch may
    // carry has one statement rather than one per route in.
    const auto launch
        = omaweb::readDevelopmentLaunch(arguments, QProcess::splitCommand(engineCommandLine));
    if (!launch.refusal.isEmpty()) {
        qCritical("Omaweb refuses to start: %s", qPrintable(launch.refusal));
        return 2;
    }

    omaweb::RuntimeSecurity runtimeSecurity(omaweb::SandboxHost::fromEnvironment(),
        {QString::fromLatin1(qWebEngineVersion()), QString::fromLatin1(qWebEngineChromiumVersion()),
            QString::fromLatin1(qWebEngineChromiumSecurityPatchVersion())});
    if (!runtimeSecurity.rendererIsolated()) {
        qCritical("Omaweb refuses to start: %s", qPrintable(runtimeSecurity.sandboxDiagnostic()));
        return 2;
    }
    if (!runtimeSecurity.meetsSecurityBaseline()) {
        qWarning("%s", qPrintable(runtimeSecurity.securityBaseline()));
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

    if (!addedFlags.isEmpty()) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", engineCommandLine.toLocal8Bit());
    }

#if defined(Q_OS_LINUX)
    // Say where this engine's own files are, before the engine asks QtCore and
    // is told where the distribution's Qt keeps files this engine did not put
    // there. See EnginePaths. The engine is found by asking the loader where it
    // actually mapped it from, so this is right for an installed tree, a build
    // tree, and a reader who moved it, without a path compiled in.
    {
        Dl_info engineLibrary {};
        const bool located
            = dladdr(reinterpret_cast<const void *>(&qWebEngineVersion), &engineLibrary) != 0
            && engineLibrary.dli_fname != nullptr;
        const QString libraryDirectory = located
            ? QFileInfo(QString::fromLocal8Bit(engineLibrary.dli_fname)).absolutePath()
            : QString {};
        const auto paths = omaweb::EnginePaths::beside(libraryDirectory);
        // An engine that is part of the Qt it was built against is already
        // where QtCore will look, and saying so again would only be a way to
        // get it wrong. That is the engine in Qt's own library directory, not
        // one anywhere under it: /usr/lib/omaweb/lib is under /usr/lib.
        const bool privatePrefix = !libraryDirectory.isEmpty()
            && QDir::cleanPath(libraryDirectory)
                != QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::LibrariesPath));
        if (privatePrefix) {
            // Each one only if it is there, and never over the reader: a
            // missing file is the packaging's fault and pointing the engine at
            // nothing would replace a clear failure with a confusing one.
            const auto say = [](const char *name, const QString &path, bool isDirectory) {
                if (qEnvironmentVariableIsSet(name)) {
                    return;
                }
                const QFileInfo there(path);
                if (isDirectory ? there.isDir() : there.isExecutable()) {
                    qputenv(name, QFile::encodeName(path));
                }
            };
            say("QTWEBENGINE_RESOURCES_PATH", paths.resources, true);
            say("QTWEBENGINE_LOCALES_PATH", paths.locales, true);
            say("QTWEBENGINEPROCESS_PATH", paths.renderer, false);
        }
    }
#endif

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

    const auto launchUrl = omaweb::readLaunchUrl(arguments);
    // Claimed before anything expensive is built, so a launch that only carries
    // a link for the browser already running costs a D-Bus call rather than a
    // second session store, a second filter set and a second engine.
    //
    // `--validate-qml` loads the shell and leaves; it is not a reader opening
    // the browser, and handing it to another process would check nothing.
    omaweb::RunningBrowser runningBrowser;
    const auto validatingQml = arguments.contains(QStringLiteral("--validate-qml"));
    if (!runningBrowser.isPrimary() && !validatingQml) {
        if (runningBrowser.handOver(launchUrl)) {
            return 0;
        }
        // Nothing took it. Opening the page here is worse than one browser and
        // better than none, so this carries on as an ordinary launch.
    }

    omaweb::BrowserController browser(
        omaweb::SpaceStorage(dataRoot(), QStringLiteral("qt")), configRoot());
    omaweb::ContentBlocker contentBlocker(dataRoot());
    omaweb::KeyboardNavigation keyboardNavigation(
        keybindingsPath(), QStringLiteral(OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    // One answer for the whole browser: every Space's profile and the Private
    // windows' shared one tell sites the same thing about the reader.
    omaweb::GlobalPrivacyControl globalPrivacyControl(configRoot());
    // Before any page asks for a name: the engine resolves names once for the
    // whole process, and the first lookup has to go where the reader chose.
    omaweb::SecureDns secureDns(configRoot());
    omaweb::QtSecureDns engineSecureDns(&secureDns);
    omaweb::QtContentBlocker engineContentBlocker(&contentBlocker, &globalPrivacyControl);
    // A page's own address goes over HTTPS unless the reader let its site
    // through, which the ordinary windows' store remembers per Space. A
    // Private window's Space is the empty name, which remembers nothing.
    omaweb::HttpsOnly httpsOnly(configRoot());
    httpsOnly.setRemembered([&browser](const QString &spaceId, const QString &origin) {
        return browser.plainHttpRemembered(spaceId, origin);
    });
    engineContentBlocker.setHttpsOnly(&httpsOnly);
    // One filter for the process, attached to every Space's profile as it is
    // built. Third-party cookies are blocked by it; whether an origin has been
    // given an allowance is the core's answer, read per Space.
    omaweb::QtCookiePolicy engineCookiePolicy;
    omaweb::QtHeldDownloads engineHeldDownloads;
#if defined(Q_OS_LINUX)
    // Omarchy is the desktop Omaweb is built for, so following its theme is
    // the default rather than a template the reader has to install by hand.
    omaweb::followOmarchyTheme(
        omaweb::OmarchyThemePaths::fromEnvironment(), QStringLiteral(OMAWEB_OMARCHY_TEMPLATE_PATH));
#endif
    omaweb::ThemeController theme(themePaths());
    // A page asks what colour scheme it is being read in, and Qt answers for
    // the whole application. Nothing was answering, so the engine fell back to
    // the desktop's idea, which on a plain Linux session is light however dark
    // the browser around the page is: an extension's popup drawn in Omaweb's
    // chrome came up white. The browser's own theme is the honest answer, so
    // the window colour decides it, and a theme that changes changes it.
    const auto followThemeColorScheme = [&theme] {
        const QColor window(theme.palette().value(QStringLiteral("window")).toString());
        if (!window.isValid()) {
            return;
        }
        QGuiApplication::styleHints()->setColorScheme(
            window.lightnessF() < 0.5 ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    };
    QObject::connect(
        &theme, &omaweb::ThemeController::paletteChanged, &theme, followThemeColorScheme);
    followThemeColorScheme();
    // The reader's type: an interface size over the theme's, and a page's
    // fonts over the engine's. One answer for every window, Private ones
    // included, which is why it lives beside the theme and not in a store.
    omaweb::FontSettings fontSettings(configRoot(), QFontDatabase::families());
    omaweb::QtPageFonts pageFonts(&fontSettings);
    QObject::connect(&engineContentBlocker, &omaweb::QtContentBlocker::profileAttached, &pageFonts,
        &omaweb::QtPageFonts::attachToProfile);
    // What a page's call may learn about the reader's network, one answer for
    // every profile and set on each as it is built, the way the fonts are.
    omaweb::WebRtcPolicy webRtcPolicy(configRoot());
    omaweb::QtWebRtcPolicy engineWebRtcPolicy(&webRtcPolicy);
    QObject::connect(&engineContentBlocker, &omaweb::QtContentBlocker::profileAttached,
        &engineWebRtcPolicy, &omaweb::QtWebRtcPolicy::attachToProfile);
    // The plugin an input method needs is the desktop's to install, and a
    // desktop that names one it has not installed leaves Qt with no input
    // context and Omaweb with no text-input protocol bound. Nothing about that
    // is printed by Qt, so the browser says it once here and again in Settings,
    // because a browser that drops every composed character without a word is
    // worse than one that names what is missing.
    omaweb::InputMethodReport inputMethod(omaweb::InputMethodHost::fromEnvironment());
    if (!inputMethod.available()) {
        qWarning("%s", qPrintable(inputMethod.diagnostic()));
    }
    omaweb::WindowManager windowManager(configRoot(), launch.privateWindowsAvailable);
    const auto developmentSyncModule = QStringLiteral(OMAWEB_SYNC_MODULE_PATH);
    const auto syncModulePath = QFileInfo::exists(developmentSyncModule)
        ? developmentSyncModule
        : QStringLiteral(OMAWEB_SYNC_INSTALLED_MODULE_PATH);
    omaweb::SyncLauncher syncLauncher(
        &browser, &contentBlocker, &keyboardNavigation, dataRoot(), configRoot(), syncModulePath);
    // Omaweb is alpha and releases often, and a reader who installed one has no
    // other way to learn that another exists. The version it compares against
    // is the one the tag gave this build (ADR 0028).
    //
    // A build whose version did not come from a tag asks nothing. The fallback
    // reads as an ordinary version number, so it would compare as one and leave
    // its reader told to upgrade to a release they may already be running, with
    // no way to be right about it.
    omaweb::ReleaseWatch releaseWatch(QStringLiteral(OMAWEB_VERSION),
        OMAWEB_VERSION_DERIVED ? omaweb::ReleaseWatch::Ask::GitHub
                               : omaweb::ReleaseWatch::Ask::Never);
    releaseWatch.follow(&browser);

    omaweb::registerBrowserController();
    omaweb::registerDownloads();
    omaweb::registerFaviconTint();
    omaweb::registerFontSettings();
    omaweb::registerEngineCapabilities();
    omaweb::registerEngineBuild();
    omaweb::registerPageImages();
    omaweb::registerDefaultBrowser();
    omaweb::registerSystemClipboard();
    omaweb::registerExternalProtocolHandler();
    omaweb::registerPagePrinter();
    omaweb::registerSystemNotifier();
    omaweb::registerSoundingTabs();
    omaweb::registerMediaAnnouncer();
    omaweb::registerProcessResources();
    omaweb::registerSavedDownload();
    omaweb::registerRuntimeSecurity(&runtimeSecurity);
    omaweb::registerInputMethodReport(&inputMethod);
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
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineHeldDownloads"), &engineHeldDownloads);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("fontSettings"), &fontSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("pageFonts"), &pageFonts);
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(QStringLiteral("syncLauncher"), &syncLauncher);
    engine.rootContext()->setContextProperty(QStringLiteral("releaseWatch"), &releaseWatch);
    engine.rootContext()->setContextProperty(
        QStringLiteral("globalPrivacyControl"), &globalPrivacyControl);
    engine.rootContext()->setContextProperty(QStringLiteral("httpsOnly"), &httpsOnly);
    engine.rootContext()->setContextProperty(QStringLiteral("webRtcPolicy"), &webRtcPolicy);
    engine.rootContext()->setContextProperty(QStringLiteral("secureDns"), &secureDns);
    engine.rootContext()->setContextProperty(QStringLiteral("engineSecureDns"), &engineSecureDns);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineWebRtcPolicy"), &engineWebRtcPolicy);
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
    omaweb::KitTheme kitTheme(&engine, &theme, &fontSettings);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral(OMAWEB_MAIN_QML_URL)));

    // The desktop asked for an address, so it goes on screen once the shell
    // exists to put it on. Queued rather than called here: the shell restores
    // the session while it loads, and an address opened before that would be
    // buried under the tabs coming back.
    if (launchUrl.isValid() && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(
            0, &browser, [&browser, launchUrl] { browser.openInput(launchUrl.toString(), true); });
    }

    // Every later launch arrives here instead, as the address it was asked to
    // open rather than as a browser of its own.
    QObject::connect(&runningBrowser, &omaweb::RunningBrowser::openRequested, &browser,
        [&browser](const QUrl &url) { browser.openInput(url.toString(), true); });

    if (arguments.contains(QStringLiteral("--validate-qml"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
