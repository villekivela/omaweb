#include "SiteStyles.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
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
    void stylesAnAddressByItsOwnFileAlone();
    void ignoresWhatIsNotAStyle();
    void followsAFileSavedInPlace();
    void followsAFileReplacedByRename();
    void followsAFileRemoved();
    void followsTheDirectoryCreatedLater();
    void hidesAnElementOnALineOfItsOwn_data();
    void hidesAnElementOnALineOfItsOwn();
    void undoesAHideAndNothingElse();
    void readsAndUndoesAHideInAFileWithWindowsLineEnds();
    void restylesAnEditAtOnceAndWritesItAfterAPause();
    void writesAnEditStillWaitingWhenItCloses();
    void switchesAStyleOffAndKeepsItsFile();
    void listsEveryStyleByHost();
    void removesAStyleWithItsSwitch();
    void offersEachHostUpToTheSite_data();
    void offersEachHostUpToTheSite();
    void opensAStyleInTheReadersEditor();
    void opensTheStylesFolder();
    void leavesOutAHidePreviewed();
    void saysWhichPagesAHostStyles_data();
    void saysWhichPagesAHostStyles();
};

namespace {

void write(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(contents), contents.size());
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
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

// An IP address has no hosts under it: its last numbers are not a domain, so
// `1.1.css` styles no address that ends in them. An IPv6 address has no file,
// because Qt reads a path that starts with a colon as one of its resources. A
// name written with the root's dot is the same name.
void SiteStylesTest::stylesAnAddressByItsOwnFileAlone()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/1.1.css")), "a { color: red; }");
    write(root.filePath(QStringLiteral("config/styles/10.0.1.1.css")), "a { color: blue; }");
    write(root.filePath(QStringLiteral("config/styles/::1.css")), "a { color: green; }");
    write(root.filePath(QStringLiteral("config/styles/github.com.css")), "a { color: purple; }");
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    QVERIFY(styles.styleSheet(QUrl(QStringLiteral("http://192.168.1.1/"))).isEmpty());
    QCOMPARE(styles.styleSheet(QUrl(QStringLiteral("http://10.0.1.1:8080/"))),
        QStringLiteral("a { color: blue; }"));
    QVERIFY(styles.styleSheet(QUrl(QStringLiteral("http://[::1]:8080/"))).isEmpty());
    QCOMPARE(styles.styleSheet(QUrl(QStringLiteral("https://github.com./"))),
        QStringLiteral("a { color: purple; }"));
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

void SiteStylesTest::hidesAnElementOnALineOfItsOwn_data()
{
    QTest::addColumn<QByteArray>("before");
    QTest::addColumn<QByteArray>("after");
    const QByteArray hidden(
        "/* Hidden by Omaweb */ .promo > aside { display: none !important; }\n");
    QTest::newRow("no file yet") << QByteArray() << hidden;
    QTest::newRow("after the reader's own CSS")
        << QByteArray("a { color: red; }\n") << QByteArray("a { color: red; }\n") + hidden;
    QTest::newRow("after a last line with no end")
        << QByteArray("a { color: red; }") << QByteArray("a { color: red; }\n") + hidden;
}

// A hidden element is an ordinary rule in the site's file, marked so the panel
// can read it back, and nothing the reader wrote is rewritten to add it.
void SiteStylesTest::hidesAnElementOnALineOfItsOwn()
{
    QFETCH(QByteArray, before);
    QFETCH(QByteArray, after);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    if (!before.isEmpty()) {
        write(path, before);
    }
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    QVERIFY(styles.hide(QStringLiteral("github.com"), QStringLiteral(".promo > aside")));
    QCOMPARE(read(path), after);
    QCOMPARE(styles.hidden(QStringLiteral("github.com")),
        QStringList {QStringLiteral(".promo > aside")});
    QVERIFY(styles.styleSheet(QUrl(QStringLiteral("https://gist.github.com/")))
            .contains(QStringLiteral(".promo > aside { display: none !important; }")));
    QVERIFY(changed.count() >= 1);
}

// The reader may have written around Omaweb's lines, or moved them: undoing a
// hide takes out its line wherever it stands, and no other.
void SiteStylesTest::undoesAHideAndNothingElse()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path,
        "/* Hidden by Omaweb */ .cookie-notice { display: none !important; }\n"
        "a { color: red; }\n"
        "/* Hidden by Omaweb */ #feedback { display: none !important; }\n"
        "  /* Hidden by Omaweb */ #feedback { display: none !important; }\n");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QCOMPARE(styles.hidden(QStringLiteral("github.com")),
        (QStringList {QStringLiteral(".cookie-notice"), QStringLiteral("#feedback")}));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    QVERIFY(styles.unhide(QStringLiteral("github.com"), QStringLiteral("#feedback")));
    QCOMPARE(read(path),
        QByteArray("/* Hidden by Omaweb */ .cookie-notice { display: none !important; }\n"
                   "a { color: red; }\n"
                   "  /* Hidden by Omaweb */ #feedback { display: none !important; }\n"));
    QCOMPARE(styles.styleSheet(QUrl(QStringLiteral("https://github.com/")))
                 .count(QStringLiteral("#feedback")),
        1);
    QVERIFY(changed.count() >= 1);
    QVERIFY(!styles.unhide(QStringLiteral("github.com"), QStringLiteral("#feedback")));
}

