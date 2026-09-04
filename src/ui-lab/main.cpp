#include "BrowserController.h"
#include "ContentBlocker.h"
#include "FaviconTint.h"
#include "ExternalProtocolHandler.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "PagePrinter.h"
#include "ProcessResources.h"
#include "Quickshell.h"
#include "SystemClipboard.h"
#include "SystemNotifier.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QFile>
#include <QImage>
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
        const auto path
            = QDir(directory).filePath(QStringLiteral("favicon-%1.png").arg(index));
        if (icon.save(path)) {
            urls.append(QUrl::fromLocalFile(path));
        }
    }
    return urls;
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

    omaweb::BrowserController browser(dataRoot.path(), QStringLiteral("mock"));
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
    omaweb::ThemeController theme(themeOverride.isEmpty()
            ? QStringLiteral(OMAWEB_THEME_PATH)
            : themeOverride);
    omaweb::WindowManager windowManager(QStringLiteral("mock"));

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
        QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
    // The lab runs no engine, so there is no third-party filter to attach.
    // Site information reads the gap off the adapter's capabilities.
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineCookiePolicy"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_PROFILE_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"),
        drawMockFavicons(dataRoot.filePath(QStringLiteral("favicons"))));
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

    const auto arguments = application.arguments();
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
    struct ShowProperty
    {
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
