import QtQuick
import QtTest
import "../../integrations/omarchy/bar"

// The Tab switcher's card that Omaweb ships for Omarchy's bar (#906), driven
// by its keys over a stand-in for the browser, on the vendored Omarchy kit.
// What a test reads is what the card draws: the Spaces whose headings are in
// view, left to right.
TestCase {
    id: testCase
    name: "OmarchyTabSwitcher"
    when: windowShown
    width: 1400
    height: 860

    // Eight Spaces, a tab or two in each but Idle.
    function state(activeSpace) {
        const names = "Personal Home Research Work Reading Travel Idle Music".split(" ");
        const counts = [1, 2, 3, 2, 1, 3, 0, 1];
        const spaces = names.map((name, index) => [name.toLowerCase(), name, counts[index]]);
        return {
            activeSpace: activeSpace,
            spaces: spaces.map(([id, name, count]) => {
                const tabs = [];
                for (let i = 0; i < count; ++i)
                    tabs.push({
                                  id: id + i,
                                  title: name + " " + i,
                                  host: id + ".example",
                                  icon: "",
                                  sound: "",
                                  poppedOut: false,
                                  loading: false,
                                  agent: false,
                                  agentAct: ""
                              });
                return {
                    id: id,
                    name: name,
                    colour: "#7aa2f7",
                    active: tabs.length > 0 ? tabs[0].id : "",
                    pins: [],
                    tabs: tabs
                };
            })
        };
    }

    Component {
        id: linkComponent

        Item {
            property bool live: false
            property var browser: null
            property var sent: []
            function send(message) {
                sent.push(message);
            }
            function activate(tab, space) {
                send({
                         "do": "activate",
                         "tab": tab.id,
                         "space": space
                     });
            }
            function windowOf(tab) {
                return null;
            }
        }
    }

    // The screen the overlay covers, wide enough for four Spaces under any
    // theme the suite runs with.
    Component {
        id: screenComponent

        Item {
            width: 20000
            height: 4000
        }
    }

    Component {
        id: cardComponent

        TabSwitcher {}
    }

    // The card as the overlay opens it: the link goes live once it is
    // connected, the card is reset for it, and the browser's state arrives
    // after.
    function open(activeSpace) {
        const link = createTemporaryObject(linkComponent, testCase);
        const screen = createTemporaryObject(screenComponent, testCase);
        const card = createTemporaryObject(cardComponent, screen, {
                                               "link": link
                                           });
        link.live = true;
        card.reset();
        link.browser = state(activeSpace);
        waitForRendering(card);
        return card;
    }

    function headings(item, names, found) {
        for (const child of item.children) {
            if (child instanceof Text && child.font.bold && names.indexOf(child.text) >= 0)
                found.push(child);
            headings(child, names, found);
        }
        return found;
    }

    function inView(card) {
        const names = card.link.browser.spaces.map(space => space.name);
        const left = card.contentLeftInset;
        const right = card.width - card.contentRightInset;
        const shown = [];
        for (const text of headings(card, names, [])) {
            const x = text.mapToItem(card, 0, 0).x;
            if (x >= left && x < right)
                shown.push({
                               "name": text.text,
                               "x": x
                           });
        }
        shown.sort((a, b) => a.x - b.x);
        return shown.map(each => each.name);
    }

    function press(keys) {
        for (const key of keys) {
            keyClick(key);
            waitForRendering(testCase);
        }
    }

    // On a Space past the first four, the card opens with it last in view.
    function test_theCardOpensWithTheSpaceOnShowInView() {
        const card = open("travel");

        compare(inView(card), ["Research", "Work", "Reading", "Travel"]);
    }

    // Moving onto a Space already in view leaves the board where it is, with
    // the Space on show out of view too.
    function test_movingToASpaceInViewLeavesTheBoard() {
        const card = open("research");
        press([Qt.Key_L, Qt.Key_L, Qt.Key_L, Qt.Key_L]);
        compare(inView(card), ["Reading", "Travel", "Idle", "Music"]);

        press([Qt.Key_H]);

        compare(inView(card), ["Reading", "Travel", "Idle", "Music"]);
    }

    // Opened again, with the browser already there, the board starts from
    // the Space on show and then moves only when the cursor's Space would
    // leave it.
    function test_openingAgainStartsTheBoardAfresh() {
        const card = open("travel");
        press([Qt.Key_L]);
        compare(inView(card), ["Reading", "Travel", "Idle", "Music"]);

        card.reset();
        waitForRendering(card);
        compare(inView(card), ["Research", "Work", "Reading", "Travel"]);
        press([Qt.Key_H]);

        compare(inView(card), ["Research", "Work", "Reading", "Travel"]);
    }
}
