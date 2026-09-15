#include "ReleaseCheck.h"

#include <QDateTime>
#include <QTest>

using omaweb::ReleaseCheck::Origin;

Q_DECLARE_METATYPE(omaweb::ReleaseCheck::Question)
Q_DECLARE_METATYPE(omaweb::ReleaseCheck::Origin)

class ReleaseCheckTest final : public QObject {
    Q_OBJECT

private slots:
    void tellsTheReaderWhenTheirVersionIsBehind_data();
    void tellsTheReaderWhenTheirVersionIsBehind();
    void saysNothingToABuildAheadOfItsTag_data();
    void saysNothingToABuildAheadOfItsTag();
    void saysNothingAboutAVersionItCannotRead_data();
    void saysNothingAboutAVersionItCannotRead();
    void readsTheNewestReleaseIncludingPrereleases();
    void readsNoReleaseFromAnAnswerItCannotUse_data();
    void readsNoReleaseFromAnAnswerItCannotUse();
    void readsTheReleaseNumberOffABuildDescription_data();
    void readsTheReleaseNumberOffABuildDescription();
    void sendsTheReaderToOmawebsOwnReleasePage();
    void sendsTheReaderToTheListWhenATagHasNoPage_data();
    void sendsTheReaderToTheListWhenATagHasNoPage();
    void asksAgainTheNextDay_data();
    void asksAgainTheNextDay();
    void readsHowTheBrowserGotHere_data();
    void readsHowTheBrowserGotHere();
    void tellsAPackagedReaderToUpgradeWithTheSystem();
    void tellsAHandInstalledReaderWhatASystemUpgradeWillNotDo();
    void tellsACheckoutReaderWhereTheVersionIs();
    void announcesAReleaseTheReaderHasNotSeen();
    void keepsQuiet_data();
    void keepsQuiet();
    void announcesTheNextReleaseAfterADismissedOne();
};

// The running version is what CMake derived from the tag (ADR 0028), so it is
// either a bare `x.y.z` on a tag or the full description a commit past one.
void ReleaseCheckTest::tellsTheReaderWhenTheirVersionIsBehind_data()
{
    QTest::addColumn<QString>("running");
    QTest::addColumn<QString>("latest");

    QTest::newRow("a patch behind") << QStringLiteral("0.4.0") << QStringLiteral("v0.4.1");
    QTest::newRow("a minor behind") << QStringLiteral("0.4.0") << QStringLiteral("v0.5.0");
    QTest::newRow("a major behind") << QStringLiteral("0.4.0") << QStringLiteral("v1.0.0");
    QTest::newRow("several behind") << QStringLiteral("0.2.1") << QStringLiteral("v0.5.0");
    // Ten sorts after nine everywhere but in a string comparison, which is the
    // way this goes wrong and keeps a reader on an old browser for a year.
    QTest::newRow("past a version with two digits")
        << QStringLiteral("0.9.0") << QStringLiteral("v0.10.0");
    // A build between releases is still behind the release that came after it.
    QTest::newRow("a described build behind")
        << QStringLiteral("0.4.0-14-gabc1234") << QStringLiteral("v0.5.0");
}

void ReleaseCheckTest::tellsTheReaderWhenTheirVersionIsBehind()
{
    QFETCH(QString, running);
    QFETCH(QString, latest);

    QVERIFY(omaweb::ReleaseCheck::behind(running, latest));
}

void ReleaseCheckTest::saysNothingToABuildAheadOfItsTag_data()
{
    QTest::addColumn<QString>("running");
    QTest::addColumn<QString>("latest");

    QTest::newRow("exactly the release") << QStringLiteral("0.4.0") << QStringLiteral("v0.4.0");
    // The commit count says this build is past the tag it is named for, so it
    // has the release's work and more. Telling it to upgrade would send a
    // reader backwards.
    QTest::newRow("past the newest release")
        << QStringLiteral("0.4.0-14-gabc1234") << QStringLiteral("v0.4.0");
    QTest::newRow("past it and dirty")
        << QStringLiteral("0.4.0-14-gabc1234-dirty") << QStringLiteral("v0.4.0");
    QTest::newRow("ahead of it") << QStringLiteral("0.5.0") << QStringLiteral("v0.4.0");
    QTest::newRow("ahead by a patch") << QStringLiteral("0.4.1") << QStringLiteral("v0.4.0");
}

