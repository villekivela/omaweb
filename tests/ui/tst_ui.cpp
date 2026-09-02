#include "BrowserController.h"
#include "ContentBlocker.h"
#include "FaviconTint.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "Quickshell.h"
#include "SystemClipboard.h"
#include "ThemeController.h"
#include "WindowManager.h"

#include <QColor>
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
        tanto::quickshell::installShim();
        tanto::registerFaviconTint();
        tanto::registerSystemClipboard();
        m_dataRoot = std::make_unique<QTemporaryDir>();
        m_browser = std::make_unique<tanto::BrowserController>(
            m_dataRoot->path(), QStringLiteral("mock"));
        m_contentBlocker = std::make_unique<tanto::ContentBlocker>(m_dataRoot->path());
        const auto keybindingsPath = m_dataRoot->filePath(QStringLiteral("keybindings.json"));
        QFile::copy(QStringLiteral(TANTO_DEFAULT_KEYBINDINGS_PATH), keybindingsPath);
        QFile::setPermissions(keybindingsPath,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        m_keyboardNavigation = std::make_unique<tanto::KeyboardNavigation>(keybindingsPath);
        m_theme = std::make_unique<tanto::ThemeController>(QStringLiteral(TANTO_THEME_PATH));
        m_windowManager = std::make_unique<tanto::WindowManager>(QStringLiteral("mock"));
        engine->rootContext()->setContextProperty(QStringLiteral("browser"), m_browser.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("contentBlocker"), m_contentBlocker.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("keyboardNavigation"), m_keyboardNavigation.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(QStringLiteral("theme"), m_theme.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("windowManager"), m_windowManager.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineViewSource"), QUrl(QStringLiteral(TANTO_MOCK_ENGINE_VIEW_URL)));
        engine->rootContext()->setContextProperty(QStringLiteral("engineProfileSource"),
            QUrl(QStringLiteral(TANTO_MOCK_ENGINE_PROFILE_URL)));
        engine->rootContext()->setContextProperty(
            QStringLiteral("iconFontSource"), QUrl(QStringLiteral(TANTO_ICON_FONT_URL)));
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
        engine->addImportPath(QStringLiteral(TANTO_UI_DIRECTORY));
        engine->addImportPath(QStringLiteral(TANTO_OMARCHY_IMPORT_PATH));
        m_kitTheme = std::make_unique<tanto::KitTheme>(engine, m_theme.get());
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
    std::unique_ptr<tanto::BrowserController> m_browser;
    std::unique_ptr<tanto::ContentBlocker> m_contentBlocker;
    std::unique_ptr<tanto::KeyboardNavigation> m_keyboardNavigation;
    std::unique_ptr<tanto::ThemeController> m_theme;
    std::unique_ptr<tanto::KitTheme> m_kitTheme;
    std::unique_ptr<tanto::WindowManager> m_windowManager;
};

QUICK_TEST_MAIN_WITH_SETUP(tanto_ui, UiTestSetup)

#include "tst_ui.moc"
