#include "BrowserController.h"
#include "ContentBlocker.h"
#include "GlobalPrivacyControl.h"
#include "SecureDns.h"
#include "WebRtcPolicy.h"
#include "ReleaseWatch.h"
#include "EngineCapabilities.h"
#include "FaviconTint.h"
#include "FontSettings.h"
#include "DefaultBrowser.h"
#include "ExternalProtocolHandler.h"
#include "InputMethod.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "MediaAnnouncer.h"
#include "PagePrinter.h"
#include "ProcessResources.h"
#include "Quickshell.h"
#include "RuntimeSecurity.h"
#include "SavedDownload.h"
#include "SoundingTabs.h"
#include "SystemClipboard.h"
#include "SystemNotifier.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>
#include <optional>

namespace {

// Stand-in favicons for the lab, which runs no engine and therefore has no
// icon store of its own. The set is deliberately mixed: coloured marks to show
// a chip taking a site's own colour, and a white one to show the neutral chip
// an icon with no colour to give leaves behind.
QVariantList drawMockFavicons(const QString &directory)
{
    static const QList<QColor> marks = {
        QColor(0xe5, 0x4b, 0x4b),
        QColor(0x3f, 0x8f, 0xe8),
        QColor(0x2f, 0xb2, 0x8a),
        QColor(0xe8, 0x9f, 0x2a),
        QColor(0x9c, 0x5c, 0xe0),
        QColor(0xff, 0xff, 0xff),
    };
    QDir().mkpath(directory);
    QVariantList urls;
    for (qsizetype index = 0; index < marks.size(); ++index) {
        QImage icon(32, 32, QImage::Format_ARGB32);
        icon.fill(Qt::transparent);
        QPainter painter(&icon);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(marks.at(index));
        painter.drawRoundedRect(QRectF(4, 4, 24, 24), 7, 7);
        painter.end();
        const auto path = QDir(directory).filePath(QStringLiteral("favicon-%1.png").arg(index));
        if (icon.save(path)) {
            urls.append(QUrl::fromLocalFile(path));
        }
    }
    return urls;
}

// A day's worth of tabs for a Space the lab would otherwise bring up at rest.
// The sidebar's shape is the point — Pinned tiles above an ordinary list — so
// these are named the way a reader's own tabs are named rather than by their
// position in this array.
struct SampleTab {
    const char *url;
    const char *title;
    bool pinned;
};

const QList<SampleTab> &sampleTabs()
{
    static const QList<SampleTab> tabs = {
        {"https://mail.proton.me/u/0/inbox", "Inbox", true},
        {"https://calendar.google.com/calendar/r/week", "Calendar", true},
        {"https://github.com/notifications", "Notifications", true},
        {"https://music.youtube.com/library", "Library", true},
        {"https://github.com/villekivela/omaweb/pull/124", "Generate the website's screenshots",
            false},
        {"https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html", "Qt Quick Scene Graph",
            false},
        {"https://wayland.app/protocols/xdg-shell", "xdg-shell protocol", false},
        {"https://archlinux.org/packages/extra/x86_64/qt6-webengine/", "Arch Linux - qt6-webengine",
            false},
        {"https://omarchy.org/", "Omarchy", false},
    };
    return tabs;
}

// The id of the tab a background open has just appended. A seeded tab is
// addressed by id for its title, its icon and its pin, and the controller
// hands back no id of its own for a background open.
QString lastTabId(QAbstractItemModel *tabs)
{
    if (tabs == nullptr || tabs->rowCount() == 0) {
        return {};
    }
    const auto roles = tabs->roleNames();
    const auto role = roles.key(QByteArrayLiteral("tabId"), -1);
    if (role < 0) {
        return {};
    }
    return tabs->data(tabs->index(tabs->rowCount() - 1, 0), role).toString();
}

// Seeds the Space the lab came up on. The blank tab it came up with is left
// active and taken back at the end, so the viewport still draws the Start page
// with a populated sidebar beside it: a reader opening a new tab on a working
// day, which is the state a screenshot of this browser wants.
// The tab the seeded day ends on: the blank tab unless `onShow` names one of
// the sample addresses, which a capture of the browser in use asks for.
// The sample tab `--browse` ends the seeded day on.
constexpr const char *browsedTab = "https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html";

// The two filter lists a first run subscribes to, written into the lab's
// content-blocking settings as the browser would leave them after updating:
// seeded, enabled, current as of now, with a list file on disk. The lab
// builds its blocker without the defaults, since seeding them fetches, and
// a list updated less than a day ago with its file present is one the
// blocker does not fetch.
void writeSampleLists(const QDir &dataRoot)
{
    const auto folder = dataRoot.filePath(QStringLiteral("content-blocking"));
    QDir().mkpath(QDir(folder).filePath(QStringLiteral("lists")));
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QJsonArray subscriptions;
    const QList<std::pair<const char *, const char *>> lists = {
        {"easylist", "EasyList"},
        {"easyprivacy", "EasyPrivacy"},
    };
    for (const auto &[id, title] : lists) {
        const auto name = QString::fromUtf8(id);
        subscriptions.append(QJsonObject {
            {QStringLiteral("id"), name},
            {QStringLiteral("title"), QString::fromUtf8(title)},
            {QStringLiteral("source"), QStringLiteral("https://easylist.to/")},
            {QStringLiteral("license"), QStringLiteral("GPLv3 or CC BY-SA 3.0")},
            {QStringLiteral("updateAddress"),
                QStringLiteral("https://easylist.to/easylist/%1.txt").arg(name)},
            {QStringLiteral("updateStatus"), QStringLiteral("current")},
            {QStringLiteral("lastUpdated"), now},
            {QStringLiteral("enabled"), true},
        });
        QFile list(QDir(folder).filePath(QStringLiteral("lists/%1.txt").arg(name)));
        if (list.open(QIODevice::WriteOnly)) {
            list.write("! Sample list for the UI lab\n");
        }
    }
    QFile settings(QDir(folder).filePath(QStringLiteral("settings.json")));
    if (settings.open(QIODevice::WriteOnly)) {
        settings.write(QJsonDocument(QJsonObject {
                                         {QStringLiteral("version"), 1},
                                         {QStringLiteral("seeded"), true},
                                         {QStringLiteral("userRules"), QString()},
                                         {QStringLiteral("disabledSites"), QJsonArray()},
                                         {QStringLiteral("subscriptions"), subscriptions},
                                     })
                .toJson(QJsonDocument::Indented));
    }
}

void seedSampleTabs(
    omaweb::BrowserController &browser, const QVariantList &favicons, const QString &onShow)
{
    auto shownTabId = browser.activeTabId();
    auto *unpinned = browser.unpinnedTabs();
    qsizetype icon = 0;
    for (const auto &sample : sampleTabs()) {
        const QUrl url(QString::fromUtf8(sample.url));
        browser.openInputInBackground(url);
        const auto tabId = lastTabId(unpinned);
        if (tabId.isEmpty()) {
            continue;
        }
        const auto favicon
            = favicons.isEmpty() ? QUrl {} : favicons.at(icon++ % favicons.size()).toUrl();
        browser.reportTabPageState(
            tabId, url, QString::fromUtf8(sample.title), favicon, false, false);
        if (!onShow.isEmpty() && url.toString() == onShow) {
            shownTabId = tabId;
        }
        // A tab that was opened was also visited. Without this History is a
        // page saying the Space has none, which is a state of the empty lab
        // rather than a state of the browser.
        browser.recordVisit(url, QString::fromUtf8(sample.title));
        // Pinning is an operation on the tab on show, so a seeded pin is
        // activated and pinned in turn. Nothing sees the intermediate state:
        // the blank tab is active again before control reaches the event loop.
        if (sample.pinned) {
            browser.activateTab(tabId);
            browser.toggleActivePinned();
        }
    }
    browser.activateTab(shownTabId);
}

// Two more Spaces, each with a page or two of its own, so that switching
// Space has somewhere to go. The first Space is put back on show at the
// end. This runs before the QML loads: a switch with the interface up tears
// the leaving Space's engines down, which a seed has no reason to pay for.
void seedSampleSpaces(omaweb::BrowserController &browser, const QVariantList &favicons)
{
    const auto firstSpaceId = browser.activeSpaceId();
    auto *unpinned = browser.unpinnedTabs();
    qsizetype icon = 0;
    struct SampleSpace {
        const char *name;
        QList<SampleTab> tabs;
    };
    const QList<SampleSpace> spaces = {
        {"Work",
            {{"https://github.com/pulls", "Pull requests", true},
                {"https://doc.qt.io/qt-6/qmlapplications.html", "QML Applications", false},
                {"http://localhost:3000/", "localhost:3000", false}}},
    };
    for (const auto &space : spaces) {
        const auto spaceId = browser.createSpace(QString::fromUtf8(space.name));
        if (spaceId.isEmpty() || !browser.switchSpace(spaceId)) {
            continue;
        }
        const auto restTabId = browser.activeTabId();
        for (const auto &sample : space.tabs) {
            const QUrl url(QString::fromUtf8(sample.url));
            browser.openInputInBackground(url);
            const auto tabId = lastTabId(unpinned);
            if (tabId.isEmpty()) {
                continue;
            }
            const auto favicon
                = favicons.isEmpty() ? QUrl {} : favicons.at(icon++ % favicons.size()).toUrl();
            browser.reportTabPageState(
                tabId, url, QString::fromUtf8(sample.title), favicon, false, false);
            if (sample.pinned) {
                browser.activateTab(tabId);
                browser.toggleActivePinned();
            }
        }
        browser.activateTab(restTabId);
    }
    browser.switchSpace(firstSpaceId);
}

} // namespace

