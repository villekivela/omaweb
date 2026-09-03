#include "BrowserController.h"
#include "ContentBlocker.h"
#include "FaviconTint.h"
#include "ExternalProtocolHandler.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "PagePrinter.h"
#include "ProcessResources.h"
#include "SystemNotifier.h"
#include "Quickshell.h"
#include "SystemClipboard.h"
#include "ThemeController.h"
#include "WindowManager.h"

#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QQmlContext>
#include <QFile>
#include <QQuickStyle>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include <memory>

namespace {

// A favicon on disk for the tests that check what colour a site's chip takes.
// A mark on a transparent plate is the shape a real favicon has.
QUrl writeFavicon(const QString &path, const QColor &mark)
{
    QImage icon(32, 32, QImage::Format_ARGB32);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setPen(Qt::NoPen);
    painter.setBrush(mark);
    painter.drawRect(6, 6, 20, 20);
    painter.end();
    return icon.save(path) ? QUrl::fromLocalFile(path) : QUrl{};
}

} // namespace

class UiTestSetup final : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The settings page's about section reads Qt.application.version, so the
        // test host has to carry the version the browser does.
        QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));
        omaweb::quickshell::installShim(*engine);
        omaweb::registerFaviconTint();
        omaweb::registerSystemClipboard();
        omaweb::registerExternalProtocolHandler();
        omaweb::registerPagePrinter();
        omaweb::registerSystemNotifier();
        omaweb::registerProcessResources();
        m_dataRoot = std::make_unique<QTemporaryDir>();
        m_browser = std::make_unique<omaweb::BrowserController>(
            m_dataRoot->path(), QStringLiteral("mock"));
        m_contentBlocker = std::make_unique<omaweb::ContentBlocker>(m_dataRoot->path());
        const auto keybindingsPath = m_dataRoot->filePath(QStringLiteral("keybindings.json"));
        QFile::copy(QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH), keybindingsPath);
        QFile::setPermissions(keybindingsPath,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        m_keyboardNavigation = std::make_unique<omaweb::KeyboardNavigation>(keybindingsPath);
        m_theme = std::make_unique<omaweb::ThemeController>(QStringLiteral(OMAWEB_THEME_PATH));
        m_windowManager = std::make_unique<omaweb::WindowManager>(QStringLiteral("mock"));
        engine->rootContext()->setContextProperty(QStringLiteral("browser"), m_browser.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("contentBlocker"), m_contentBlocker.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("keyboardNavigation"), m_keyboardNavigation.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
        // The lab runs no engine, so there is no third-party filter to attach.
        // Site information reads the gap off the adapter's capabilities.
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineCookiePolicy"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(QStringLiteral("theme"), m_theme.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("windowManager"), m_windowManager.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_MOCK_ENGINE_VIEW_URL)));
        engine->rootContext()->setContextProperty(QStringLiteral("engineProfileSource"),
            QUrl(QStringLiteral(OMAWEB_MOCK_ENGINE_PROFILE_URL)));
        engine->rootContext()->setContextProperty(
            QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
        // The mock engine reports no icon of its own here; the tests that care
        // about artwork set one themselves.
        engine->rootContext()->setContextProperty(
            QStringLiteral("mockFaviconUrls"), QVariantList{});
        engine->rootContext()->setContextProperty(QStringLiteral("colouredFaviconUrl"),
            writeFavicon(m_dataRoot->filePath(QStringLiteral("coloured.png")),
                QColor(0x2f, 0x5c, 0xe6)));
        engine->rootContext()->setContextProperty(QStringLiteral("colourlessFaviconUrl"),
            writeFavicon(m_dataRoot->filePath(QStringLiteral("colourless.png")), Qt::white));
        // The shim picks the Qt Quick Controls style the vendored kit needs; a
        // native style refuses the kit's replaced `background` and paints its
        // own. Reading the resolved name back is the only way QML can tell.
        engine->rootContext()->setContextProperty(
            QStringLiteral("controlsStyle"), QQuickStyle::name());
        engine->addImportPath(QStringLiteral(OMAWEB_UI_DIRECTORY));
        engine->addImportPath(QStringLiteral(OMAWEB_OMARCHY_IMPORT_PATH));
        m_kitTheme = std::make_unique<omaweb::KitTheme>(engine, m_theme.get());
    }

    void cleanupTestCase()
    {
        m_kitTheme.reset();
        m_theme.reset();
        m_windowManager.reset();
        m_browser.reset();
        m_contentBlocker.reset();
        m_keyboardNavigation.reset();
        m_dataRoot.reset();
    }

private:
    std::unique_ptr<QTemporaryDir> m_dataRoot;
    std::unique_ptr<omaweb::BrowserController> m_browser;
    std::unique_ptr<omaweb::ContentBlocker> m_contentBlocker;
    std::unique_ptr<omaweb::KeyboardNavigation> m_keyboardNavigation;
    std::unique_ptr<omaweb::ThemeController> m_theme;
    std::unique_ptr<omaweb::KitTheme> m_kitTheme;
    std::unique_ptr<omaweb::WindowManager> m_windowManager;
};

QUICK_TEST_MAIN_WITH_SETUP(omaweb_ui, UiTestSetup)

#include "tst_ui.moc"
