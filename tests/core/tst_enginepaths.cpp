#include "EnginePaths.h"

#include <QTest>

using namespace omaweb;

// Where Omaweb looks for the files its own engine cannot start without. The
// engine is installed beside the system Qt rather than over it, so QtCore
// points it at the distribution's directories and it has to be told otherwise.
class EnginePathsTest : public QObject {
    Q_OBJECT

private slots:
    void theEnginesFilesAreFoundFromItsLibraries()
    {
        const EnginePaths paths = EnginePaths::beside(QStringLiteral("/usr/lib/omaweb/lib"));
        QCOMPARE(paths.resources, QStringLiteral("/usr/lib/omaweb/share/qt6/resources"));
        QCOMPARE(paths.locales,
            QStringLiteral("/usr/lib/omaweb/share/qt6/translations/qtwebengine_locales"));
        QCOMPARE(paths.renderer, QStringLiteral("/usr/lib/omaweb/lib/qt6/QtWebEngineProcess"));
    }

    // A platform that cannot say where its engine is says nothing rather than
    // pointing the engine at the root of the filesystem.
    void anEngineThatCannotBeFoundNamesNothing()
    {
        const EnginePaths paths = EnginePaths::beside(QString {});
        QVERIFY(paths.resources.isEmpty());
        QVERIFY(paths.locales.isEmpty());
        QVERIFY(paths.renderer.isEmpty());
    }
};

QTEST_MAIN(EnginePathsTest)
#include "tst_enginepaths.moc"