void ReleaseCheckTest::saysNothingToABuildAheadOfItsTag()
{
    QFETCH(QString, running);
    QFETCH(QString, latest);

    QVERIFY(!omaweb::ReleaseCheck::behind(running, latest));
}

// A version neither side can read is not an upgrade the reader is missing. The
// check is silent about it, the way it is silent about being offline.
void ReleaseCheckTest::saysNothingAboutAVersionItCannotRead_data()
{
    QTest::addColumn<QString>("running");
    QTest::addColumn<QString>("latest");

    QTest::newRow("no running version") << QString() << QStringLiteral("v0.5.0");
    QTest::newRow("no release") << QStringLiteral("0.4.0") << QString();
    QTest::newRow("a fallback that names nothing")
        << QStringLiteral("unknown") << QStringLiteral("v0.5.0");
    QTest::newRow("a tag that is not a version")
        << QStringLiteral("0.4.0") << QStringLiteral("nightly");
    QTest::newRow("a tag with no numbers") << QStringLiteral("0.4.0") << QStringLiteral("v");
}

void ReleaseCheckTest::saysNothingAboutAVersionItCannotRead()
{
    QFETCH(QString, running);
    QFETCH(QString, latest);

    QVERIFY(!omaweb::ReleaseCheck::behind(running, latest));
}

// Every `v0.*` tag is published as a prerelease (ADR 0028), so the releases
// endpoint is read in order rather than asking GitHub for `latest`, which
// leaves prereleases out and would answer nothing at all for this project.
void ReleaseCheckTest::readsTheNewestReleaseIncludingPrereleases()
{
    const auto answer = QByteArrayLiteral(R"([
        {"tag_name": "v0.5.0", "prerelease": true, "draft": false},
        {"tag_name": "v0.4.0", "prerelease": true, "draft": false}
    ])");

    QCOMPARE(omaweb::ReleaseCheck::newestRelease(answer), QStringLiteral("v0.5.0"));
}

void ReleaseCheckTest::readsNoReleaseFromAnAnswerItCannotUse_data()
{
    QTest::addColumn<QByteArray>("answer");

    QTest::newRow("nothing at all") << QByteArray();
    QTest::newRow("not JSON") << QByteArrayLiteral("<html>rate limited</html>");
    QTest::newRow("no releases yet") << QByteArrayLiteral("[]");
    QTest::newRow("an error object") << QByteArrayLiteral(R"({"message": "Not Found"})");
    QTest::newRow("a release with no tag") << QByteArrayLiteral(R"([{"prerelease": true}])");
    // A draft is visible only to the maintainer and is not something a reader
    // can install, so it is not an upgrade to announce.
    QTest::newRow("a draft") << QByteArrayLiteral(R"([{"tag_name": "v9.9.9", "draft": true}])");
}

void ReleaseCheckTest::readsNoReleaseFromAnAnswerItCannotUse()
{
    QFETCH(QByteArray, answer);

    QVERIFY(omaweb::ReleaseCheck::newestRelease(answer).isEmpty());
}

// What the request says it is. The description a build carries past its tag
// names a commit and whether the tree was dirty, and the endpoint is told
// neither.
void ReleaseCheckTest::readsTheReleaseNumberOffABuildDescription_data()
{
    QTest::addColumn<QString>("version");
    QTest::addColumn<QString>("number");

    QTest::newRow("on a tag") << QStringLiteral("0.4.0") << QStringLiteral("0.4.0");
    QTest::newRow("past a tag") << QStringLiteral("0.4.0-14-gabc1234") << QStringLiteral("0.4.0");
    QTest::newRow("past a tag and dirty")
        << QStringLiteral("0.4.0-14-gabc1234-dirty") << QStringLiteral("0.4.0");
    QTest::newRow("a tag") << QStringLiteral("v0.5.0") << QStringLiteral("0.5.0");
    QTest::newRow("nothing readable") << QStringLiteral("unknown") << QString();
}

