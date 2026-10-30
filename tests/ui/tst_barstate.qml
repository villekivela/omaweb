import QtQuick
import QtTest
import "../../src/ui/BarState.mjs" as BarState

// What the window tells Omarchy's bar about itself (#906), from what it
// already knows: the Spaces the browser lists, their colours as the sidebar
// draws them, the Agent tabs and the page on show.
TestCase {
    name: "BarState"

    function tab(id, url, extra) {
        return Object.assign({
                                 id: id,
                                 title: "Title of " + id,
                                 url: url,
                                 iconUrl: "image://favicon/" + url,
                                 sound: "",
                                 poppedOut: false
                             }, extra || {});
    }

    function inputs(extra) {
        return Object.assign({
                                 spaces: [
                                     {
                                         id: "work",
                                         name: "Work",
                                         colour: "blue",
                                         active: "issue",
                                         pins: [tab("mail", "https://mail.example/inbox")],
                                         tabs: [tab("issue", "https://github.com/a/b/issues/1"), tab(
                                                 "docs", "https://doc.qt.io/qt-6/")]
                                     },
                                     {
                                         id: "home",
                                         name: "Home",
                                         colour: "green",
                                         active: "",
                                         pins: [],
                                         tabs: []
                                     }
                                 ],
                                 activeSpace: "work",
                                 colourOf: name => name === "blue" ? "#7aa2f7" : "#9ece6a",
                                 agentActivity: ({}),
                                 window: "Issue — Work — Omaweb",
                                 page: {
                                     url: "https://github.com/a/b/issues/1",
                                     title: "Issue",
                                     secure: true,
                                     canGoBack: true,
                                     canGoForward: false
                                 },
                                 downloads: {
                                     running: 0,
                                     fraction: 0,
                                     files: []
                                 }
                             }, extra || {});
    }

    function test_eachSpaceIsDrawnInItsSpaceColourWithItsCount() {
        const state = BarState.build(inputs());

        compare(state.type, "state");
        compare(state.activeSpace, "work");
        compare(state.spaces.length, 2);
        compare(state.spaces[0].colour, "#7aa2f7");
        compare(state.spaces[0].count, 3);
        compare(state.spaces[0].active, "issue");
        compare(state.spaces[1].colour, "#9ece6a");
        compare(state.spaces[1].count, 0);
    }

    // The bar's widget names a site by its host and draws the icon the tab
    // draws, whatever scheme the page reported it under.
    function test_aTabCarriesItsHostAndTheIconItsRowDraws() {
        const issue = BarState.build(inputs()).spaces[0].tabs[0];

        compare(issue.id, "issue");
        compare(issue.host, "github.com");
        compare(issue.title, "Title of issue");
        compare(issue.icon, "image://favicon/https://github.com/a/b/issues/1");
        compare(issue.agent, false);
    }

    function test_anAgentTabIsMarkedWithItsLastAct() {
        const state = BarState.build(inputs({
                                                agentActivity: {
                                                    docs: {
                                                        spaceId: "work",
                                                        name: "claude",
                                                        act: "read the page"
                                                    }
                                                }
                                            }));

        const docs = state.spaces[0].tabs[1];
        compare(docs.agent, true);
        compare(docs.agentAct, "read the page");
    }

    // The location shows the page on show: its host and title, whether the
    // connection is secure, and whether back and forward go anywhere.
    function test_thePageOnShowIsTheLocation() {
        const state = BarState.build(inputs());

        compare(state.window, "Issue — Work — Omaweb");
        compare(state.nav.host, "github.com");
        compare(state.nav.title, "Issue");
        compare(state.nav.secure, true);
        compare(state.nav.canBack, true);
        compare(state.nav.canForward, false);
    }

    function test_aRunningDownloadIsAStatusMark() {
        const state = BarState.build(inputs({
                                                downloads: {
                                                    running: 1,
                                                    fraction: 0.5,
                                                    files: [
                                                        {
                                                            fileName: "omarchy.iso",
                                                            fraction: 0.5
                                                        }
                                                    ]
                                                }
                                            }));

        compare(state.status.downloads.running, 1);
        compare(state.status.downloads.progress, 50);
        compare(state.status.downloads.files[0].name, "omarchy.iso");
        compare(state.status.downloads.files[0].progress, 50);
    }
}
