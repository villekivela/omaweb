#include "LaunchRequest.h"

#include <QTest>

using omaweb::readLaunchUrl;

class LaunchRequestTest final : public QObject {
    Q_OBJECT

private slots:
    void readsTheAddressTheDesktopAskedFor();
    void ignoresOmawebsOwnSwitches();
    void refusesASchemeThatIsNotTheWeb();
    void refusesAnArgumentThatIsNotAnAddress();
    void takesTheFirstAddressOnly();
};

// Being the default browser is being run with an address, so this is the whole
// of that contract.
void LaunchRequestTest::readsTheAddressTheDesktopAskedFor()
{
    QCOMPARE(readLaunchUrl({QStringLiteral("omaweb"), QStringLiteral("https://example.com/a")}),
        QUrl(QStringLiteral("https://example.com/a")));
    QCOMPARE(readLaunchUrl({QStringLiteral("omaweb"), QStringLiteral("http://example.com")}),
        QUrl(QStringLiteral("http://example.com")));
    // A local page is a page. `file:` is how a desktop opens one.
    QCOMPARE(readLaunchUrl({QStringLiteral("omaweb"), QStringLiteral("file:///tmp/page.html")}),
        QUrl(QStringLiteral("file:///tmp/page.html")));
    // Nothing asked for is nothing opened.
    QVERIFY(!readLaunchUrl({QStringLiteral("omaweb")}).isValid());
    QVERIFY(!readLaunchUrl({}).isValid());
}

void LaunchRequestTest::ignoresOmawebsOwnSwitches()
{
    QCOMPARE(readLaunchUrl({QStringLiteral("omaweb"), QStringLiteral("--remote-debugging=9222"),
                 QStringLiteral("https://example.com")}),
        QUrl(QStringLiteral("https://example.com")));
    QVERIFY(!readLaunchUrl({QStringLiteral("omaweb"), QStringLiteral("--validate-qml")}).isValid());
}

// The address arrives from whatever put it in front of the reader, so a scheme
// that runs code in a page, or reaches something that is not the web, is not
// one an outside caller gets to name.
void LaunchRequestTest::refusesASchemeThatIsNotTheWeb()
{
    for (const auto &refused : {QStringLiteral("javascript:alert(1)"),
             QStringLiteral("data:text/html,<script>alert(1)</script>"),
             QStringLiteral("omaweb://settings"), QStringLiteral("chrome://net-internals"),
             QStringLiteral("ftp://example.com/file")}) {
        QVERIFY2(
            !readLaunchUrl({QStringLiteral("omaweb"), refused}).isValid(), qPrintable(refused));
    }
}

// A word on the command line is not an address, and repairing it into one would
// turn a typo into a search the reader never asked for.
void LaunchRequestTest::refusesAnArgumentThatIsNotAnAddress()
{
    for (const auto &refused : {QStringLiteral("example.com"), QStringLiteral("not a url"),
             QStringLiteral("/tmp/page.html"), QStringLiteral("")}) {
        QVERIFY2(
            !readLaunchUrl({QStringLiteral("omaweb"), refused}).isValid(), qPrintable(refused));
    }
}

// A desktop wanting several pages open runs the browser several times, which is
// what the handover between them is for.
void LaunchRequestTest::takesTheFirstAddressOnly()
{
    QCOMPARE(readLaunchUrl({QStringLiteral("omaweb"), QStringLiteral("https://first.example"),
                 QStringLiteral("https://second.example")}),
        QUrl(QStringLiteral("https://first.example")));
}

QTEST_APPLESS_MAIN(LaunchRequestTest)

#include "tst_launchrequest.moc"
