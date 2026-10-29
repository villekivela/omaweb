#include "DefaultBrowser.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace omaweb;

namespace {

// `xdg-settings` as it answers on a desktop that keeps the browser in
// `mimeapps.list`. On Omarchy, `routed` stands for Hyprland's own list, which
// names Omarchy's router and is read first while `XDG_CURRENT_DESKTOP` names
// the desktop. With `BROWSER` set the generic backend answers from that
// instead. A `set` checks itself by reading the answer back, as the real tool
// does, and fails when the answer is not what it set.
const auto kSettingsTool = QStringLiteral(R"(#!/bin/sh
dir='%1'
if [ "$1" = set ]; then
    printf '%s\n' "$3" > "$dir/browser"
fi
if [ -n "$BROWSER" ]; then
    answer="$BROWSER"
elif [ -n "$XDG_CURRENT_DESKTOP" ] && [ -e "$dir/routed" ]; then
    answer=omarchy-browser.desktop
else
    read -r answer < "$dir/browser"
fi
if [ "$1" = get ]; then
    printf '%s\n' "$answer"
    exit 0
fi
[ "$answer" = "$3" ]
)");

// Omarchy's own answer: the browser the router opens, whichever list names it.
const auto kOmarchyTool = QStringLiteral(R"(#!/bin/sh
read -r answer < '%1/browser'
printf '%s\n' "$answer"
)");

} // namespace

class DefaultBrowserTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void takesTheDesktopsAnswer();
    void seesTheBrowserBehindOmarchysRouter();
    void becomesTheDefault();
    void becomesTheDefaultBehindOmarchysRouter();
    void readsTheAnswerAgain();

private:
    // A desktop with `browser` as its default, and with Omarchy's router in
    // front of it when `omarchy` is true.
    void desktop(const QString &browser, bool omarchy);
    void setBrowser(const QString &browser);
    void install(const QString &name, const QString &body);

    QTemporaryDir m_tools;
    QByteArray m_path;
    QByteArray m_desktop;
    QByteArray m_browser;
};

void DefaultBrowserTest::init()
{
    QVERIFY(m_tools.isValid());
    m_path = qgetenv("PATH");
    m_desktop = qgetenv("XDG_CURRENT_DESKTOP");
    m_browser = qgetenv("BROWSER");
    // Only the stubs: this machine's own Omarchy would otherwise answer.
    qputenv("PATH", QFile::encodeName(m_tools.path()));
    install(QStringLiteral("xdg-settings"), kSettingsTool);
}

void DefaultBrowserTest::cleanup()
{
    qputenv("PATH", m_path);
    qputenv("XDG_CURRENT_DESKTOP", m_desktop);
    qputenv("BROWSER", m_browser);
    QFile::remove(m_tools.filePath(QStringLiteral("omarchy-cmd-default-browser")));
    QFile::remove(m_tools.filePath(QStringLiteral("routed")));
}

void DefaultBrowserTest::desktop(const QString &browser, bool omarchy)
{
    setBrowser(browser);
    if (omarchy) {
        install(QStringLiteral("omarchy-cmd-default-browser"), kOmarchyTool);
        QFile routed(m_tools.filePath(QStringLiteral("routed")));
        QVERIFY(routed.open(QIODevice::WriteOnly));
        qputenv("XDG_CURRENT_DESKTOP", "Hyprland");
        // The session points `BROWSER` at Omarchy's launcher.
        qputenv("BROWSER", "omarchy-launch-browser");
    } else {
        qputenv("XDG_CURRENT_DESKTOP", "GNOME");
        qunsetenv("BROWSER");
    }
}

void DefaultBrowserTest::setBrowser(const QString &browser)
{
    QFile file(m_tools.filePath(QStringLiteral("browser")));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(browser.toUtf8() + '\n');
}

void DefaultBrowserTest::install(const QString &name, const QString &body)
{
    QFile file(m_tools.filePath(name));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(body.arg(m_tools.path()).toUtf8());
    file.close();
    QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
}

// Without Omarchy, what `xdg-settings` names is the default.
void DefaultBrowserTest::takesTheDesktopsAnswer()
{
    desktop(QStringLiteral("omaweb.desktop"), false);
    QVERIFY(DefaultBrowser().isDefault());

    desktop(QStringLiteral("firefox.desktop"), false);
    QVERIFY(!DefaultBrowser().isDefault());
}

// On Omarchy `xdg-settings` names the router, which opens whatever browser
// Omarchy has recorded behind it. That one is the default.
void DefaultBrowserTest::seesTheBrowserBehindOmarchysRouter()
{
    desktop(QStringLiteral("omaweb.desktop"), true);
    QVERIFY(DefaultBrowser().isDefault());

    desktop(QStringLiteral("firefox.desktop"), true);
    QVERIFY(!DefaultBrowser().isDefault());
}

void DefaultBrowserTest::becomesTheDefault()
{
    desktop(QStringLiteral("firefox.desktop"), false);
    DefaultBrowser browser;
    QSignalSpy changed(&browser, &DefaultBrowser::changed);
    QVERIFY(browser.makeDefault());
    QVERIFY(browser.isDefault());
    QCOMPARE(changed.count(), 1);
}

// Omarchy sets the browser with `BROWSER` unset and no desktop named, so that
// `xdg-settings` reads back `mimeapps.list` rather than the router's list.
void DefaultBrowserTest::becomesTheDefaultBehindOmarchysRouter()
{
    desktop(QStringLiteral("firefox.desktop"), true);
    DefaultBrowser browser;
    QVERIFY(!browser.isDefault());
    QVERIFY(browser.makeDefault());
    QVERIFY(browser.isDefault());
}

// A reader can change the default outside the browser, and `refresh` hears it.
void DefaultBrowserTest::readsTheAnswerAgain()
{
    desktop(QStringLiteral("firefox.desktop"), true);
    DefaultBrowser browser;
    QVERIFY(!browser.isDefault());
    QSignalSpy changed(&browser, &DefaultBrowser::changed);

    setBrowser(QStringLiteral("omaweb.desktop"));
    browser.refresh();
    QTRY_VERIFY(browser.isDefault());
    QCOMPARE(changed.count(), 1);
}

QTEST_GUILESS_MAIN(DefaultBrowserTest)
#include "tst_defaultbrowser.moc"
