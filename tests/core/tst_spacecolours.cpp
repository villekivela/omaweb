#include "BrowserController.h"
#include "SessionFixture.h"
#include "SpaceListModel.h"
#include "SqliteSessionStore.h"

#include <QTemporaryDir>
#include <QTest>

using omaweb::BrowserController;
using omaweb::SpaceListModel;
using omaweb::SpaceStorage;
using omaweb::SqliteSessionStore;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

namespace {

QString colourOf(BrowserController &browser, const QString &spaceId)
{
    auto *spaces = browser.spaces();
    for (int row = 0; row < spaces->rowCount(); ++row) {
        const auto index = spaces->index(row, 0);
        if (spaces->data(index, SpaceListModel::IdRole).toString() == spaceId) {
            return spaces->data(index, SpaceListModel::ColorRole).toString();
        }
    }
    return {};
}

QStringList spaceOrder(BrowserController &browser)
{
    QStringList ids;
    auto *spaces = browser.spaces();
    for (int row = 0; row < spaces->rowCount(); ++row) {
        ids.append(spaces->data(spaces->index(row, 0), SpaceListModel::IdRole).toString());
    }
    return ids;
}

} // namespace

class SpaceColoursTest final : public QObject {
    Q_OBJECT

private slots:
    void givesEachNewSpaceTheLeastUsedColour();
    void keepsTheColourTheReaderSets();
    void refusesAColourOutsideTheSix();
    void listsAgentSpacesAfterTheReaders();
    void movesASpaceTakenOverToTheEndOfTheReaders();
    void coloursASessionFromBeforeSpaceColours();
    void renamesTheBrightColours();
};

// A Space's colour is a palette name the theme resolves, never a colour of its
// own. The reader's first Space takes the first of the six, and each Space made
// after it the one fewest Spaces have, so two Spaces made in turn differ.
void SpaceColoursTest::givesEachNewSpaceTheLeastUsedColour()
{
    QTemporaryDir root;
    BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
    QVERIFY2(browser.ready(), qPrintable(browser.errorMessage()));

    const auto personal = browser.activeSpaceId();
    QCOMPARE(colourOf(browser, personal), QStringLiteral("orange"));
    const auto work = browser.createSpace(QStringLiteral("Work"));
    QCOMPARE(colourOf(browser, work), QStringLiteral("yellow"));
    const auto home = browser.createSpace(QStringLiteral("Home"));
    QCOMPARE(colourOf(browser, home), QStringLiteral("green"));
}

// Any of the six, including one another Space already has, and it is still
// the Space's colour after a restart.
void SpaceColoursTest::keepsTheColourTheReaderSets()
{
    QTemporaryDir root;
    QString work;
    {
        BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
        const auto personal = browser.activeSpaceId();
        work = browser.createSpace(QStringLiteral("Work"));
        for (const auto *name : {"orange", "yellow", "green", "teal", "blue"}) {
            QVERIFY2(browser.setSpaceColour(work, QString::fromLatin1(name)), name);
            QCOMPARE(colourOf(browser, work), QString::fromLatin1(name));
        }
        QVERIFY(browser.setSpaceColour(work, QStringLiteral("violet")));
        QVERIFY(browser.setSpaceColour(personal, QStringLiteral("violet")));
        QCOMPARE(colourOf(browser, personal), QStringLiteral("violet"));
        QVERIFY(!browser.setSpaceColour(QStringLiteral("no-such-space"), QStringLiteral("blue")));
    }
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(colourOf(restored, work), QStringLiteral("violet"));
    QCOMPARE(colourOf(restored, restored.activeSpaceId()), QStringLiteral("violet"));
}

