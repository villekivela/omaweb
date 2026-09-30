#include "HttpsOnly.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::HttpsOnly;

namespace {

const QString space = QStringLiteral("space-1");

} // namespace

class HttpsOnlyTest final : public QObject {
    Q_OBJECT

private slots:
    void isOnUntilTheReaderTurnsItOff();
    void upgradesAPagesOwnPlainAddress();
    void leavesLocalDevelopmentAddressesAlone();
    void letsASiteThroughForTheLoadTheReaderAskedFor();
    void letsARememberedSiteThroughInItsSpaceOnly();
    void asksAboutARememberedSiteByItsStoredOrigin();
    void namesAFailedUpgradeByThePlainAddress();
    void forgetsAFailedUpgrade();
    void namesAPageItSentOverHttps();
    void refusesASiteThatSendsTheLoadBack();
    void namesAFormItRefused();
};

// On by default, and off only when the reader says so, which lasts.
void HttpsOnlyTest::isOnUntilTheReaderTurnsItOff()
{
    QTemporaryDir root;
    {
        HttpsOnly mode(root.path());
        QVERIFY(mode.enabled());
        QSignalSpy changed(&mode, &HttpsOnly::enabledChanged);
        mode.setEnabled(false);
        QCOMPARE(changed.size(), 1);
        QVERIFY(!mode.upgrade(QUrl(QStringLiteral("http://site.example/")), space).isValid());
    }
    HttpsOnly again(root.path());
    QVERIFY(!again.enabled());
}

void HttpsOnlyTest::upgradesAPagesOwnPlainAddress()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    QCOMPARE(mode.upgrade(QUrl(QStringLiteral("http://site.example/a?b=c#d")), space),
        QUrl(QStringLiteral("https://site.example/a?b=c#d")));
    QCOMPARE(mode.upgrade(QUrl(QStringLiteral("http://site.example:8080/")), space),
        QUrl(QStringLiteral("https://site.example:8080/")));
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("https://site.example/")), space).isValid());
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("ftp://site.example/")), space).isValid());
}

// The same set the Omnibar sends over plain HTTP: a Local-development site,
// and a name without a dot given a port, such as a machine on the network.
void HttpsOnlyTest::leavesLocalDevelopmentAddressesAlone()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    for (const auto *address :
        {"http://localhost:3000/", "http://app.localhost/", "http://site.test/",
            "http://127.0.0.1/", "http://192.168.1.4/", "http://[::1]/", "http://devbox:8080/"}) {
        QVERIFY2(!mode.upgrade(QUrl(QString::fromLatin1(address)), space).isValid(), address);
    }
    QVERIFY(mode.upgrade(QUrl(QStringLiteral("http://devbox/")), space).isValid());
}

// The load the reader asked for, same-site redirects included, and not the
// next visit.
void HttpsOnlyTest::letsASiteThroughForTheLoadTheReaderAskedFor()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl plain(QStringLiteral("http://plain.example/"));
    mode.allowOnce(space, plain);
    QVERIFY(!mode.sendsOverHttps(space, plain));
    QVERIFY(mode.sendsOverHttps(QStringLiteral("space-2"), plain));
    QVERIFY(!mode.upgrade(plain, space).isValid());
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("http://plain.example/moved")), space).isValid());
    QVERIFY(mode.upgrade(plain, QStringLiteral("space-2")).isValid());
    QVERIFY(!mode.arrived(space, QUrl(QStringLiteral("http://plain.example/moved"))));
    QVERIFY(mode.sendsOverHttps(space, plain));
    QVERIFY(mode.upgrade(plain, space).isValid());
}

void HttpsOnlyTest::letsARememberedSiteThroughInItsSpaceOnly()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    mode.setRemembered([](const QString &spaceId, const QString &origin) {
        return spaceId == space && origin == QStringLiteral("http://plain.example");
    });
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("http://plain.example/a")), space).isValid());
    QVERIFY(mode.upgrade(QUrl(QStringLiteral("http://plain.example/a")), QStringLiteral("space-2"))
            .isValid());
    QVERIFY(mode.upgrade(QUrl(QStringLiteral("http://other.example/")), space).isValid());
}

// A site's permissions are kept under its origin as the browser writes it:
// the default port left out and the host in its ASCII form. The address a
// page asks for may spell either differently.
void HttpsOnlyTest::asksAboutARememberedSiteByItsStoredOrigin()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    QStringList asked;
    mode.setRemembered([&asked](const QString &, const QString &origin) {
        asked.append(origin);
        return true;
    });
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("http://Plain.example:80/a")), space).isValid());
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("http://bücher.example/")), space).isValid());
    QCOMPARE(asked,
        QStringList({QStringLiteral("http://plain.example"),
            QStringLiteral("http://xn--bcher-kva.example")}));
}