void ReleaseCheckTest::readsTheReleaseNumberOffABuildDescription()
{
    QFETCH(QString, version);
    QFETCH(QString, number);

    QCOMPARE(omaweb::ReleaseCheck::releaseNumber(version), number);
}

// Omaweb's own page rather than GitHub's: it carries the notes and, beside
// them, how to upgrade — which is the other half of what the notice is for.
void ReleaseCheckTest::sendsTheReaderToOmawebsOwnReleasePage()
{
    QCOMPARE(omaweb::ReleaseCheck::notesPage(QStringLiteral("v0.5.0")),
        QUrl(QStringLiteral("https://omaweb.app/releases/v0.5.0/")));
}

// The site makes a page for every tag it can name a directory after, and
// nothing else. A tag it would skip has to land somewhere that exists.
void ReleaseCheckTest::sendsTheReaderToTheListWhenATagHasNoPage_data()
{
    QTest::addColumn<QString>("tag");

    QTest::newRow("no tag") << QString();
    QTest::newRow("a path of its own") << QStringLiteral("v0.5.0/../etc");
    QTest::newRow("a space") << QStringLiteral("v0.5.0 final");
    QTest::newRow("leading punctuation") << QStringLiteral(".hidden");
}

void ReleaseCheckTest::sendsTheReaderToTheListWhenATagHasNoPage()
{
    QFETCH(QString, tag);

    QCOMPARE(
        omaweb::ReleaseCheck::notesPage(tag), QUrl(QStringLiteral("https://omaweb.app/releases/")));
}

// At most once a day, counted from when the last check finished.
void ReleaseCheckTest::asksAgainTheNextDay_data()
{
    QTest::addColumn<QDateTime>("lastCheck");
    QTest::addColumn<bool>("due");

    const auto now = QDateTime::fromString(QStringLiteral("2026-09-15T12:00:00Z"), Qt::ISODate);

    QTest::newRow("never checked") << QDateTime() << true;
    QTest::newRow("a day ago") << now.addDays(-1) << true;
    QTest::newRow("a week ago") << now.addDays(-7) << true;
    QTest::newRow("an hour ago") << now.addSecs(-3600) << false;
    QTest::newRow("a moment ago") << now << false;
    // A clock that has moved backwards, or a stored time from a machine whose
    // clock was wrong. Checking is the safe answer: the cost is one request.
    QTest::newRow("in the future") << now.addDays(1) << true;
}

void ReleaseCheckTest::asksAgainTheNextDay()
{
    QFETCH(QDateTime, lastCheck);
    QFETCH(bool, due);

    const auto now = QDateTime::fromString(QStringLiteral("2026-09-15T12:00:00Z"), Qt::ISODate);
    QCOMPARE(omaweb::ReleaseCheck::due(lastCheck, now), due);
}

void ReleaseCheckTest::readsHowTheBrowserGotHere_data()
{
    QTest::addColumn<bool>("owned");
    QTest::addColumn<bool>("foreign");
    QTest::addColumn<QString>("packageName");
    QTest::addColumn<Origin>("origin");

    QTest::newRow("from the repository")
        << true << false << QStringLiteral("omaweb") << Origin::Repository;
    // The case `pacman -Qo` alone cannot see: pacman knows the package is
    // there, and no repository has a newer one to give it.
    QTest::newRow("a downloaded release, installed by hand")
        << true << true << QStringLiteral("omaweb") << Origin::DownloadedPackage;
    QTest::newRow("makepkg -si from a checkout")
        << true << true << QStringLiteral("omaweb-git") << Origin::Checkout;
    QTest::newRow("no package owns it") << false << false << QString() << Origin::Checkout;
}

void ReleaseCheckTest::readsHowTheBrowserGotHere()
{
    QFETCH(bool, owned);
    QFETCH(bool, foreign);
    QFETCH(QString, packageName);
    QFETCH(Origin, origin);

    QCOMPARE(omaweb::ReleaseCheck::originOf(owned, foreign, packageName), origin);
}

