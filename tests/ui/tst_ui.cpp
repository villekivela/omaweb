#include "BrowserController.h"
#include "ContentBlocker.h"
#include "KeyboardNavigation.h"
#include "ThemeController.h"
#include "WindowManager.h"

#include <QQmlContext>
#include <QFile>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include <memory>

class UiTestSetup final : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        m_dataRoot = std::make_unique<QTemporaryDir>();
        m_browser = std::make_unique<tanto::BrowserController>(
            m_dataRoot->path(), QStringLiteral("mock"));
        m_contentBlocker = std::make_unique<tanto::ContentBlocker>(m_dataRoot->path());
        const auto keybindingsPath = m_dataRoot->filePath(QStringLiteral("keybindings.json"));
        QFile keybindings(keybindingsPath);
        if (keybindings.open(QIODevice::WriteOnly)) {
            keybindings.write(R"JSON({"version":1,"enabled":false,"bindings":{"j":"scroll-down"},"passthrough":{}})JSON");
        }
        keybindings.close();
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
        engine->addImportPath(QStringLiteral(TANTO_UI_DIRECTORY));
    }

    void cleanupTestCase()
    {
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
    std::unique_ptr<tanto::WindowManager> m_windowManager;
};

QUICK_TEST_MAIN_WITH_SETUP(tanto_ui, UiTestSetup)

#include "tst_ui.moc"