// Typing restyles the page with each key, and the file is written once the
// typing pauses, so the reader's editor, the watch and Sync see one save and
// not one per key. A read of the directory meanwhile keeps what was typed.
void SiteStylesTest::restylesAnEditAtOnceAndWritesItAfterAPause()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path, "a { color: red; }");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    const QUrl github(QStringLiteral("https://github.com/"));

    styles.edit(QStringLiteral("github.com"), QStringLiteral("a { color: blue; }"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(styles.styleSheet(github), QStringLiteral("a { color: blue; }"));
    QCOMPARE(styles.source(QStringLiteral("github.com")), QStringLiteral("a { color: blue; }"));
    QCOMPARE(styles.unsaved(), QStringList {QStringLiteral("github.com")});
    QCOMPARE(read(path), QByteArray("a { color: red; }"));

    styles.reload();
    QCOMPARE(styles.styleSheet(github), QStringLiteral("a { color: blue; }"));

    QTRY_COMPARE(read(path), QByteArray("a { color: blue; }"));
    QVERIFY(styles.unsaved().isEmpty());
    QCOMPARE(styles.styleSheet(github), QStringLiteral("a { color: blue; }"));
}

void SiteStylesTest::writesAnEditStillWaitingWhenItCloses()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    {
        SiteStyles styles(root.filePath(QStringLiteral("config")));
        styles.edit(QStringLiteral("github.com"), QStringLiteral("a { color: blue; }"));
    }
    QCOMPARE(read(root.filePath(QStringLiteral("config/styles/github.com.css"))),
        QByteArray("a { color: blue; }"));
}

// Off is the reader's setting, in settings.json, so it syncs with the rest and
// the file stays as it is. A switch turned off on another machine arrives as
// an edit of that file.
void SiteStylesTest::switchesAStyleOffAndKeepsItsFile()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    const auto settings = root.filePath(QStringLiteral("config/settings.json"));
    write(path, "a { color: red; }");
    write(root.filePath(QStringLiteral("config/styles/gist.github.com.css")), "p { color: blue; }");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);
    const QUrl gist(QStringLiteral("https://gist.github.com/"));
    QVERIFY(styles.enabled(QStringLiteral("github.com")));

    QVERIFY(styles.setEnabled(QStringLiteral("github.com"), false));
    QVERIFY(!styles.enabled(QStringLiteral("github.com")));
    QCOMPARE(styles.styleSheet(gist), QStringLiteral("p { color: blue; }"));
    QVERIFY(changed.count() >= 1);
    QCOMPARE(read(path), QByteArray("a { color: red; }"));
    QCOMPARE(QJsonDocument::fromJson(read(settings)).object().value(u"site-styles").toObject(),
        (QJsonObject {{QStringLiteral("github.com"), false}}));

    QVERIFY(styles.setEnabled(QStringLiteral("github.com"), true));
    QVERIFY(styles.styleSheet(gist).contains(QStringLiteral("color: red")));
    QVERIFY(!QJsonDocument::fromJson(read(settings)).object().contains(u"site-styles"));

    write(settings, R"({"version": 1, "site-styles": {"gist.github.com": false}})");
    QTRY_COMPARE(styles.styleSheet(gist), QStringLiteral("a { color: red; }"));
    QVERIFY(!styles.enabled(QStringLiteral("gist.github.com")));
}

// What Settings lists: each file by the host it names, how long it is, how
// many elements it hides, and whether it is on.
void SiteStylesTest::listsEveryStyleByHost()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/github.com.css")),
        "a { color: red; }\n"
        "/* Hidden by Omaweb */ .promo { display: none !important; }\n"
        "/* Hidden by Omaweb */ #feedback { display: none !important; }\n");
    write(root.filePath(QStringLiteral("config/styles/news.ycombinator.com.css")), "td { }");
    write(root.filePath(QStringLiteral("config/settings.json")),
        R"({"version": 1, "site-styles": {"news.ycombinator.com": false}})");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&styles, &SiteStyles::changed);

    const auto listed = styles.styles();
    QCOMPARE(listed.size(), 2);
    QCOMPARE(listed.at(0).toMap(),
        (QVariantMap {{QStringLiteral("host"), QStringLiteral("github.com")},
            {QStringLiteral("lines"), 3}, {QStringLiteral("hidden"), 2},
            {QStringLiteral("enabled"), true},
            {QStringLiteral("path"),
                root.filePath(QStringLiteral("config/styles/github.com.css"))}}));
    QCOMPARE(listed.at(1).toMap().value(QStringLiteral("host")),
        QVariant(QStringLiteral("news.ycombinator.com")));
    QCOMPARE(listed.at(1).toMap().value(QStringLiteral("lines")), QVariant(1));
    QCOMPARE(listed.at(1).toMap().value(QStringLiteral("enabled")), QVariant(false));
}

