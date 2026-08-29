#include "BrowserController.h"
#include "ThemeController.h"

#include <QQmlContext>
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
        m_theme = std::make_unique<tanto::ThemeController>(QStringLiteral(TANTO_THEME_PATH));
        engine->rootContext()->setContextProperty(QStringLiteral("browser"), m_browser.get());
        engine->rootContext()->setContextProperty(QStringLiteral("theme"), m_theme.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineViewSource"), QUrl(QStringLiteral(TANTO_MOCK_ENGINE_VIEW_URL)));
        engine->rootContext()->setContextProperty(QStringLiteral("iconFontSource"), QUrl{});
        engine->addImportPath(QStringLiteral(TANTO_UI_DIRECTORY));
    }

    void cleanupTestCase()
    {
        m_theme.reset();
        m_browser.reset();
        m_dataRoot.reset();
    }

private:
    std::unique_ptr<QTemporaryDir> m_dataRoot;
    std::unique_ptr<tanto::BrowserController> m_browser;
    std::unique_ptr<tanto::ThemeController> m_theme;
};

QUICK_TEST_MAIN_WITH_SETUP(tanto_ui, UiTestSetup)

#include "tst_ui.moc"