int main(int argc, char *argv[])
{
    // Startup is measured from here: what the process pays before `main` is
    // the loader's and the same for every build, and what comes after is what
    // an Omaweb change can slow down.
    QElapsedTimer startup;
    startup.start();
    QGuiApplication application(argc, argv);
    const auto captureFontFile = qEnvironmentVariable("OMAWEB_CAPTURE_FONT_FILE");
    if (!captureFontFile.isEmpty() && QFontDatabase::addApplicationFont(captureFontFile) < 0) {
        qCritical("Could not load capture font: %s", qPrintable(captureFontFile));
        return 1;
    }
    omaweb::installWindowChrome(&application);
    QCoreApplication::setOrganizationName(QStringLiteral("Omaweb"));
    QCoreApplication::setApplicationName(QStringLiteral("Omaweb UI Lab"));
    // The settings page reads Qt.application.version for its about section, so
    // the lab has to carry the same version the browser does.
    QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));

    // The lab keeps nothing between runs, so its data root is temporary unless
    // a caller hands it one: the restore probe seeds a Space on disk first and
    // then measures the lab bringing it back.
    const auto arguments = application.arguments();
    std::optional<QTemporaryDir> temporaryRoot;
    QString dataRootPath;
    const auto dataRootIndex = arguments.indexOf(QStringLiteral("--data-root"));
    if (dataRootIndex >= 0 && dataRootIndex + 1 < arguments.size()) {
        dataRootPath = arguments.at(dataRootIndex + 1);
    } else {
        temporaryRoot.emplace();
        if (!temporaryRoot->isValid()) {
            qCritical("Could not create temporary UI-lab data directory.");
            return 1;
        }
        dataRootPath = temporaryRoot->path();
    }
    const QDir dataRoot(dataRootPath);
    if (arguments.contains(QStringLiteral("--sample-lists"))) {
        writeSampleLists(dataRoot);
    }

    omaweb::BrowserController browser(omaweb::SpaceStorage(dataRootPath, QStringLiteral("mock")));
    omaweb::ContentBlocker contentBlocker(dataRootPath, omaweb::ContentBlocker::DefaultLists::None);
    const auto keybindingsPath = dataRoot.filePath(QStringLiteral("keybindings.json"));
    QFile::copy(QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH), keybindingsPath);
    omaweb::KeyboardNavigation keyboardNavigation(
        keybindingsPath, QStringLiteral(OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    // The lab reviews chrome, and chrome is drawn in a palette, so it honours
    // the same override the browser does: `OMAWEB_THEME_FILE=<path>` reviews a
    // theme without installing it. Nothing else in the lab's search order —
    // the config directory, the desktop's theme — applies: a lab that read the
    // machine's theme would review a different palette on every machine.
    const auto themeOverride = qEnvironmentVariable("OMAWEB_THEME_FILE");
    omaweb::ThemeController theme(
        themeOverride.isEmpty() ? QStringLiteral(OMAWEB_THEME_PATH) : themeOverride);
    omaweb::WindowManager windowManager;
    // The size control is reviewed here; what it sets is written under the
    // lab's own data root rather than the reader's configuration.
    omaweb::FontSettings fontSettings(dataRootPath, QFontDatabase::families());
    // No engine runs here to report its own fonts, so the group is reviewed
    // over the values a Linux engine reports.
    fontSettings.setEngineFonts(
        {QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Sans Mono"), 16, 0});

    omaweb::registerBrowserController();
    omaweb::registerDownloads();
    omaweb::registerFaviconTint();
    omaweb::registerFontSettings();
    omaweb::registerEngineCapabilities();
    omaweb::registerDefaultBrowser();
    omaweb::registerSystemClipboard();
    omaweb::registerExternalProtocolHandler();
    omaweb::registerPagePrinter();
    omaweb::registerSystemNotifier();
    omaweb::registerSoundingTabs();
    omaweb::registerMediaAnnouncer();
    omaweb::registerProcessResources();
    omaweb::registerSavedDownload();
    static omaweb::RuntimeSecurity runtimeSecurity({}, {});
    omaweb::registerRuntimeSecurity(&runtimeSecurity);
    // The lab reviews chrome rather than the desktop it runs on, so the report
    // it draws is of a desktop that asked for no input method.
    static omaweb::InputMethodReport inputMethod {omaweb::InputMethodHost {}};
    omaweb::registerInputMethodReport(&inputMethod);
    QQmlApplicationEngine engine;
    omaweb::quickshell::installShim(engine);
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("contentBlocker"), &contentBlocker);
    engine.rootContext()->setContextProperty(
        QStringLiteral("keyboardNavigation"), &keyboardNavigation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
    // The lab runs no engine, so there is no third-party filter to attach.
    // Site information reads the gap off the adapter's capabilities.
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineCookiePolicy"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineHeldDownloads"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("fontSettings"), &fontSettings);
    engine.rootContext()->setContextProperty(
        QStringLiteral("pageFonts"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("syncLauncher"), static_cast<QObject *>(nullptr));
    // A release the lab is behind, so the Release mark is there to be reviewed.
    // The lab asks GitHub nothing: what is being drawn is the mark, and the
    // answer it would get depends on when somebody last cut a release.
    static omaweb::ReleaseWatch releaseWatch(
        QStringLiteral("0.0.1"), omaweb::ReleaseWatch::Ask::Never);
    releaseWatch.showRelease(QStringLiteral("v9.9.9"));
    engine.rootContext()->setContextProperty(QStringLiteral("releaseWatch"), &releaseWatch);
    // The switch is reviewed here; what it flips is written under the lab's
    // own data root rather than the reader's configuration.
    static omaweb::GlobalPrivacyControl globalPrivacyControl(dataRootPath);
    engine.rootContext()->setContextProperty(
        QStringLiteral("globalPrivacyControl"), &globalPrivacyControl);
    static omaweb::WebRtcPolicy webRtcPolicy(dataRootPath);
    static omaweb::SecureDns secureDns(dataRootPath);
    engine.rootContext()->setContextProperty(QStringLiteral("secureDns"), &secureDns);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineSecureDns"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("webRtcPolicy"), &webRtcPolicy);
    // The lab runs no engine, so there is nothing to report unreachable.
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineWebRtcPolicy"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_PROFILE_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
    const auto mockFavicons = drawMockFavicons(dataRoot.filePath(QStringLiteral("favicons")));
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"), mockFavicons);
    // `--browse` shows the browser in use: the seeded day ends on a page, and
    // the stand-in view draws a sample one where it would otherwise say no
    // engine is running.
    const auto browse = arguments.contains(QStringLiteral("--browse"));
    engine.rootContext()->setContextProperty(QStringLiteral("labSamplePages"), browse);
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
    // `--spaces` seeds the Spaces to switch between; see seedSampleSpaces.
    if (arguments.contains(QStringLiteral("--spaces"))) {
        seedSampleSpaces(browser, mockFavicons);
    }
    engine.load(QUrl(QStringLiteral(OMAWEB_MAIN_QML_URL)));

    // The two startup numbers the tests keep, in milliseconds since `main`:
    // the first frame the window drew, and the first frame with the visible
    // Space's page in it. A Space at rest has no page to draw, so the report
    // ends at the first frame; anything else waits for the page.
    if (arguments.contains(QStringLiteral("--report-startup"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (window == nullptr) {
            qCritical("The root object is not a window");
            return 1;
        }
        auto *engineLoader = window->findChild<QObject *>(QStringLiteral("engineLoader"));
        QObject::connect(window, &QQuickWindow::frameSwapped, window,
            [engineLoader, &startup, &browser, firstFrame = true, done = false]() mutable {
                if (done) {
                    return;
                }
                const auto elapsed = startup.nsecsElapsed() / 1e6;
                if (firstFrame) {
                    firstFrame = false;
                    printf("first_frame_milliseconds=%.1f\n", elapsed);
                    if (browser.atRest()) {
                        done = true;
                        fflush(stdout);
                        QCoreApplication::quit();
                        return;
                    }
                }
                if (engineLoader == nullptr
                    || engineLoader->property("item").value<QObject *>() == nullptr) {
                    return;
                }
                printf("page_frame_milliseconds=%.1f\n", elapsed);
                done = true;
                fflush(stdout);
                QCoreApplication::quit();
            });
    }

    // The lab comes up on a Space at rest, which draws neither the Pinned
    // section nor the tab list, so the sidebar that distinguishes this browser
    // is the one thing a capture of it cannot show. `--tabs` seeds a day.
    if (arguments.contains(QStringLiteral("--tabs"))) {
        seedSampleTabs(browser, mockFavicons, browse ? QString::fromUtf8(browsedTab) : QString());
    }
    // Private chrome is a whole palette of its own, and the lab is where it is
    // reviewed. Nothing else about the window changes.
    if (arguments.contains(QStringLiteral("--private")) && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().constFirst()->setProperty("privateWindow", true);
    }
    // The chromeless state and the two places — settings and history — are
    // reached by a key or a click in the browser, which a capture cannot
    // press. Naming the state is how the lab reviews them.
    //
    // A state is a list of properties rather than one flag, because settings is
    // many sections deep and a section can have a dialog standing over it. An
    // empty object name means the window; anything else is found by the name
    // the surface already carries, so naming a state costs the browser nothing.
    // "settings" alone still opens the page on whatever section it was left on.
    struct ShowProperty {
        const char *object;
        const char *property;
        QVariant value;
    };
    const auto showIndex = arguments.indexOf(QStringLiteral("--show"));
    if (showIndex >= 0 && showIndex + 1 < arguments.size() && !engine.rootObjects().isEmpty()) {
        static const QHash<QString, QList<ShowProperty>> states = {
            {QStringLiteral("collapsed"),
                {{"", "sidebarCollapsed", true}, {"", "sidebarPeeked", false}}},
            {QStringLiteral("peek"),
                {{"", "sidebarCollapsed", true}, {"", "sidebarPeeked", true},
                    {"", "floatingControls", false}, {"", "easeChrome", false}}},
            {QStringLiteral("settings"), {{"", "settingsOpen", true}}},
            {QStringLiteral("settings:clear"),
                {{"", "settingsOpen", true}, {"settingsSurface", "clearDataOpen", true}}},
            {QStringLiteral("site"), {{"sidebar", "statusOpen", true}}},
            {QStringLiteral("history"), {{"", "historyOpen", true}}},
            {QStringLiteral("shortcuts"), {{"", "shortcutsOpen", true}}},
            // The last two seeded tabs side by side, the last one active.
            {QStringLiteral("split"), {{"", "sidebarPeeked", false}}},
            // Steps to the next Space shortly before a capture, so the frame
            // is taken part way through the list's arrival.
            {QStringLiteral("space-step"), {}},
            {QStringLiteral("space-settled"), {}},
            {QStringLiteral("omnibar-step"), {}},
            {QStringLiteral("omnibar-settled"), {}},
            {QStringLiteral("tab-step"), {}},
            {QStringLiteral("tab-settled"), {}},
            {QStringLiteral("settings-step"), {}},
            {QStringLiteral("settings-settled"), {}},
        };
        const auto requested = arguments.at(showIndex + 1);
        auto *root = engine.rootObjects().constFirst();

        // A visible page gives the peek capture detail whose blur can be
        // reviewed. The other seeded captures keep the blank tab active.
        if (requested == QLatin1String("peek")) {
            const auto tabId = lastTabId(browser.unpinnedTabs());
            if (!tabId.isEmpty()) {
                browser.activateTab(tabId);
            }
        }
        if (requested == QLatin1String("split")) {
            auto *unpinned = browser.unpinnedTabs();
            const auto rows = unpinned->rowCount();
            if (rows >= 2) {
                browser.activateTab(lastTabId(unpinned));
                browser.addSplit(
                    unpinned->data(unpinned->index(rows - 2, 0), Qt::UserRole + 1).toString());
            }
        }

        // Settings has a section for each part of the browser, and a review of
        // its layout wants a capture of each. The rail's own list is what names them, so the
        // section is looked up there rather than written down again here:
        // adding a section, or moving one, cannot leave the lab pointing at
        // the wrong page. `settings:privacy:clear` still stands the dialog on
        // the section it names.
        auto state = states.value(requested);
        auto parts = requested.split(QLatin1Char(':'));
        if (state.isEmpty() && parts.size() >= 2 && parts.first() == QLatin1String("settings")) {
            auto *surface = root->findChild<QObject *>(QStringLiteral("settingsSurface"));
            if (surface == nullptr) {
                qCritical("No settingsSurface to show %s on", qPrintable(requested));
                return 1;
            }
            const auto sections = surface->property("sections").toStringList();
            // The rail draws "content blocking"; a command line spells it
            // "content-blocking".
            auto wanted = parts.at(1);
            wanted.replace(QLatin1Char('-'), QLatin1Char(' '));
            const auto section = sections.indexOf(wanted);
            if (section < 0) {
                qCritical("No settings section named %s", qPrintable(parts.at(1)));
                return 1;
            }
            state = {{"", "settingsOpen", true}, {"settingsSurface", "section", section}};
            if (parts.size() > 2) {
                const auto rest = states.value(QStringLiteral("settings:") + parts.at(2));
                if (rest.isEmpty()) {
                    qCritical("Unknown --show state %s", qPrintable(requested));
                    return 1;
                }
                state.append(rest);
            }
        }
        if (requested.endsWith(QLatin1String("-step"))
            || requested.endsWith(QLatin1String("-settled"))) {
            const auto delay = requested.endsWith(QLatin1String("-step")) ? 620 : 300;
            const auto what = requested.section(QLatin1Char('-'), 0, 0);
            QTimer::singleShot(delay, root, [root, what] {
                if (what == QLatin1String("space")) {
                    QMetaObject::invokeMethod(root, "stepSpace", Q_ARG(QVariant, 1));
                } else if (what == QLatin1String("tab")) {
                    QMetaObject::invokeMethod(root, "stepTab", Q_ARG(QVariant, 1));
                } else if (what == QLatin1String("settings")) {
                    QMetaObject::invokeMethod(root, "requestSettings");
                } else {
                    QMetaObject::invokeMethod(root, "openOmnibar", Q_ARG(QVariant, false));
                }
            });
        } else if (state.isEmpty()) {
            qCritical("Unknown --show state %s", qPrintable(requested));
            return 1;
        }
        for (const auto &property : state) {
            auto *target = *property.object == '\0'
                ? root
                : root->findChild<QObject *>(QString::fromLatin1(property.object));
            if (target == nullptr) {
                qCritical("No %s to show %s on", property.object, qPrintable(requested));
                return 1;
            }
            target->setProperty(property.property, property.value);
        }
        // The hidden sidebar answers a pointer the lab does not have. The real
        // peek stays up because the pointer moves from the reveal edge into
        // it, and the browser's leave timer puts it away when it has not. The
        // offscreen platform parks its cursor at device pixel (10, 10), which
        // at a scale factor of two is inside the six-pixel reveal edge, so the
        // browser's hold timer summons the sidebar over a `collapsed` capture.
        // Repeat the named state until capture, so the shot is the state asked
        // for rather than the one the browser's timers arrive at.
        if (requested == QLatin1String("collapsed") || requested == QLatin1String("peek")) {
            const auto peeked = requested == QLatin1String("peek");
            auto *sidebarKeeper = new QTimer(root);
            sidebarKeeper->setInterval(16);
            QObject::connect(sidebarKeeper, &QTimer::timeout, root, [root, peeked] {
                root->setProperty("sidebarCollapsed", true);
                root->setProperty("sidebarPeeked", peeked);
                if (!peeked) {
                    return;
                }
                root->setProperty("floatingControls", false);
                const auto views = root->findChildren<QObject *>(QStringLiteral("mockEngineView"));
                for (auto *view : views) {
                    view->setProperty("blurReviewPattern", true);
                }
            });
            sidebarKeeper->start();
        }
    }
    // The palette the browser resolves, rather than the template it was
    // rendered from. A theme file names a handful of colours; Omaweb derives
    // every role it draws from them, floors the quiet ones against the ground
    // they sit on, and tints the private grounds. Anything drawing Omaweb's
    // colours outside Omaweb — the website's own palettes — has to read the
    // resolved roles or it is approximating them a second time.
    const auto paletteIndex = arguments.indexOf(QStringLiteral("--dump-palette"));
    if (paletteIndex >= 0 && paletteIndex + 1 < arguments.size()) {
        QFile out(arguments.at(paletteIndex + 1));
        if (!out.open(QIODevice::WriteOnly)) {
            qCritical("Could not write %s", qPrintable(arguments.at(paletteIndex + 1)));
            return 1;
        }
        out.write(QJsonDocument(QJsonObject::fromVariantMap(theme.palette())).toJson());
        out.close();
    }

    const auto captureIndex = arguments.indexOf(QStringLiteral("--capture"));
    if (captureIndex >= 0 && captureIndex + 1 < arguments.size()) {
        const auto capturePath = arguments.at(captureIndex + 1);
        QTimer::singleShot(700, &application, [&engine, capturePath] {
            if (engine.rootObjects().isEmpty()) {
                QCoreApplication::exit(1);
                return;
            }
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            if (!window || !window->grabWindow().save(capturePath)) {
                qCritical("Could not capture %s", qPrintable(capturePath));
                QCoreApplication::exit(1);
                return;
            }
            QCoreApplication::quit();
        });
    }

    if (application.arguments().contains(QStringLiteral("--validate-qml"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
