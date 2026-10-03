#include "AgentPick.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

using omaweb::tabPicks;

namespace {

QJsonObject tab(const QString &id, const QString &title, const QString &url, const QString &space)
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("title"), title},
        {QStringLiteral("url"), url}, {QStringLiteral("spaceName"), space}};
}

// What Omarchy's picker returns for a row with a subtext: the label and the
// subtext, tab-separated, and never the glyph.
QString chosen(const QString &row) { return row.section(u'\t', 1); }

} // namespace

class AgentPickTest final : public QObject {
    Q_OBJECT

private slots:
    void showsTheTitleWithTheHostAndTheSpaceUnderIt();
    void givesTheAddressToATabWithoutATitle();
    void tellsTwoTabsOfOneTitleApart();
    void keepsATitleToOneLine();
    void knowsNoTabForAnAnswerItNeverOffered();
};

void AgentPickTest::showsTheTitleWithTheHostAndTheSpaceUnderIt()
{
    const auto picks = tabPicks({tab(QStringLiteral("t1"), QStringLiteral("Inbox"),
        QStringLiteral("https://mail.example/u/0"), QStringLiteral("Personal"))});

    QCOMPARE(picks.rows.size(), 1);
    const auto fields = picks.rows.constFirst().split(u'\t');
    QCOMPARE(fields.size(), 3);
    QVERIFY(!fields.at(0).isEmpty());
    QCOMPARE(fields.at(1), QStringLiteral("Inbox"));
    QCOMPARE(fields.at(2), QStringLiteral("mail.example · Personal"));
    QCOMPARE(picks.idFor(chosen(picks.rows.constFirst())), QStringLiteral("t1"));
}

void AgentPickTest::givesTheAddressToATabWithoutATitle()
{
    const auto picks = tabPicks({tab(QStringLiteral("t1"), QString(),
        QStringLiteral("https://work.example/board"), QStringLiteral("Work"))});

    QCOMPARE(
        picks.rows.constFirst().split(u'\t').at(1), QStringLiteral("https://work.example/board"));
}

// The picker returns only the label and the subtext, so two rows of one title,
// host and Space would come back the same unless the subtext told them apart.
void AgentPickTest::tellsTwoTabsOfOneTitleApart()
{
    const auto picks = tabPicks({
        tab(QStringLiteral("a"), QStringLiteral("Docs"), QStringLiteral("https://docs.example/1"),
            QStringLiteral("Work")),
        tab(QStringLiteral("b"), QStringLiteral("Docs"), QStringLiteral("https://docs.example/2"),
            QStringLiteral("Work")),
        tab(QStringLiteral("c"), QStringLiteral("Docs"), QStringLiteral("https://docs.example/3"),
            QStringLiteral("Home")),
    });

    QCOMPARE(picks.rows.size(), 3);
    QCOMPARE(picks.idFor(chosen(picks.rows.at(0))), QStringLiteral("a"));
    QCOMPARE(picks.idFor(chosen(picks.rows.at(1))), QStringLiteral("b"));
    QCOMPARE(picks.idFor(chosen(picks.rows.at(2))), QStringLiteral("c"));
    // A tab that is alone of its kind shows nothing it has no need of.
    QVERIFY(picks.rows.at(2).endsWith(QStringLiteral("docs.example · Home")));
}

void AgentPickTest::keepsATitleToOneLine()
{
    const auto picks = tabPicks({
        tab(QStringLiteral("a"), QStringLiteral("One\tof\n  two"),
            QStringLiteral("https://x.example/"), QStringLiteral("Work")),
        tab(QStringLiteral("b"), QStringLiteral("One of two"), QStringLiteral("https://x.example/"),
            QStringLiteral("Work")),
    });

    for (const auto &row : picks.rows) {
        QCOMPARE(row.count(u'\t'), 2);
        QVERIFY(!row.contains(u'\n'));
    }
    QCOMPARE(picks.idFor(chosen(picks.rows.at(0))), QStringLiteral("a"));
    QCOMPARE(picks.idFor(chosen(picks.rows.at(1))), QStringLiteral("b"));
}

void AgentPickTest::knowsNoTabForAnAnswerItNeverOffered()
{
    const auto picks = tabPicks({tab(QStringLiteral("t1"), QStringLiteral("Inbox"),
        QStringLiteral("https://mail.example/"), QStringLiteral("Personal"))});

    QVERIFY(picks.idFor(QStringLiteral("Inbox\tsomewhere else")).isEmpty());
    QVERIFY(picks.idFor(QString()).isEmpty());
}

QTEST_APPLESS_MAIN(AgentPickTest)
#include "tst_agentpick.moc"
