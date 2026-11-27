#include "EngineVariant.h"

#include <QTest>

using omaweb::EngineVariant;

class EngineVariantTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsTheQtBuildWhereItAlwaysWas();
    void givesLadybirdItsOwnConfigRoot();
    void givesLadybirdItsOwnApplicationDataDirectory();
};

// A reader's settings and Spaces already sit at these paths; the variant must
// not move them.
void EngineVariantTest::keepsTheQtBuildWhereItAlwaysWas()
{
    QCOMPARE(EngineVariant::configRoot(EngineVariant::Qt, QStringLiteral("/home/r/.config")),
        QStringLiteral("/home/r/.config/omaweb"));
    QCOMPARE(EngineVariant::applicationName(EngineVariant::Qt), QStringLiteral("Omaweb"));
}

// The two browsers never write the same file, and neither root sits inside the
// other, so a recursive delete or a scan of one cannot reach the other.
void EngineVariantTest::givesLadybirdItsOwnConfigRoot()
{
    const auto home = QStringLiteral("/home/r/.config");
    const auto qt = EngineVariant::configRoot(EngineVariant::Qt, home);
    const auto ladybird = EngineVariant::configRoot(EngineVariant::Ladybird, home);

    QCOMPARE(ladybird, QStringLiteral("/home/r/.config/omaweb-ladybird"));
    QVERIFY(!ladybird.startsWith(qt + u'/'));
    QVERIFY(!qt.startsWith(ladybird + u'/'));
    QVERIFY(qt != ladybird);
}

// Qt derives the application data directory from the application name.
void EngineVariantTest::givesLadybirdItsOwnApplicationDataDirectory()
{
    QCOMPARE(
        EngineVariant::applicationName(EngineVariant::Ladybird), QStringLiteral("Omaweb-Ladybird"));
}

QTEST_APPLESS_MAIN(EngineVariantTest)

#include "tst_enginevariant.moc"
