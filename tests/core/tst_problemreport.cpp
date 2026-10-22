#include "ProblemReport.h"

#include <QTest>
#include <QUrl>
#include <QUrlQuery>

using omaweb::ProblemReportSection;

class ProblemReportTest final : public QObject {
    Q_OBJECT

private slots:
    void redactionKeepsShortPlainValues();
    void redactionDropsUserInformationAndFragment();
    void redactionCoversThePath();
    void writesEachSectionUnderItsTitle();
    void fillsTheBugFormsReportField();
    void cutsLaterSectionsFirst();
    void cutsTheRecordingFromItsOldestEnd();
};

namespace {

QString reportField(const QUrl &form)
{
    return QUrlQuery(form).queryItemValue(QStringLiteral("report"), QUrl::FullyDecoded);
}

QStringList filler(const QString &mark, int count)
{
    QStringList lines;
    for (int index = 0; index < count; ++index) {
        lines.append(mark + QStringLiteral(" %1 ").arg(index) + QString(990, u'x'));
    }
    return lines;
}

} // namespace

// The address #711's sign-in was refused at. `rrk=46` was the fact the report
// needed, and `dsh`, `epd` and `idnf` are where Google keeps the session and
// the account.
void ProblemReportTest::redactionKeepsShortPlainValues()
{
    const QUrl refused(QStringLiteral(
        "https://accounts.google.com/v3/signin/rejected?authuser=0"
        "&continue=https%3A%2F%2Fmyaccount.google.com%2F%3Fnlr%3D1"
        "&dsh=S-1873462851%3A1760512345678901&ec=GAlAwAE"
        "&epd=AKe5zXUZ7sS8nV0aPp3Qq_tR9yWb4hK2mL6jN1cE0fG"
        "&flowEntry=AddSession&flowName=GlifWebSignIn&hl=en-GB&idnf=reader%40example.com"
        "&rhlk=le&rip=1&rrk=46"));

    QCOMPARE(omaweb::redactedAddress(refused),
        QStringLiteral("https://accounts.google.com/…/signin/rejected?authuser=0&continue=…"
                       "&dsh=…&ec=GAlAwAE&epd=…&flowEntry=AddSession&flowName=GlifWebSignIn"
                       "&hl=en-GB&idnf=…&rhlk=le&rip=1&rrk=46"));
}

void ProblemReportTest::redactionDropsUserInformationAndFragment()
{
    QCOMPARE(omaweb::redactedAddress(QUrl(QStringLiteral(
                 "https://reader:secret@example.com/inbox?folder=work&token=a1b2c3d4#message-9"))),
        QStringLiteral("https://example.com/inbox?folder=work&token=…"));
    QCOMPARE(omaweb::redactedAddress(QUrl(QStringLiteral("https://example.com/?q"))),
        QStringLiteral("https://example.com/?q"));
    QCOMPARE(omaweb::redactedAddress(
                 QUrl(QStringLiteral("https://example.com/?page=12345678901234567"))),
        QStringLiteral("https://example.com/?page=…"));
}

// A path segment is kept by the query value's rule, and the host whatever it
// is: a token in a hand-off's path goes, the application it went to stays. The
// host is written as an address is read, in lowercase.
void ProblemReportTest::redactionCoversThePath()
{
    QCOMPARE(omaweb::redactedAddress(
                 QUrl(QStringLiteral("https://github.com/villekivela/omaweb/pull/770"))),
        QStringLiteral("https://github.com/villekivela/omaweb/pull/770"));
    QCOMPARE(omaweb::redactedAddress(
                 QUrl(QStringLiteral("slack://T0123/magic-login/xoxe-1-My4xLTE2MTk0OTg"))),
        QStringLiteral("slack://t0123/magic-login/…"));
    QCOMPARE(omaweb::redactedAddress(
                 QUrl(QStringLiteral("https://example.com/reset/a1b2c3d4e5f6/?step=2"))),
        QStringLiteral("https://example.com/reset/…/?step=2"));
    QCOMPARE(omaweb::redactedAddress(QUrl(QStringLiteral("https://example.com/"))),
        QStringLiteral("https://example.com/"));
}

