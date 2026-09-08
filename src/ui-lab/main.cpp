#include "BrowserController.h"
#include "ContentBlocker.h"
#include "EngineCapabilities.h"
#include "FaviconTint.h"
#include "DefaultBrowser.h"
#include "ExternalProtocolHandler.h"
#include "InputMethod.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "PagePrinter.h"
#include "ProcessResources.h"
#include "Quickshell.h"
#include "RuntimeSecurity.h"
#include "SavedDownload.h"
#include "SystemClipboard.h"
#include "SystemNotifier.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QQuickWindow>
#include <QTimer>

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
void seedSampleTabs(omaweb::BrowserController &browser, const QVariantList &favicons)
{
    const auto blankTabId = browser.activeTabId();
    auto *unpinned = browser.unpinnedTabs();
    qsizetype icon = 0;
    for (const auto &sample : sampleTabs()) {
        const QUrl url(QString::fromUtf8(sample.url));
        browser.openInputInBackground(url);
        const auto tabId = lastTabId(unpinned);
        if (tabId.isEmpty()) {
            continue;
        }
        browser.updateTab(tabId, url, QString::fromUtf8(sample.title));
        // A tab that was opened was also visited. Without this History is a
        // page saying the Space has none, which is a state of the empty lab
        // rather than a state of the browser.
        browser.recordVisit(url, QString::fromUtf8(sample.title));
        if (!favicons.isEmpty()) {
            browser.setTabIcon(tabId, favicons.at(icon++ % favicons.size()).toUrl());
        }
        // Pinning is an operation on the tab on show, so a seeded pin is
        // activated and pinned in turn. Nothing sees the intermediate state:
        // the blank tab is active again before control reaches the event loop.
        if (sample.pinned) {
            browser.activateTab(tabId);
            browser.toggleActivePinned();
        }
    }
    browser.activateTab(blankTabId);
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    omaweb::installWindowChrome(&application);
    QCoreApplication::setOrganizationName(QStringLiteral("Omaweb"));
    QCoreApplication::setApplicationName(QStringLiteral("Omaweb UI Lab"));
    // The settings page reads Qt.application.version for its about section, so
    // the lab has to carry the same version the browser does.
    QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));

    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        qCritical("Could not create temporary UI-lab data directory.");
        return 1;
    }

    omaweb::BrowserController browser(
        omaweb::SpaceStorage(dataRoot.path(), QStringLiteral("mock")));
    omaweb::ContentBlocker contentBlocker(
        dataRoot.path(), omaweb::ContentBlocker::DefaultLists::None);
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

    omaweb::registerFaviconTint();
    omaweb::registerEngineCapabilities();
    omaweb::registerDefaultBrowser();
    omaweb::registerSystemClipboard();
    omaweb::registerExternalProtocolHandler();
    omaweb::registerPagePrinter();
    omaweb::registerSystemNotifier();
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
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_PROFILE_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
    const auto mockFavicons = drawMockFavicons(dataRoot.filePath(QStringLiteral("favicons")));
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"), mockFavicons);
    engine.addImportPath(QStringLiteral(OMAWEB_UI_DIRECTORY));
    // The vendored Omarchy component kit: qs.Ui and qs.Commons.
    engine.addImportPath(QStringLiteral(OMAWEB_OMARCHY_IMPORT_PATH));
    // The kit's own colour and type come from an Omarchy theme on disk. Omaweb's
    // palette is the source of truth, so it is pushed into the kit's singletons
    // once the engine can resolve them.
    omaweb::KitTheme kitTheme(&engine, &theme);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral(OMAWEB_MAIN_QML_URL)));

    const auto arguments = application.arguments();
    // The lab comes up on a Space at rest, which draws neither the Pinned
    // section nor the tab list, so the sidebar that distinguishes this browser
    // is the one thing a capture of it cannot show. `--tabs` seeds a day.
    if (arguments.contains(QStringLiteral("--tabs"))) {
        seedSampleTabs(browser, mockFavicons);
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
    // eight sections deep and a section can have a dialog standing over it. An
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
            {QStringLiteral("collapsed"), {{"", "sidebarCollapsed", true}}},
            {QStringLiteral("settings"), {{"", "settingsOpen", true}}},
            {QStringLiteral("settings:clear"),
                {{"", "settingsOpen", true}, {"settingsSurface", "clearDataOpen", true}}},
            {QStringLiteral("site"), {{"sidebar", "statusOpen", true}}},
            {QStringLiteral("history"), {{"", "historyOpen", true}}},
            {QStringLiteral("shortcuts"), {{"", "shortcutsOpen", true}}},
        };
        const auto requested = arguments.at(showIndex + 1);
        auto *root = engine.rootObjects().constFirst();

        // Settings is eight sections deep, and a review of its layout wants a
        // capture of each. The rail's own list is what names them, so the
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
        if (state.isEmpty()) {
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
