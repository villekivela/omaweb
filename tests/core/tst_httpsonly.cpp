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
    void namesAFailedUpgradeByThePlainAddress();
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

// The same set the Omnibar sends over plain HTTP.
void HttpsOnlyTest::leavesLocalDevelopmentAddressesAlone()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    for (const auto *address : {"http://localhost:3000/", "http://app.localhost/",
             "http://site.test/", "http://127.0.0.1/", "http://192.168.1.4/", "http://[::1]/"}) {
        QVERIFY2(!mode.upgrade(QUrl(QString::fromLatin1(address)), space).isValid(), address);
    }
}

// The load the reader asked for, same-site redirects included, and not the
// next visit.
void HttpsOnlyTest::letsASiteThroughForTheLoadTheReaderAskedFor()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl plain(QStringLiteral("http://plain.example/"));
    mode.allowOnce(space, plain);
    QVERIFY(!mode.upgrade(plain, space).isValid());
    QVERIFY(!mode.upgrade(QUrl(QStringLiteral("http://plain.example/moved")), space).isValid());
    QVERIFY(mode.upgrade(plain, QStringLiteral("space-2")).isValid());
    mode.arrived(space, QUrl(QStringLiteral("http://plain.example/moved")));
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

// A failed load of the upgraded address is the mode's to explain, naming the
// address the reader asked for; any other failure is not.
void HttpsOnlyTest::namesAFailedUpgradeByThePlainAddress()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl plain(QStringLiteral("http://plain.example/page"));
    const auto upgraded = mode.upgrade(plain, space);
    QVERIFY(mode.upgradedTo(space, upgraded));
    const auto failure = mode.failure(space, upgraded);
    QCOMPARE(failure.value(QStringLiteral("plainUrl")).toUrl(), plain);
    QCOMPARE(failure.value(QStringLiteral("host")).toString(), QStringLiteral("plain.example"));
    QCOMPARE(failure.value(QStringLiteral("reason")).toString(), QStringLiteral("unreachable"));
    QVERIFY(mode.failure(space, QUrl(QStringLiteral("https://other.example/"))).isEmpty());
    QVERIFY(mode.failure(QStringLiteral("space-2"), upgraded).isEmpty());
    QVERIFY(!mode.upgradedTo(space, QUrl(QStringLiteral("https://other.example/"))));
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
    QVERIFY(mode.refusesDowngrade(plain, space));
    QCOMPARE(mode.failure(space, plain).value(QStringLiteral("reason")).toString(),
        QStringLiteral("downgrade"));
    QVERIFY(!mode.refusesDowngrade(plain, space));

    QVERIFY(mode.upgrade(plain, space).isValid());
    mode.arrived(space, QUrl(QStringLiteral("https://loop.example/")));
    QVERIFY(!mode.refusesDowngrade(plain, space));

    // A site the reader let through may send its load where it likes.
    QVERIFY(mode.upgrade(plain, space).isValid());
    mode.allowOnce(space, plain);
    QVERIFY(!mode.refusesDowngrade(plain, space));
    QVERIFY(mode.failure(space, plain).isEmpty());
}

void HttpsOnlyTest::namesAFormItRefused()
{
    QTemporaryDir root;
    HttpsOnly mode(root.path());
    const QUrl form(QStringLiteral("http://form.example/submit"));
    QVERIFY(mode.upgrade(form, space).isValid());
    mode.refuse(form, space);
    QCOMPARE(mode.failure(space, form).value(QStringLiteral("reason")).toString(),
        QStringLiteral("form"));
    QVERIFY(!mode.upgradedTo(space, QUrl(QStringLiteral("https://form.example/submit"))));
    QVERIFY(!mode.refusesDowngrade(form, space));
}

QTEST_GUILESS_MAIN(HttpsOnlyTest)
#include "tst_httpsonly.moc"
