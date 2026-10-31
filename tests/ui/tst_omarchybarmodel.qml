import QtQuick
import QtTest
import "../../integrations/omarchy/bar/BarModel.mjs" as BarModel

// The layout rules of the Omarchy bar widget that Omaweb ships (#906), which
// run in Quickshell rather than in the browser and so are tested here, apart
// from the widget that draws them.
TestCase {
    name: "OmarchyBarModel"

    function tabs(count) {
        const listed = [];
        for (let i = 0; i < count; i++)
            listed.push({
                            id: "t" + i
                        });
        return listed;
    }

    function ids(listed) {
        return listed.map(tab => tab.id);
    }

    // Each tab is a 16 pixel favicon with its spacing on either side.
    function test_everyTabShowsWhileThereIsRoomAtTheWidestSpacing() {
        const layout = BarModel.fitTabs(tabs(5), "t2", 5 * (16 + 14), 16);

        compare(layout.pad, 7);
        compare(ids(layout.tabs), ["t0", "t1", "t2", "t3", "t4"]);
        compare(layout.hidden, 0);
    }

    function test_theSpacingShrinksBeforeAnyTabIsHidden() {
        const layout = BarModel.fitTabs(tabs(5), "t2", 5 * (16 + 6), 16);

        compare(layout.pad, 3);
        compare(layout.hidden, 0);
    }

    // The ones furthest from the active tab go first, from either side, and
    // the active tab always stays.
    function test_theTabsFurthestFromTheActiveOneCollapseIntoACount() {
        const layout = BarModel.fitTabs(tabs(9), "t5", 4 * (16 + 6), 16);

        compare(ids(layout.tabs), ["t3", "t4", "t5", "t6"]);
        compare(layout.hidden, 5);
    }

    function test_theActiveTabStaysWithNoRoomAtAll() {
        const layout = BarModel.fitTabs(tabs(3), "t2", 0, 16);

        compare(ids(layout.tabs), ["t2"]);
        compare(layout.hidden, 2);
    }

    function test_aSpaceIsShortenedToItsFirstLetterInUpperCase() {
        compare(BarModel.spaceLabel("work", true), "W");
        compare(BarModel.spaceLabel("ändringar", true), "Ä");
        compare(BarModel.spaceLabel("work", false), "work");
    }

    // ----- the tab switcher (#906): every Space's tabs side by side.

    function state() {
        const tab = (id, title, host) => ({
            id: id,
            title: title,
            host: host
        });
        return {
            activeSpace: "research",
            spaces: [
                {
                    id: "home",
                    name: "Home",
                    colour: "#9ece6a",
                    active: "news",
                    pins: [],
                    tabs: [tab("news", "Kernel news", "lwn.net")]
                },
                {
                    id: "research",
                    name: "Research",
                    colour: "#e0af68",
                    active: "issue",
                    pins: [tab("mail", "Inbox", "mail.example")],
                    tabs: [tab("issue", "Issue #906", "github.com"), tab("lwn", "Welcome to LWN.net",
                                                                         "lwn.net")]
                }
            ]
        };
    }

    function described(board) {
        return board.columns.map(column => column.space.id + ": " + column.tabs.map(entry
                                                                                    => entry.tab.id).join(
                                               " "));
    }

    function at(board) {
        const column = board.columns[board.column];
        return column.space.id + "/" + column.tabs[board.row].tab.id;
    }

    // Every Space side by side in the browser's order, its pins first, and
    // the switcher starts on the tab on show.
    function test_theBoardHasAColumnForEverySpaceStartingOnTheTabOnShow() {
        const board = BarModel.board(state(), "");

        compare(described(board), ["home: news", "research: mail issue lwn"]);
        verify(board.columns[1].tabs[0].pinned);
        verify(!board.columns[1].tabs[1].pinned);
        compare(at(board), "research/issue");
    }

    // Every word typed has to be in a tab's title or host; a Space with no
    // tab left goes. The cursor starts in the Space on show if anything there
    // matches.
    function test_typingKeepsTheTabsThatMatchEveryWordInAnySpace() {
        const board = BarModel.board(state(), "LWN kern");

        compare(described(board), ["home: news"]);
        compare(at(board), "home/news");
        compare(at(BarModel.board(state(), "lwn")), "research/lwn");
    }

    function test_nothingMatchingLeavesNothingToTake() {
        const board = BarModel.board(state(), "zzz");

        compare(board.columns.length, 0);
        compare(board.column, -1);
        compare(board.row, -1);
    }

    // Sideways keeps the row where the next Space has one, and wraps.
    function test_sidewaysMovesToTheNextSpaceKeepingTheRowWhereItCan() {
        const columns = BarModel.board(state(), "").columns;

        compare(BarModel.moveOnBoard(columns, 1, 2, -1, 0), {
                    "column": 0,
                    "row": 0
                });
        compare(BarModel.moveOnBoard(columns, 1, 0, 1, 0), {
                    "column": 0,
                    "row": 0
                });
        compare(BarModel.moveOnBoard(columns, 0, 0, -1, 0), {
                    "column": 1,
                    "row": 0
                });
    }

    // A Space with nothing open has its column, but nothing to stop on.
    function test_sidewaysPassesASpaceWithNothingOpen() {
        const browser = state();
        browser.spaces.splice(1, 0, {
                                  id: "idle",
                                  name: "Idle",
                                  colour: "#7aa2f7",
                                  active: "",
                                  pins: [],
                                  tabs: []
                              });
        const columns = BarModel.board(browser, "").columns;

        compare(columns.length, 3);
        compare(BarModel.moveOnBoard(columns, 0, 0, 1, 0), {
                    "column": 2,
                    "row": 0
                });
    }

    function test_upAndDownStayInTheColumn() {
        const columns = BarModel.board(state(), "").columns;

        compare(BarModel.moveOnBoard(columns, 1, 0, 0, 1), {
                    "column": 1,
                    "row": 1
                });
        compare(BarModel.moveOnBoard(columns, 1, 2, 0, 1), {
                    "column": 1,
                    "row": 2
                });
        compare(BarModel.moveOnBoard(columns, 1, 0, 0, -1), {
                    "column": 1,
                    "row": 0
                });
    }
}
