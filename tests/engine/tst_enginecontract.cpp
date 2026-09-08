#include "EngineCapabilities.h"
#include "EngineCapabilityExpectations.h"
#include "EngineViewContract.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTest>

#include <memory>

using omaweb::EngineCapabilities;
using omaweb::validateEngineViewContract;

class EngineContractTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void mockExposesSharedContractAndCapabilities();
    void mockSwitchesOptionalCapabilities_data();
    void mockSwitchesOptionalCapabilities();
};

void EngineContractTest::initTestCase() { omaweb::registerEngineCapabilities(); }

void EngineContractTest::mockExposesSharedContractAndCapabilities()
{
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"), QVariantList {});
    QQmlComponent component(
        &engine, QUrl::fromLocalFile(QStringLiteral(OMAWEB_MOCK_ENGINE_VIEW_PATH)));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    const auto missingContract = validateEngineViewContract(*adapter);
    QVERIFY2(missingContract.isEmpty(), qPrintable(missingContract.join(QStringLiteral("; "))));
    QCOMPARE(adapter->property("capabilities").toInt(), omaweb::test::expectedMockCapabilities());
}

void EngineContractTest::mockSwitchesOptionalCapabilities_data()
{
    QTest::addColumn<QByteArray>("propertyName");
    QTest::addColumn<int>("capability");
    QTest::addColumn<bool>("initiallyAvailable");

    QTest::newRow("persistent profiles")
        << QByteArrayLiteral("persistentProfilesAvailable")
        << static_cast<int>(EngineCapabilities::PersistentProfiles) << false;
    QTest::newRow("developer tools")
        << QByteArrayLiteral("inspectorAvailable")
        << static_cast<int>(EngineCapabilities::DeveloperTools) << true;
    QTest::newRow("page find") << QByteArrayLiteral("findAvailable")
                               << static_cast<int>(EngineCapabilities::PageFind) << true;
    QTest::newRow("page zoom") << QByteArrayLiteral("zoomAvailable")
                               << static_cast<int>(EngineCapabilities::PageZoom) << true;
    QTest::newRow("printing") << QByteArrayLiteral("printingAvailable")
                              << static_cast<int>(EngineCapabilities::Printing) << true;
    QTest::newRow("site fullscreen")
        << QByteArrayLiteral("siteFullscreenAvailable")
        << static_cast<int>(EngineCapabilities::SiteFullscreen) << true;
    QTest::newRow("inline PDF viewing")
        << QByteArrayLiteral("inlinePdfViewingAvailable")
        << static_cast<int>(EngineCapabilities::InlinePdfViewing) << true;
    QTest::newRow("certificate decisions")
        << QByteArrayLiteral("certificateDecisionsAvailable")
        << static_cast<int>(EngineCapabilities::CertificateDecisions) << true;
    QTest::newRow("third-party cookie control")
        << QByteArrayLiteral("thirdPartyCookieControlAvailable")
        << static_cast<int>(EngineCapabilities::ThirdPartyCookieControl) << true;
}

void EngineContractTest::mockSwitchesOptionalCapabilities()
{
    QFETCH(QByteArray, propertyName);
    QFETCH(int, capability);
    QFETCH(bool, initiallyAvailable);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"), QVariantList {});
    QQmlComponent component(
        &engine, QUrl::fromLocalFile(QStringLiteral(OMAWEB_MOCK_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> adapter(component.create());
    QVERIFY2(adapter, qPrintable(component.errorString()));

    const auto initialCapabilities = adapter->property("capabilities").toInt();
    QCOMPARE((initialCapabilities & capability) != 0, initiallyAvailable);
    QVERIFY(adapter->setProperty(propertyName, !initiallyAvailable));
    QCOMPARE(adapter->property("capabilities").toInt(), initialCapabilities ^ capability);
}

QTEST_MAIN(EngineContractTest)

#include "tst_enginecontract.moc"