// Red, magenta and cyan say urgent, Private and Agent, a colour of the Space's
// own would not follow the theme, and the bright names are gone.
void SpaceColoursTest::refusesAColourOutsideTheSix()
{
    QTemporaryDir root;
    BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
    const auto personal = browser.activeSpaceId();
    for (const auto *name :
        {"red", "magenta", "cyan", "bright_red", "bright_magenta", "bright_cyan", "bright_green",
            "bright_yellow", "bright_blue", "#7c6cff", "", "Orange"}) {
        QVERIFY2(!browser.setSpaceColour(personal, QString::fromLatin1(name)), name);
    }
    QCOMPARE(colourOf(browser, personal), QStringLiteral("orange"));
}

// The Space list is the footer's order and `select-space` counts along it: the
// reader's Spaces, then the Agent Spaces. An Agent making a Space never moves
// one of the reader's to another number, and a Space the reader makes later
// still comes before every Agent Space.
void SpaceColoursTest::listsAgentSpacesAfterTheReaders()
{
    QTemporaryDir root;
    QStringList expected;
    {
        BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
        const auto personal = browser.activeSpaceId();
        const auto signup
            = browser.createAgentSpace(QStringLiteral("Signup"), QStringLiteral("codex"));
        const auto work = browser.createSpace(QStringLiteral("Work"));
        const auto research
            = browser.createAgentSpace(QStringLiteral("Research"), QStringLiteral("codex"));
        const auto home = browser.createSpace(QStringLiteral("Home"));
        expected = {personal, work, home, signup, research};
        QCOMPARE(spaceOrder(browser), expected);
        QCOMPARE(colourOf(browser, work), QStringLiteral("yellow"));
        QCOMPARE(colourOf(browser, home), QStringLiteral("green"));

        // A move stays on its own side.
        QVERIFY(!browser.moveSpaceBy(home, 1));
        QVERIFY(!browser.moveSpaceBy(signup, -1));
        QVERIFY(browser.moveSpaceBy(research, -1));
        expected = {personal, work, home, research, signup};
        QCOMPARE(spaceOrder(browser), expected);
    }
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(spaceOrder(restored), expected);
}

// Once it is the reader's it is a square like the others: the last one, in the
// colour fewest of the reader's Spaces have.
void SpaceColoursTest::movesASpaceTakenOverToTheEndOfTheReaders()
{
    QTemporaryDir root;
    QStringList expected;
    QString research;
    {
        BrowserController browser(SpaceStorage(root.path(), QStringLiteral("test")));
        const auto personal = browser.activeSpaceId();
        const auto work = browser.createSpace(QStringLiteral("Work"));
        const auto signup
            = browser.createAgentSpace(QStringLiteral("Signup"), QStringLiteral("codex"));
        research = browser.createAgentSpace(QStringLiteral("Research"), QStringLiteral("codex"));
        QVERIFY(browser.setSpaceColour(work, QStringLiteral("blue")));

        QVERIFY(browser.takeOverSpace(research));
        expected = {personal, work, research, signup};
        QCOMPARE(spaceOrder(browser), expected);
        QCOMPARE(colourOf(browser, research), QStringLiteral("yellow"));
    }
    BrowserController restored(SpaceStorage(root.path(), QStringLiteral("test")));
    QCOMPARE(spaceOrder(restored), expected);
    QCOMPARE(colourOf(restored, research), QStringLiteral("yellow"));
}