void ReleaseCheckTest::tellsAPackagedReaderToUpgradeWithTheSystem()
{
    const auto instruction
        = omaweb::ReleaseCheck::upgradeInstruction(Origin::Repository, QStringLiteral("v0.5.0"));

    QVERIFY(instruction.contains(QStringLiteral("pacman -Syu")));
}

// `pacman -Syu` passes over a package no repository carries, so telling this
// reader to run it would be telling them to watch nothing happen.
void ReleaseCheckTest::tellsAHandInstalledReaderWhatASystemUpgradeWillNotDo()
{
    const auto instruction = omaweb::ReleaseCheck::upgradeInstruction(
        Origin::DownloadedPackage, QStringLiteral("v0.5.0"));

    QVERIFY(!instruction.contains(QStringLiteral("pacman -Syu")));
    QVERIFY(instruction.contains(QStringLiteral("pacman -U")));
    QVERIFY(instruction.contains(QStringLiteral("v0.5.0")));
}

// A checkout build is not owned by any package, so the command that upgrades a
// packaged reader would find nothing. It is told where the version is instead.
void ReleaseCheckTest::tellsACheckoutReaderWhereTheVersionIs()
{
    const auto instruction
        = omaweb::ReleaseCheck::upgradeInstruction(Origin::Checkout, QStringLiteral("v0.5.0"));

    QVERIFY(!instruction.contains(QStringLiteral("pacman")));
    QVERIFY(instruction.contains(QStringLiteral("v0.5.0")));
}

void ReleaseCheckTest::announcesAReleaseTheReaderHasNotSeen()
{
    QVERIFY(omaweb::ReleaseCheck::announce({.checkEnabled = true,
        .privateWindow = false,
        .runningVersion = QStringLiteral("0.4.0"),
        .newestRelease = QStringLiteral("v0.5.0"),
        .dismissedRelease = QString()}));
}

void ReleaseCheckTest::keepsQuiet_data()
{
    QTest::addColumn<omaweb::ReleaseCheck::Question>("question");

    const auto behind = omaweb::ReleaseCheck::Question {.checkEnabled = true,
        .privateWindow = false,
        .runningVersion = QStringLiteral("0.4.0"),
        .newestRelease = QStringLiteral("v0.5.0"),
        .dismissedRelease = QString()};

    auto off = behind;
    off.checkEnabled = false;
    QTest::newRow("the reader turned the check off") << off;

    // A Private window says nothing about this installation, and its age is
    // something about this installation.
    auto priv = behind;
    priv.privateWindow = true;
    QTest::newRow("a Private window") << priv;

    auto current = behind;
    current.newestRelease = QStringLiteral("v0.4.0");
    QTest::newRow("already the newest release") << current;

    auto dismissed = behind;
    dismissed.dismissedRelease = QStringLiteral("v0.5.0");
    QTest::newRow("this release was dismissed") << dismissed;

    // The tag as the reader dismissed it and as the endpoint spells it are the
    // same release, and a notice returning because of a `v` would be a bug the
    // reader could not get rid of.
    auto spelled = behind;
    spelled.dismissedRelease = QStringLiteral("0.5.0");
    QTest::newRow("dismissed, spelled without the v") << spelled;

    auto unknown = behind;
    unknown.newestRelease = QString();
    QTest::newRow("no answer about releases") << unknown;
}

void ReleaseCheckTest::keepsQuiet()
{
    QFETCH(omaweb::ReleaseCheck::Question, question);

    QVERIFY(!omaweb::ReleaseCheck::announce(question));
}

// Dismissing is for one release, not for the feature. Story 6: a reader who
// dismissed one is told about the next.
void ReleaseCheckTest::announcesTheNextReleaseAfterADismissedOne()
{
    QVERIFY(omaweb::ReleaseCheck::announce({.checkEnabled = true,
        .privateWindow = false,
        .runningVersion = QStringLiteral("0.4.0"),
        .newestRelease = QStringLiteral("v0.6.0"),
        .dismissedRelease = QStringLiteral("v0.5.0")}));
}

QTEST_APPLESS_MAIN(ReleaseCheckTest)

#include "tst_releasecheck.moc"
