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
}
