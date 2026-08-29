// THROWAWAY PROTOTYPE — chrome-design runner. See README.md in this directory.
//
// Runs the variant shell against the real BrowserController, seeded with enough
// tabs that the sidebar has to cope with realistic density. Session data goes
// to a temporary directory and is discarded on exit.
#include "BrowserController.h"
#include "ThemeController.h"
#include "WindowChrome.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QUrl>

namespace {

struct Seed {
    const char *address;
    const char *title;
    bool pinned;
};

constexpr Seed seeds[] = {
    {"github.com/villekivela/tanto", "villekivela/tanto: keyboard-driven browser", true},
    {"localhost:5173/product/requirements", "Tanto requirements — dev", true},
    {"linear.app/tanto/team/TAN/board", "TAN board · Linear", true},
    {"mail.proton.me/u/0/inbox", "Inbox (4) · Proton Mail", true},
    {"doc.qt.io/qt-6/qtquick-index.html", "Qt Quick | Qt 6.11", true},
    {"news.ycombinator.com", "Hacker News", false},
    {"developer.mozilla.org/en-US/docs/Web/API/Window", "Window — Web APIs | MDN", false},
    {"ladybird.org", "Ladybird Browser", false},
    {"wayland.freedesktop.org/docs/html/apa.html", "Wayland Protocol Specification", false},
    {"github.com/LadybirdBrowser/ladybird/pull/1234",
        "Add an embedding API surface for host applications by nico · Pull Request #1234", false},
    {"duckduckgo.com/?q=qt+frameless+window+wayland", "qt frameless window wayland at DuckDuckGo", false},
    {"localhost:8080/metrics", "metrics — 127.0.0.1:8080", false},
};

void seedTabs(tanto::BrowserController &browser)
{
    const auto blankTabId = browser.activeTabId();
    QString tabToActivate;

    for (const auto &seed : seeds) {
        const auto address = QString::fromUtf8(seed.address);
        browser.openInput(address, true);
        browser.updateTab(
            browser.activeTabId(), browser.activeUrl(), QString::fromUtf8(seed.title));
        if (seed.pinned) {
            browser.toggleActivePinned();
        }
        if (qstrcmp(seed.address, "ladybird.org") == 0) {
            tabToActivate = browser.activeTabId();
        }
    }

    // Drop the empty tab the controller creates for a fresh Space.
    browser.activateTab(blankTabId);
    browser.closeActiveTab();

    if (!tabToActivate.isEmpty()) {
        browser.activateTab(tabToActivate);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    tanto::installWindowChrome(&application);
    QCoreApplication::setOrganizationName(QStringLiteral("Tanto"));
    QCoreApplication::setApplicationName(QStringLiteral("Tanto UI Prototype"));

    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        qCritical("Could not create temporary prototype data directory.");
        return 1;
    }

    tanto::BrowserController browser(dataRoot.path(), QStringLiteral("mock"));
    if (!browser.ready()) {
        qCritical("%s", qPrintable(browser.errorMessage()));
        return 1;
    }
    seedTabs(browser);

    tanto::ThemeController theme(QStringLiteral(TANTO_THEME_PATH));

    auto variant = qEnvironmentVariable("TANTO_UI_VARIANT", QStringLiteral("A"));
    if (variant.isEmpty()) {
        variant = QStringLiteral("A");
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("prototypeVariant"), variant);
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(TANTO_ICON_FONT_URL)));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &application, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral(TANTO_PROTOTYPE_SHELL_URL)));

    return application.exec();
}