// Removing a style deletes its file and forgets its switch, so a style made
// later for the host starts on.
void SiteStylesTest::removesAStyleWithItsSwitch()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path, "a { color: red; }");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QVERIFY(styles.setEnabled(QStringLiteral("github.com"), false));
    styles.edit(QStringLiteral("github.com"), QStringLiteral("a { color: blue; }"));
    QSignalSpy changed(&styles, &SiteStyles::changed);

    QVERIFY(styles.remove(QStringLiteral("github.com")));
    QVERIFY(!QFile::exists(path));
    QVERIFY(styles.styles().isEmpty());
    QVERIFY(styles.unsaved().isEmpty());
    QVERIFY(styles.enabled(QStringLiteral("github.com")));
    QVERIFY(changed.count() >= 1);
    QTest::qWait(500);
    QVERIFY(!QFile::exists(path));
}

void SiteStylesTest::offersEachHostUpToTheSite_data()
{
    QTest::addColumn<QString>("address");
    QTest::addColumn<QStringList>("hosts");
    QTest::addColumn<QString>("site");
    QTest::newRow("a host under a site")
        << QStringLiteral("https://gist.github.com/villekivela")
        << QStringList {QStringLiteral("gist.github.com"), QStringLiteral("github.com")}
        << QStringLiteral("github.com");
    QTest::newRow("the site itself")
        << QStringLiteral("https://github.com/") << QStringList {QStringLiteral("github.com")}
        << QStringLiteral("github.com");
    QTest::newRow("under a public suffix of two labels")
        << QStringLiteral("https://www.bbc.co.uk/news")
        << QStringList {QStringLiteral("www.bbc.co.uk"), QStringLiteral("bbc.co.uk")}
        << QStringLiteral("bbc.co.uk");
    QTest::newRow("an IP address")
        << QStringLiteral("http://10.0.1.1:8080/") << QStringList {QStringLiteral("10.0.1.1")}
        << QStringLiteral("10.0.1.1");
    QTest::newRow("a name with no suffix")
        << QStringLiteral("http://localhost:8000/") << QStringList {QStringLiteral("localhost")}
        << QStringLiteral("localhost");
    QTest::newRow("no host") << QStringLiteral("about:blank") << QStringList() << QString();
}

// The panel's host picker: the page's host and each one above it that is
// still the site, broadest last, with the site itself chosen first. A file
// for `co.uk` would style every British site, so it is not offered.
void SiteStylesTest::offersEachHostUpToTheSite()
{
    QFETCH(QString, address);
    QFETCH(QStringList, hosts);
    QFETCH(QString, site);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/github.com.css")), "a { color: red; }");
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    const auto offered = styles.hostsFor(QUrl(address));
    QStringList names;
    for (const auto &host : offered) {
        names.append(host.toMap().value(QStringLiteral("host")).toString());
        QCOMPARE(host.toMap().value(QStringLiteral("exists")).toBool(),
            names.constLast() == u"github.com");
    }
    QCOMPARE(names, hosts);
    QCOMPARE(styles.siteFor(QUrl(address)), site);
}

// The reader's own editor is the one Omarchy launches for them. The file is
// written first, so a style typed and not yet saved, or one with nothing in
// it yet, is there to open.
void SiteStylesTest::opensAStyleInTheReadersEditor()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto launched = root.filePath(QStringLiteral("launched"));
    write(root.filePath(QStringLiteral("bin/omarchy-launch-editor")),
        "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"" + launched.toUtf8() + "\"\n");
    QFile::setPermissions(root.filePath(QStringLiteral("bin/omarchy-launch-editor")),
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const auto path = qgetenv("PATH");
    qputenv("PATH", root.filePath(QStringLiteral("bin")).toUtf8() + ':' + path);
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    styles.edit(QStringLiteral("github.com"), QStringLiteral("a { color: blue; }"));

    const auto opened = styles.openInEditor(QStringLiteral("github.com"));
    qputenv("PATH", path);
    QVERIFY(opened);
    const auto file = root.filePath(QStringLiteral("config/styles/github.com.css"));
    QCOMPARE(read(file), QByteArray("a { color: blue; }"));
    QTRY_COMPARE(read(launched), file.toUtf8() + '\n');
}

