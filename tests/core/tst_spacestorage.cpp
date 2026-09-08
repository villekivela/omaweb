#include "SpaceStorage.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using omaweb::SpaceStorage;

class SpaceStorageTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsEachSpaceUnderTheDataRoot();
    void keepsEachEngineOutOfAnothersProfile();
    void answersWithoutCreatingAnything();
};

// A Space's database and its Engine profile are both under that Space, so a
// deleted Space takes everything it held with it (ADR 0008).
void SpaceStorageTest::keepsEachSpaceUnderTheDataRoot()
{
    const SpaceStorage storage(QStringLiteral("/data"), QStringLiteral("qt"));

    QCOMPARE(storage.dataRoot(), QStringLiteral("/data"));
    QCOMPARE(storage.engineName(), QStringLiteral("qt"));
    QCOMPARE(storage.databasePathFor(QStringLiteral("personal")),
        QStringLiteral("/data/spaces/personal/browser.sqlite"));
    QCOMPARE(storage.profilePathFor(QStringLiteral("personal")),
        QStringLiteral("/data/spaces/personal/engines/qt"));
    QVERIFY(storage.profilePathFor(QStringLiteral("personal"))
        != storage.profilePathFor(QStringLiteral("work")));
}

// One Space browsed with two engines is one browsing identity, but the two
// engines keep their own state and neither reads the other's (ADR 0001).
void SpaceStorageTest::keepsEachEngineOutOfAnothersProfile()
{
    const SpaceStorage qt(QStringLiteral("/data"), QStringLiteral("qt"));
    const SpaceStorage ladybird(QStringLiteral("/data"), QStringLiteral("ladybird"));

    QVERIFY(qt.profilePathFor(QStringLiteral("personal"))
        != ladybird.profilePathFor(QStringLiteral("personal")));
    // The Space is the same Space either way, so its session is the same file.
    QCOMPARE(qt.databasePathFor(QStringLiteral("personal")),
        ladybird.databasePathFor(QStringLiteral("personal")));
}

// Saying where something belongs is not asking for it to exist.
void SpaceStorageTest::answersWithoutCreatingAnything()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const SpaceStorage storage(root.path(), QStringLiteral("qt"));

    QVERIFY(!storage.profilePathFor(QStringLiteral("personal")).isEmpty());
    QVERIFY(!storage.databasePathFor(QStringLiteral("personal")).isEmpty());

    QVERIFY(QDir(root.path()).isEmpty());
}

QTEST_APPLESS_MAIN(SpaceStorageTest)
#include "tst_spacestorage.moc"