// A failed load of the upgraded address is the mode's to explain, naming the
// address the reader asked for; any other failure is not.
void HttpsOnlyTest::namesAFailedUpgradeByThePlainAddress()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl plain(QStringLiteral("http://plain.example/page"));
    const auto upgraded = mode.upgrade(plain, space);
    QVERIFY(mode.failed(QStringLiteral("space-2"), upgraded).isEmpty());
    QVERIFY(mode.failed(space, QUrl(QStringLiteral("https://other.example/"))).isEmpty());
    const auto failure = mode.failed(space, upgraded);
    QCOMPARE(failure.value(QStringLiteral("plainUrl")).toUrl(), plain);
    QCOMPARE(failure.value(QStringLiteral("host")).toString(), QStringLiteral("plain.example"));
    QCOMPARE(failure.value(QStringLiteral("reason")).toString(), QStringLiteral("unreachable"));
}

// The load that failed is over. A plain request to the same site after it is
// the reader trying again, not the site sending an upgraded load back, and a
// later visit to the secure address that fails is not the mode's to explain.
void HttpsOnlyTest::forgetsAFailedUpgrade()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl plain(QStringLiteral("http://plain.example/page"));
    const auto upgraded = mode.upgrade(plain, space);
    QVERIFY(!mode.failed(space, upgraded).isEmpty());
    QVERIFY(!mode.refusesDowngrade(plain, space));
    QVERIFY(mode.failed(space, upgraded).isEmpty());
}

// Site information says a page came over HTTPS because the mode sent it
// there, including when the site then moved it on its own host, and not for
// a later visit to the same secure address.
void HttpsOnlyTest::namesAPageItSentOverHttps()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    QVERIFY(mode.upgrade(QUrl(QStringLiteral("http://site.example/")), space).isValid());
    QVERIFY(
        !mode.arrived(QStringLiteral("space-2"), QUrl(QStringLiteral("https://site.example/"))));
    QVERIFY(mode.arrived(space, QUrl(QStringLiteral("https://site.example/home"))));
    QVERIFY(!mode.arrived(space, QUrl(QStringLiteral("https://site.example/"))));
    QVERIFY(mode.failed(space, QUrl(QStringLiteral("https://site.example/"))).isEmpty());
    QVERIFY(!mode.arrived(space, QUrl(QStringLiteral("https://other.example/"))));
}

// Upgrading the plain address a site redirects back to would go round for
// good, so it is refused once; after the page arrives, a plain link to the
// same site is a new load and upgraded as usual.
void HttpsOnlyTest::refusesASiteThatSendsTheLoadBack()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl plain(QStringLiteral("http://loop.example/"));
    QVERIFY(mode.upgrade(plain, space).isValid());
    QVERIFY(!mode.sendsOverHttps(space, plain));
    QVERIFY(mode.refusesDowngrade(plain, space));
    QVERIFY(!mode.refusesDowngrade(plain, space));
    QCOMPARE(mode.failed(space, plain).value(QStringLiteral("reason")).toString(),
        QStringLiteral("downgrade"));

    QVERIFY(mode.upgrade(plain, space).isValid());
    mode.arrived(space, QUrl(QStringLiteral("https://loop.example/")));
    QVERIFY(!mode.refusesDowngrade(plain, space));

    // A site the reader let through may send its load where it likes.
    QVERIFY(mode.upgrade(plain, space).isValid());
    mode.allowOnce(space, plain);
    QVERIFY(!mode.refusesDowngrade(plain, space));
    QVERIFY(mode.failed(space, plain).isEmpty());
}

void HttpsOnlyTest::namesAFormItRefused()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl form(QStringLiteral("http://form.example/submit"));
    QVERIFY(mode.upgrade(form, space).isValid());
    mode.refuse(form, space);
    QVERIFY(!mode.refusesDowngrade(form, space));
    QCOMPARE(mode.failed(space, form).value(QStringLiteral("reason")).toString(),
        QStringLiteral("form"));
    QVERIFY(!mode.arrived(space, QUrl(QStringLiteral("https://form.example/submit"))));
}

QTEST_GUILESS_MAIN(HttpsOnlyTest)
#include "tst_httpsonly.moc"