// The folder is opened even before it has a style, so a reader can drop
// files into it.
void SiteStylesTest::opensTheStylesFolder()
{
#ifdef Q_OS_MACOS
    QSKIP("The desktop opens a folder with `open` on macOS, which is not faked here.");
#endif
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto launched = root.filePath(QStringLiteral("launched"));
    write(root.filePath(QStringLiteral("bin/xdg-open")),
        "#!/bin/sh\nprintf '%s\\n' \"$@\" > \"" + launched.toUtf8() + "\"\n");
    QFile::setPermissions(root.filePath(QStringLiteral("bin/xdg-open")),
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const auto path = qgetenv("PATH");
    qputenv("PATH", root.filePath(QStringLiteral("bin")).toUtf8() + ':' + path);
    SiteStyles styles(root.filePath(QStringLiteral("config")));

    const auto opened = styles.openFolder();
    qputenv("PATH", path);
    QVERIFY(opened);
    const auto folder = root.filePath(QStringLiteral("config/styles"));
    QVERIFY(QDir(folder).exists());
    QTRY_COMPARE(read(launched), folder.toUtf8() + '\n');
}

// A preview shows a hidden element as the page draws it, so the sheet leaves
// that hide's line out rather than overriding it.
void SiteStylesTest::leavesOutAHidePreviewed()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    write(root.filePath(QStringLiteral("config/styles/github.com.css")),
        "a { color: red; }\n"
        "/* Hidden by Omaweb */ .promo { display: none !important; }\n"
        "/* Hidden by Omaweb */ #feedback { display: none !important; }\n");
    const SiteStyles styles(root.filePath(QStringLiteral("config")));
    QCOMPARE(styles.styleSheetShowing(
                 QUrl(QStringLiteral("https://gist.github.com/")), {QStringLiteral(".promo")}),
        QStringLiteral("a { color: red; }\n"
                       "/* Hidden by Omaweb */ #feedback { display: none !important; }\n"));
    QCOMPARE(styles.styleSheetShowing(QUrl(QStringLiteral("https://github.com/")), {}),
        styles.styleSheet(QUrl(QStringLiteral("https://github.com/"))));
}

void SiteStylesTest::saysWhichPagesAHostStyles_data()
{
    QTest::addColumn<QString>("host");
    QTest::addColumn<QString>("address");
    QTest::addColumn<bool>("styled");
    QTest::newRow("its own") << QStringLiteral("gist.github.com")
                             << QStringLiteral("https://gist.github.com/x") << true;
    QTest::newRow("one under it") << QStringLiteral("github.com")
                                  << QStringLiteral("https://gist.github.com/x") << true;
    QTest::newRow("one above it") << QStringLiteral("gist.github.com")
                                  << QStringLiteral("https://github.com/") << false;
    QTest::newRow("in its own script")
        << QStringLiteral("bücher.example") << QStringLiteral("https://www.xn--bcher-kva.example/")
        << true;
    QTest::newRow("a name ending the same")
        << QStringLiteral("github.com") << QStringLiteral("https://notgithub.com/") << false;
    QTest::newRow("no host") << QStringLiteral("github.com") << QStringLiteral("about:blank")
                             << false;
}

// Whether a host's file reaches a page, as the panel and Settings ask it of
// the page on show and of each tab.
void SiteStylesTest::saysWhichPagesAHostStyles()
{
    QFETCH(QString, host);
    QFETCH(QString, address);
    QFETCH(bool, styled);
    const SiteStyles styles {QString()};
    QCOMPARE(styles.reaches(host, QUrl(address)), styled);
}

// An editor on another system may have saved the file with CR LF.
void SiteStylesTest::readsAndUndoesAHideInAFileWithWindowsLineEnds()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto path = root.filePath(QStringLiteral("config/styles/github.com.css"));
    write(path,
        "a { color: red; }\r\n"
        "/* Hidden by Omaweb */ .promo { display: none !important; }\r\n"
        "p { margin: 0; }\r\n");
    SiteStyles styles(root.filePath(QStringLiteral("config")));
    QCOMPARE(styles.hidden(QStringLiteral("github.com")), QStringList {QStringLiteral(".promo")});
    QVERIFY(styles.unhide(QStringLiteral("github.com"), QStringLiteral(".promo")));
    QCOMPARE(read(path), QByteArray("a { color: red; }\r\np { margin: 0; }\r\n"));
}

QTEST_GUILESS_MAIN(SiteStylesTest)
#include "tst_sitestyles.moc"
