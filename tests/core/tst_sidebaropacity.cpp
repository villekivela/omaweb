#include "SidebarOpacity.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::SidebarOpacity;

class SidebarOpacityTest final : public QObject {
    Q_OBJECT

private slots:
    void leavesTheSidebarToTheThemeUntilTold();
    void takesTheNearestStepWithinTheRange();
    void resetsToTheThemeByLeavingTheFile();
    void keepsTheValueAcrossARestart();
    void followsAValueWrittenInTheFile();
    void readsAValueOutOfRangeAsNone();
};

namespace {

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(contents);
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}

} // namespace

void SidebarOpacityTest::leavesTheSidebarToTheThemeUntilTold()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    SidebarOpacity opacity(root.filePath(QStringLiteral("config")));
    QVERIFY(!opacity.overridden());
    QVERIFY(!opacity.opacity());
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/settings.json"))));
}

// The slider moves by 5%, and a value between steps, as a float sum leaves, is
// the step it is nearest rather than a number the file would carry to sixteen
// places. The ends are ends.
void SidebarOpacityTest::takesTheNearestStepWithinTheRange()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    SidebarOpacity opacity(root.path());
    QSignalSpy changed(&opacity, &SidebarOpacity::changed);

    opacity.set(0.1 + 0.6);
    QCOMPARE(opacity.opacity(), std::optional<double>(0.7));
    QVERIFY(opacity.overridden());
    QCOMPARE(changed.count(), 1);
    QVERIFY(readFile(path).contains("\"sidebar-opacity\": 0.7\n"));

    opacity.set(0.7);
    QCOMPARE(changed.count(), 1);

    opacity.set(0.2);
    QCOMPARE(opacity.opacity(), std::optional<double>(0.5));
    opacity.set(3);
    QCOMPARE(opacity.opacity(), std::optional<double>(1.0));
}

void SidebarOpacityTest::resetsToTheThemeByLeavingTheFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    SidebarOpacity opacity(root.path());
    opacity.set(0.6);
    QSignalSpy changed(&opacity, &SidebarOpacity::changed);

    opacity.reset();

    QVERIFY(!opacity.overridden());
    QVERIFY(!opacity.opacity());
    QCOMPARE(changed.count(), 1);
    QVERIFY(!readFile(path).contains("sidebar-opacity"));
    opacity.reset();
    QCOMPARE(changed.count(), 1);
}

void SidebarOpacityTest::keepsTheValueAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    {
        SidebarOpacity opacity(root.path());
        opacity.set(0.55);
    }
    const SidebarOpacity restarted(root.path());
    QCOMPARE(restarted.opacity(), std::optional<double>(0.55));
}

void SidebarOpacityTest::followsAValueWrittenInTheFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("settings.json"));
    SidebarOpacity opacity(root.path());
    QSignalSpy changed(&opacity, &SidebarOpacity::changed);

    writeFile(path, R"({"version": 1, "sidebar-opacity": 0.8})");
    QTRY_COMPARE(opacity.opacity(), std::optional<double>(0.8));
    QCOMPARE(changed.count(), 1);

    writeFile(path, R"({"version": 1})");
    QTRY_VERIFY(!opacity.overridden());
    QCOMPARE(changed.count(), 2);
}

void SidebarOpacityTest::readsAValueOutOfRangeAsNone()
{
    for (const auto *bad : {"0.2", "1.5", "\"high\""}) {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        writeFile(root.filePath(QStringLiteral("settings.json")),
            R"({"version": 1, "sidebar-opacity": )" + QByteArray(bad) + "}");
        const SidebarOpacity opacity(root.path());
        QVERIFY2(!opacity.overridden(), bad);
    }
}

QTEST_MAIN(SidebarOpacityTest)
#include "tst_sidebaropacity.moc"