void ProblemReportTest::writesEachSectionUnderItsTitle()
{
    const QList<ProblemReportSection> sections {
        {.title = QStringLiteral("Versions"),
            .lines = {QStringLiteral("Omaweb 0.17.0"), QStringLiteral("Qt 6.11.2")}},
        {.title = QStringLiteral("Sync"), .lines = {QStringLiteral("Off")}},
    };

    QCOMPARE(omaweb::problemReportText(sections),
        QStringLiteral("Versions\n- Omaweb 0.17.0\n- Qt 6.11.2\n\nSync\n- Off\n"));
}

void ProblemReportTest::fillsTheBugFormsReportField()
{
    const QList<ProblemReportSection> sections {
        {.title = QStringLiteral("Versions"), .lines = {QStringLiteral("Omaweb 0.17.0 & more")}},
    };

    const auto form = omaweb::problemReportForm(sections);

    QVERIFY(!form.cut);
    QCOMPARE(form.address.adjusted(QUrl::RemoveQuery),
        QUrl(QStringLiteral("https://github.com/villekivela/omaweb/issues/new")));
    QCOMPARE(QUrlQuery(form.address).queryItemValue(QStringLiteral("template")),
        QStringLiteral("bug.yml"));
    QCOMPARE(reportField(form.address), QStringLiteral("Versions\n- Omaweb 0.17.0 & more\n"));
}

// About 8 KB of link: the five sections that come first fit, and the crash
// after them does not, so neither does anything after it.
void ProblemReportTest::cutsLaterSectionsFirst()
{
    const QList<ProblemReportSection> sections {
        {.title = QStringLiteral("Versions"), .lines = filler(QStringLiteral("version"), 1)},
        {.title = QStringLiteral("What sites see"), .lines = filler(QStringLiteral("site"), 1)},
        {.title = QStringLiteral("Tab"), .lines = filler(QStringLiteral("tab"), 1)},
        {.title = QStringLiteral("Settings"), .lines = filler(QStringLiteral("setting"), 1)},
        {.title = QStringLiteral("Sync"), .lines = filler(QStringLiteral("sync"), 1)},
        {.title = QStringLiteral("Last crash"), .lines = filler(QStringLiteral("crash"), 3)},
        {.title = QStringLiteral("Problem recording"),
            .lines = {QStringLiteral("12:00:00 navigated")},
            .trimOldest = true},
    };

    const auto form = omaweb::problemReportForm(sections);

    QVERIFY(form.cut);
    QVERIFY(form.address.toEncoded().size() <= omaweb::problemReportLinkLimit);
    const auto field = reportField(form.address);
    for (const auto &kept : {"version 0", "site 0", "tab 0", "setting 0", "sync 0"}) {
        QVERIFY2(field.contains(QLatin1StringView(kept)), kept);
    }
    QVERIFY(!field.contains(QStringLiteral("Last crash")));
    QVERIFY(!field.contains(QStringLiteral("12:00:00 navigated")));
    QVERIFY(field.endsWith(QStringLiteral(
        "Cut to fit the link. The whole report is on the clipboard: paste it below.\n")));
}

void ProblemReportTest::cutsTheRecordingFromItsOldestEnd()
{
    const QList<ProblemReportSection> sections {
        {.title = QStringLiteral("Versions"), .lines = {QStringLiteral("Omaweb 0.17.0")}},
        {.title = QStringLiteral("Problem recording"),
            .lines = filler(QStringLiteral("event"), 10),
            .trimOldest = true},
    };

    const auto form = omaweb::problemReportForm(sections);

    QVERIFY(form.cut);
    QVERIFY(form.address.toEncoded().size() <= omaweb::problemReportLinkLimit);
    const auto field = reportField(form.address);
    QVERIFY(field.contains(QStringLiteral("Omaweb 0.17.0")));
    QVERIFY(field.contains(QStringLiteral("Problem recording\n")));
    QVERIFY(field.contains(QStringLiteral("event 9 ")));
    QVERIFY(field.contains(QStringLiteral("event 5 ")));
    QVERIFY(!field.contains(QStringLiteral("event 0 ")));
    // In the order they happened.
    QVERIFY(field.indexOf(QStringLiteral("event 8 ")) < field.indexOf(QStringLiteral("event 9 ")));
    // What is kept runs unbroken to the newest.
    for (auto index = 0; index < 9; ++index) {
        if (field.contains(QStringLiteral("event %1 ").arg(index))) {
            QVERIFY(field.contains(QStringLiteral("event %1 ").arg(index + 1)));
        }
    }
}

QTEST_GUILESS_MAIN(ProblemReportTest)
#include "tst_problemreport.moc"