// Every Space stored before Spaces had colours holds the one hex value they
// were all given. The first start replaces each with a palette name, along the
// footer: the reader's Spaces in their order, then the Agent Spaces. A Space
// that already holds a name keeps it.
void SpaceColoursTest::coloursASessionFromBeforeSpaceColours()
{
    const auto space = [](const QString &id, const QString &colour) {
        return SpaceSpec {.id = id,
            .name = id,
            .color = colour,
            .tabs = {TabSpec {.id = id + QStringLiteral("-tab"),
                .url = QUrl(QStringLiteral("https://example.com/"))}}};
    };
    const auto legacy = QStringLiteral("#7c6cff");
    SessionFixture fixture(SessionSpec {
        .spaces = {space(QStringLiteral("personal"), legacy),
            space(QStringLiteral("signup"), legacy), space(QStringLiteral("work"), legacy),
            space(QStringLiteral("travel"), QStringLiteral("green")),
            space(QStringLiteral("home"), legacy)},
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);
    {
        SqliteSessionStore store(fixture.dataRoot());
        QVERIFY(store.open());
        QVERIFY(store.saveAgentSpace(QStringLiteral("signup"), QStringLiteral("codex"), false));
    }

    const QStringList footer {QStringLiteral("personal"), QStringLiteral("work"),
        QStringLiteral("travel"), QStringLiteral("home"), QStringLiteral("signup")};
    const QStringList colours {QStringLiteral("orange"), QStringLiteral("yellow"),
        QStringLiteral("green"), QStringLiteral("teal"), QStringLiteral("blue")};
    {
        auto browser = fixture.createController();
        QCOMPARE(spaceOrder(*browser), footer);
        for (qsizetype index = 0; index < footer.size(); ++index) {
            QCOMPARE(colourOf(*browser, footer.at(index)), colours.at(index));
        }
    }
    // Written down, so the next start has nothing to replace.
    SqliteSessionStore store(fixture.dataRoot());
    QVERIFY(store.open());
    const auto stored = store.loadSpaces();
    QCOMPARE(stored.size(), footer.size());
    for (qsizetype index = 0; index < footer.size(); ++index) {
        QCOMPARE(stored.at(index).id, footer.at(index));
        QCOMPARE(stored.at(index).color, colours.at(index));
    }
}

// The six were green, yellow, blue and their bright twins before Omaweb owned
// them. Each bright twin is renamed to the colour that took its place, an Agent
// Space's too, rather than given whichever colour fewest Spaces have, so a
// reader's Spaces stay as far apart as they were.
void SpaceColoursTest::renamesTheBrightColours()
{
    const auto space = [](const QString &id, const QString &colour) {
        return SpaceSpec {.id = id,
            .name = id,
            .color = colour,
            .tabs = {TabSpec {.id = id + QStringLiteral("-tab"),
                .url = QUrl(QStringLiteral("https://example.com/"))}}};
    };
    SessionFixture fixture(SessionSpec {
        .spaces = {space(QStringLiteral("personal"), QStringLiteral("bright_yellow")),
            space(QStringLiteral("work"), QStringLiteral("bright_green")),
            space(QStringLiteral("travel"), QStringLiteral("bright_blue")),
            space(QStringLiteral("home"), QStringLiteral("green")),
            space(QStringLiteral("signup"), QStringLiteral("bright_blue"))},
        .activeSpaceId = QStringLiteral("personal"),
    });
    QVERIFY_SESSION_READY(fixture);
    {
        SqliteSessionStore store(fixture.dataRoot());
        QVERIFY(store.open());
        QVERIFY(store.saveAgentSpace(QStringLiteral("signup"), QStringLiteral("codex"), false));
    }

    const QStringList footer {QStringLiteral("personal"), QStringLiteral("work"),
        QStringLiteral("travel"), QStringLiteral("home"), QStringLiteral("signup")};
    const QStringList colours {QStringLiteral("orange"), QStringLiteral("teal"),
        QStringLiteral("violet"), QStringLiteral("green"), QStringLiteral("violet")};
    {
        auto browser = fixture.createController();
        for (qsizetype index = 0; index < footer.size(); ++index) {
            QCOMPARE(colourOf(*browser, footer.at(index)), colours.at(index));
        }
    }
    SqliteSessionStore store(fixture.dataRoot());
    QVERIFY(store.open());
    const auto stored = store.loadSpaces();
    QCOMPARE(stored.size(), footer.size());
    for (qsizetype index = 0; index < footer.size(); ++index) {
        QCOMPARE(stored.at(index).id, footer.at(index));
        QCOMPARE(stored.at(index).color, colours.at(index));
    }
}

QTEST_GUILESS_MAIN(SpaceColoursTest)

#include "tst_spacecolours.moc"
