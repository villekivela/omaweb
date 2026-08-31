#include "BrowserController.h"
#include "ContentBlocker.h"
#include "FaviconTint.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "Quickshell.h"
#include "ThemeController.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <QColor>
#include <QCoreApplication>
#include <QDir>
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
    tanto::installWindowChrome(&application);
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

    tanto::quickshell::installShim();
    tanto::registerFaviconTint();
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
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"),
        drawMockFavicons(dataRoot.filePath(QStringLiteral("favicons"))));
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

    const auto arguments = application.arguments();
    // Private chrome is a whole palette of its own, and the lab is where it is
    // reviewed. Nothing else about the window changes.
    if (arguments.contains(QStringLiteral("--private")) && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().constFirst()->setProperty("privateWindow", true);
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
