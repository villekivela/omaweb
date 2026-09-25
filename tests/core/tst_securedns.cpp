#include "SecureDns.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::SecureDns;

class SecureDnsTest final : public QObject {
    Q_OBJECT

private slots:
    void isOffWithoutBeingSetUp();
    void keepsTheReadersResolverAcrossARestart();
    void namesTheResolversItOffers();
    void refusesAResolverItDoesNotName();
    void takesAnAddressTheReaderTypes_data();
    void takesAnAddressTheReaderTypes();
    void turningItOffIsKept();
    void namesTheResolverInUse();
};

// A resolver the reader did not choose is not a safer default, so a first run
// leaves names to the system and writes nothing.
void SecureDnsTest::isOffWithoutBeingSetUp()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const SecureDns dns(root.filePath(QStringLiteral("config")));
    QVERIFY(dns.resolver().isEmpty());
    QVERIFY(dns.serverTemplate().isEmpty());
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/privacy.json"))));
}

void SecureDnsTest::keepsTheReadersResolverAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        SecureDns dns(configRoot);
        QSignalSpy changed(&dns, &SecureDns::changed);
        QVERIFY(dns.useResolver(QStringLiteral("quad9")));
        QCOMPARE(changed.count(), 1);
    }
    const SecureDns restarted(configRoot);
    QCOMPARE(restarted.resolver(), QStringLiteral("quad9"));
    QCOMPARE(restarted.serverTemplate(), QStringLiteral("https://dns.quad9.net/dns-query"));
}

// Settings offers these by name, and says whose resolver is in use.
void SecureDnsTest::namesTheResolversItOffers()
{
    QTemporaryDir root;
    const SecureDns dns(root.filePath(QStringLiteral("config")));
    QStringList ids;
    for (const auto &entry : dns.resolvers()) {
        const auto resolver = entry.toMap();
        QVERIFY(!resolver.value(QStringLiteral("title")).toString().isEmpty());
        QVERIFY(resolver.value(QStringLiteral("template"))
                .toString()
                .startsWith(QStringLiteral("https://")));
        ids.append(resolver.value(QStringLiteral("id")).toString());
    }
    QCOMPARE(ids,
        (QStringList {
            QStringLiteral("quad9"), QStringLiteral("cloudflare"), QStringLiteral("mullvad")}));
}

void SecureDnsTest::refusesAResolverItDoesNotName()
{
    QTemporaryDir root;
    SecureDns dns(root.filePath(QStringLiteral("config")));
    QVERIFY(!dns.useResolver(QStringLiteral("example")));
    QVERIFY(dns.resolver().isEmpty());
}

// A custom address is a DoH template, which is an `https:` address with a
// host. Anything else would either send names in the clear or send them
// nowhere, so it is refused before it is saved.
void SecureDnsTest::takesAnAddressTheReaderTypes_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<bool>("taken");
    QTest::newRow("path") << QStringLiteral("https://dns.example/dns-query") << true;
    QTest::newRow("template") << QStringLiteral("https://dns.example/dns-query{?dns}") << true;
    QTest::newRow("plain http") << QStringLiteral("http://dns.example/dns-query") << false;
    QTest::newRow("no host") << QStringLiteral("https:///dns-query") << false;
    QTest::newRow("not an address") << QStringLiteral("dns.example") << false;
    QTest::newRow("empty") << QString() << false;
}

void SecureDnsTest::takesAnAddressTheReaderTypes()
{
    QFETCH(QString, address);
    QFETCH(bool, taken);
    QTemporaryDir root;
    const auto configRoot = root.filePath(QStringLiteral("config"));
    SecureDns dns(configRoot);
    QCOMPARE(dns.useCustom(address), taken);
    const SecureDns restarted(configRoot);
    QCOMPARE(restarted.resolver(), taken ? QStringLiteral("custom") : QString());
    QCOMPARE(restarted.serverTemplate(), taken ? address : QString());
}

void SecureDnsTest::turningItOffIsKept()
{
    QTemporaryDir root;
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        SecureDns dns(configRoot);
        QVERIFY(dns.useResolver(QStringLiteral("mullvad")));
        dns.turnOff();
    }
    const SecureDns restarted(configRoot);
    QVERIFY(restarted.resolver().isEmpty());
    QVERIFY(restarted.serverTemplate().isEmpty());
}

// What Settings and Site information call the resolver: a named one by its
// name, a typed one by its address, and nothing when the system looks up.
void SecureDnsTest::namesTheResolverInUse()
{
    QTemporaryDir root;
    SecureDns dns(root.filePath(QStringLiteral("config")));
    QVERIFY(dns.resolverTitle().isEmpty());
    QVERIFY(dns.useResolver(QStringLiteral("cloudflare")));
    QCOMPARE(dns.resolverTitle(), QStringLiteral("Cloudflare"));
    QVERIFY(dns.useCustom(QStringLiteral("https://dns.example/dns-query")));
    QCOMPARE(dns.resolverTitle(), QStringLiteral("https://dns.example/dns-query"));
}

QTEST_GUILESS_MAIN(SecureDnsTest)

#include "tst_securedns.moc"
