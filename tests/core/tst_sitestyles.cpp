#include "SiteStyles.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::SiteStyles;

class SiteStylesTest final : public QObject {
    Q_OBJECT

private slots:
    void stylesNothingWithoutAFile();
    void stylesAHostAndTheHostsUnderIt_data();
    void stylesAHostAndTheHostsUnderIt();
    void putsANarrowerHostsFileLast();
    void readsAHostWrittenInItsOwnScript();
    void ignoresWhatIsNotAStyle();
    void followsAFileSavedInPlace();
    void followsAFileReplacedByRename();
    void followsAFileRemoved();
    void followsTheDirectoryCreatedLater();
};

namespace {

void write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}

} // namespace

void SiteStylesTest::stylesNothingWithoutAFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    QVERIFY(styles.styleSheet(QUrl(QStringLiteral("https://github.com/"))).isEmpty());
    QVERIFY(!QDir(root.filePath(QStringLiteral("config/styles"))).exists());
    const SiteStyles inMemory {QString()};
    QVERIFY(inMemory.styleSheet(QUrl(QStringLiteral("https://github.com/"))).isEmpty());
}

void SiteStylesTest::stylesAHostAndTheHostsUnderIt_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<bool>("styled");
    QTest::newRow("the host") << QStringLiteral("https://github.com/villekivela") << true;
    QTest::newRow("a host under it") << QStringLiteral("https://gist.github.com/") << true;
    QTest::newRow("any scheme and port") << QStringLiteral("http://github.com:8080/") << true;
    QTest::newRow("in capitals") << QStringLiteral("https://GitHub.COM/") << true;
    QTest::newRow("a name ending the same") << QStringLiteral("https://notgithub.com/") << false;
    QTest::newRow("a host above it") << QStringLiteral("https://com/") << false;
    QTest::newRow("a name starting the same")
        << QStringLiteral("https://github.com.example.org/") << false;
    QTest::newRow("no host") << QStringLiteral("about:blank") << false;
}

void SiteStylesTest::stylesAHostAndTheHostsUnderIt()
{
    QFETCH(QString, address);
    QFETCH(bool, styled);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/github.com.css")), "body { color: red; }");
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    QCOMPARE(styles.styleSheet(QUrl(address)).contains(QStringLiteral("color: red")), styled);
}

// An adopted sheet's later rule wins a tie, so the file for the narrower host
// has the last word over the one for the site.
void SiteStylesTest::putsANarrowerHostsFileLast()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/gist.github.com.css")), "p { color: blue; }");
    write(root.filePath(QStringLiteral("config/styles/github.com.css")), "p { color: red; }");
    write(root.filePath(QStringLiteral("config/styles/localhost.css")), "p { color: green; }");
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    const auto css = styles.styleSheet(QUrl(QStringLiteral("https://gist.github.com/")));
    QVERIFY(css.contains(QStringLiteral("color: red")));
    QVERIFY(css.contains(QStringLiteral("color: blue")));
    QVERIFY(!css.contains(QStringLiteral("color: green")));
    QVERIFY(css.indexOf(QStringLiteral("color: red")) < css.indexOf(QStringLiteral("color: blue")));
    QCOMPARE(styles.styleSheet(QUrl(QStringLiteral("http://localhost:8000/"))),
        QStringLiteral("p { color: green; }"));
}

// A reader names a host as they read it, and the address bar may carry it
// either way.
void SiteStylesTest::readsAHostWrittenInItsOwnScript()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/bücher.example.css")), "a { color: red; }");
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    QVERIFY(!styles.styleSheet(QUrl(QStringLiteral("https://xn--bcher-kva.example/"))).isEmpty());
    QVERIFY(!styles.styleSheet(QUrl(QStringLiteral("https://www.bücher.example/"))).isEmpty());
}

void SiteStylesTest::ignoresWhatIsNotAStyle()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/github.com.css~")), "a { color: red; }");
    write(root.filePath(QStringLiteral("config/styles/github.com.txt")), "a { color: red; }");
    write(root.filePath(QStringLiteral("config/styles/.github.com.css")), "a { color: red; }");
    write(root.filePath(QStringLiteral("config/styles/.css")), "a { color: red; }");
    QDir().mkpath(root.filePath(QStringLiteral("config/styles/example.com.css")));
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    QVERIFY(styles.styleSheet(QUrl(QStringLiteral("https://github.com/"))).isEmpty());
    QVERIFY(styles.styleSheet(QUrl(QStringLiteral("https://example.com/"))).isEmpty());
}

void SiteStylesTest::followsAFileSavedInPlace()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path, "a { color: red; }");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    write(path, "a { color: blue; }");
    QTRY_VERIFY(styles.styleSheet(QUrl(QStringLiteral("https://github.com/")))
            .contains(QStringLiteral("blue")));
    QVERIFY(changed.count() >= 1);
}

// Most editors save by writing a new file and renaming it over the old one,
// which leaves a watch on the old file watching nothing. The next save must
// be seen too.
void SiteStylesTest::followsAFileReplacedByRename()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path, "a { color: red; }");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    const QUrl github(QStringLiteral("https://github.com/"));
    for (const auto &colour : {QByteArray("blue"), QByteArray("green")}) {
        QSaveFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("a { color: " + colour + "; }");
        QVERIFY(file.commit());
        QTRY_VERIFY(styles.styleSheet(github).contains(QString::fromLatin1(colour)));
    }
    write(path, "a { color: purple; }");
    QTRY_VERIFY(styles.styleSheet(github).contains(QStringLiteral("purple")));
}

void SiteStylesTest::followsAFileRemoved()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path, "a { color: red; }");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    QVERIFY(QFile::remove(path));
    QTRY_VERIFY(styles.styleSheet(QUrl(QStringLiteral("https://github.com/"))).isEmpty());
    QCOMPARE(changed.count(), 1);
}

// A first style is written into a directory that did not exist at start.
void SiteStylesTest::followsTheDirectoryCreatedLater()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    write(root.filePath(QStringLiteral("config/styles/github.com.css")), "a { color: red; }");
    QTRY_VERIFY(!styles.styleSheet(QUrl(QStringLiteral("https://github.com/"))).isEmpty());
    QVERIFY(changed.count() >= 1);
}

QTEST_GUILESS_MAIN(SiteStylesTest)
#include "tst_sitestyles.moc"
