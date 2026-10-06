import QtQuick
import QtTest
import Omaweb
import Omaweb.Engine
import "../../src/ui" as Omaweb
import qs.Commons

TestCase {
    id: testCase
    name: "BrowserChrome"
    when: true

    property var window: null
    // The frame watches still listening, so a test that fails before it stops
    // its own does not leave it reading into the tests after it.
    property var frameWatches: []
    // `browser` is a context property, and a component that declares one of its
    // own cannot name it: the declaration shadows the context.
    readonly property var browserController: browser
    readonly property var syncLauncherContext: syncLauncher
    // The Agent activity log, as the window finds it by name.
    readonly property var agentActivityContext: agentActivity

    // The page's width, counted rather than sampled: what a layout costs is
    // paid once per width the viewport is given.
    SignalSpy {
        id: viewportWidthSpy
        signalName: "widthChanged"
    }

    // The same count for the sidebar, whose rows would be laid out again with
    // every width of its own.
    SignalSpy {
        id: sidebarWidthSpy
        signalName: "widthChanged"
    }

    // Whether a page of a Space that is away stopped or started while it was
    // only being listed.
    SignalSpy {
        id: awayPageSpy
        signalName: "pageFrozenChanged"
    }

    SignalSpy {
        id: otherAwayPageSpy
        signalName: "pageFrozenChanged"
    }

    // What the sidebar asks of the window when a press on it becomes a drag.
    SignalSpy {
        id: moveSpy
        signalName: "windowMoveRequested"
    }

    // Whether the Space on show changed, even for a moment, while the other
    // Spaces' tabs were only being listed.
    SignalSpy {
        id: listingSpaceSpy
        signalName: "activeSpaceChanged"
    }

    Component {
        id: windowComponent
        Omaweb.Main {}
    }

    // Content blocking as the chrome reads it, having refused one request on
    // the page on show.
    Component {
        id: oneRefusalBlockerComponent

        QtObject {
            property int refusalTallyGeneration: 0

            function refusalTally(spaceId, pageAddress) {
                return 1;
            }
            function refusedRequests(spaceId, pageAddress) {
                return [
                            {
                                "address": "https://ads.example/banner.js",
                                "canonicalName": ""
                            }
                        ];
            }
            function siteEnabled(url) {
                return true;
            }
            function setSiteEnabled(url, enabled) {
            }
        }
    }

    // Content blocking as the chrome reads it, having refused five requests on
    // the page on show, one of them through the canonical name its host's
    // CNAME chain ends at.
    Component {
        id: refusingBlockerComponent

        QtObject {
            property int refusalTallyGeneration: 0

            function refusalTally(spaceId, pageAddress) {
                return 5;
            }

            function refusedRequests(spaceId, pageAddress) {
                return [
                            {
                                "address": "https://ads.example/banner.js",
                                "canonicalName": ""
                            },
                            {
                                "address": "https://metrics.news.example/collect.js?id=1",
                                "canonicalName": "collect.tracker.example"
                            },
                            {
                                "address": "https://ads.example/second.js",
                                "canonicalName": ""
                            },
                            {
                                "address": "https://ads.example/third.js",
                                "canonicalName": ""
                            },
                            {
                                "address": "https://ads.example/fourth.js",
                                "canonicalName": ""
                            }
                        ];
            }

            function siteEnabled(url) {
                return true;
            }

            function setSiteEnabled(url, enabled) {
            }
        }
    }

    // The harness loads Omaweb's own default keymap, which this build knows
    // every command in, so nothing is ever ignored in it. These stand the two
    // surfaces up against a keymap that did report something.
    Component {
        id: reportingSettingsComponent

        Omaweb.SettingsPage {
            id: reportingSettings

            property string report: ""

            colors: testCase.window.colors
            iconFontFamily: ""
            keyboard: QtObject {
                property string errorMessage: reportingSettings.report
            }
            open: true
            section: sections.indexOf("keyboard")
            width: 900
            height: 700
        }
    }

    // A settings page stood up fresh against the same preference store, which
    // is what a restart looks like from the page's side.
    Component {
        id: restartedSettingsComponent

        Omaweb.SettingsPage {
            colors: testCase.window.colors
            iconFontFamily: ""
            browser: testCase.browserController
            open: true
            section: sections.indexOf("privacy")
            width: 900
            height: 700
        }
    }

    Component {
        id: attentionOutlineComponent

        Omaweb.SpaceOutline {
            colors: testCase.window.colors
            iconFontFamily: ""
            settingsAttention: true
            width: 300
            height: 600
        }
    }

    Component {
        id: authenticatedOutlineComponent

        Omaweb.SpaceOutline {
            colors: testCase.window.colors
            iconFontFamily: ""
            width: 300
            height: 600
            sync: QtObject {
                property bool enabled: true
                property string login: "octocat"
                property string status: "Sync is on"
                property string errorMessage: ""
                property string avatarPath: ""
            }
        }
    }

    SignalSpy {
        id: avatarSettingsSpy
        signalName: "settingsRequested"
    }

    // The type the theme sets, so a size the sidebar derives from it can be
    // driven rather than read back, and the theme every test starts from, so
    // the chrome is measured against one the suite names rather than the
    // desktop's (#277).
    ThemeAxis {
        id: themeAxis
    }

    function initTestCase() {
        themeAxis.remember();
        themeAxis.useStatedTheme();
        window = windowComponent.createObject(null);
        verify(window !== null);
        window.show();
        wait(50);
    }

    function cleanupTestCase() {
        themeAxis.restore();
        window.destroy();
    }

    // Every test shares one window, and a test that fails part-way leaves it as
    // it was when the failure stopped it: Settings, History, a menu or the
    // Space dialog over the page, a Split, a hidden or resized sidebar. The
    // next test would click on what was left and measure around it (#507), so
    // each one starts from a settled window. It is put back as a key would put
    // it, so nothing restored here is still moving when the test begins.
    function init() {
        while (frameWatches.length > 0)
            stopWatching(frameWatches[0]);
        InputOrigin.pointer = false;
        themeAxis.useStatedTheme();
        window.endStartPageDrive();
        window.startPageSummoned = false;
        window.shortcutsOpen = false;
        window.settingsOpen = false;
        window.historyOpen = false;
        window.dialogMode = "";
        window.closeSiteInformation();
        window.tabMenuOpen = false;
        window.pageMenuOpen = false;
        window.extensionMenuOpen = false;
        window.spaceOverflowMenuOpen = false;
        if (browser.splitOnShow)
            browser.separateSplit();
        window.sidebarCollapsed = false;
        window.sidebarPeeked = false;
        if (window.sidebarWidth !== window.sidebarDefaultWidth)
            window.setSidebarWidth(window.sidebarDefaultWidth);
        if (!window.floatingControls)
            window.setFloatingControls(true);
        if (window.sidebarSide !== "left")
            window.setSidebarSide("left");
        if (window.startPageScene !== "crt-road")
            window.setStartPageScene("crt-road");
        if (!window.startPageGlass)
            window.setStartPageGlass(true);
        if (fontSettings.interfaceFontSizeOverridden)
            fontSettings.resetInterfaceFontSize();
        // Whatever the last test pressed, this one starts from the pointer and
        // a desktop that has not asked for reduced motion.
        InputOrigin.pointer = true;
        SystemMotion.reduced = false;
        // A slide the last test started still runs: a test that ends by
        // bringing the sidebar back leaves it on its way, and on a loaded
        // machine the next test's first checks ran before it was drawn.
        const sidebar = findChild(window.contentItem, "sidebar");
        tryVerify(function () {
            return sidebar.visible && Math.round(sidebar.x) === 0;
        });
    }

    // A Download record outlives the test that made it, and every test here
    // shares one window, so a test about the list starts from an empty one.
    function clearDownloads() {
        for (let row = window.downloads.count - 1; row >= 0; --row)
            window.downloads.forget(row);
    }

    // One row of the window's download list, by the role the model names.
    function downloadRole(row, role) {
        return window.downloads.data(window.downloads.index(row, 0), role);
    }

    // Where a download landed in the list, which is newest first.
    function downloadRowFor(runtimeId) {
        for (let row = 0; row < window.downloads.count; ++row)
            if (downloadRole(row, Downloads.RuntimeIdRole) === runtimeId)
                return row;
        return -1;
    }

    // A Space at rest shows the start page: no engine is spent on the blank
    // tab standing in for a page, and the outline lists no ordinary tab row.
    // A test about a page, an engine or a tab row opens a page first.
    function openPage(url) {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        browser.openInput(url, false);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item.currentUrl.toString() === url;
        });
        return engineHost.item;
    }

    // Whatever the chrome is still moving: a Space or a page arriving, or the
    // Start page dropping away from a page that replaced it. A test about a
    // movement of its own starts from rest, and in a turn of its own: the page
    // area counts every page that came on screen in one turn as arriving
    // together, and a test whose checks all pass at once never ends a turn. A
    // pane paired after that would bring the page it was paired with along
    // with it, which no reader's input can do (#507). The turn ends first, so
    // a movement the chrome starts at the end of it is waited for too.
    function settleMotion() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const outline = findChild(window.contentItem, "sidebar");
        wait(0);
        tryCompare(outline, "arriving", false);
        tryCompare(engineHost, "tabNudgeX", 0);
        tryCompare(engineHost, "tabNudgeY", 0);
        tryCompare(findChild(window.contentItem, "startPage"), "visible", false);
    }

    // Where the keyboard is, asked of the chrome rather than of the window's
    // own idea of its regions.
    function focusIsInside(item) {
        let at = window.activeFocusItem;
        while (at) {
            if (at === item)
                return true;
            at = at.parent;
        }
        return false;
    }

    // A button in a row the positioner has not laid out yet sits on top of its
    // neighbours, so a press meant for one lands on another. The first answer
    // in a row is at the left edge; every one after it has been moved.
    function settleActions(action) {
        if (!action)
            return;
        tryVerify(function () {
            return action.width > 0;
        });
        wait(50);
    }

    // Clicks the middle of an item, and when `landed` says the click did not
    // do what it was for, returns what the window held at that moment: what a
    // press there reaches, what the item's own MouseArea saw, the Space dialog,
    // what was moving and where the keyboard was. Empty when it landed. A miss
    // CI saw and this machine never did names its cause in the log (#507).
    function clickReportingAMiss(item, landed) {
        const at = item.mapToItem(window.contentItem, item.width / 2, item.height / 2);
        const target = pressTargetIn(window.contentItem, at.x, at.y);
        const area = mouseAreaIn(item);
        const seen = [];
        const watched = {};
        for (const name of ["pressedChanged", "released", "canceled", "clicked"]) {
            watched[name] = function () {
                seen.push(name === "pressedChanged" ? "pressed=" + area.pressed : name);
            };
            if (area)
                area[name].connect(watched[name]);
        }
        mouseClick(item, item.width / 2, item.height / 2);
        if (area) {
            for (const name in watched)
                area[name].disconnect(watched[name]);
        }
        if (landed())
            return "";

        const dialog = findChild(window.contentItem, "spaceDialog");
        const panel = findChild(dialog, "commandDialogPanel");
        const settings = findChild(window.contentItem, "settingsSurface");
        const moving = [];
        movingIn(window.contentItem, moving);
        return ["The click on " + itemName(item) + " missed.", "click at (" + at.x + ", " + at.y + "): taken by " + (
                    target === area ? "its own MouseArea" : itemName(target)), "button saw: " + (
                    seen.length > 0 ? seen.join(", ") : "nothing"), "dialog: mode \""
                + window.dialogMode + "\", visible " + dialog.visible + (panel ? ", panel at "
                                                                                 + JSON.stringify(
                                                                                     panel.mapToItem(
                                                                                         window.contentItem,
                                                                                         0, 0)) + " "
                                                                                 + panel.width
                                                                                 + "x" + panel.height :
                                                                                 ""), "settings: visible "
                + settings.visible + ", lift " + settings.lift + ", opacity " + settings.opacity
                + ", section " + settings.section, "moving: " + (moving.length > 0 ? moving.join(", ") :
                                                                                     "nothing"),
                "focus: " + itemName(window.activeFocusItem) + ", pointer origin "
                + InputOrigin.pointer + ", reduced motion " + SystemMotion.reduced].join("\n");
    }

    // The item a left press at a point in the window reaches, found as the
    // window delivers it: the topmost child first, hidden and disabled
    // subtrees and points outside a clip passed over. Only what takes a press
    // stops it: a MouseArea, a Flickable, or an item with a tap or drag handler.
    function pressTargetIn(item, x, y) {
        if (!item.visible || !item.enabled)
            return null;
        const inside = item.contains(item.mapFromItem(window.contentItem, x, y));
        if (item.clip && !inside)
            return null;
        const children = [];
        for (let index = 0; index < item.children.length; ++index)
            children.push({
                              "child": item.children[index],
                              "index": index
                          });
        children.sort(function (left, right) {
            return right.child.z - left.child.z || right.index - left.index;
        });
        for (const entry of children) {
            const found = pressTargetIn(entry.child, x, y);
            if (found)
                return found;
        }
        return inside && takesPress(item) ? item : null;
    }

    function takesPress(item) {
        if (String(item).indexOf("QQuickMouseArea") === 0)
            return Boolean(item.acceptedButtons & Qt.LeftButton);
        if (item.flicking !== undefined)
            return item.interactive;
        for (let index = 0; index < item.data.length; ++index) {
            const kind = String(item.data[index]);
            if ((kind.indexOf("QQuickTapHandler") === 0 || kind.indexOf("QQuickDragHandler") === 0)
                    && item.data[index].enabled)
                return true;
        }
        return false;
    }

    function mouseAreaIn(item) {
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index];
            if (String(child).indexOf("QQuickMouseArea") === 0)
                return child;
            const found = mouseAreaIn(child);
            if (found)
                return found;
        }
        return null;
    }

    // Every animation, transition, sheet lift and timer still running under an
    // item, each named with the item that holds it.
    function movingIn(item, moving) {
        const holder = itemName(item);
        const running = function (thing) {
            return Boolean(thing) && thing.running === true;
        };
        for (let index = 0; index < item.data.length; ++index) {
            const thing = item.data[index];
            if (running(thing) || running(thing.animation) || running(thing.arrival) || running(
                        thing.departure))

                moving.push((thing.objectName || kindOf(thing)) + " in " + holder);
        }
        for (let index = 0; index < item.transitions.length; ++index) {
            if (running(item.transitions[index]))
                moving.push("Transition in " + holder);
        }
        for (let index = 0; index < item.children.length; ++index)
            movingIn(item.children[index], moving);
    }

    function kindOf(thing) {
        return String(thing).replace(/\(0x.*$/, "");
    }

    // An item by its objectName, or by its type in the nearest named item.
    function itemName(item) {
        if (!item)
            return "nothing";
        if (item.objectName.length > 0)
            return item.objectName;
        for (let at = item.parent; at; at = at.parent) {
            if (at.objectName.length > 0)
                return kindOf(item) + " in " + at.objectName;
        }
        return kindOf(item);
    }

    // What `read` returns at every frame the window draws from here on, read
    // once the frame's animations have advanced and its bindings settled, so
    // what several items show together is read as one frame shows it.
    function watchFrames(read) {
        return watchSignal(window.afterAnimating, read);
    }

    // Every value a property is set to from here on, drawn in a frame or not.
    // A movement eases by setting values between its ends, and a test that
    // polls for them sees only those it gets a turn between: on a loaded
    // machine one turn can outlast a whole 120 ms slide, and the poll saw
    // only its two ends (#507). Watch before the input that starts the
    // movement, because the input can run a whole turn while it is delivered.
    function watchChanges(item, name) {
        return watchSignal(item[name + "Changed"], function () {
            return item[name];
        });
    }

    // What `read` returns each time `signal` is emitted from here on, and the
    // time it was heard.
    function watchSignal(signal, read) {
        const watch = {
            "signal": signal,
            "read": read,
            "seen": [],
            "seenAt": [],
            "since": Date.now()
        };
        watch.heard = function () {
            watch.seen.push(read());
            watch.seenAt.push(Date.now());
        };
        signal.connect(watch.heard);
        frameWatches.push(watch);
        return watch;
    }

    function stopWatching(watch) {
        const at = frameWatches.indexOf(watch);
        if (at < 0)
            return;
        frameWatches.splice(at, 1);
        watch.signal.disconnect(watch.heard);
    }

    // Whether a property watched by `watchChanges` eased from `from` to `to`:
    // a movement that eased is set to values strictly between the two, one
    // that settled at once is set straight to `to`. It waits for such a value,
    // or for the property to have rested at `to` for 250 ms, long after a
    // movement that was going to start has started. It returns at the first
    // value between, so a caller that reads where the movement ended waits
    // for it.
    //
    // A machine starved of frames can advance a whole movement in one step,
    // and the property is then set only to its end. Such a movement is still
    // told from one that settled at once by when it ended. Qt credits an
    // animation that starts with at most 50 ms from before it started, so a
    // movement of 120 ms, the shortest this is used on, ends no sooner than
    // 70 ms after the input, while one that settled at once ends while the
    // input is delivered. The key-path checks beside each use of this are
    // what catch a movement that settles at once.
    function passedBetween(watch, from, to) {
        const low = Math.min(from, to);
        const high = Math.max(from, to);
        const margin = Math.min(0.5, (high - low) / 20);
        const between = function (at) {
            return at > low + margin && at < high - margin;
        };
        let restingSince = -1;
        tryVerify(function () {
            if (watch.seen.some(between))
                return true;
            if (Math.abs(watch.read() - to) > margin) {
                restingSince = -1;
                return false;
            }
            if (restingSince < 0)
                restingSince = Date.now();
            return Date.now() - restingSince >= 250;
        }, 5000);
        stopWatching(watch);
        if (watch.seen.some(between))
            return true;
        const ended = watch.seen.findIndex(function (at) {
            return Math.abs(at - to) <= margin;
        });
        return ended >= 0 && watch.seenAt[ended] - watch.since >= 60;
    }

    // A row whose place in the list has stopped moving. The outline fills in
    // behind the model, so a row read too early is read where it is not going
    // to be.
    function settleRow(row) {
        let previous = -1;
        let steady = 0;
        tryVerify(function () {
            const at = row.mapToItem(window.contentItem, 0, 0).y;
            steady = at === previous ? steady + 1 : 0;
            previous = at;
            return steady >= 3;
        });
    }

    // A hand moving, rather than one jump. The events are sent in the window's
    // own coordinates because the row is about to leave the list and follow the
    // pointer: measured against the row itself, every step would be measured
    // from somewhere the row has already moved to.
    function dragRowBy(from, distance) {
        const steps = 6;
        for (let step = 1; step <= steps; ++step) {
            mouseMove(window.contentItem, from.x, from.y + distance * step / steps);
            wait(1);
        }
    }

    // The speaker press as the sidebar reports it, so the test exercises the
    // shell's decision about what a press means rather than reaching past it.
    function sidebar_tabMuteToggled(tabId) {
        const outline = findChild(window.contentItem, "sidebar");
        verify(outline !== null);
        outline.tabMuteToggled(tabId);
    }

    function verifyApplicationWindowFlags(applicationWindow) {
        verify(Boolean(applicationWindow.flags & Qt.Window));
        if (Qt.platform.os === "osx") {
            verify(!Boolean(applicationWindow.flags & Qt.FramelessWindowHint));
            verify(Boolean(applicationWindow.flags & Qt.ExpandedClientAreaHint));
            verify(Boolean(applicationWindow.flags & Qt.NoTitleBarBackgroundHint));
            compare(applicationWindow.topPadding, 0);
        } else {
            verify(Boolean(applicationWindow.flags & Qt.FramelessWindowHint));
        }
    }

    // What a test that failed part-way would have left: whatever came before,
    // the next test starts from a settled window.
    function test_aTestThatStopsPartWayLeavesTheNextOneASettledWindow() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const settings = findChild(window.contentItem, "settingsSurface");
        const dialog = findChild(window.contentItem, "spaceDialog");
        const sidebar = findChild(window.contentItem, "sidebar");
        const history = findChild(window.contentItem, "historySurface");
        openPage("https://left-open.example/");
        const besideTabId = browser.activeTabId;
        openPage("https://left-beside.example/");
        const openTabId = browser.activeTabId;
        verify(browser.addSplit(besideTabId));
        tryVerify(function () {
            return browser.splitOnShow && engineHost.besideEngine !== null;
        });
        window.sidebarCollapsed = true;
        window.setFloatingControls(false);
        window.setSidebarWidth(window.sidebarMinimumWidth);
        window.openTabMenu(besideTabId, 0, 0);
        window.requestSettings();
        window.dialogMode = "new";
        tryCompare(sidebar, "visible", false);
        tryCompare(settings, "visible", true);
        verify(dialog.visible);
        const leftWatching = watchChanges(sidebar, "x");

        init();

        verify(!settings.visible);
        verify(!dialog.visible);
        verify(!window.tabMenuOpen);
        verify(!browser.splitOnShow);
        compare(engineHost.besideEngine, null);
        verify(sidebar.visible);
        compare(sidebar.x, 0);
        compare(window.sidebarWidth, window.sidebarDefaultWidth);
        verify(window.floatingControls);
        // The watch a failed test did not stop hears none of the frames after.
        const heard = leftWatching.seen.length;
        window.sidebarCollapsed = true;
        tryCompare(sidebar, "visible", false);
        compare(leftWatching.seen.length, heard);
        // A sidebar left sliding back in has arrived when the next test starts.
        window.sidebarCollapsed = false;
        init();
        verify(sidebar.visible);
        compare(Math.round(sidebar.x), 0);

        window.historyOpen = true;
        tryCompare(history, "visible", true);

        init();

        verify(!history.visible);
        // Its tabs go with it, so a test after it still finds its own among
        // the first nine, the ones with keys.
        browser.closeTab(openTabId);
        browser.closeTab(besideTabId);
    }

    // Omaweb draws the page's context menu, so it offers what Omaweb can do with
    // what was under the pointer — and the engine's own menu never appears.
    function test_pageContextMenuOffersWhatWasUnderThePointer() {
        const engine = openPage("https://context.example/page");
        const menu = findChild(window.contentItem, "pageMenu");
        verify(menu !== null);
        verify(!menu.visible);

        function labels() {
            return window.pageMenuActions.map(function (row) {
                return row.separator === true ? "—" : row.label;
            });
        }

        // Bare page: navigation, the address, and the inspector.
        engine.simulateContextMenu({});
        tryVerify(function () {
            return menu.visible;
        });
        compare(labels(), ["Back", "Forward", "Reload", "Retry over insecure HTTP", "—",
                           "Copy address", "—", "Inspect element"]);
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !menu.visible;
        });

        // A link offers what you do with a link, first.
        engine.simulateContextMenu({
                                       "linkUrl": "https://linked.example/target"
                                   });
        tryVerify(function () {
            return menu.visible;
        });
        compare(labels().slice(0, 5), ["Open link in new tab", "Open link in background",
                                       "Copy link address", "Save link as", "—"]);

        // And running a row does that thing to that link.
        SystemClipboard.copyText("stale");
        window.runPageMenu(2);
        compare(SystemClipboard.text(), "https://linked.example/target");
        tryVerify(function () {
            return !menu.visible;
        });

        // A selection offers copying it; an image offers its own address.
        engine.simulateContextMenu({
                                       "selectedText": "chosen words",
                                       "mediaUrl": "https://linked.example/cat.png",
                                       "mediaType": "image"
                                   });
        tryVerify(function () {
            return menu.visible;
        });
        compare(labels().slice(0, 5), ["Open image in new tab", "Copy image address", "Copy image",
                                       "Save image as", "—"]);
        window.runPageMenu(5);
        compare(SystemClipboard.text(), "chosen words");

        // Opening a link in a background tab leaves the reader where they were.
        const before = browser.activeTabId;
        engine.simulateContextMenu({
                                       "linkUrl": "https://background.example/"
                                   });
        tryVerify(function () {
            return menu.visible;
        });
        window.runPageMenu(1);
        compare(browser.activeTabId, before);
        tryVerify(function () {
            for (let row = 0; row < browser.tabs.rowCount(); ++row) {
                const index = browser.tabs.index(row, 0);
                if (String(browser.tabs.data(index, Qt.UserRole + 3))
                        === "https://background.example/")
                    return true;
            }
            return false;
        });
    }

    // A row that cannot be run is listed and passed over rather than hidden:
    // the reader learns the command exists, and the keyboard never lands on it.
    function test_pageContextMenuSkipsRowsItCannotRun() {
        const engine = openPage("https://skips.example/");
        const menu = findChild(window.contentItem, "pageMenu");
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });

        engine.simulateContextMenu({});
        tryVerify(function () {
            return menu.visible;
        });
        // Back and Forward have nowhere to go on a tab with one page, so the
        // first row the keyboard can reach is Reload.
        compare(window.pageMenuActions[0].enabled, false);
        compare(window.pageMenuActions[1].enabled, false);
        compare(menu.selected, 2);

        // Stepping never stops on a separator.
        menu.step(1);
        compare(window.pageMenuActions[menu.selected].separator, undefined);
        menu.step(1);
        compare(window.pageMenuActions[menu.selected].separator, undefined);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !menu.visible;
        });
    }

    function test_pageContextMenuOpensFromKeyboardAndCommandScope() {
        const engine = openPage("https://keyboard-context.example/");
        const menu = findChild(window.contentItem, "pageMenu");
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });

        keyClick(Qt.Key_F10, Qt.ShiftModifier);
        tryVerify(function () {
            return menu.visible;
        });
        compare(engine.contextMenuRequestCount, 1);
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !menu.visible;
        });

        verify(window.commands.available("open-page-context-menu"));
        verify(window.commands.run("open-page-context-menu", -1));
        tryVerify(function () {
            return menu.visible;
        });
        compare(engine.contextMenuRequestCount, 2);
    }

    // A menu takes the keyboard one turn after it opens, so it is laid out
    // before it is stepped through. Dismissed inside that gap, it used to take
    // the keyboard anyway — off whatever had replaced it, and while invisible.
    function test_aDismissedMenuDoesNotTakeTheKeyboardBackAfterwards() {
        const engine = openPage("https://dismissed-menu.example/page");
        const menu = findChild(window.contentItem, "pageMenu");
        verify(menu !== null);

        // Opened and dismissed inside one turn, so the deferred grab is still
        // pending when the menu stops being on screen.
        engine.simulateContextMenu({
                                       "linkUrl": "https://dismissed-menu.example/link"
                                   });
        window.pageMenuOpen = false;

        // Long enough for the grab to have run had it not been checked. What
        // holds the keyboard afterwards is the page's business; what must not
        // hold it is a menu nobody can see.
        wait(50);
        verify(!menu.visible);
        verify(!menu.activeFocus);
        verify(window.activeFocusItem !== menu);
    }

    function test_pageContextMenuRejectsAStaleTarget() {
        const engine = openPage("https://stale-target.example/before");
        SystemClipboard.copyText("keep this");
        engine.simulateContextMenu({
                                       "linkUrl": "https://stale-target.example/link"
                                   });
        tryVerify(function () {
            return window.pageMenuOpen;
        });

        engine.currentUrl = "https://stale-target.example/after";
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://stale-target.example/after";
        });
        window.runPageMenu(2);
        compare(SystemClipboard.text(), "keep this");
        verify(!window.pageMenuOpen);
    }

    function test_saveDialogRejectsATargetThatBecameStaleWhileOpen() {
        const engine = openPage("https://stale-save.example/before");
        window.recordPendingSave(engine, "save-link");
        engine.currentUrl = "https://stale-save.example/after";
        window.completeTargetSave("file:///tmp/archive.zip");
        compare(engine.lastContextAction, "");
    }

    function test_javascriptPromptsAreTabModalCancelableAndStoppable() {
        const engine = openPage("https://prompts.example/page");
        const bar = findChild(window.contentItem, "browserPromptBar");
        verify(bar !== null);

        engine.simulateJavaScriptPrompt("prompt", "https://prompts.example",
                                        "What should this page use?", "suggested");
        tryVerify(function () {
            return bar.visible;
        });
        compare(window.pendingBrowserPrompt.kind, "javascript-prompt");
        compare(window.pendingBrowserPrompt.origin, "https://prompts.example");
        compare(window.pendingBrowserPrompt.defaultText, "suggested");

        window.respondToBrowserPrompt(false, "", "", "", true, false);
        compare(engine.lastPromptAccepted, false);
        verify(engine.javaScriptDialogsBlocked);
        verify(!bar.visible);

        engine.simulateJavaScriptPrompt("confirm", "https://prompts.example", "This must not open",
                                        "");
        wait(20);
        verify(!bar.visible);
    }

    // A bar over the page is translucent, so the page under its ground is
    // blurred rather than read through it. The blur covers the ground alone and
    // stops sampling once the bar closes.
    function test_thePageUnderAPageBarIsBlurred() {
        const engine = openPage("https://blurred.example/");
        const engineHost = findChild(window.contentItem, "engineLoader");

        const promptBar = findChild(window.contentItem, "browserPromptBar");
        const promptBackdrop = findChild(promptBar, "pageBarBackdrop");
        verify(promptBackdrop !== null);
        verify(!promptBackdrop.sampling);
        engine.simulateJavaScriptPrompt("confirm", "https://blurred.example", "Leave the page?",
                                        "");
        tryCompare(window, "browserPromptOpen", true);
        compare(promptBackdrop.source, engineHost);
        verify(promptBackdrop.sampling);
        const promptGround = findChild(promptBar, "pagePromptGround");
        compare(promptBackdrop.y, promptGround.y);
        compare(promptBackdrop.width, promptGround.width);
        compare(promptBackdrop.height, promptGround.height);
        verify(promptBackdrop.height < promptBar.height);
        window.respondToBrowserPrompt(false, "", "", "", false, false);
        verify(!promptBackdrop.sampling);

        const questionBar = findChild(window.contentItem, "sitePermissionBar");
        const questionBackdrop = findChild(questionBar, "pageBarBackdrop");
        verify(questionBackdrop !== null);
        verify(engine.simulateSitePermission("https://blurred.example", "notifications").length
               > 0);
        tryCompare(window, "permissionOpen", true);
        compare(questionBackdrop.source, engineHost);
        verify(questionBackdrop.sampling);
        compare(questionBackdrop.width, questionBar.width);
        compare(questionBackdrop.height, questionBar.height);
        window.respondToPermission(BrowserController.Block);
        verify(!questionBackdrop.sampling);

        // Every bar over the page, not only the two opened here, blurs the
        // page on show.
        const backdrops = [];
        const collect = function (item) {
            if (item.objectName === "pageBarBackdrop")
                backdrops.push(item);
            for (let index = 0; index < item.children.length; ++index)
                collect(item.children[index]);
        };
        collect(window.contentItem);
        verify(backdrops.length >= 6);
        for (let index = 0; index < backdrops.length; ++index)
            compare(backdrops[index].parent.backdropSource, engineHost);

        // Beside a page, a blank tab's Start page fills its own pane alone.
        // The page host holds both panes, so it is what the bars blur.
        const pageTabId = browser.activeTabId;
        verify(browser.addSplit(""));
        tryCompare(browser, "splitOnShow", true);
        tryCompare(window, "startPageShown", true);
        compare(window.pageBarBackdropSource, engineHost);
        verify(browser.separateSplit());
        browser.closeActiveTab();
        browser.activateTab(pageTabId);
    }

    function test_pagePromptDoesNotFollowTheReaderToAnotherTab() {
        const engine = openPage("https://prompt-tab.example/");
        const promptTabId = browser.activeTabId;
        engine.simulateJavaScriptPrompt("confirm", "https://prompt-tab.example", "Stay on this tab?",
                                        "");
        tryVerify(function () {
            return window.browserPromptOpen;
        });

        browser.openInput("https://another-tab.example/", true);
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://another-tab.example/";
        });
        verify(!window.browserPromptOpen);
        verify(!engine.lastPromptAccepted);

        browser.activateTab(promptTabId);
        tryVerify(function () {
            return window.browserPromptOpen;
        });
        window.respondToBrowserPrompt(false, "", "", "", false, false);
        verify(!window.browserPromptOpen);
    }

    function test_httpAuthenticationCredentialsStayInTheLiveEngine() {
        const engine = openPage("https://auth.example/private");
        engine.simulateHttpAuthentication("https://auth.example", "Members");
        tryVerify(function () {
            return window.browserPromptOpen;
        });
        compare(window.pendingBrowserPrompt.kind, "http-authentication");
        compare(window.pendingBrowserPrompt.detail, "Members");

        window.respondToBrowserPrompt(true, "", "reader", "secret", false, false);
        compare(engine.lastPromptResponse.user, "reader");
        compare(engine.lastPromptResponse.password, "secret");
        verify(!window.browserPromptOpen);
        compare(browser.preference("http-authentication", "missing"), "missing");
    }

    // The bar is hidden rather than destroyed between prompts, so a credential
    // left in a field outlives the question it answered.
    function test_authenticationPromptKeepsNoCredentialAfterItCloses() {
        const engine = openPage("https://leftover.example/private");
        const bar = findChild(window.contentItem, "browserPromptBar");
        const user = findChild(bar, "browserPromptUser");
        const password = findChild(bar, "browserPromptPassword");
        verify(user !== null);
        verify(password !== null);

        engine.simulateHttpAuthentication("https://leftover.example", "Members");
        tryVerify(function () {
            return bar.visible;
        });
        user.text = "reader";
        password.text = "secret";
        bar.submit(true);
        compare(engine.lastPromptResponse.user, "reader");
        compare(engine.lastPromptResponse.password, "secret");
        compare(user.text, "");
        compare(password.text, "");

        engine.simulateHttpAuthentication("https://leftover.example", "Members");
        tryVerify(function () {
            return bar.visible;
        });
        compare(user.text, "");
        compare(password.text, "");
        user.text = "reader";
        password.text = "secret";
        bar.submit(false);
        compare(engine.lastPromptAccepted, false);
        compare(user.text, "");
        compare(password.text, "");

        engine.simulateHttpAuthentication("https://leftover.example", "Members");
        tryVerify(function () {
            return bar.visible;
        });
        password.text = "secret";
        window.refuseRequestsFrom(engine);
        verify(!window.browserPromptOpen);
        compare(password.text, "");
    }

    // A question bar's button, by the label the reader reads on it.
    function questionAction(bar, label) {
        for (let index = 0; index < bar.actions.length; ++index) {
            if (bar.actions[index].label === label)
                return findChild(bar, "questionAction" + index);
        }
        return null;
    }

    // A security key's request is one prompt that changes as the engine moves
    // through it, and it names who is asking and in which Space.
    function test_aSecurityKeyTouchNamesTheSiteAndSpaceAndCancelDeclines() {
        const engine = openPage("https://key.example/sign-in");
        const bar = findChild(window.contentItem, "securityKeyBar");
        verify(bar !== null);
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        compare(bar.message, "Touch your security key");
        compare(bar.detail, "https://key.example · " + browser.activeSpaceName);

        mouseClick(questionAction(bar, "Cancel"));
        compare(engine.lastSecurityKeyAnswer.action, "cancel");
        verify(!bar.visible);
    }

    // The PIN is typed into a masked field and Enter sends it. A wrong one
    // comes back as the same step, which says so in the same bar with the
    // attempts the key has left, and the field starts empty again.
    function test_aSecurityKeyPinIsMaskedAndAWrongOneIsSaidInPlace() {
        const engine = openPage("https://pin.example/sign-in");
        const bar = findChild(window.contentItem, "securityKeyBar");
        const field = findChild(bar, "securityKeyPin");
        engine.simulateSecurityKey({
                                       "state": "pin",
                                       "pin": {
                                           "purpose": "unlock",
                                           "error": "",
                                           "attemptsLeft": -1,
                                           "minimumLength": 4
                                       }
                                   });
        tryVerify(function () {
            return bar.visible && field.activeFocus;
        });
        compare(bar.message, "Enter your security key's PIN");
        compare(field.echoMode, TextInput.Password);
        compare(bar.note, "");

        keyClick(Qt.Key_1);
        keyClick(Qt.Key_2);
        keyClick(Qt.Key_3);
        keyClick(Qt.Key_4);
        keyClick(Qt.Key_Return);
        compare(engine.lastSecurityKeyAnswer.action, "pin");
        compare(engine.lastSecurityKeyAnswer.pin, "1234");
        verify(bar.visible);

        engine.simulateSecurityKey({
                                       "state": "pin",
                                       "pin": {
                                           "purpose": "unlock",
                                           "error": "wrong",
                                           "attemptsLeft": 7,
                                           "minimumLength": 4
                                       }
                                   });
        tryCompare(bar, "note", "Wrong PIN. 7 attempts left.");
        compare(field.text, "");
        verify(field.activeFocus);

        engine.simulateSecurityKey({
                                       "state": "pin",
                                       "pin": {
                                           "purpose": "unlock",
                                           "error": "wrong",
                                           "attemptsLeft": 1,
                                           "minimumLength": 4
                                       }
                                   });
        tryCompare(bar, "note", "Wrong PIN. 1 attempt left.");

        keyClick(Qt.Key_Escape);
        compare(engine.lastSecurityKeyAnswer.action, "cancel");
        verify(!bar.visible);
    }

    // A key holding several accounts for the site lists them, one row each.
    // The arrows move between rows and Enter chooses the one on.
    function test_aSecurityKeyAccountIsChosenFromTheKeyboard() {
        const engine = openPage("https://accounts.example/sign-in");
        const bar = findChild(window.contentItem, "securityKeyBar");
        engine.simulateSecurityKey({
                                       "state": "accounts",
                                       "accounts": [
                                           {
                                               "name": "reader@accounts.example"
                                           },
                                           {
                                               "name": "admin@accounts.example"
                                           }
                                       ]
                                   });
        tryVerify(function () {
            return bar.visible && bar.activeFocus;
        });
        compare(bar.message, "Choose an account");
        const first = findChild(bar, "securityKeyAccount0");
        const second = findChild(bar, "securityKeyAccount1");
        verify(first !== null && second !== null);
        compare(first.name, "reader@accounts.example");
        compare(second.name, "admin@accounts.example");
        verify(first.current);

        keyClick(Qt.Key_Down);
        verify(second.current);
        verify(!first.current);
        keyClick(Qt.Key_Return);
        compare(engine.lastSecurityKeyAnswer.action, "account");
        compare(engine.lastSecurityKeyAnswer.name, "admin@accounts.example");

        engine.simulateSecurityKey({
                                       "state": "closed"
                                   });
        verify(!bar.visible);
    }

    // A request that failed says why in one line, and Close is the only
    // answer left: it declines, so the site hears the reader gave up.
    function test_aSecurityKeyFailureNamesItsCauseAndCloses_data() {
        return [
                    {
                        "tag": "no key",
                        "failure": "no-key",
                        "message": "This security key has no sign-in for this site"
                    },
                    {
                        "tag": "already registered",
                        "failure": "already-registered",
                        "message": "This security key is already registered with this site"
                    },
                    {
                        "tag": "too many attempts",
                        "failure": "too-many-attempts",
                        "message": "Too many wrong PINs. Remove the key and insert it again."
                    },
                    {
                        "tag": "locked",
                        "failure": "locked",
                        "message": "Too many wrong PINs. The key is locked until it is reset."
                    },
                    {
                        "tag": "timed out",
                        "failure": "timed-out",
                        "message": "The security key was not used in time"
                    },
                    {
                        "tag": "not supported",
                        "failure": "not-supported",
                        "message": "This security key cannot do what the site asks"
                    },
                    {
                        "tag": "key removed",
                        "failure": "key-removed",
                        "message": "The security key was removed"
                    },
                    {
                        "tag": "key full",
                        "failure": "key-full",
                        "message": "This security key has no room for another sign-in"
                    },
                    {
                        "tag": "unknown",
                        "failure": "something-new",
                        "message": "The security key could not be used"
                    }
                ];
    }

    function test_aSecurityKeyFailureNamesItsCauseAndCloses(data) {
        const engine = openPage("https://fails.example/sign-in");
        const bar = findChild(window.contentItem, "securityKeyBar");
        engine.simulateSecurityKey({
                                       "state": "failed",
                                       "failure": data.failure
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        compare(bar.message, data.message);
        compare(bar.detail, "https://fails.example · " + browser.activeSpaceName);
        compare(bar.actions.length, 1);
        compare(bar.actions[0].label, "Close");

        mouseClick(questionAction(bar, "Close"));
        compare(engine.lastSecurityKeyAnswer.action, "cancel");
        verify(!bar.visible);
    }

    // A reader whose passkey is on their phone is told this engine cannot
    // reach it, rather than left waiting on a key that will never answer.
    function test_aSecurityKeyPromptSaysWhichAuthenticatorsTheEngineCannotReach() {
        const engine = openPage("https://reach.example/sign-in");
        const bar = findChild(window.contentItem, "securityKeyBar");
        compare(engine.securityKeyTransports, ["usb"]);
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        compare(bar.note,
                "Only a USB security key works here. A phone, or a passkey stored on this computer, cannot be used.");
        engine.simulateSecurityKey({
                                       "state": "failed",
                                       "failure": "timed-out"
                                   });
        compare(bar.note,
                "Only a USB security key works here. A phone, or a passkey stored on this computer, cannot be used.");
        keyClick(Qt.Key_Escape);
        verify(!bar.visible);

        engine.securityKeyTransports = ["usb", "platform"];
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        compare(bar.note, "A phone cannot be used here.");
        keyClick(Qt.Key_Escape);

        engine.securityKeyTransports = ["usb", "hybrid", "platform"];
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        compare(bar.note, "");
        keyClick(Qt.Key_Escape);
        engine.securityKeyTransports = ["usb"];
        verify(!bar.visible);
    }

    // A touch answers whatever the key was asked, so it is only ever asked for
    // the page in front of the reader. Leaving the tab declines the request,
    // and a page nobody is looking at is declined without a prompt.
    function test_aSecurityKeyRequestEndsWhenItsPageLeavesTheReader() {
        const engine = openPage("https://leaves.example/sign-in");
        const tabId = browser.activeTabId;
        const bar = findChild(window.contentItem, "securityKeyBar");
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        engine.lastSecurityKeyAnswer = ({});
        browser.openInput("https://elsewhere.example/", true);
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://elsewhere.example/";
        });
        verify(!bar.visible);
        compare(engine.lastSecurityKeyAnswer.action, "cancel");

        engine.lastSecurityKeyAnswer = ({});
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        verify(!bar.visible);
        compare(engine.lastSecurityKeyAnswer.action, "cancel");

        browser.activateTab(tabId);
        tryVerify(function () {
            return findChild(window.contentItem, "engineLoader").item === engine;
        });
        verify(!bar.visible);
        engine.simulateSecurityKey({
                                       "state": "touch"
                                   });
        tryVerify(function () {
            return bar.visible;
        });
        engine.lastSecurityKeyAnswer = ({});
        window.refuseRequestsFrom(engine);
        verify(!bar.visible);
        compare(engine.lastSecurityKeyAnswer.action, "cancel");
    }

    // A Private window gets the same prompts, and names itself where a Space
    // would be named.
    function test_aPrivateWindowAsksForASecurityKeyTheSameWay() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://private-key.example/sign-in", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        const bar = findChild(privateBrowser.contentItem, "securityKeyBar");
        privateEngine.item.simulateSecurityKey({
                                                   "state": "touch"
                                               });
        tryVerify(function () {
            return bar.visible;
        });
        compare(bar.message, "Touch your security key");
        compare(bar.detail, "https://private-key.example · Private window");
        mouseClick(questionAction(bar, "Cancel"));
        compare(privateEngine.item.lastSecurityKeyAnswer.action, "cancel");
        verify(!bar.visible);

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
    }

    // An Auxiliary window is where a sign-in finishes, so a key is often asked
    // for there. It asks in the window that opened it, like a permission, and a
    // tab switch there does not take the question from the window it is for.
    function test_anAuxiliaryWindowAsksForASecurityKeyInItsOpener() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        openPage("https://key-opener.example/start");
        engineLoader.item.simulateNewWindowRequest("https://key-popup.example/sign-in", true);
        const auxiliary = findChild(window, "auxiliaryWindow");
        tryVerify(function () {
            return auxiliary !== null && auxiliary.visible;
        });
        const auxiliaryEngine = findChild(auxiliary.contentItem, "auxiliaryEngineLoader");
        tryVerify(function () {
            return auxiliaryEngine.item !== null;
        });
        const bar = findChild(window.contentItem, "securityKeyBar");
        auxiliaryEngine.item.simulateSecurityKey({
                                                     "state": "touch"
                                                 });
        tryVerify(function () {
            return bar.visible;
        });
        browser.openInput("https://other-tab.example/", true);
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://other-tab.example/";
        });
        verify(bar.visible);
        mouseClick(questionAction(bar, "Cancel"));
        compare(auxiliaryEngine.item.lastSecurityKeyAnswer.action, "cancel");
        verify(!bar.visible);

        // The window closing mid-request takes its question with it.
        const closing = auxiliaryEngine.item;
        closing.simulateSecurityKey({
                                        "state": "touch"
                                    });
        tryVerify(function () {
            return bar.visible;
        });
        closing.lastSecurityKeyAnswer = ({});
        closing.simulateWindowCloseRequest();
        tryVerify(function () {
            return !auxiliary.visible;
        });
        verify(!bar.visible);
        window.requestActivate();
    }

    // The routes out that never reach the Sign in or Cancel button: the key
    // that dismisses the bar, and the page the question belonged to being
    // replaced under it.
    function test_authenticationPromptKeepsNoCredentialWhenItIsDroppedUnanswered() {
        const engine = openPage("https://dropped.example/private");
        const bar = findChild(window.contentItem, "browserPromptBar");
        const password = findChild(bar, "browserPromptPassword");

        engine.simulateHttpAuthentication("https://dropped.example", "Members");
        tryVerify(function () {
            return bar.visible && bar.activeFocus;
        });
        password.text = "secret";
        keyClick(Qt.Key_Escape);
        verify(!window.browserPromptOpen);
        compare(password.text, "");

        engine.simulateHttpAuthentication("https://dropped.example", "Members");
        tryVerify(function () {
            return bar.visible;
        });
        password.text = "secret";
        engine.currentUrl = "https://dropped.example/elsewhere";
        window.presentBrowserPromptForActiveTab();
        verify(!window.browserPromptOpen);
        compare(password.text, "");
    }

    // A second tab asking while the first is still asking swaps the prompt
    // under a bar that never closes.
    function test_authenticationPromptKeepsNoCredentialAcrossTabs() {
        const first = openPage("https://tab-one.example/private");
        const firstTabId = browser.activeTabId;
        const bar = findChild(window.contentItem, "browserPromptBar");
        const password = findChild(bar, "browserPromptPassword");
        first.simulateHttpAuthentication("https://tab-one.example", "Members");
        tryVerify(function () {
            return bar.visible;
        });
        password.text = "first secret";

        const second = openPageInNewTab("https://tab-two.example/private");
        second.simulateHttpAuthentication("https://tab-two.example", "Members");
        tryVerify(function () {
            return window.browserPromptOpen && window.pendingBrowserPrompt.origin
                    === "https://tab-two.example";
        });
        compare(password.text, "");

        password.text = "second secret";
        browser.activateTab(firstTabId);
        tryVerify(function () {
            return window.browserPromptOpen && window.pendingBrowserPrompt.origin
                    === "https://tab-one.example";
        });
        compare(password.text, "");
    }

    function test_externalProtocolConfirmationNamesDestinationAndCanBeRemembered() {
        const engine = openPage("https://calendar.example/event");
        const destination = "webcal://calendar.example/team?id=42";
        engine.simulateExternalProtocol("Calendar", destination);
        tryVerify(function () {
            return window.browserPromptOpen;
        });
        compare(window.pendingBrowserPrompt.kind, "external-protocol");
        compare(window.pendingBrowserPrompt.application, "Calendar");
        compare(window.pendingBrowserPrompt.scheme, "webcal");
        compare(window.pendingBrowserPrompt.origin, "https://calendar.example");
        compare(window.pendingBrowserPrompt.destination, destination);

        window.respondToBrowserPrompt(true, "", "", "", false, true);
        compare(engine.externalOpenCount, 1);
        verify(browser.externalProtocolAllowed("https://calendar.example/elsewhere", "webcal"));

        engine.simulateExternalProtocol("Calendar", "webcal://calendar.example/next");
        compare(engine.externalOpenCount, 2);
        verify(!window.browserPromptOpen);
    }

    function test_targetActionsUseNativeSaveAndFileSelectionBoundaries() {
        const engine = openPage("https://files.example/form");
        const nativeOpen = findChild(window, "openFileDialog");
        const nativeSelection = findChild(window, "pageFileDialog");
        const nativeSave = findChild(window, "saveTargetDialog");
        verify(nativeOpen !== null);
        verify(nativeSelection !== null);
        verify(nativeSave !== null);
        verify(window.commands.available("open-file"));

        engine.simulateContextMenu({
                                       "linkUrl": "https://files.example/archive.zip",
                                       "mediaUrl": "https://files.example/photo.png",
                                       "mediaType": "image"
                                   });
        tryVerify(function () {
            return window.pageMenuOpen;
        });
        const labels = window.pageMenuActions.map(function (row) {
            return row.label || "";
        });
        verify(labels.indexOf("Save link as") >= 0);
        verify(labels.indexOf("Copy image") >= 0);
        verify(labels.indexOf("Save image as") >= 0);

        engine.simulateFileSelection("open-multiple", ["image/png"]);
        compare(window.pendingFileSelection.mode, "open-multiple");
        window.respondToFileSelection(["/tmp/one.png", "/tmp/two.png"]);
        compare(engine.lastSelectedFiles.length, 2);
        engine.simulateFileSelection("open", ["text/plain"]);
        window.respondToFileSelection([]);
        verify(engine.fileSelectionCancelled);
    }

    // The address of the page on show goes to the clipboard on its own: no
    // title, no markup, and nothing at all from a tab that has no address.
    function test_copyAddressPutsOnlyTheAddressOnTheClipboard() {
        SystemClipboard.copyText("something the reader already had");
        browser.openInput("about:blank", true);
        tryVerify(function () {
            return browser.activeTabBlank;
        });
        verify(!window.commands.available("copy-address"));
        window.commands.run("copy-address", -1);
        compare(SystemClipboard.text(), "something the reader already had");
        browser.closeActiveTab();

        openPage("https://copy-me.example/path?q=1");
        verify(window.commands.available("copy-address"));
        window.commands.run("copy-address", -1);
        compare(SystemClipboard.text(), "https://copy-me.example/path?q=1");
        compare(window.commands.keymap.keysFor("copy-address"), "Ctrl+Shift+C");
    }

    // An engine that supplies no inspector leaves the command listed and
    // unavailable, and takes an open dock with it.
    function test_developerToolsAreUnavailableWithoutAnInspector() {
        const dock = findChild(window.contentItem, "developerToolsDock");
        const engine = openPage("https://no-inspector.example");
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });

        engine.inspectorAvailable = false;
        tryVerify(function () {
            return !window.developerToolsAvailable;
        });
        browser.closeDeveloperTools();
        tryVerify(function () {
            return !dock.visible;
        });

        // Neither command can be run, and neither promises a key.
        window.commands.run("developer-tools", -1);
        window.commands.run("inspect-element", -1);
        compare(browser.developerToolsTabId, "");
        compare(engine.inspectedElementCount, 0);
        verify(!dock.visible);
        const shortcutSheet = findChild(window.contentItem, "shortcutSheet");
        compare(shortcutSheet.sections.filter(function (section) {
            return section.group === "developer";
        }).length, 0);

        engine.inspectorAvailable = true;
        tryVerify(function () {
            return window.developerToolsAvailable;
        });
    }

    // A Space at rest is running no engine at all, so there is nothing to
    // inspect there either.
    function test_developerToolsAreUnavailableWithoutAnEngine() {
        const dock = findChild(window.contentItem, "developerToolsDock");
        const engineHost = findChild(window.contentItem, "engineLoader");
        // A blank tab is given no engine, so it is the one tab in any Space
        // that has nothing to inspect.
        browser.openInput("about:blank", true);
        tryVerify(function () {
            return engineHost.item === null;
        });
        verify(window.pagelessViewport);
        verify(!window.developerToolsAvailable);

        const listed = window.commands.actions().filter(function (action) {
            return action.command === "developer-tools" || action.command === "inspect-element";
        });
        compare(listed.length, 2);
        for (let index = 0; index < listed.length; ++index) {
            verify(!listed[index].enabled);
            window.commands.invoke(listed[index]);
        }
        compare(browser.developerToolsTabId, "");
        verify(!dock.visible);

        // The sheet of keys promises nothing it cannot carry out either, so the
        // registry is the only place that decides.
        const shortcutSheet = findChild(window.contentItem, "shortcutSheet");
        verify(shortcutSheet !== null);
        const developerSections = shortcutSheet.sections.filter(function (section) {
            return section.group === "developer";
        });
        compare(developerSections.length, 0);

        browser.closeActiveTab();
    }

    // The bar a webpage scrolls in is the chrome's, not the engine's. The
    // engine hides the one it would have drawn for the document's own scroller
    // and reports where the page stands; the shell draws that in the same
    // component the sidebar and Settings scroll in.
    //
    // It is parented into the engine rather than placed beside it, which is
    // what carries it into a pane of a split, a Glance, or a window of its own
    // without any of those having to know the bar exists.
    function test_theChromeDrawsTheBarAWebpageScrollsIn() {
        const engine = openPage("https://scrolling.example");
        const bar = findChild(engine, "pageScrollBar");
        verify(bar !== null);
        compare(bar.parent, engine);

        // A page that fits shows nothing at all.
        engine.pageScrollLength = 400;
        engine.pageViewportLength = 400;
        engine.pageScrollOffset = 0;
        verify(!bar.visible);

        // A page four times its viewport gives a thumb a quarter of the track,
        // standing at the top.
        engine.pageScrollLength = 4000;
        engine.pageViewportLength = 1000;
        verify(bar.visible);
        fuzzyCompare(bar.size, 0.25, 0.001);
        fuzzyCompare(bar.position, 0, 0.001);

        // Halfway down its travel puts the thumb halfway down the track it has
        // left to run in, not halfway down the track.
        engine.pageScrollOffset = 1500;
        fuzzyCompare(bar.position, 0.375, 0.001);

        // The end of the page puts the thumb's far edge at the end of the bar.
        engine.pageScrollOffset = 3000;
        fuzzyCompare(bar.position + bar.size, 1, 0.001);
    }

    // The page is driven only while the reader is driving the bar. `position`
    // follows what the page reported, so a bar that asked the page to scroll
    // every time it moved would send the page's own scrolling straight back to
    // it and fight the reader's wheel.
    function test_theBarScrollsThePageOnlyWhileItIsBeingDragged() {
        const engine = openPage("https://dragging.example");
        const bar = findChild(engine, "pageScrollBar");
        verify(bar !== null);

        engine.pageScrollLength = 4000;
        engine.pageViewportLength = 1000;
        engine.pageScrollOffset = 0;

        // The page scrolling itself moves the bar and is not sent back.
        engine.pageScrollOffset = 600;
        fuzzyCompare(bar.position, 0.15, 0.001);
        compare(engine.pageScrollOffset, 600);
    }

    // A sheet or overlay standing over the page takes the wheel, at the end
    // of its own scroll view's travel and over a margin alike. The page is
    // still there beneath it and still answers, and a wheel let through would
    // move it under the sheet that covers it.
    function test_aSheetOverThePageTakesTheWheel() {
        const engine = openPage("https://covered.example");
        settleMotion();
        engine.pageScrollLength = 4000;
        engine.pageViewportLength = 1000;
        engine.pageScrollOffset = 600;

        // Somewhere a sheet's own scroll view stands, and somewhere it does
        // not: the margin beside the Settings rail, the inset above a
        // heading, the Shortcut sheet's gutter.
        const spots = [Qt.point(engine.width / 2, engine.height / 2), Qt.point(12, engine.height
                                                                               / 2), Qt.point(
                           engine.width / 2, 20)];
        const wheelOverThePage = function (sheet) {
            for (let index = 0; index < spots.length; ++index) {
                mouseWheel(engine, spots[index].x, spots[index].y, 0, 120);
                compare(engine.pageScrollOffset, 600, sheet + " let the wheel up through at "
                        + spots[index]);
                mouseWheel(engine, spots[index].x, spots[index].y, 0, -120);
                compare(engine.pageScrollOffset, 600, sheet + " let the wheel down through at "
                        + spots[index]);
            }
        };

        // With nothing over it the page scrolls: the wheel reaches it.
        mouseWheel(engine, spots[0].x, spots[0].y, 0, -120);
        compare(engine.pageScrollOffset, 720);
        mouseWheel(engine, spots[1].x, spots[1].y, 0, 120);
        compare(engine.pageScrollOffset, 600);

        const sheets = [
                  {
                      "name": "Settings",
                      "open": function () {
                          window.settingsOpen = true;
                      },
                      "close": function () {
                          window.settingsOpen = false;
                      }
                  },
                  {
                      "name": "History",
                      "open": function () {
                          window.historyOpen = true;
                      },
                      "close": function () {
                          window.historyOpen = false;
                      }
                  },
                  {
                      "name": "the Shortcut sheet",
                      "open": function () {
                          window.shortcutsOpen = true;
                      },
                      "close": function () {
                          window.shortcutsOpen = false;
                      }
                  },
                  {
                      "name": "the Omnibar",
                      "open": function () {
                          window.openOmnibar(false);
                      },
                      "close": function () {
                          window.closeOmnibar();
                      }
                  },
                  {
                      "name": "the Start page over a page",
                      "open": function () {
                          window.openOmnibar(true);
                      },
                      "close": function () {
                          window.closeOmnibar();
                      }
                  },
                  {
                      "name": "the clear-browsing-data dialog",
                      "open": function () {
                          window.settingsOpen = true;
                          findChild(window.contentItem, "settingsSurface").clearDataOpen = true;
                      },
                      "close": function () {
                          findChild(window.contentItem, "settingsSurface").clearDataOpen = false;
                          window.settingsOpen = false;
                      }
                  },
                  {
                      "name": "the Space dialog",
                      "open": function () {
                          window.dialogMode = "new";
                      },
                      "close": function () {
                          window.dialogMode = "";
                      }
                  }
              ];
        for (let index = 0; index < sheets.length; ++index) {
            sheets[index].open();
            wait(400);
            wheelOverThePage(sheets[index].name);
            sheets[index].close();
            wait(400);
        }

        // The Glance's page takes the wheel over the panel; the scrim beside
        // it takes the wheel for the page beneath.
        const glance = openGlance("https://glanced.example/page");
        const glanced = window.glanceEngine;
        glanced.pageScrollLength = 4000;
        glanced.pageViewportLength = 1000;
        glanced.pageScrollOffset = 0;
        const panel = findChild(glance, "glancePanel");
        mouseWheel(panel, panel.width / 2, panel.height / 2, 0, -120);
        compare(glanced.pageScrollOffset, 120, "the Glance's page did not take the wheel");
        compare(engine.pageScrollOffset, 600);
        mouseWheel(engine, 4, engine.height / 2, 0, -120);
        mouseWheel(engine, 4, engine.height / 2, 0, 120);
        compare(engine.pageScrollOffset, 600, "the Glance let the wheel through");
        window.closeGlance();

        // Once everything has closed the wheel reaches the page again.
        wait(250);
        mouseWheel(engine, spots[0].x, spots[0].y, 0, -120);
        compare(engine.pageScrollOffset, 720);
        engine.pageScrollOffset = 0;
    }

    // Site information stands over the outline as well as the page, and the
    // outline's tab list under it does not scroll by a wheel meant for it.
    function test_siteInformationTakesTheWheelOverTheOutline() {
        openPage("https://reported.example");
        const sidebar = findChild(window.contentItem, "sidebar");
        const panel = siteCard();
        const tabScroll = findChild(sidebar, "tabScroll");
        verify(tabScroll !== null);
        const tabList = tabScroll.contentItem;
        // Enough rows to overflow the list, closed again at the end so the
        // tests after this one find the outline as they would have.
        const opened = [];
        for (let index = 0; index < 40; ++index) {
            browser.openInput("https://reported.example/" + index, true);
            opened.push(browser.activeTabId);
        }
        tryVerify(function () {
            return tabList.contentHeight > tabList.height;
        });
        tabList.contentY = 0;

        window.openSiteInformation("");
        tryVerify(function () {
            return panel.visible;
        });
        wait(250);
        // The card's foot, over the list rather than over the address it
        // unfolded from. With the card away the list scrolls there; with it
        // open, the same wheel stops at the card.
        const foot = panel.mapToItem(tabList, 40, panel.height - 8);
        verify(tabList.contains(foot), "the card does not reach the tab list");
        window.closeSiteInformation();
        wait(250);
        // The list scrolls over the frames that follow rather than at once.
        mouseWheel(tabList, foot.x, foot.y, 0, -120);
        tryVerify(function () {
            return tabList.contentY > 0;
        }, 2000, "the list does not scroll by wheel");
        tryCompare(tabList, "moving", false);
        tabList.contentY = 0;

        window.openSiteInformation("");
        wait(250);
        mouseWheel(tabList, foot.x, foot.y, 0, -120);
        wait(400);
        compare(tabList.contentY, 0);
        window.closeSiteInformation();
        for (let index = 0; index < opened.length; ++index)
            browser.closeTab(opened[index]);
    }

    // Developer tools take a column of their own beside the tab they inspect:
    // the page gives up that width rather than being covered by it, and the
    // view in the dock is the one the engine handed over.
    function test_developerToolsDockBesideTheTabTheyInspect() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const engine = openPage("https://inspected.example");
        const dock = findChild(window.contentItem, "developerToolsDock");
        verify(dock !== null);
        verify(!dock.visible);
        verify(window.developerToolsAvailable);

        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });
        verify(engine.developerToolsAttached);
        compare(browser.developerToolsTabId, browser.activeTabId);
        tryCompare(dock, "width", window.developerToolsWidth);
        // The page stops where the dock starts rather than running under it,
        // and the two of them together are the whole viewport. Measured against
        // the viewport rather than against a remembered width, because the
        // sidebar beside it owns the rest of the row.
        const viewport = findChild(window.contentItem, "engineViewport");
        verify(viewport !== null);
        tryVerify(function () {
            return Math.round(engineHost.width + dock.width) === Math.round(viewport.width);
        });
        compare(Math.round(engineHost.x + engineHost.width), Math.round(dock.x));

        // The dock arrives beside the page rather than in front of it, so the
        // keyboard stays where the reader left it.
        verify(engine.activeFocus);

        // The engine's own view is what the dock shows, at the dock's shape.
        const view = engine.developerToolsView;
        verify(view !== null);
        compare(view.parent, dock);
        tryCompare(view, "width", dock.width);
        // Drawn in the window's colours rather than the engine's own, and
        // actually painted there: the probe is taken off the dock itself
        // rather than off the property that was set on it.
        compare(String(view.color), String(window.colors.windowOpaque));
        const painted = grabImage(dock);
        verify(Qt.colorEqual(painted.pixel(Math.round(dock.width / 2), Math.round(dock.height / 2)),
                             window.colors.windowOpaque));

        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return !dock.visible;
        });
        verify(!engine.developerToolsAttached);
        compare(browser.developerToolsTabId, "");
        verify(engine.activeFocus);
        // The page takes the whole viewport back.
        tryVerify(function () {
            return Math.round(engineHost.width) === Math.round(viewport.width);
        });
    }

    // One inspector, attached to one tab. It hides behind another tab and comes
    // back with its own, and a Space switch does not move or lose it.
    function test_developerToolsFollowTheTabTheyAreAttachedTo() {
        const dock = findChild(window.contentItem, "developerToolsDock");
        const inspected = openPage("https://follows.example");
        const inspectedTabId = browser.activeTabId;
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });
        const view = inspected.developerToolsView;

        browser.openInput("https://other.example", true);
        const otherTabId = browser.activeTabId;
        tryVerify(function () {
            return !dock.visible;
        });
        compare(browser.developerToolsTabId, inspectedTabId);

        browser.activateTab(inspectedTabId);
        tryVerify(function () {
            return dock.visible;
        });
        compare(inspected.developerToolsView, view);

        const otherSpaceId = browser.createSpace("Inspecting");
        verify(browser.switchSpace(otherSpaceId));
        tryVerify(function () {
            return !dock.visible;
        });
        compare(browser.developerToolsTabId, inspectedTabId);

        verify(browser.switchSpace(browser.spaces.data(browser.spaces.index(0, 0), Qt.UserRole
                                                       + 1)));


        tryVerify(function () {
            return dock.visible;
        });
        // The same page, still inspected by the same inspector.
        compare(browser.developerToolsTabId, inspectedTabId);
        compare(findChild(window.contentItem, "engineLoader").developerToolsView, view);

        // Asking for them on the other tab moves them rather than opening a
        // second inspector.
        browser.activateTab(otherTabId);
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });
        compare(browser.developerToolsTabId, otherTabId);
        verify(!inspected.developerToolsAttached);

        browser.closeDeveloperTools();
        tryVerify(function () {
            return !dock.visible;
        });
    }

    // Inspect element opens the dock and asks the page for the target the
    // reader pointed at, which is more than opening the inspector.
    function test_inspectElementOpensTheDockAndAsksForTheTarget() {
        const dock = findChild(window.contentItem, "developerToolsDock");
        const engine = openPage("https://inspect-element.example");
        compare(engine.inspectedElementCount, 0);

        window.commands.run("inspect-element", -1);
        tryVerify(function () {
            return dock.visible;
        });
        compare(engine.inspectedElementCount, 1);
        compare(browser.developerToolsTabId, browser.activeTabId);

        browser.closeDeveloperTools();
        tryVerify(function () {
            return !dock.visible;
        });
    }

    // The inspector's own close button, and the tab going away underneath it.
    function test_developerToolsDetachWithTheirTabOrOnRequest() {
        const dock = findChild(window.contentItem, "developerToolsDock");
        const engine = openPage("https://detaches.example");
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });

        engine.simulateDeveloperToolsClose();
        tryVerify(function () {
            return !dock.visible;
        });
        compare(browser.developerToolsTabId, "");

        browser.openInput("https://kept.example", true);
        const inspectedTabId = browser.activeTabId;
        const second = findChild(window.contentItem, "engineLoader").item;
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });
        browser.closeTab(inspectedTabId);
        tryVerify(function () {
            return !dock.visible;
        });
        compare(browser.developerToolsTabId, "");
    }

    // The seam on the right answers the same way the sidebar's does, and the
    // key that points away from the window's edge widens the panel.
    function test_developerToolsWidthAnswersToBothPointerAndKeyboard() {
        window.settingsOpen = false;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        openPage("https://resized.example");
        window.commands.run("developer-tools", -1);
        const dock = findChild(window.contentItem, "developerToolsDock");
        tryVerify(function () {
            return dock.visible;
        });
        const resizer = findChild(window.contentItem, "developerToolsResizer");
        verify(resizer !== null);
        window.setDeveloperToolsWidth(window.developerToolsDefaultWidth);
        tryCompare(dock, "width", window.developerToolsDefaultWidth);

        // The handle rides the seam it moves.
        compare(Math.round(resizer.x + resizer.width / 2), Math.round(dock.x));

        resizer.forceActiveFocus();
        compare(window.activeFocusItem.objectName, "developerToolsResizer");
        keyClick(Qt.Key_Left);
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth + 16);
        keyClick(Qt.Key_Right);
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth);
        keyClick(Qt.Key_Left, Qt.ShiftModifier);
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth + 48);
        keyClick(Qt.Key_Space);
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth);

        // A request past either end stops at the end.
        window.setDeveloperToolsWidth(window.developerToolsMaximumWidth + 400);
        compare(window.developerToolsWidth, window.developerToolsMaximumWidth);
        window.setDeveloperToolsWidth(0);
        compare(window.developerToolsWidth, window.developerToolsMinimumWidth);
        window.setDeveloperToolsWidth(window.developerToolsDefaultWidth);

        // The handle reads as part of the dock, so it leaves like the rest of
        // the chrome does.
        keyClick(Qt.Key_Escape);
        const engineHost = findChild(window.contentItem, "engineLoader");
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        browser.closeDeveloperTools();
        tryVerify(function () {
            return !dock.visible;
        });
    }

    function test_applicationWindowUsesPlatformAppropriateFrame() {
        verifyApplicationWindowFlags(window);
    }

    // `omaweb commands` and `omaweb run` reach the registry through this. The
    // core has already refused what is not public, and the window refuses it
    // again, answers from `available`, and says whether the command ran.
    function test_answersTheAgentSocketsCommands() {
        // No split is on show, so separating one is offered but not available.
        verify(!browser.splitOnShow);
        verify(!window.commands.available("separate-split"));
        const offered = ["toggle-sidebar", "separate-split", "not-a-command"];
        const listed = window.commands.answerAgent({
                                                       verb: "commands",
                                                       commands: offered
                                                   });
        verify(listed.ok);
        const names = listed.commands.map(function (row) {
            return row.command;
        });
        compare(names, ["toggle-sidebar"]);
        compare(listed.commands[0].title, "Hide or show the sidebar");

        const unavailable = window.commands.answerAgent({
                                                            verb: "run",
                                                            commands: offered,
                                                            command: "separate-split",
                                                            argument: -1
                                                        });
        verify(!unavailable.ok);
        compare(unavailable.code, "unavailable");

        const collapsed = window.sidebarCollapsed;
        verify(window.commands.answerAgent({
                                               verb: "run",
                                               commands: offered,
                                               command: "toggle-sidebar",
                                               argument: -1
                                           }).ok);
        compare(window.sidebarCollapsed, !collapsed);
        window.commands.run("toggle-sidebar", -1);
        compare(window.sidebarCollapsed, collapsed);

        const kept = window.commands.answerAgent({
                                                     verb: "run",
                                                     commands: offered,
                                                     command: "private-window",
                                                     argument: -1
                                                 });
        verify(!kept.ok);
        compare(kept.code, "refused");
    }

    // `omaweb space work && omaweb run toggle-sidebar` from a keybind: through
    // the core, with Allow agents off, to this window's registry and back.
    function test_aKeybindSwitchesSpaceAndRunsACommand() {
        const startSpaceId = browser.activeSpaceId;
        const workSpaceId = browser.createSpace("Keybind work");
        verify(workSpaceId.length > 0);

        const switched = agentSocket.ask({
                                             verb: "space",
                                             name: "keybind",
                                             space: "Keybind work"
                                         });
        verify(switched.ok);
        compare(browser.activeSpaceId, workSpaceId);

        const collapsed = window.sidebarCollapsed;
        const ran = agentSocket.ask({
                                        verb: "run",
                                        name: "keybind",
                                        command: "toggle-sidebar"
                                    });
        verify(ran.ok);
        compare(window.sidebarCollapsed, !collapsed);

        const listed = agentSocket.ask({
                                           verb: "commands",
                                           name: "keybind"
                                       });
        verify(listed.ok);
        verify(listed.commands.some(function (row) {
            return row.command === "toggle-sidebar";
        }));

        const kept = agentSocket.ask({
                                         verb: "run",
                                         name: "keybind",
                                         command: "private-window"
                                     });
        verify(!kept.ok);
        compare(kept.code, "refused");

        window.commands.run("toggle-sidebar", -1);
        compare(window.sidebarCollapsed, collapsed);
        verify(browser.switchSpace(startSpaceId));
        verify(browser.deleteSpace(workSpaceId, "Keybind work"));
    }

    // An Agent asking for one of the reader's Spaces is asked about over the
    // page on show, whichever Space it wants, and the bar leaves the reader's
    // keyboard where it was. The reader's answer is the Agent's.
    function test_anAgentAsksOnceForASpaceOverThePage() {
        const startSpaceId = browser.activeSpaceId;
        openPage("https://reader-typing.example/");
        const grantSpaceId = browser.createSpace("Grant work");
        verify(grantSpaceId.length > 0);
        const opened = agentSocket.ask({
                                           verb: "open",
                                           name: "claude",
                                           url: "https://work.example/",
                                           space: "Grant work"
                                       });
        verify(opened.ok);
        compare(browser.activeSpaceId, startSpaceId);
        agentControl.allowAgents = true;

        const bar = findChild(window.contentItem, "agentGrantBar");
        verify(bar !== null);
        compare(bar.visible, false);
        const look = {
            verb: "look",
            name: "claude",
            tab: opened.tab.id
        };
        const before = agentSocket.replies.length;
        agentSocket.send(look);
        tryCompare(bar, "visible", true);
        compare(bar.prompt.message, "An Agent named claude wants to use Space Grant work");
        compare(bar.activeFocus, false);
        compare(agentSocket.replies.length, before);

        mouseClick(findChild(bar, "browserPromptRefuse"));
        tryCompare(bar, "visible", false);
        compare(agentSocket.replies.length, before + 1);
        compare(agentSocket.replies[before].code, "denied");
        compare(agentControl.grantedSpaces.length, 0);

        // The connection denied is not asked again; another one is.
        agentSocket.send(look);
        compare(agentSocket.replies.length, before + 2);
        compare(agentSocket.replies[before + 1].code, "denied");
        compare(bar.visible, false);
        agentSocket.send(Object.assign({}, look, {
                                           name: "script"
                                       }));
        tryCompare(bar, "visible", true);
        compare(bar.prompt.message, "An Agent named script wants to use Space Grant work");
        mouseClick(findChild(bar, "browserPromptAccept"));
        tryCompare(bar, "visible", false);
        compare(agentControl.grantedSpaces.length, 1);
        compare(agentControl.grantedSpaces[0].spaceName, "Grant work");

        // Revoked, the Space is the reader's alone again.
        verify(agentControl.revokeGrant(grantSpaceId));
        compare(agentControl.grantedSpaces.length, 0);
        agentControl.allowAgents = false;
        verify(browser.deleteSpace(grantSpaceId, "Grant work"));
        compare(browser.activeSpaceId, startSpaceId);
    }

    // `:ask` is listed whether or not Agents are allowed, and the words after
    // its name are its request. With Allow agents off the reader is asked
    // first: Not now leaves everything as it was, and Turn on goes on with
    // what was typed.
    function test_askingAnAgentOffersToAllowAgentsFirst() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const bar = findChild(window.contentItem, "askAgentBar");
        const notice = findChild(window.contentItem, "pageNotice");
        openPage("https://ask-agent.example/");
        activateWindow();
        notice.dismiss();
        compare(agentControl.allowAgents, false);
        agentControl.agentCommand = "sh";

        window.openCommandScope();
        tryVerify(function () {
            return input.activeFocus;
        });
        input.text = "ask";
        compare(panel.rows[0].command, "ask");
        compare(panel.rows[0].title, "Ask your agent about this tab");
        verify(panel.rows[0].enabled);
        // However the name is typed, the request is what follows it.
        input.text = " Ask  summarize this";
        compare(panel.rows.length, 1);
        compare(panel.rows[0].argument, "summarize this");
        input.text = "ask summarize this";
        compare(panel.rows.length, 1);
        compare(panel.rows[0].command, "ask");
        compare(panel.rows[0].argument, "summarize this");
        keyClick(Qt.Key_Return);
        tryCompare(bar, "visible", true);
        compare(bar.message, "Asking an agent needs Allow agents. Turn it on?");
        compare(bar.actions.map(function (action) {
            return action.label;
        }), ["Turn on", "Not now"]);

        mouseClick(findChild(bar, "questionAction1"));
        tryCompare(bar, "visible", false);
        compare(agentControl.allowAgents, false);
        // Nothing was asked of the agent, which would have said so, and
        // nothing is asked of the reader again. A notice another test's late
        // download raised may still stand, so only this command's are read.
        wait(50);
        verify(!bar.visible);
        verify(!notice.showing || !notice.message.startsWith("Omaweb could"), notice.message);

        window.openCommandScope();
        tryVerify(function () {
            return input.activeFocus;
        });
        input.text = "ask summarize this";
        keyClick(Qt.Key_Return);
        tryCompare(bar, "visible", true);
        mouseClick(findChild(bar, "questionAction0"));
        tryCompare(bar, "visible", false);
        compare(agentControl.allowAgents, true);
        // It went on, as far as the terminal this machine does not have.
        tryCompare(notice, "showing", true);
        compare(notice.message, "Omaweb could not find omaweb-test-no-terminal");

        agentControl.allowAgents = false;
        agentControl.agentCommand = "";
        notice.dismiss();
    }

    // The question is about the tab that was on show. Once another tab is,
    // it is put away, and nothing is asked of either tab.
    function test_theAgentQuestionGoesWithTheTabItWasAbout() {
        const bar = findChild(window.contentItem, "askAgentBar");
        openPage("https://ask-agent.example/");
        const askedTabId = browser.activeTabId;
        openPageInNewTab("https://ask-agent-elsewhere.example/");
        const otherTabId = browser.activeTabId;
        browser.activateTab(askedTabId);
        compare(agentControl.allowAgents, false);

        verify(window.commands.run("ask", "summarize this"));
        tryCompare(bar, "visible", true);
        browser.activateTab(otherTabId);
        tryCompare(bar, "visible", false);
        compare(agentControl.allowAgents, false);

        browser.closeTab(otherTabId);
    }

    // An agent that cannot be started says which program it was, rather than
    // leaving the reader to wonder whether anything happened.
    function test_askingAnAgentThatIsNotThereSaysSo() {
        const notice = findChild(window.contentItem, "pageNotice");
        openPage("https://ask-agent.example/");
        notice.dismiss();
        agentControl.allowAgents = true;
        agentControl.agentCommand = "omaweb-test-no-agent --model sonnet";

        verify(window.commands.run("ask", "summarize this"));
        tryCompare(notice, "showing", true);
        compare(notice.message, "Omaweb could not find omaweb-test-no-agent");
        compare(notice.detail, "Change the agent command in Settings, under agents.");
        verify(!findChild(window.contentItem, "askAgentBar").visible);

        agentControl.allowAgents = false;
        agentControl.agentCommand = "";
        notice.dismiss();
    }

    // A Private window is never an Agent's: `:ask` is not listed there, and
    // run anyway it says so and starts nothing.
    function test_aPrivateWindowAsksNoAgent() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        privateBrowser.windowBrowser.openInput("https://private-ask.example/", false);
        const panel = findChild(privateBrowser.contentItem, "omnibar");
        const input = findChild(privateBrowser.contentItem, "omnibarInput");
        const notice = findChild(privateBrowser.contentItem, "pageNotice");
        agentControl.allowAgents = true;

        privateBrowser.openCommandScope();
        input.text = "ask";
        verify(!panel.rows.some(function (row) {
            return row.command === "ask";
        }));
        input.text = "ask summarize this";
        compare(panel.rows.length, 0);
        privateBrowser.closeOmnibar();

        verify(privateBrowser.commands.run("ask", "summarize this"));
        tryCompare(notice, "showing", true);
        compare(notice.message, "Asking an agent is not available here");
        verify(!findChild(privateBrowser.contentItem, "askAgentBar").visible);

        agentControl.allowAgents = false;
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    function test_sidebarHasNoNewTabButton() {
        const newTabButton = findChild(window.contentItem, "newTabButton");
        verify(newTabButton === null);
    }

    function test_newTabRequestWaitsForCommittedDestination() {
        const previousTabId = browser.activeTabId;
        window.openOmnibar(true);
        tryCompare(window, "omnibarShown", true);
        verify(findChild(window.contentItem, "startPage").open);
        compare(browser.activeTabId, previousTabId);

        const omnibarInput = findChild(window.contentItem, "omnibarInput");
        verify(omnibarInput !== null);
        omnibarInput.text = "https://example.com";
        omnibarInput.forceActiveFocus();
        keyClick(Qt.Key_Return);

        tryVerify(function () {
            return browser.activeTabId !== previousTabId;
        });
        compare(browser.activeUrl.toString(), "https://example.com");
    }

    function test_regularTabShowsItsCloseButtonOnHover() {
        const activeTabId = browser.activeTabId;
        browser.openInputInBackground("https://close-me.example");
        const tabIndex = browser.tabs.index(browser.tabs.rowCount() - 1, 0);
        const tabId = browser.tabs.data(tabIndex, Qt.UserRole + 1);
        const tabRow = findChild(window.contentItem, "tab-" + tabId);
        const closeButton = findChild(window.contentItem, "close-" + tabId);
        const tabPointer = findChild(window.contentItem, "tabPointer-" + tabId);
        verify(tabRow !== null);
        verify(closeButton !== null);
        verify(tabPointer !== null);
        compare(closeButton.parent, tabPointer);
        verify(closeButton.visible);
        compare(closeButton.foreground.a, 0);

        mouseMove(tabRow, tabRow.width / 2, tabRow.height / 2);
        tryVerify(function () {
            return closeButton.foreground.a > 0.5;
        });
        mouseClick(tabRow, tabRow.width - closeButton.width / 2 - 4, tabRow.height / 2);

        tryVerify(function () {
            return findChild(window.contentItem, "tab-" + tabId) === null;
        });
        compare(browser.activeTabId, activeTabId);
    }

    // A tab that starts making sound turns its site chip into a speaker, and
    // the speaker is where the sound is given back. It takes the chip's own
    // box rather than a box of its own in front of it: a row that widened for
    // it would shove its own title sideways every time a page started and
    // stopped playing.
    function test_soundingTabTurnsItsChipIntoASpeaker() {
        const engine = openPage("https://sounding.example");
        // The speaker is about muting here, so the origin is dealt with first:
        // a site the reader has not touched is held silent, and the speaker
        // answers for that instead.
        engine.simulateUserActivation();
        engine.simulateAudible(false);
        const tabId = browser.activeTabId;
        const tabRow = findChild(window.contentItem, "tab-" + tabId);
        const speaker = findChild(window.contentItem, "audio-" + tabId);
        const tile = findChild(window.contentItem, "siteTile-" + tabId);
        verify(tabRow !== null);
        verify(speaker !== null);
        verify(tile !== null);

        // A silent tab says nothing about sound and shows its chip.
        verify(!speaker.visible);
        verify(tile.visible);
        const chipX = tile.x;
        const chipWidth = tile.width;

        engine.simulateAudible(true);
        tryVerify(function () {
            return speaker.visible;
        });
        verify(!tile.visible);
        // The speaker stands in the chip's box, so nothing after it moves.
        compare(speaker.x, chipX);
        compare(speaker.width, chipWidth);
        compare(tile.x, chipX);

        mouseClick(tabRow, speaker.x + speaker.width / 2, speaker.y + speaker.height / 2);
        tryVerify(function () {
            return engine.audioMuted;
        });
        // Muting the tab is not the page falling silent: the page stops
        // reporting sound, and the speaker stays because it is the only way
        // back.
        engine.simulateAudible(false);
        verify(speaker.visible);
        verify(!tile.visible);

        mouseClick(tabRow, speaker.x + speaker.width / 2, speaker.y + speaker.height / 2);
        tryVerify(function () {
            return !engine.audioMuted;
        });
        tryVerify(function () {
            return !speaker.visible;
        });
        verify(tile.visible);
    }

    // Pinning the first tab in a Space is where the section appears, and the
    // outline has to make room for it: the pins belong under the address, not
    // over the controls at the top.
    function test_pinningTheFirstTabPutsTheSectionUnderTheAddress() {
        const section = findChild(window.contentItem, "pinnedList");
        const address = findChild(window.contentItem, "addressButton");
        verify(section !== null);
        verify(address !== null);

        openPage("https://pinned-place.example");
        const tabId = browser.activeTabId;
        browser.toggleActivePinned();
        tryVerify(function () {
            return section.visible;
        });

        const addressBottom = address.mapToItem(window.contentItem, 0, address.height).y;
        const sectionTop = section.mapToItem(window.contentItem, 0, 0).y;
        verify(sectionTop >= addressBottom);
        verify(section.height > 0);

        browser.toggleActivePinned();
        tryVerify(function () {
            return !section.visible;
        });
    }

    // The lock is the engine's report, not a reading of the address bar. A
    // certificate failure is drawn as one and stays drawn while the exception
    // the reader granted is in effect.
    // An Auxiliary window is where a sign-in or a payment finishes, so it is
    // exactly where a certificate failure must not be waved through. It asks
    // the same question of the same rule as an ordinary tab, in the window that
    // opened it.
    function test_auxiliaryWindowsAskTheSameCertificateQuestion() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        openPage("https://auxiliary-opener.example/start");
        engineLoader.item.simulateNewWindowRequest("https://localhost:9443/callback", true);
        const auxiliary = findChild(window, "auxiliaryWindow");
        tryVerify(function () {
            return auxiliary !== null && auxiliary.visible;
        });
        const auxiliaryEngine = findChild(auxiliary.contentItem, "auxiliaryEngineLoader");
        tryVerify(function () {
            return auxiliaryEngine.item !== null;
        });

        const bar = findChild(window.contentItem, "certificateQuestionBar");
        verify(bar !== null);
        verify(!bar.open);

        // A public site inside an Auxiliary window is refused, with no question
        // asked of the reader.
        const refused = auxiliaryEngine.item.simulateCertificateError({
                                                                          "url": "https://bank.example/callback"
                                                                      });
        verify(!bar.open);
        compare(auxiliaryEngine.item.certificateDecisions[refused], false);

        // The Local-development main frame is the one case that is offered, and
        // the answer reaches the Auxiliary window's own engine.
        const offered = auxiliaryEngine.item.simulateCertificateError({});
        tryVerify(function () {
            return bar.open;
        });
        const action = findChild(bar, "questionAction0");
        settleActions(action);
        mouseClick(action, action.width / 2, action.height / 2);
        tryVerify(function () {
            return !bar.open;
        });
        compare(auxiliaryEngine.item.certificateDecisions[offered], true);

        auxiliaryEngine.item.simulateWindowCloseRequest();
        tryVerify(function () {
            return !auxiliary.visible;
        });
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    function test_addressTriggerReportsOnlyWhatTheEngineKnows() {
        const engine = openPage("https://reported-secure.example/page");
        const security = findChild(window.contentItem, "securityIndicator");
        verify(security !== null);
        tryCompare(security, "text", "lock");

        // The engine says the connection is in error; the trigger follows it
        // rather than the https it can see in the address.
        const requestId = engine.simulateCertificateError({});
        tryCompare(security, "text", "warning");
        compare(engine.connectionState, "certificate-error");

        // Granting the exception does not turn the warning into a lock: the
        // check was waived, not passed.
        engine.respondToCertificateError(requestId, true);
        tryCompare(security, "text", "warning");

        openPage("http://reported-insecure.example/page");
        tryCompare(security, "text", "lock_open");
        openPage("https://reported-secure-again.example/page");
        tryCompare(security, "text", "lock");
    }

    // An engine stops reporting a certificate failure once it has been told to
    // accept the certificate, and starts calling the connection secure. Coming
    // back to a site whose check the reader waived must still say so.
    function test_aWaivedCertificateCheckStaysVisibleForTheSession() {
        const engine = openPage("https://localhost:7443/waived");
        const security = findChild(window.contentItem, "securityIndicator");
        const bar = findChild(window.contentItem, "certificateQuestionBar");

        const requestId = engine.simulateCertificateError({});
        tryVerify(function () {
            return bar.open;
        });
        const action = findChild(bar, "questionAction0");
        settleActions(action);
        mouseClick(action, action.width / 2, action.height / 2);
        tryVerify(function () {
            return !bar.open;
        });
        verify(browser.certificateExceptionInEffect("https://localhost:7443/waived"));

        // The engine has forgotten the failure — this is exactly what it does
        // after accepting — and the trigger still warns, because Omaweb has not.
        engine.certificateErrorOrigin = "";
        compare(engine.connectionState, "secure");
        tryCompare(security, "text", "warning");

        // Reading elsewhere and coming back to the same origin says it again.
        openPage("https://elsewhere-after-waiver.example/page");
        tryCompare(security, "text", "lock");
        openPage("https://localhost:7443/another-page");
        tryCompare(security, "text", "warning");

        mouseClick(security, security.width / 2, security.height / 2);
        const card = siteCard();
        tryVerify(function () {
            return card.visible;
        });
        compare(findChild(card, "siteInformationVerdict").text,
                "Certificate could not be verified");
        compare(findChild(card, "siteInformationVerdictDetail").text, "Waived for this session");
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });
    }

    // Every piece of text the card shows at the moment, so a test can say what
    // is and is not on it.
    function visibleTexts(item) {
        const texts = [];
        const walk = function (node) {
            if (!node || !node.visible)
                return;
            if (node.text !== undefined && typeof node.text === "string" && node.text.length > 0)
                texts.push(node.text);
            for (let index = 0; index < node.children.length; ++index)
                walk(node.children[index]);
        };
        walk(item);
        return texts;
    }

    function tileSummary(card) {
        return card.tiles.map(function (tile) {
            return tile.key + "=" + tile.value;
        });
    }

    // The card leads with a verdict for each kind of page, and the rows under
    // it. Nothing in it is about what this build's engine cannot do: that is
    // Settings' to say, and a caveat beside a site reads as the site's problem.
    function test_siteInformationStatesEachKindOfPage() {
        const card = siteCard();
        const read = function () {
            const state = {
                "verdict": findChild(card, "siteInformationVerdict").text,
                "detail": findChild(card, "siteInformationVerdictDetail").text,
                "host": findChild(card, "siteInformationHost").text,
                "tiles": tileSummary(card),
                "texts": visibleTexts(card)
            };
            closeSiteCard();
            return state;
        };

        openPage("https://verdict-secure.example/page");
        openSiteCard("");
        const secure = read();

        openPage("http://verdict-plain.example/page");
        openSiteCard("");
        const plain = read();

        const engine = openPage("https://verdict-refused.example/page");
        engine.simulateCertificateError({});
        tryCompare(engine, "connectionState", "certificate-error");
        openSiteCard("");
        const refused = read();
        engine.certificateErrorOrigin = "";

        const engineHost = findChild(window.contentItem, "engineLoader");
        browser.openInput("about:blank", true);
        tryVerify(function () {
            return engineHost.item === null;
        });
        openSiteCard("");
        const start = read();
        browser.closeActiveTab();

        compare([secure.verdict, secure.detail, secure.host], ["Connection is secure", "Encrypted",
                                                               "verdict-secure.example"]);
        compare(secure.tiles, ["certificate=Omaweb Lab Intermediate", "blocked=None",
                               "cookies=No cookies", "third-parties=None allowed"]);
        compare([plain.verdict, plain.detail, plain.host], ["Not secure",
                                                            "Anything sent here can be read on the way",
                                                            "verdict-plain.example"]);
        compare(plain.tiles, ["blocked=None", "cookies=No cookies", "third-parties=None allowed"]);
        compare([refused.verdict, refused.detail], ["Certificate could not be verified",
                                                    "Waived for this session"]);
        // The certificate the failure was raised for signed itself.
        compare(refused.tiles[0], "certificate=Self-signed");
        compare([start.verdict, start.detail, start.host], ["Omaweb's own page",
                                                            "Nothing was loaded from the network",
                                                            "Start page"]);
        compare(start.tiles, ["blocked=None", "cookies=None", "third-parties=None allowed"]);

        for (const state of [secure, plain, refused, start]) {
            const caveats = state.texts.filter(function (text) {
                return /engine|cannot/i.test(text);
            });
            compare(caveats, [], state.verdict);
        }
    }

    // The card is drawn as the chrome draws a panel: every corner on Omarchy's
    // corner radius, the same 12 px inside every edge, an opaque ground, and
    // the badge centred on the verdict and the line under it together.
    function test_siteInformationFollowsTheChromesCornersAndPadding() {
        // Rounded, as a desktop whose Hyprland corners are rounded has it:
        // square corners would pass whatever the card drew.
        const before = Style.cornerRadius;
        Style.cornerRadius = 6;
        openPage("https://card-shape.example/page");
        const card = openSiteCard("");
        browser.setPermissionDecision("https://card-shape.example/page", "camera", 3);
        closeSiteCard();
        openSiteCard("");
        const host = findChild(card, "siteInformationHost");
        const band = findChild(card, "siteInformationBand");
        const badge = findChild(card, "siteInformationBadge");
        const block = findChild(card, "siteInformationVerdictBlock");
        const reset = findChild(card, "resetSitePermissions");
        const tile = findChild(card, "siteInformationTile_certificate");
        const choice = findChild(card, "sitePermissionChoice_camera");
        settleActions(reset);
        tryVerify(function () {
            return findChild(card, "siteInformationTile_third-parties").y > 0;
        });
        wait(250);

        const left = card.borderLeft;
        const right = card.width - card.borderRight;
        const bottom = card.height - card.borderBottom;
        const hostAt = host.mapToItem(card, 0, 0);
        const resetAt = reset.mapToItem(card, 0, 0);
        const tileAt = tile.mapToItem(card, 0, 0);
        const badgeMiddle = badge.mapToItem(band, 0, badge.height / 2).y;
        const blockMiddle = block.mapToItem(band, 0, block.height / 2).y;
        const shape = {
            "corners": [card.radius, tile.radius, badge.radius, choice.radius, reset.radius],
            "padding": [hostAt.x - left, tileAt.x - left, right - (resetAt.x + reset.width), bottom - (
                    resetAt.y + reset.height)],
            "opaque": card.color.a,
            "badge": Math.abs(badgeMiddle - blockMiddle) <= 0.5,
            "lines": findChild(card, "siteInformationVerdictDetail").visible
        };
        browser.resetSitePermissions("https://card-shape.example/page");
        closeSiteCard();
        Style.cornerRadius = before;
        compare(shape.corners, [6, 6, 6, 6, 6]);
        compare(shape.padding, [12, 12, 12, 12]);
        compare(shape.opaque, 1);
        verify(shape.lines);
        verify(shape.badge, "the badge is not centred on the verdict and its line");
    }

    // An https page's certificate is in the card's certificate detail: the
    // chain from the site's own certificate to the trust anchor, each entry
    // selectable, each field copied whole. There is no dialog of its own.
    function test_siteInformationShowsTheChainAPageArrivedOver() {
        activateWindow();
        openPage("https://chain.example/page");
        settleMotion();
        const card = openSiteCard("");
        const tile = findChild(card, "siteInformationTile_certificate");
        compare(findChild(card, "siteInformationTileValue_certificate").text,
                "Omaweb Lab Intermediate");
        tryVerify(function () {
            return findChild(card, "siteInformationTile_third-parties").y > 0;
        });
        mouseClick(tile, tile.width / 2, tile.height / 2);
        tryCompare(card, "detail", "certificate");
        compare(findChild(window.contentItem, "certificatePanel"), null);
        // The card grows to the detail before a press inside it lands.
        wait(250);

        const detail = findChild(card, "certificateDetail");
        compare(findChild(detail, "certificateOrigin").text,
                "chain.example · verified by the engine");
        compare(findChild(detail, "certificateRole").text, "the site's own");
        compare(findChild(detail, "certificateValue_subject").text, "CN=chain.example");
        compare(findChild(detail, "certificateValue_subjectAlternativeNames").text,
                "DNS:chain.example, DNS:www.chain.example");
        compare(findChild(detail, "certificateValue_notAfter").text, "2026-12-31 23:59:59 UTC");
        // As the dialog's foot did, the detail says which keys work it.
        const keysHint = findChild(detail, "certificateKeys");
        verify(keysHint.visible);
        compare(keysHint.text, "←→ chain · ↑↓ field · ⏎ copy");

        // Each certificate in the chain is picked by pointer or by arrow.
        const anchor = findChild(detail, "certificateChainEntry2");
        verify(findChild(detail, "certificateChainEntry1") !== null);
        settleActions(anchor);
        mouseClick(anchor, anchor.width / 2, anchor.height / 2);
        compare(findChild(detail, "certificateRole").text, "trust anchor");
        compare(findChild(detail, "certificateValue_subject").text,
                "O=Omaweb Lab, CN=Omaweb Lab Root");
        compare(findChild(detail, "certificateValue_subjectAlternativeNames").text, "none");
        keyClick(Qt.Key_Left);
        compare(findChild(detail, "certificateRole").text, "intermediate");

        // The copy button puts the whole value on the clipboard and says so.
        SystemClipboard.copyText("something the reader already had");
        const copy = findChild(detail, "copyCertificateField_sha256");
        settleActions(copy);
        mouseClick(copy, copy.width / 2, copy.height / 2);
        compare(SystemClipboard.text(), "22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:"
                + "22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11");
        compare(copy.label, "copied");

        // So does Return, on the field the arrows are on: the button the
        // pointer pressed has not taken the keyboard.
        keyClick(Qt.Key_Up);
        keyClick(Qt.Key_Return);
        compare(SystemClipboard.text(), "2030-01-01 00:00:00 UTC");

        // Every field has its own button, and each copies what its row shows.
        const keys = ["subject", "issuer", "notBefore", "notAfter", "sha256",
                      "subjectAlternativeNames"];
        const copied = keys.map(function (key) {
            SystemClipboard.copyText("something the reader already had");
            const button = findChild(detail, "copyCertificateField_" + key);
            settleActions(button);
            mouseClick(button, button.width / 2, button.height / 2);
            return SystemClipboard.text() === findChild(detail, "certificateValue_" + key).text;
        });
        compare(copied, keys.map(function () {
            return true;
        }));

        keyClick(Qt.Key_Escape);
        tryCompare(card, "detail", "");
        verify(card.visible);
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });
    }

    // A Local-development site's certificate usually signed itself, and the
    // certificate question shows it before the reader decides whether to let
    // it through, in the card's certificate detail. Looking at it answers
    // nothing: the question stays, and has the keyboard back once the card is
    // put away.
    function test_theCertificateQuestionShowsTheCertificateItRefused() {
        activateWindow();
        const engine = openPage("https://localhost:6443/app");
        const bar = findChild(window.contentItem, "certificateQuestionBar");
        const requestId = engine.simulateCertificateError({});
        tryVerify(function () {
            return bar.open;
        });
        const view = findChild(bar, "questionAction1");
        compare(view.label, "View certificate");

        settleActions(view);
        mouseClick(view, view.width / 2, view.height / 2);

        const card = siteCard();
        tryVerify(function () {
            return card.visible;
        });
        compare(card.detail, "certificate");
        const detail = findChild(card, "certificateDetail");
        compare(findChild(detail, "certificateOrigin").text,
                "localhost:6443 · could not be verified");
        compare(findChild(detail, "certificateRole").text, "self-signed");
        verify(findChild(detail, "certificateChainEntry1") === null);
        compare(findChild(detail, "certificateValue_issuer").text, "CN=localhost, O=Omaweb Lab");
        SystemClipboard.copyText("something the reader already had");
        keyClick(Qt.Key_Return);
        compare(SystemClipboard.text(), "CN=localhost, O=Omaweb Lab");

        keyClick(Qt.Key_Escape);
        tryCompare(card, "detail", "");
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });
        verify(bar.open);
        verify(focusIsInside(bar));
        verify(engine.certificateDecisions[requestId] === undefined);
        const block = findChild(bar, "questionAction2");
        settleActions(block);
        mouseClick(block, block.width / 2, block.height / 2);
        tryVerify(function () {
            return !bar.open;
        });
        compare(engine.certificateDecisions[requestId], false);
    }

    // Plain HTTP has no certificate, so there is no certificate tile. An https
    // page whose engine has not reported its chain has the tile with nothing
    // behind it, and nothing on it about the engine.
    function test_siteInformationOffersACertificateOnlyWhereThereIsOne() {
        const card = siteCard();
        const certificateTile = function () {
            const tile = card.tiles.filter(function (entry) {
                return entry.key === "certificate";
            })[0];
            return tile ? tile.value + (tile.drills ? " ›" : "") : "none";
        };
        openPage("http://plain.example/page");
        openSiteCard("");
        const overHttp = certificateTile();
        closeSiteCard();

        const engine = openPage("https://stock-engine.example/page");
        engine.pageCertificatesAvailable = false;
        engine.certificateChain = [];
        openSiteCard("");
        const stock = certificateTile();
        closeSiteCard();
        engine.pageCertificatesAvailable = true;

        // Plain HTTP offers none even where the engine still holds a chain
        // from the page before, and a Space at rest has no page to ask.
        const plain = openPage("http://stale.example/page");
        plain.certificateChain = plain.labCertificateChain("https://stale.example/page");
        verify(plain.certificateChain.length > 0);
        openSiteCard("");
        const staleOverHttp = certificateTile();
        closeSiteCard();

        const engineHost = findChild(window.contentItem, "engineLoader");
        browser.openInput("about:blank", true);
        tryVerify(function () {
            return engineHost.item === null;
        });
        openSiteCard("");
        const atRest = certificateTile();
        closeSiteCard();
        browser.closeActiveTab();

        compare(overHttp, "none");
        compare(stock, "Not reported");
        compare(staleOverHttp, "none");
        compare(atRest, "none");
    }

    // A Private window's page arrived over a certificate like any other, and
    // its Site information shows it the same way.
    function test_aPrivateWindowShowsTheCertificateLikeAnyOther() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://private-chain.example/page", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        privateBrowser.openSiteInformation("certificate");
        const card = findChild(privateBrowser.contentItem, "siteInformationCard");
        tryVerify(function () {
            return card.visible;
        });
        const detail = findChild(card, "certificateDetail");
        const shown = detail.visible;
        const origin = findChild(detail, "certificateOrigin").text;
        const subject = findChild(detail, "certificateValue_subject").text;
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        verify(shown);
        compare(origin, "private-chain.example · verified by the engine");
        compare(subject, "CN=private-chain.example");
    }

    // Settings names CNAME uncloaking as unsupported only on an engine that
    // lacks it, and the window is what tells it which engine this build has
    // (ADR 0050).
    function test_settingsKnowsWhetherThisBuildsEngineUncloaks() {
        const settings = findChild(window.contentItem, "settingsSurface");
        compare(settings.cnameUncloakingAvailable, EngineBuild.cnameUncloaking);
    }

    // Settings states what this build's engine cannot do about a site, and the
    // window is what tells it.
    function test_settingsKnowsWhatThisBuildsEngineCannotDoAboutASite() {
        const settings = findChild(window.contentItem, "settingsSurface");
        compare([settings.certificateDecisionsAvailable, settings.pageCertificatesAvailable,
                 settings.thirdPartyCookieControlAvailable, settings.siteDataOnDisk,
                 settings.insecureContentBlocked], [window.certificateDecisionsAvailable,
                                                    window.pageCertificatesAvailable,
                                                    window.thirdPartyCookieControlAvailable,
                                                    window.siteDataOnDisk,
                                                    window.insecureContentBlocked]);
    }

    // And the same for procedural cosmetic rules (ADR 0052).
    function test_settingsKnowsWhetherThisBuildsEngineAppliesProceduralRules() {
        const settings = findChild(window.contentItem, "settingsSurface");
        compare(settings.proceduralCosmeticFilteringAvailable,
                EngineBuild.proceduralCosmeticFiltering);
    }

    // The tally says how many; the list says which. An uncloaked refusal is
    // listed under the address the page asked for, which is the one in the
    // page's own network log, with the canonical name that explains it.
    function test_siteInformationListsTheRequestsItRefused() {
        openPage("https://news.example/story");
        const card = siteCard();
        const blocker = card.blocker;
        card.blocker = refusingBlockerComponent.createObject(testCase);
        openSiteCard("");
        const tally = findChild(card, "siteInformationTileValue_blocked").text;
        closeSiteCard();
        openSiteCard("blocked");

        const first = findChild(card, "refusedRequest0");
        const cloaked = findChild(card, "refusedRequest1");
        const fourth = findChild(card, "refusedRequest4");
        const overflow = findChild(card, "refusedRequestOverflow");
        const firstThrough = findChild(card, "refusedRequestThrough0");
        const cloakedThrough = findChild(card, "refusedRequestThrough1");
        const texts = [tally, first ? first.text : null, cloaked ? cloaked.text : null, fourth
                       !== null && fourth.visible ? fourth.text : null, overflow !== null
                       && overflow.visible, firstThrough !== null && firstThrough.visible,
                       cloakedThrough !== null && cloakedThrough.visible ? cloakedThrough.text :
                                                                           null];

        closeSiteCard();
        card.blocker = blocker;
        compare(texts, ["5 requests", "ads.example/banner.js", "metrics.news.example/collect.js",
                        "ads.example/fourth.js", false, false, "through collect.tracker.example"]);
    }

    // With Secure DNS on, a page whose name could not be looked up says whose
    // resolver failed to find it, because the engine's own error page says only
    // that the name was not found, and the reader's system resolver might have
    // found it.
    function test_siteInformationNamesTheResolverThatCouldNotFindTheSite() {
        const engine = openPage("https://unresolved.example/page");
        verify(secureDns.useResolver("quad9"));
        engine.lastLoadNameUnresolved = true;
        // Choosing another resolver afterwards does not move the blame.
        verify(secureDns.useResolver("cloudflare"));
        const card = openSiteCard("");
        const text = findChild(card, "siteInformationVerdictDetail").text;
        closeSiteCard();
        engine.lastLoadNameUnresolved = false;
        secureDns.turnOff();
        compare(text, "Quad9 could not find this site, over Secure DNS");
    }

    // A resolver the engine would not take looks nothing up: the system did,
    // so a name it could not find is not the chosen resolver's to answer for.
    function test_siteInformationBlamesNoResolverTheEngineRefused() {
        const engine = openPage("https://refused-resolver.example/page");
        verify(secureDns.useResolver("quad9"));
        engineSecureDns.applied = false;
        engine.lastLoadNameUnresolved = true;
        const card = openSiteCard("");
        const text = findChild(card, "siteInformationVerdictDetail").text;
        closeSiteCard();
        engine.lastLoadNameUnresolved = false;
        engineSecureDns.applied = true;
        secureDns.turnOff();
        verify(text.indexOf("could not find this site") < 0, text);
    }

    function test_siteInformationCountsOneRefusalInTheSingular() {
        openPage("https://one-refusal.example/page");
        const card = siteCard();
        const blocker = card.blocker;
        card.blocker = oneRefusalBlockerComponent.createObject(testCase);
        openSiteCard("");
        const text = findChild(card, "siteInformationTileValue_blocked").text;
        closeSiteCard();
        card.blocker = blocker;
        compare(text, "1 request");
    }

    // Nothing in the card reaches past its own border, and the card stays on
    // the window however narrow the window is. Every answer it offers is
    // something the reader has to be able to hit.
    function test_siteInformationKeepsEveryAnswerInsideItsBorder() {
        const engine = openPage("https://panel-geometry.example/page");
        const card = siteCard();
        engine.persistentProfilesAvailable = true;
        const control = card.thirdPartyCookieControlAvailable;
        card.thirdPartyCookieControlAvailable = true;
        browser.setPermissionDecision("https://panel-geometry.example/page", "camera", 2);
        browser.setPermissionDecision("https://panel-geometry.example/page", "geolocation", 3);
        browser.setPermissionDecision("https://panel-geometry.example/page", "notifications", 3);
        browser.allowThirdPartyCookies("https://collector-pxxxxxx.eu-north-1.example", "payment");
        const problems = [];
        const measure = function (names) {
            const left = card.mapToItem(window.contentItem, 0, 0).x;
            const right = left + card.width;
            if (left < 0 || right > window.width)
                problems.push("the card spans " + left + " to " + right + " in a window of "
                              + window.width);
            for (const name of names) {
                const item = findChild(card, name);
                if (item === null) {
                    problems.push(name + " is missing");
                    continue;
                }
                if (!item.visible)
                    continue;
                const at = item.mapToItem(window.contentItem, 0, 0);
                if (at.x < left)
                    problems.push(name + " starts at " + at.x + ", left of " + left);
                if (at.x + item.width > right)
                    problems.push(name + " ends at " + (at.x + item.width) + ", past " + right);
            }
        };

        openSiteCard("");
        settleActions(findChild(card, "resetSitePermissions"));
        wait(250);
        measure(["siteInformationHost", "siteInformationTile_third-parties",
                 "sitePermissionChoice_camera", "sitePermissionChoice_geolocation",
                 "sitePermissionChoice_notifications", "clearSiteStorage", "resetSitePermissions"]);
        closeSiteCard();
        openSiteCard("third-parties");
        card.refusedThirdParties = ["https://private-user-images.githubusercontent.com",
                                    "https://avatars.githubusercontent.com"];
        settleActions(findChild(card, "manageThirdParties"));
        wait(250);
        measure(["refusedThirdParty0", "cookieAllowance0", "manageThirdParties"]);

        closeSiteCard();
        browser.revokeThirdPartyCookieAllowance("https://collector-pxxxxxx.eu-north-1.example");
        browser.resetSitePermissions("https://panel-geometry.example/page");
        card.refusedThirdParties = [];
        card.thirdPartyCookieControlAvailable = control;
        engine.persistentProfilesAvailable = false;
        compare(problems.join("; "), "");
    }

    // Every question the card leads to is asked in the window's own centred
    // dialog, which has room to name the scope. The card goes away when the
    // dialog opens, so one surface holds the question.
    // `prepare` runs once the card is open, for the state the lab has no
    // engine to supply — the card reads that when it opens, so naming it
    // earlier would be overwritten.
    function openSiteAction(name, prepare, detail) {
        const card = openSiteCard(detail === undefined ? "" : detail);
        if (prepare !== undefined)
            prepare(card);
        const trigger = findChild(card, name);
        verify(trigger !== null, name + " is missing");
        verify(trigger.enabled, name + " is not enabled");
        settleActions(trigger);
        // The card grows to what `prepare` named before a press inside it
        // lands where the trigger is drawn.
        wait(250);
        mouseClick(trigger, trigger.width / 2, trigger.height / 2);
        const dialog = findChild(window.contentItem, "spaceDialog");
        tryVerify(function () {
            return dialog.open;
        });
        // One surface holds the question: the card goes away behind it.
        tryVerify(function () {
            return !card.visible;
        });
        return dialog;
    }

    // The only clearing that is about the site the card is headed by. The
    // engine exposes no per-origin removal, so the page is asked to empty its
    // own storage and reports what it managed to take.
    function test_siteInformationEmptiesOneSitesStorageThroughItsPage() {
        const engine = openPage("https://site-storage.example/app");
        settleMotion();
        const dialog = openSiteAction("clearSiteStorage");

        // The dialog names the site, the scope, and what it cannot take.
        verify(window.dialogMode === "site-storage");
        verify(dialog.message.indexOf("site-storage.example") !== -1);
        verify(dialog.message.indexOf("local storage, databases") !== -1);
        verify(dialog.message.indexOf("cookies are not included") !== -1);
        compare(engine.pageSiteDataClearCount, 0);

        // Put away unanswered, nothing is taken.
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !dialog.open;
        });
        compare(engine.pageSiteDataClearCount, 0);

        openSiteAction("clearSiteStorage");
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return engine.pageSiteDataClearCount === 1;
        });
        const notice = findChild(window.contentItem, "pageNotice");
        tryVerify(function () {
            return notice.showing;
        });
        compare(notice.message, "Emptied site-storage.example's storage");
        verify(notice.detail.indexOf("cookies are cleared for the whole Space") !== -1);

        // A page holding nothing says so rather than reporting a success the
        // reader would read as having taken something.
        engine.pageSiteData = [];
        openSiteAction("clearSiteStorage");
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return notice.message === "site-storage.example had nothing stored";
        });

        // And a page that cannot answer is not reported as one that did.
        engine.pageSiteDataRefusal = "databases could not be emptied";
        openSiteAction("clearSiteStorage");
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return notice.message === "Could not empty site-storage.example's storage";
        });
        compare(notice.detail, "databases could not be emptied");

        engine.pageSiteDataRefusal = "";
        engine.pageSiteData = ["local storage", "databases"];
    }

    // The third-party dialog opens over the card's button, so a row can appear
    // under a pointer that has not moved. That row does not take the keyboard's
    // place in the list; a pointer moving over one does.
    function test_aDialogRowUnderAStillPointerLeavesTheKeyboardsRow() {
        openPage("https://still-pointer.example/page");
        settleMotion();
        const card = siteCard();
        card.refusedThirdParties = ["https://pay.example", "https://cdn.example"];
        window.refreshThirdPartyRows();
        window.dialogMode = "third-party";
        const dialog = findChild(window.contentItem, "spaceDialog");
        tryVerify(function () {
            return dialog.open;
        });
        const row = findChild(window.contentItem, "commandDialogRow3");
        tryVerify(function () {
            return row !== null && row.width > 0;
        });
        settleActions(row);
        // What the window reports for a row arriving under the pointer: an
        // entry and a move to the same spot.
        mouseMove(row, row.width / 2, row.height / 2);
        const still = dialog.selected;
        mouseMove(row, row.width / 2 + 6, row.height / 2);
        const moved = dialog.selected;
        window.dialogMode = "";
        card.refusedThirdParties = [];
        compare(still, 0);
        compare(moved, 3);
    }

    // Resetting is confirmed first, and then this site's decisions in this
    // Space are gone.
    function test_siteInformationResetsTheSitesPermissionsOnceConfirmed() {
        openPage("https://reset-site.example/page");
        settleMotion();
        verify(browser.setPermissionDecision("https://reset-site.example/page", "camera", 2));
        const dialog = openSiteAction("resetSitePermissions");
        verify(window.dialogMode === "reset-permissions");
        verify(dialog.message.indexOf("reset-site.example") !== -1);
        compare(browser.sitePermissions("https://reset-site.example/page").length, 1);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !dialog.open;
        });
        compare(browser.sitePermissions("https://reset-site.example/page").length, 1);

        openSiteAction("resetSitePermissions");
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return browser.sitePermissions("https://reset-site.example/page").length === 0;
        });
        const notice = findChild(window.contentItem, "pageNotice");
        tryVerify(function () {
            return notice.message === "Reset every decision for this site";
        });
    }

    // Clearing cookies and cache is the Space's, because the engine can only
    // take those for every site at once, so it is not one of this site's
    // actions: Settings' Browsing data is where it is done. The card still
    // says how much the Space holds, as the Space's.
    function test_siteInformationLeavesClearingTheSpaceToSettings() {
        const engine = openPage("https://space-data.example/page");
        // An engine that keeps a profile on disk and names what it keeps
        // there, which the lab otherwise does not.
        engine.persistentProfilesAvailable = true;
        const card = siteCard();
        const entries = card.siteDataEntries;
        const retainedEntries = card.retainedDataEntries;
        card.siteDataEntries = ["Cookies", "cache"];
        card.retainedDataEntries = ["Local Storage", "IndexedDB"];

        openSiteCard("cookies");
        const siteData = findChild(card, "siteInformationSiteData");
        const sizes = [siteData !== null && siteData.visible ? findChild(siteData, "detailValue").text :
                                                               null];
        card.retainedDataBytes = 900 * 1024 * 1024;
        tryVerify(function () {
            return findChild(card, "siteInformationRetainedData") !== null;
        });
        const retained = findChild(card, "siteInformationRetainedData");
        sizes.push(findChild(retained, "detailValue").text);
        closeSiteCard();
        openSiteCard("");
        const actions = ["clearSiteStorage", "resetSitePermissions", "clearSiteData"].map(function (
            name) {
            return findChild(card, name) !== null;
        });
        closeSiteCard();

        window.settingsOpen = true;
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("privacy");
        const clearButton = findChild(settings, "clearBrowsingDataButton");
        const inSettings = clearButton !== null && clearButton.visible;
        window.settingsOpen = false;

        card.siteDataEntries = entries;
        card.retainedDataEntries = retainedEntries;
        engine.persistentProfilesAvailable = false;
        verify(sizes[0] !== null && [" B", " kB", " MB"].some(function (unit) {
            return sizes[0].endsWith(unit);
        }), String(sizes[0]));
        compare(sizes[1], "900 MB");
        compare(actions, [true, true, false]);
        verify(inSettings);
    }

    // A blocked third party is named in the card's third-party detail and
    // answered in the dialog, where there is room to say what allowing one is
    // for. A reader looking at an embedded asset host cannot judge it from its
    // name alone.
    function test_thirdPartyAllowanceIsChosenInTheDialog() {
        openPage("https://allowance.example/checkout");
        settleMotion();
        const card = siteCard();
        const control = card.thirdPartyCookieControlAvailable;
        card.thirdPartyCookieControlAvailable = true;

        // With nothing refused and nothing allowed there is nothing to open.
        openSiteCard("");
        const empty = card.tiles[card.tiles.length - 1];
        closeSiteCard();

        // The lab has no third-party filter, so the origins one would have
        // refused are named here.
        const refused = ["https://pay.example", "https://cdn.example", "https://images.example",
                         "https://fonts.example"];
        const name = function (surface) {
            surface.refusedThirdParties = refused;
        };
        openSiteCard("third-parties");
        name(card);
        tryVerify(function () {
            return findChild(card, "refusedThirdParty3") !== null;
        });
        const listed = findChild(findChild(card, "refusedThirdParty3"), "detailValue").text;
        closeSiteCard();

        const dialog = openSiteAction("manageThirdParties", name, "third-parties");
        verify(dialog.message.indexOf("not working") !== -1);
        verify(dialog.message.indexOf("does not need it") !== -1);
        compare(window.thirdPartyRows.length, 8);

        // The second row is the payment answer for the first origin.
        keyClick(Qt.Key_Down);
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return browser.thirdPartyCookieAllowances().length === 1;
        });
        compare(browser.thirdPartyCookieAllowances()[0].origin, "https://pay.example");
        compare(browser.thirdPartyCookieAllowances()[0].purpose, "payment");
        verify(browser.thirdPartyCookiesAllowed(browser.activeSpaceId, "https://pay.example"));
        const notice = findChild(window.contentItem, "pageNotice");
        tryVerify(function () {
            return notice.showing;
        });
        compare(notice.message, "Allowing https://pay.example");
        verify(notice.detail.indexOf("for a payment") !== -1);

        // The card now counts it, and names it as allowed beside the ones
        // still refused.
        openSiteCard("");
        const tile = findChild(card, "siteInformationTileValue_third-parties").text;
        closeSiteCard();
        openSiteCard("third-parties");
        name(card);
        const allowed = findChild(card, "cookieAllowance0");
        const allowance = [findChild(allowed, "detailValue").text, allowed.label];
        closeSiteCard();

        // An allowance is taken back the same way, from the top of the list.
        openSiteAction("manageThirdParties", name, "third-parties");
        compare(window.thirdPartyRows[0].purpose, "");
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return browser.thirdPartyCookieAllowances().length === 0;
        });
        verify(!browser.thirdPartyCookiesAllowed(browser.activeSpaceId, "https://pay.example"));
        tryVerify(function () {
            return notice.message === "Stopped allowing https://pay.example";
        });

        card.refusedThirdParties = [];
        card.thirdPartyCookieControlAvailable = control;
        compare([empty.value, empty.drills], ["None allowed", false]);
        compare(listed, "https://fonts.example");
        compare(tile, "1 allowed");
        compare(allowance, ["https://pay.example", "Allowed for a payment"]);
    }

    // The permissions a site has rows for, as `kind=choice`.
    function permissionSummary(card) {
        return card.permissionRows.map(function (row) {
            const choice = findChild(card, "sitePermissionChoice_" + row.permission);
            return row.permission + "=" + (choice ? findChild(choice,
                                                              "permissionDropdownValue").text :
                                                    "missing");
        });
    }

    // Only what this site asked for or the reader decided in this Space has a
    // row. A site that never asked has none, rather than a line saying so.
    function test_siteInformationListsOnlyPermissionsAskedOrDecided() {
        const site = "https://asked-or-decided.example/page";
        const engine = openPage(site);
        const card = openSiteCard("");
        const never = permissionSummary(card);
        const noRow = findChild(card, "sitePermission_camera");
        closeSiteCard();

        verify(browser.setPermissionDecision(site, "camera", BrowserController.AllowPersistently));
        verify(browser.setPermissionDecision(site, "notifications", BrowserController.Block));
        openSiteCard("");
        const decided = permissionSummary(card);
        closeSiteCard();

        // A question the page has open is a permission it asked for, before
        // there is any answer.
        verify(engine.simulateSitePermission("https://asked-or-decided.example",
                                             "geolocation").length > 0);
        tryCompare(window, "permissionOpen", true);
        openSiteCard("");
        const asked = permissionSummary(card);
        closeSiteCard();
        window.respondToPermission(BrowserController.Block);
        browser.resetSitePermissions(site);

        // Another Space holds no decision of this one's.
        compare(never, []);
        compare(noRow, null);
        compare(decided, ["camera=Allow", "notifications=Block"]);
        compare(asked, ["camera=Allow", "geolocation=Ask", "notifications=Block"]);
    }

    function test_siteInformationPermissionTakesEachChoice_data() {
        return [
                    {
                        "tag": "allow",
                        "from": BrowserController.Block,
                        "label": "Allow",
                        "decision": BrowserController.AllowPersistently
                    },
                    {
                        "tag": "ask",
                        "from": BrowserController.AllowPersistently,
                        "label": "Ask",
                        "decision": BrowserController.Ask
                    },
                    {
                        "tag": "block",
                        "from": BrowserController.AllowOnce,
                        "label": "Block",
                        "decision": BrowserController.Block
                    }
                ];
    }

    // Choosing in a permission's dropdown is the decision: the core answers
    // the site's next request with it, the engine is told to forget its own
    // record so that request reaches the core, and it is kept in the Space,
    // where the card finds it again.
    function test_siteInformationPermissionTakesEachChoice(data) {
        const site = "https://choice-" + data.tag + ".example/page";
        openPage(site);
        settleMotion();
        verify(browser.setPermissionDecision(site, "camera", data.from));
        const resets = window.spaceProfileHost.resetPermissionOrigins.length;
        const card = openSiteCard("");
        const choice = findChild(card, "sitePermissionChoice_camera");
        verify(choice !== null);
        settleActions(choice);
        wait(250);
        mouseClick(choice, choice.width / 2, choice.height / 2);
        let option = null;
        tryVerify(function () {
            option = findChild(choice.optionItems, "permissionOption_" + data.tag);
            return option !== null && option.visible && option.width > 0;
        });
        mouseClick(option, option.width / 2, option.height / 2);
        tryCompare(findChild(choice, "permissionDropdownValue"), "text", data.label);
        closeSiteCard();

        const stored = browser.sitePermissions(site).map(function (row) {
            return row.permission + "=" + row.decision;
        });
        const engineTold = window.spaceProfileHost.resetPermissionOrigins.slice(resets);
        openSiteCard("");
        const again = permissionSummary(card);
        closeSiteCard();
        const answered = browser.permissionDecision(site, "camera");
        browser.resetSitePermissions(site);

        compare(stored, ["camera=" + data.decision]);
        compare(engineTold, [site]);
        compare(again, ["camera=" + data.label]);
        compare(answered, data.decision);
    }

    // The dropdown answers the keyboard as the kit's does: Return opens it,
    // the arrows walk it, Return picks, and Escape puts it away before the
    // card's own Escape is reached.
    function test_siteInformationPermissionAnswersTheKeyboard() {
        const site = "https://choice-keys.example/page";
        openPage(site);
        settleMotion();
        verify(browser.setPermissionDecision(site, "notifications", BrowserController.Block));
        const card = openSiteCard("");
        const choice = findChild(card, "sitePermissionChoice_notifications");
        choice.forceActiveFocus();
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return choice.popupOpen;
        });
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !choice.popupOpen;
        });
        const stillOpen = card.visible;
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return choice.popupOpen;
        });
        keyClick(Qt.Key_Up);
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return !choice.popupOpen;
        });
        const answered = browser.permissionDecision(site, "notifications");
        closeSiteCard();
        browser.resetSitePermissions(site);
        verify(stillOpen);
        compare(answered, BrowserController.Ask);
    }

    // Site information, as the window hosts it: a card over the page edge,
    // floating from the address it reports on.
    function siteCard() {
        const card = findChild(window.contentItem, "siteInformationCard");
        verify(card !== null, "there is no Site information card");
        return card;
    }

    function openSiteCard(detail) {
        window.openSiteInformation(detail === undefined ? "" : detail);
        const card = siteCard();
        tryVerify(function () {
            return card.visible && card.open;
        });
        return card;
    }

    function closeSiteCard() {
        window.closeSiteInformation();
        const card = siteCard();
        tryVerify(function () {
            return !card.visible;
        });
    }

    // The lock opens the card at its top, under the address, about 400 px wide
    // and over the edge of the page rather than inside the sidebar.
    function test_theLockOpensSiteInformationAtItsTop() {
        openPage("https://status-position.example/page");
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const address = findChild(window.contentItem, "addressButton");
        const security = findChild(window.contentItem, "securityIndicator");
        const card = siteCard();
        verify(!card.visible);

        mouseClick(security, security.width / 2, security.height / 2);
        tryVerify(function () {
            return card.visible;
        });
        compare(card.detail, "");
        compare(findChild(card, "siteInformationVerdict").text, "Connection is secure");
        compare(findChild(card, "siteInformationHost").text, "status-position.example");
        // It unfolds from the address, so its place is read once it settles.
        const addressBottom = address.mapToItem(window.contentItem, 0, address.height).y;
        tryVerify(function () {
            const top = card.mapToItem(window.contentItem, 0, 0).y;
            return top >= addressBottom + 6 && top <= addressBottom + 10;
        });
        compare(card.mapToItem(window.contentItem, 0, 0).x, address.mapToItem(window.contentItem, 0,
                                                                              0).x);
        compare(card.width, 400);
        verify(card.mapToItem(window.contentItem, card.width, 0).x > sidebar.width);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });

        // A click anywhere off the card puts it away, and only that.
        mouseClick(security, security.width / 2, security.height / 2);
        tryVerify(function () {
            return card.visible;
        });
        mouseClick(window.contentItem, window.width - 40, window.height - 40);
        tryVerify(function () {
            return !card.visible;
        });
    }

    // The shield beside the address counts what Content blocking refused, and
    // a click on it opens the card straight at those requests.
    function test_theShieldOpensSiteInformationAtTheBlockedRequests() {
        openPage("https://news.example/story");
        const sidebar = findChild(window.contentItem, "sidebar");
        const card = siteCard();
        const outlineBlocker = sidebar.blocker;
        const cardBlocker = card.blocker;
        const refusing = refusingBlockerComponent.createObject(testCase);
        sidebar.blocker = refusing;
        card.blocker = refusing;
        const shield = findChild(window.contentItem, "blockedRequestIndicator");
        tryVerify(function () {
            return shield.visible;
        });

        mouseClick(shield, shield.width / 2, shield.height / 2);
        tryVerify(function () {
            return card.visible;
        });
        const detail = card.detail;
        const first = findChild(card, "refusedRequest0");
        const cloakedThrough = findChild(card, "refusedRequestThrough1");
        const texts = [first ? first.text : null, cloakedThrough && cloakedThrough.visible
                       ? cloakedThrough.text : null];
        closeSiteCard();
        sidebar.blocker = outlineBlocker;
        card.blocker = cardBlocker;
        compare(detail, "blocked");
        compare(texts, ["ads.example/banner.js", "through collect.tracker.example"]);
    }

    // The command and its key open the card at its top, and both are named
    // where the reader looks keys up: the Shortcut sheet, and the command's
    // own row in command scope.
    function test_siteInformationOpensFromItsCommandAndItsKey() {
        openPage("https://command-site.example/page");
        activateWindow();
        const card = siteCard();
        verify(window.commands.run("site-information", -1));
        tryVerify(function () {
            return card.visible;
        });
        compare(card.detail, "");
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });

        keyClick(Qt.Key_L, Qt.ControlModifier | Qt.ShiftModifier);
        tryVerify(function () {
            return card.visible;
        });
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });

        const keys = window.commands.keymap.keysFor("site-information");
        compare(keys, window.commands.keymap.displayFor("Primary+Shift+L"));

        const sheet = findChild(window.contentItem, "shortcutSheet");
        let sheetKeys = "";
        for (let group = 0; group < sheet.sections.length; ++group) {
            const entries = sheet.sections[group].entries;
            for (let index = 0; index < entries.length; ++index) {
                if (entries[index].title === "Site information")
                    sheetKeys = entries[index].keys;
            }
        }
        compare(sheetKeys, keys);

        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const list = findChild(window.contentItem, "omnibarRowList");
        // The keys above left the window keyboard-driven, so the Omnibar
        // opens without easing in.
        window.openCommandScope();
        tryCompare(panel, "visible", true);
        input.text = "site information";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        const row = panel.rows[0];
        const caps = findChild(omnibarRowItem(list, 0), "omnibarRowKeys");
        const shown = caps ? caps.keys : "";
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
        compare(row.command, "site-information");
        compare(row.keys, keys);
        compare(shown, keys);
    }

    // Every tile opens its detail inside the card and comes back to the top,
    // by pointer and by keyboard. Escape steps back, and Escape again closes.
    function test_eachSiteInformationTileDrillsInAndBack() {
        openPage("https://drill.example/page");
        activateWindow();
        const sidebar = findChild(window.contentItem, "sidebar");
        const card = siteCard();
        const cardBlocker = card.blocker;
        const control = card.thirdPartyCookieControlAvailable;
        card.blocker = refusingBlockerComponent.createObject(testCase);
        card.thirdPartyCookieControlAvailable = true;
        openSiteCard("");
        card.refusedThirdParties = ["https://pay.example"];

        const tiles = ["certificate", "blocked", "cookies", "third-parties"];
        // The grid places its tiles on its next pass; until then the last one
        // lies over the first.
        tryVerify(function () {
            return findChild(card, "siteInformationTile_third-parties").y > 0;
        });
        const byPointer = [];
        for (const name of tiles) {
            const tile = findChild(card, "siteInformationTile_" + name);
            verify(tile !== null, name);
            mouseClick(tile, tile.width / 2, tile.height / 2);
            tryCompare(card, "detail", name);
            const width = card.width;
            const back = findChild(card, "siteInformationBack");
            verify(back.visible);
            compare(back.text, "‹ drill.example");
            mouseClick(back, back.width / 2, back.height / 2);
            tryCompare(card, "detail", "");
            byPointer.push(width);
        }
        compare(byPointer, [400, 400, 400, 400]);

        // The keyboard is on the first tile when the card opens; the arrows
        // walk the grid, two to a row.
        const byKeyboard = [];
        const moves = [[], [Qt.Key_Right], [Qt.Key_Down], [Qt.Key_Down, Qt.Key_Right]];
        for (let index = 0; index < tiles.length; ++index) {
            closeSiteCard();
            window.commands.run("site-information", -1);
            tryVerify(function () {
                return card.visible;
            });
            // The lab has no third-party filter, and the card asks it again
            // each time it opens.
            card.refusedThirdParties = ["https://pay.example"];
            for (const key of moves[index])
                keyClick(key);
            keyClick(Qt.Key_Return);
            byKeyboard.push(card.detail);
            keyClick(Qt.Key_Escape);
            byKeyboard.push(card.detail + (card.visible ? ":open" : ":closed"));
        }
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !card.visible;
        });
        card.refusedThirdParties = [];
        card.blocker = cardBlocker;
        card.thirdPartyCookieControlAvailable = control;
        compare(byKeyboard, ["certificate", ":open", "blocked", ":open", "cookies", ":open",
                             "third-parties", ":open"]);
    }

    // A page that sets its own cursor, as the engine's view does over a link.
    Component {
        id: pageCursorComponent

        MouseArea {
            property int presses: 0

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onPressed: presses += 1
        }
    }

    // The page still gets hover under the site status, so the pointer wears
    // what the page asks for there. The click closes the status and no more.
    function test_siteStatusLeavesThePageItsCursor() {
        const engine = openPage("https://status-cursor.example");
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const page = createTemporaryObject(pageCursorComponent, engine);
        verify(page !== null);
        const x = page.width / 2;
        const y = page.height / 2;

        mouseMove(page, x, y);
        tryVerify(function () {
            return cursorProbe.shape(window) === Qt.PointingHandCursor;
        });

        window.openSiteInformation("");
        mouseMove(page, x + 2, y);
        tryVerify(function () {
            return cursorProbe.shape(window) === Qt.PointingHandCursor;
        });

        mouseClick(page, x + 2, y);
        tryCompare(window, "siteInformationOpen", false);
        compare(page.presses, 0);
        mouseClick(page, x + 2, y);
        compare(page.presses, 1);
    }

    // The Start page is still drawn while it drops away from a page that
    // replaced it, and a click in that time is the reader's first on the page.
    function test_firstClickReachesThePageThatReplacedTheStartPage() {
        const startPage = findChild(window.contentItem, "startPage");
        // A blank tab stands the Start page in, and the address typed there
        // replaces it with a page.
        browser.openInput("about:blank", true);
        tryCompare(startPage, "open", true);
        tryCompare(startPage, "visible", true);

        const engine = openPage("https://first-click.example");
        const page = createTemporaryObject(pageCursorComponent, engine);
        verify(page !== null);
        verify(!startPage.open);
        verify(startPage.visible, "the drop was over before the click");

        mouseClick(page, page.width / 2, page.height / 2);
        verify(startPage.visible, "the drop was over before the click");
        compare(page.presses, 1);
    }

    // On show over a page, the Start page stands in for it: a click on the
    // Start page is not the page's.
    function test_theStartPageOnShowKeepsClicksFromThePageBeneath() {
        const startPage = findChild(window.contentItem, "startPage");
        const engine = openPage("https://beneath-start.example");
        settleMotion();
        const page = createTemporaryObject(pageCursorComponent, engine);
        verify(page !== null);

        window.startPageSummoned = true;
        tryCompare(startPage, "opacity", 1);
        verify(startPage.open);
        mouseClick(page, page.width / 2, page.height - 20);
        compare(page.presses, 0);

        window.startPageSummoned = false;
        tryCompare(startPage, "visible", false);
        mouseClick(page, page.width / 2, page.height - 20);
        compare(page.presses, 1);
    }

    // A pin is a square holding one chip, with nothing in front of anything to
    // put a speaker before, so it wears the speaker in its top right corner.
    function test_soundingPinWearsItsSpeakerInTheCorner() {
        const engine = openPage("https://sounding-pin.example");
        // As above: the corner speaker is about muting, so the origin is dealt
        // with before the press is expected to mean it.
        engine.simulateUserActivation();
        const tabId = browser.activeTabId;
        browser.toggleActivePinned();
        tryVerify(function () {
            return findChild(window.contentItem, "pinned-" + tabId) !== null;
        });
        const pinRow = findChild(window.contentItem, "pinned-" + tabId);
        const audioButton = findChild(window.contentItem, "audio-" + tabId);
        verify(audioButton !== null);
        verify(!audioButton.visible);

        engine.simulateAudible(true);
        tryVerify(function () {
            return audioButton.visible;
        });
        verify(audioButton.x + audioButton.width > pinRow.width / 2);
        verify(audioButton.y + audioButton.height < pinRow.height / 2);

        mouseClick(pinRow, audioButton.x + audioButton.width / 2, audioButton.y + audioButton.height
                   / 2);
        tryVerify(function () {
            return engine.audioMuted;
        });
        // The pin was not activated by the click that muted it.
        compare(browser.activeTabId, tabId);

        browser.toggleTabMuted(tabId);
        tryVerify(function () {
            return !engine.audioMuted;
        });
        engine.simulateAudible(false);
        browser.toggleActivePinned();
    }

    function test_layoutKeepsChromeOutOfThePagesWay() {
        const settledSidebar = findChild(window.contentItem, "sidebar");
        window.sidebarCollapsed = false;
        tryVerify(function () {
            return settledSidebar.visible && settledSidebar.width > 200;
        });

        const spaceHeading = findChild(window.contentItem, "spaceHeading");
        const sidebarNavigation = findChild(window.contentItem, "sidebarNavigation");
        const engineViewport = findChild(window.contentItem, "engineViewport");
        const navigationCluster = findChild(window.contentItem, "navigationCluster");
        verify(spaceHeading !== null);
        verify(sidebarNavigation !== null);
        verify(engineViewport !== null);
        verify(navigationCluster !== null);
        compare(engineViewport.height, window.height);

        // Navigation floats over the page instead of taking a band above it,
        // and it floats where the outline's own controls are: at the top, so
        // hiding the sidebar does not move the commands to the floor.
        verify(navigationCluster.height < engineViewport.height / 4);
        verify(navigationCluster.y < engineViewport.height / 2);
        verify(navigationCluster.mapToItem(engineViewport, 0, 0).y < sidebarNavigation.mapToItem(
                   engineViewport, 0, 0).y + sidebarNavigation.height);

        // Navigation opens the outline; pinned rows sit above the tabs.
        browser.toggleActivePinned();
        const pinnedRow = findChild(window.contentItem, "pinned-" + browser.activeTabId);
        verify(pinnedRow !== null);
        tryVerify(function () {
            return pinnedRow.visible && pinnedRow.height > 0;
        });

        const sidebar = findChild(window.contentItem, "sidebar");
        const navigationTop = sidebarNavigation.mapToItem(sidebar, 0, 0).y;
        tryVerify(function () {
            return pinnedRow.mapToItem(sidebar, 0, 0).y > navigationTop;
        });

        verify(spaceHeading.mapToItem(sidebar, 0, 0).y > pinnedRow.mapToItem(sidebar, 0, 0).y);
        compare(findChild(sidebar, "outlineFooterRule"), null);
        const pinnedList = findChild(window.contentItem, "pinnedList");
        verify(pinnedList.capacity >= 3);
        verify(pinnedList.capacity <= 5);
        compare(pinnedList.columns, Math.min(pinnedList.capacity, browser.pinnedTabs.rowCount()));
        compare(pinnedRow.x, 0);
        compare(pinnedRow.width, pinnedList.width);
        compare(pinnedRow.height, 44);

        const tabRow = findChild(window.contentItem, "tab-" + browser.activeTabId);
        if (tabRow !== null && tabRow.visible) {
            verify(pinnedRow.mapToItem(sidebar, 0, 0).y < tabRow.mapToItem(sidebar, 0, 0).y);
        }
        browser.toggleActivePinned();
    }

    function test_hiddenSidebarCanBePeekedWithMouse_data() {
        return [
                    {
                        tag: "floating eased",
                        floating: true,
                        eased: true
                    },
                    {
                        tag: "no floating eased",
                        floating: false,
                        eased: true
                    },
                    {
                        tag: "floating immediate",
                        floating: true,
                        eased: false
                    }
                ];
    }

    function test_hiddenSidebarCanBePeekedWithMouse(data) {
        const sidebar = findChild(window.contentItem, "sidebar");
        const backdrop = findChild(window.contentItem, "sidebarBackdrop");
        const engineViewport = findChild(window.contentItem, "engineViewport");
        window.floatingControls = data.floating;
        SystemMotion.reduced = !data.eased;
        mouseMove(window.contentItem, window.width / 2, window.height / 2);
        window.sidebarCollapsed = true;
        tryCompare(sidebar, "visible", false);
        const collapsedWidth = engineViewport.width;

        mouseMove(window.contentItem, 2, window.height / 2);
        wait(300);
        verify(!sidebar.visible);
        tryCompare(sidebar, "visible", true);
        compare(window.sidebarCollapsed, true);
        compare(sidebar.floating, true);
        compare(backdrop.visible, true);
        verify(backdrop.source !== null);
        const sheetTint = Qt.color(window.colors.sheet);
        compare(backdrop.tint.r, sheetTint.r);
        compare(backdrop.tint.g, sheetTint.g);
        compare(backdrop.tint.b, sheetTint.b);
        compare(backdrop.tint.a, Math.min(sheetTint.a, 0.8));
        compare(backdrop.sourceRect.x, sidebar.x);
        compare(backdrop.sourceRect.y, sidebar.y);
        compare(backdrop.sourceRect.width, sidebar.width);
        compare(backdrop.sourceRect.height, sidebar.height);
        compare(backdrop.textureScale, 0.5);
        if (data.eased)
            verify(sidebar.x < 0);
        tryCompare(sidebar, "x", 0);
        compare(engineViewport.x, 0);
        compare(engineViewport.width, collapsedWidth);

        mouseMove(window.contentItem, window.width / 2, window.height / 2);
        wait(170);
        if (data.eased) {
            verify(sidebar.visible);
            verify(sidebar.x < 0);
            verify(sidebar.x > -sidebar.width);
        }
        tryCompare(sidebar, "visible", false);
        compare(window.sidebarCollapsed, true);
        compare(backdrop.visible, false);
        window.floatingControls = true;
        SystemMotion.reduced = false;
    }

    // A click CI saw miss, and 255 runs here never did (#507). One that misses
    // fails naming what the window held under it, so the log of the next one
    // carries its own evidence. Here something else takes the press.
    function test_aMissedClickNamesWhatTookIt() {
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("spaces");
        const create = findChild(settings, "newSpaceButton");
        settleActions(create);
        const thief = Qt.createQmlObject('import QtQuick\nMouseArea { objectName: "clickThief"; '
                                         + 'anchors.fill: parent; z: 1000 }', window.contentItem);
        const report = clickReportingAMiss(create, function () {
            return window.dialogMode === "new";
        });
        // Gone from the window's delivery now, rather than when it is deleted.
        thief.enabled = false;
        thief.destroy();
        verify(report.indexOf("taken by clickThief") >= 0, report);
        verify(report.indexOf("button saw: nothing") >= 0, report);
        verify(report.indexOf("dialog: mode \"\"") >= 0, report);
        verify(report.indexOf("focus: ") >= 0, report);
        verify(report.indexOf("moving: ") >= 0, report);

        verify(clickReportingAMiss(create, function () {
            return window.dialogMode === "new";
        }) === "");
        window.dialogMode = "";
    }

    function test_spaceActionsAreInSettings() {
        const sidebar = findChild(window.contentItem, "sidebar");
        compare(findChild(sidebar, "manageSpacesButton"), null);
        // The moves below are read against the Space on show being listed
        // first, and an earlier test may have left the window in another.
        const firstId = browser.spaces.data(browser.spaces.index(0, 0), Qt.UserRole + 1);
        if (browser.activeSpaceId !== firstId) {
            verify(browser.switchSpace(firstId));
            tryCompare(sidebar, "arriving", false);
        }
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("spaces");
        const create = findChild(settings, "newSpaceButton");
        verify(create.visible);
        settleActions(create);
        const missed = clickReportingAMiss(create, function () {
            return window.dialogMode === "new";
        });
        verify(missed.length === 0, missed);
        window.dialogMode = "";

        const activeId = browser.activeSpaceId;
        const otherId = browser.createSpace("Other Space");
        const thirdId = browser.createSpace("Third Space");
        for (const spaceId of [activeId, otherId, thirdId]) {
            const row = findChild(settings, "settingsSpace-" + spaceId);
            verify(row !== null);
            verify(row.visible);
        }
        compare(findChild(settings, "settingsSpace-" + activeId).note, "Current Space");

        // The order Spaces are listed in is the reader's. The row actions
        // follow the Space rather than the place it started in, and the
        // outline in the sidebar is listing the same model, so it moves with
        // Settings rather than waiting for a reload.
        const sidebarSpaceOrder = function () {
            return [activeId, otherId, thirdId].map(function (spaceId) {
                return findChild(sidebar, "space-" + spaceId);
            }).sort(function (left, right) {
                return left.mapToItem(sidebar, 0, 0).x - right.mapToItem(sidebar, 0, 0).x;
            }).map(function (button) {
                return button.objectName;
            });
        };
        const settingsRowY = function (spaceId) {
            return findChild(settings, "settingsSpace-" + spaceId).mapToItem(settings, 0, 0).y;
        };
        compare(sidebarSpaceOrder(), ["space-" + activeId, "space-" + otherId, "space-" + thirdId]);
        verify(!findChild(settings, "moveSpaceUp-" + activeId).enabled);
        verify(!findChild(settings, "moveSpaceDown-" + thirdId).enabled);

        const moveUp = findChild(settings, "moveSpaceUp-" + thirdId);
        settleActions(moveUp);
        mouseClick(moveUp, moveUp.width / 2, moveUp.height / 2);
        tryVerify(function () {
            return settingsRowY(thirdId) < settingsRowY(otherId);
        });
        tryCompare(browser, "activeSpaceId", activeId);
        compare(sidebarSpaceOrder(), ["space-" + activeId, "space-" + thirdId, "space-" + otherId]);
        verify(findChild(settings, "moveSpaceDown-" + thirdId).enabled);
        verify(!findChild(settings, "moveSpaceDown-" + otherId).enabled);
        // Back the way it was, so the rename and the delete below read the
        // order they were written against.
        verify(browser.moveSpaceBy(thirdId, 1));
        tryVerify(function () {
            return settingsRowY(otherId) < settingsRowY(thirdId);
        });

        const rename = findChild(settings, "renameSpace-" + otherId);
        verify(rename !== null);
        settleActions(rename);
        mouseClick(rename, rename.width / 2, rename.height / 2);
        compare(window.dialogMode, "rename");
        compare(window.dialogSpaceId, otherId);
        compare(browser.activeSpaceId, activeId);
        const dialog = findChild(window.contentItem, "spaceDialog");
        compare(dialog.presetText, "Other Space");
        dialog.accepted("Renamed Space");
        const remove = findChild(settings, "deleteSpace-" + otherId);
        settleActions(remove);
        mouseClick(remove, remove.width / 2, remove.height / 2);
        compare(window.dialogSpaceName, "Renamed Space");
        dialog.accepted("Wrong name");
        verify(findChild(settings, "settingsSpace-" + otherId) !== null);
        remove.clicked();
        dialog.accepted("Renamed Space");
        compare(browser.activeSpaceId, activeId);
        tryVerify(function () {
            return findChild(settings, "settingsSpace-" + otherId) === null;
        });
        verify(findChild(settings, "settingsSpace-" + thirdId) !== null);
        verify(browser.deleteSpace(thirdId, "Third Space"));
        window.settingsOpen = false;
    }

    function test_collapsingTheSidebarLeavesOnlyTheFloatingControls() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const engineViewport = findChild(window.contentItem, "engineViewport");
        const navigationCluster = findChild(window.contentItem, "navigationCluster");
        verify(sidebar !== null);
        verify(sidebar.visible);

        const expandedViewport = engineViewport.width;
        // While the outline is open it carries the controls itself.
        verify(!navigationCluster.visible);
        window.sidebarCollapsed = true;
        tryVerify(function () {
            return !sidebar.visible;
        });
        tryVerify(function () {
            return engineViewport.width > expandedViewport;
        });
        verify(navigationCluster.visible);

        window.sidebarCollapsed = false;
        tryVerify(function () {
            return sidebar.visible && Math.round(sidebar.x) === 0;
        });
    }

    // The seam and the strip at every frame drawn from here on.
    function watchTheSeam(sidebar, viewport, strip) {
        return watchFrames(function () {
            return {
                "seam": Math.round(viewport.x),
                "sidebarEnd": Math.round(sidebar.x + sidebar.width),
                "pageEnd": Math.round(viewport.x + viewport.width),
                "strip": strip.visible
            };
        });
    }

    // The seam in every frame drawn: the page's leading edge stays on the
    // sidebar's trailing one, and the pair still cover the row, so the
    // movement leaves no gap between them and the page is not uncovered at
    // the far edge. The strip stands in for a sidebar that has gone, so no
    // frame draws it beside one: not while the sidebar is on its way out, and
    // not once one is drawn coming back.
    function checkTheSeam(watch, row) {
        stopWatching(watch);
        verify(watch.seen.length > 0);
        for (let at = 0; at < watch.seen.length; ++at) {
            const frame = watch.seen[at];
            compare(frame.sidebarEnd, frame.seam);
            verify(frame.pageEnd >= row);
            if (frame.strip)
                compare(frame.seam, 0);
        }
    }

    // The seam eases and the page travels with it, but a width is a page
    // layout: the page keeps one for the whole slide and takes the settled one
    // when the seam stops. The sidebar keeps its own width throughout, so the
    // rows in it are not laid out again either.
    function test_theSeamEasesWithoutLayingThePageOutAgain() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        window.setSidebarWidth(window.sidebarDefaultWidth);
        const sidebar = findChild(window.contentItem, "sidebar");
        const viewport = findChild(window.contentItem, "engineViewport");
        const cluster = findChild(window.contentItem, "navigationCluster");
        verify(sidebar !== null);
        verify(viewport !== null);
        verify(cluster !== null);
        // A seam that has arrived. Its width alone would not say so: the
        // sidebar keeps that as it slides, and only where it sits does.
        tryVerify(function () {
            return Math.round(sidebar.x) === 0 && Math.round(viewport.x)
                    === window.sidebarDefaultWidth;
        });
        // The row the sidebar and the page share. Neither is allowed to leave
        // a gap in it, on the way or at either end.
        const row = Math.round(sidebar.x + sidebar.width + viewport.width);

        viewportWidthSpy.target = viewport;
        sidebarWidthSpy.target = sidebar;
        viewportWidthSpy.clear();
        sidebarWidthSpy.clear();

        let seam = watchTheSeam(sidebar, viewport, cluster);
        let slide = watchChanges(viewport, "x");
        window.commands.run("toggle-sidebar", -1);
        verify(passedBetween(slide, window.sidebarDefaultWidth, 0));
        tryVerify(function () {
            return !sidebar.visible;
        });
        checkTheSeam(seam, row);
        compare(Math.round(viewport.x), 0);
        compare(Math.round(viewport.width), row);
        compare(viewportWidthSpy.count, 1);
        verify(cluster.visible);

        viewportWidthSpy.clear();
        seam = watchTheSeam(sidebar, viewport, cluster);
        slide = watchChanges(viewport, "x");
        window.commands.run("toggle-sidebar", -1);
        verify(passedBetween(slide, 0, window.sidebarDefaultWidth));
        tryVerify(function () {
            return Math.round(viewport.width) === row - window.sidebarDefaultWidth;
        });
        checkTheSeam(seam, row);
        verify(sidebar.visible);
        compare(Math.round(sidebar.x), 0);
        compare(Math.round(viewport.x), window.sidebarDefaultWidth);
        compare(viewportWidthSpy.count, 1);

        // Neither slide laid the sidebar out again.
        compare(sidebarWidthSpy.count, 0);
        viewportWidthSpy.target = null;
        sidebarWidthSpy.target = null;
    }

    function test_sidebarWidthAnswersToBothPointerAndKeyboard() {
        window.settingsOpen = false;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        window.sidebarCollapsed = false;
        window.setSidebarWidth(window.sidebarDefaultWidth);
        const sidebar = findChild(window.contentItem, "sidebar");
        const resizer = findChild(window.contentItem, "sidebarResizer");
        verify(resizer !== null);
        tryCompare(sidebar, "width", window.sidebarDefaultWidth);

        // The handle rides the seam it moves, wherever the seam is.
        compare(Math.round(resizer.x + resizer.width / 2), Math.round(sidebar.x + sidebar.width));

        // Everything the pointer can do here, the keyboard can do too.
        resizer.forceActiveFocus();
        compare(window.activeFocusItem.objectName, "sidebarResizer");
        keyClick(Qt.Key_Right);
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 16);
        keyClick(Qt.Key_Left);
        compare(window.sidebarWidth, window.sidebarDefaultWidth);
        keyClick(Qt.Key_Right, Qt.ShiftModifier);
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 48);
        keyClick(Qt.Key_Home);
        compare(window.sidebarWidth, window.sidebarMinimumWidth);
        keyClick(Qt.Key_End);
        compare(window.sidebarWidth, window.sidebarMaximumWidth);
        keyClick(Qt.Key_Space);
        compare(window.sidebarWidth, window.sidebarDefaultWidth);
        tryCompare(sidebar, "width", window.sidebarDefaultWidth);

        // A request past either end stops at the end.
        window.setSidebarWidth(window.sidebarMaximumWidth + 400);
        compare(window.sidebarWidth, window.sidebarMaximumWidth);
        window.setSidebarWidth(0);
        compare(window.sidebarWidth, window.sidebarMinimumWidth);

        window.setSidebarWidth(window.sidebarDefaultWidth);
        window.commands.run("widen-sidebar", -1);
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 24);
        window.commands.run("narrow-sidebar", -1);
        compare(window.sidebarWidth, window.sidebarDefaultWidth);
        window.commands.run("reset-sidebar", -1);
        compare(window.sidebarWidth, window.sidebarDefaultWidth);

        // Asking a hidden sidebar for more of itself brings it back.
        window.sidebarCollapsed = true;
        window.commands.run("narrow-sidebar", -1);
        compare(window.sidebarCollapsed, true);
        window.commands.run("widen-sidebar", -1);
        compare(window.sidebarCollapsed, false);
        window.setSidebarWidth(window.sidebarDefaultWidth);
        tryCompare(sidebar, "width", window.sidebarDefaultWidth);

        // Dragging the seam moves it by the same distance the pointer travelled.
        mousePress(resizer, resizer.width / 2, 300);
        mouseMove(resizer, resizer.width / 2 + 60, 300);
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 60);
        mouseRelease(resizer, resizer.width / 2, 300);
        compare(resizer.dragging, false);
        tryCompare(sidebar, "width", window.sidebarDefaultWidth + 60);

        // The width the reader settled on outlives the session that set it.
        window.setSidebarWidth(344);
        tryVerify(function () {
            return browser.preference("sidebar-width", "") === "344";
        });
        window.setSidebarWidth(window.sidebarDefaultWidth);
        window.restoreSidebarWidth();
        compare(window.sidebarWidth, 344);

        window.setSidebarWidth(window.sidebarDefaultWidth);
    }

    function test_focusMovesBetweenTheOutlineAndThePage() {
        window.settingsOpen = false;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        window.sidebarCollapsed = false;
        const engineHost = findChild(window.contentItem, "engineLoader");
        openPage("https://focus.example");

        // Focusing the outline lands on the row the reader is already reading.
        window.commands.run("focus-sidebar", -1);
        const landed = window.activeFocusItem.objectName;
        verify(landed === "tab-" + browser.activeTabId || landed === "pinned-" + browser.activeTabId
               || landed === "addressButton");

        // Escape is the way back out of the outline.
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        window.commands.run("focus-sidebar", -1);
        verify(!engineHost.item.activeFocus);
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        // The resize handle reads as part of the sidebar, so it leaves like
        // the rest of it. Every control in there answers to the same key.
        const controls = ["addressButton", "settingsButton", "sidebarResizer", "tab-"
                          + browser.activeTabId];
        for (let index = 0; index < controls.length; ++index) {
            const control = findChild(window.contentItem, controls[index]);
            verify(control !== null);
            control.forceActiveFocus();
            compare(window.activeFocusItem.objectName, controls[index]);
            keyClick(Qt.Key_Escape);
            tryVerify(function () {
                return engineHost.item.activeFocus;
            });
        }

        // Asking a hidden outline for the keyboard shows it first.
        window.sidebarCollapsed = true;
        window.commands.run("focus-sidebar", -1);
        compare(window.sidebarCollapsed, false);
        window.commands.run("focus-page", -1);
    }

    // The sidebar's rows in the order the reader sees them: the pins, then
    // the ordinary tabs.
    function sidebarOrder() {
        const order = [];
        const models = [browser.pinnedTabs, browser.unpinnedTabs];
        for (let section = 0; section < models.length; ++section) {
            const model = models[section];
            for (let row = 0; row < model.rowCount(); ++row)
                order.push(model.data(model.index(row, 0), Qt.UserRole + 1));
        }
        return order;
    }

    // The tab whose row holds the keyboard, or nothing when no row does.
    function cursorTabId() {
        const item = window.activeFocusItem;
        return item && item.tabId !== undefined ? item.tabId : "";
    }

    function cursorDrawnOn(tabId) {
        const cursor = findChild(window.contentItem, "sidebarCursor-" + tabId);
        return cursor !== null && cursor.visible;
    }

    // While the sidebar holds the keyboard, j and k walk a cursor through its
    // rows without changing the page, l opens the row the cursor is on and
    // hands the keyboard over, and h brings the cursor back to the tab on
    // show. The cursor is drawn on the row that holds the keyboard and on no
    // other, so it is never mistaken for the tab on show.
    function test_theSidebarCursorMovesWithHjkl() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        window.settingsOpen = false;
        window.sidebarCollapsed = false;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        openPage("https://cursor-pin.example/");
        const pinId = browser.activeTabId;
        browser.toggleActivePinned();
        openPageInNewTab("https://cursor-first.example/");
        const firstId = browser.activeTabId;
        openPageInNewTab("https://cursor-second.example/");
        const secondId = browser.activeTabId;
        settleMotion();
        const order = sidebarOrder();
        verify(order.indexOf(pinId) < order.indexOf(firstId));
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        const shownPage = engineHost.item;
        const typed = shownPage.keyboardInput;

        keyClick(Qt.Key_E, Qt.ControlModifier);
        compare(cursorTabId(), secondId);
        verify(cursorDrawnOn(secondId));

        keyClick(Qt.Key_K);
        compare(cursorTabId(), order[order.indexOf(secondId) - 1]);
        compare(browser.activeTabId, secondId);
        verify(cursorDrawnOn(cursorTabId()));
        verify(!cursorDrawnOn(secondId));

        // The cursor stops at either end rather than wrapping.
        for (let step = 0; step <= order.length; ++step)
            keyClick(Qt.Key_K);
        compare(cursorTabId(), order[0]);
        for (let step = 0; step <= order.length; ++step)
            keyClick(Qt.Key_J);
        compare(cursorTabId(), order[order.length - 1]);
        compare(browser.activeTabId, secondId);

        // The pins are rows like the rest.
        for (let step = order.indexOf(pinId); step < order.length - 1; ++step)
            keyClick(Qt.Key_K);
        compare(cursorTabId(), pinId);
        // The keys went to the sidebar and none of them to the page.
        compare(shownPage.keyboardInput, typed);

        for (let step = order.indexOf(pinId); step < order.indexOf(firstId); ++step)
            keyClick(Qt.Key_J);
        compare(cursorTabId(), firstId);
        keyClick(Qt.Key_L);
        compare(browser.activeTabId, firstId);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        verify(!cursorDrawnOn(firstId));

        window.commands.run("focus-sidebar", -1);
        keyClick(Qt.Key_J);
        verify(cursorTabId() !== firstId);
        keyClick(Qt.Key_H);
        compare(cursorTabId(), firstId);

        // Opening the tab already on show changes no page, and still hands the
        // keyboard over.
        keyClick(Qt.Key_L);
        compare(browser.activeTabId, firstId);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        verify(!cursorDrawnOn(firstId));
        window.commands.run("focus-sidebar", -1);
        compare(cursorTabId(), firstId);

        // Return opens the row as l does, the tab on show too.
        keyClick(Qt.Key_Return);
        compare(browser.activeTabId, firstId);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        verify(!cursorDrawnOn(firstId));
        window.commands.run("focus-sidebar", -1);
        keyClick(Qt.Key_J);
        const belowId = cursorTabId();
        verify(belowId !== firstId);
        keyClick(Qt.Key_Enter);
        compare(browser.activeTabId, belowId);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        verify(!cursorDrawnOn(belowId));
        browser.activateTab(firstId);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        window.commands.run("focus-sidebar", -1);
        compare(cursorTabId(), firstId);

        // A press on a row focuses it, but a hand on the mouse is not steering
        // the cursor, so the row is not lit.
        const shownRow = findChild(window.contentItem, "tab-" + firstId);
        mouseClick(shownRow, shownRow.width / 3, shownRow.height / 2);
        compare(cursorTabId(), firstId);
        verify(!cursorDrawnOn(firstId));

        window.commands.run("focus-page", -1);
        browser.closeTab(secondId);
        browser.closeTab(firstId);
        browser.activateTab(pinId);
        browser.toggleActivePinned();
        browser.closeTab(pinId);
    }

    // The binding the live keymap gives a command, a chord before a single
    // key. A numbered command names the number its binding ends in.
    function bindingFor(command, number) {
        const bindings = keyboardNavigation.browserBindings;
        let single = "";
        for (const binding in bindings) {
            if (bindings[binding] !== command)
                continue;
            if (number !== undefined && binding.slice(-1) !== String(number))
                continue;
            if (binding.indexOf("+") !== -1)
                return binding;
            if (single.length === 0)
                single = binding;
        }
        return single;
    }

    // A key cap is the website's `kbd`, whichever surface draws it: 22 px
    // tall and no narrower, with a 4 px radius, the key in the accent in the
    // mono face at weight 500 and 12 px, all scaled with the interface font.
    function checkKeycap(cap, name) {
        const unit = Style.font.body / 12;
        verify(cap !== null, name);
        compare(cap.height, 22 * unit, name);
        verify(cap.width >= 22 * unit, name);
        compare(cap.radius, 4 * unit, name);
        const label = findChild(cap, "keycapLabel");
        compare(label.font.pixelSize, 12 * unit, name);
        compare(label.font.weight, Font.Medium, name);
        compare(String(label.color), String(window.colors.accent), name);
        compare(label.font.family, Style.font.family, name);
    }

    // The Start page's `?` hint is the Omnibar's hint row's: at rest, with no
    // results, the row holds `? shortcuts` as the website's key cap and its
    // word, and nothing is drawn as a line of its own under the field. With
    // results the row keeps its keys and the same hint stays at its right end.
    function test_theHintRowHoldsTheShortcutsHintAtRest() {
        const hints = findChild(window.contentItem, "omnibarHints");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting caps");
        tryVerify(function () {
            return hints.visible;
        });
        verify(findChild(window.contentItem, "startPageHint") === null);
        const hint = findChild(hints, "omnibarShortcutsHint");
        verify(hint.visible);
        const cap = findChild(hint, "keycap");
        compare(cap.text, "?");
        checkKeycap(cap, "hint");
        compare(findChild(hint, "startPageHintWord").text, "shortcuts");
        compare(childrenNamed(hints, "omnibarHintWord").length, 0);
        compare(hints.height, hints.implicitHeight);
        compare(findChild(window.contentItem, "omnibarFrame").height, panel.restHeight);

        browser.recordVisit("https://hint-rest.example/", "Hint rest");
        input.text = "hint rest";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        verify(hint.visible);
        compare(childrenNamed(hints, "omnibarHintWord").map(function (word) {
            return word.text;
        }).join(), "select,go");
        const keys = childrenNamed(hints, "omnibarHintWord")[1];
        verify(hint.mapToItem(hints, 0, 0).x > keys.mapToItem(hints, 0, 0).x + keys.width);
        const arrow = childrenNamed(hints, "keycap").filter(function (cap) {
            return cap.visible && cap.text.length > 0;
        })[0];
        compare(hint.mapToItem(hints, 0, hint.height / 2).y, arrow.mapToItem(hints, 0, arrow.height
                                                                             / 2).y);
        verify(hint.mapToItem(hints, hint.width, 0).x <= hints.width);
        input.text = "";
        leaveSpace(homeSpaceId, restingSpaceId, "Resting caps");
    }

    // Holding Primary on its own labels the chrome with the keys that run it.
    // A chord on Primary is labelled by the key that finishes it, and a key
    // pressed without Primary is labelled as itself. The labels go the moment
    // Primary is let go or another key joins it, so a chord typed at speed
    // never shows them.
    function test_holdingPrimaryLabelsTheControlsWithTheirKeys() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        window.settingsOpen = false;
        window.sidebarCollapsed = false;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        openPage("https://labels-first.example/");
        const firstId = browser.activeTabId;
        openPageInNewTab("https://labels-second.example/");
        settleMotion();
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        const controls = [["backButton", "back"], ["forwardButton", "forward"], ["reloadButton",
                                                                                 "reload"],
                          ["collapseButton", "toggle-sidebar"], ["commandScopeButton",
                                                                 "command-scope"], ["addressButton",
                                                                                    "open-address"]];
        const label = findChild(window.contentItem, "keyLabel-backButton");
        verify(label !== null);
        verify(!label.visible);

        keyPress(Qt.Key_Control);
        tryVerify(function () {
            return label.visible;
        });
        checkKeycap(findChild(label, "keycap"), "Primary held");
        for (let index = 0; index < controls.length; ++index) {
            const control = findChild(window.contentItem, "keyLabel-" + controls[index][0]);
            const binding = bindingFor(controls[index][1]);
            verify(control !== null, controls[index][0]);
            verify(control.visible, controls[index][0]);
            const chord = binding.indexOf("Primary+") === 0;
            compare(control.chord, chord, controls[index][0]);
            compare(control.text, chord ? binding.slice("Primary+".length) : binding,
                    controls[index][0]);
        }
        const spaceLabel = findChild(window.contentItem, "keyLabel-space-" + browser.activeSpaceId);
        verify(spaceLabel !== null);
        compare(spaceLabel.text, bindingFor("select-space", 1).slice("Primary+".length));

        // Each of the first nine tabs is labelled with the number that selects
        // it, and a tab past them with nothing.
        const tabs = browser.tabs;
        let other = -1;
        for (let position = 0; position < tabs.rowCount(); ++position) {
            const tabId = tabs.data(tabs.index(position, 0), Qt.UserRole + 1);
            const tabLabel = findChild(window.contentItem, "keyLabel-tab-" + tabId);
            verify(tabLabel !== null);
            if (position < 9) {
                verify(tabLabel.visible);
                compare(tabLabel.text, bindingFor("select-tab", position + 1));
                if (other < 0 && tabId !== browser.activeTabId)
                    other = position;
            } else {
                verify(!tabLabel.visible);
            }
        }
        keyRelease(Qt.Key_Control);
        verify(!label.visible);

        // The label is the truth: its number selects its tab.
        verify(other >= 0);
        const otherId = tabs.data(tabs.index(other, 0), Qt.UserRole + 1);
        keyClick(String(other + 1));
        tryCompare(browser, "activeTabId", otherId);

        // Another key joining Primary is a chord being typed, not a question,
        // and so is one a window shortcut takes before any item sees it.
        keyPress(Qt.Key_Control);
        keyPress(Qt.Key_Shift, Qt.ControlModifier);
        wait(800);
        verify(!label.visible);
        keyRelease(Qt.Key_Shift, Qt.ControlModifier);
        keyRelease(Qt.Key_Control);
        keyPress(Qt.Key_Control);
        keyPress(Qt.Key_Period, Qt.ControlModifier);
        wait(800);
        verify(!label.visible);
        keyRelease(Qt.Key_Period, Qt.ControlModifier);
        keyRelease(Qt.Key_Control);

        // The sidebar asks as the page does.
        window.commands.run("focus-sidebar", -1);
        keyPress(Qt.Key_Control);
        tryVerify(function () {
            return label.visible;
        });
        keyRelease(Qt.Key_Control);
        verify(!label.visible);

        // A key the reader moves is labelled where it now is.
        const collapseLabel = findChild(window.contentItem, "keyLabel-collapseButton");
        verify(keymapProbe.rebind("Primary+B", "Primary+Y"));
        keyPress(Qt.Key_Control);
        tryVerify(function () {
            return collapseLabel.visible;
        });
        compare(collapseLabel.text, "Y");
        verify(collapseLabel.chord);
        keyRelease(Qt.Key_Control);
        verify(keymapProbe.rebind("Primary+Y", "Primary+B"));

        // The window losing the keyboard takes the labels with it, though
        // Primary never came up in it.
        keyPress(Qt.Key_Control);
        tryVerify(function () {
            return label.visible;
        });
        windowFocusProbe.deactivate(window);
        verify(!label.visible);
        keyRelease(Qt.Key_Control);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });

        window.commands.run("focus-page", -1);
        browser.closeTab(browser.activeTabId);
        if (browser.activeTabId !== firstId)
            browser.closeTab(firstId);
    }

    // Switching Space names the Space it arrived in at the top of the page,
    // over where the page settles, and the window's title names it too. The
    // notice stands still while the page arrives and goes on its own.
    function test_aSpaceSwitchNamesTheSpaceAtTheTopOfThePage() {
        const outline = findChild(window.contentItem, "sidebar");
        const notice = findChild(window.contentItem, "spaceNotice");
        const viewport = findChild(window.contentItem, "engineViewport");
        verify(notice !== null);
        window.settingsOpen = false;
        window.sidebarCollapsed = false;
        openPage("https://notice.example/");
        settleMotion();
        const homeId = browser.activeSpaceId;
        verify(!notice.visible);
        compare(window.title, browser.activeTitle + " — " + browser.activeSpaceName + " — Omaweb");

        // Renaming the Space on show is not a switch.
        const homeName = browser.activeSpaceName;
        verify(browser.renameSpace(homeId, "Renamed Home"));
        wait(300);
        verify(!notice.visible);
        verify(browser.renameSpace(homeId, homeName));

        const otherId = browser.createSpace("Notice Space");
        verify(browser.switchSpace(otherId));
        const places = [];
        while (outline.arriving) {
            places.push(notice.mapToItem(window.contentItem, 0, 0).x);
            wait(10);
        }
        verify(places.length > 1);
        for (let index = 1; index < places.length; ++index)
            compare(places[index], places[0]);
        tryVerify(function () {
            return notice.visible;
        });
        compare(notice.text, "Notice Space");

        // It stands at the top of the page, over the middle of it.
        const place = notice.mapToItem(viewport, 0, 0);
        verify(place.y >= 0 && place.y < 24, place.y);
        fuzzyCompare(place.x + notice.width / 2, viewport.width / 2, 1);
        compare(window.title, browser.activeTitle + " — Notice Space — Omaweb");
        tryVerify(function () {
            return !notice.visible;
        }, 4000);

        // The page gives the developer tools their width, and the notice
        // stands over the middle of what is left, clear of the inspector.
        const page = findChild(window.contentItem, "engineLoader");
        const dock = findChild(window.contentItem, "developerToolsDock");
        openPage("https://notice-other.example/");
        settleMotion();
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });
        verify(browser.switchSpace(homeId));
        tryVerify(function () {
            return !notice.visible && !outline.arriving;
        }, 4000);
        verify(browser.switchSpace(otherId));
        tryVerify(function () {
            return notice.visible && !outline.arriving;
        });
        verify(dock.visible);
        const overPage = notice.mapToItem(page, 0, 0);
        fuzzyCompare(overPage.x + notice.width / 2, page.width / 2, 1);
        verify(notice.mapToItem(dock, notice.width, 0).x <= 0);
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return !dock.visible && !notice.visible;
        }, 4000);

        // Under reduced motion the notice is shown and taken away where it
        // stands.
        SystemMotion.reduced = true;
        verify(browser.switchSpace(homeId));
        tryVerify(function () {
            return notice.visible;
        });
        compare(notice.drop, 0);
        tryVerify(function () {
            return !notice.visible;
        }, 4000);
        SystemMotion.reduced = false;

        verify(browser.deleteSpace(otherId, "Notice Space"));
    }

    // A Private window has no Space to name: its title says only what it is,
    // and it never shows a Space notice, since it cannot be switched to
    // another Space.
    function test_aPrivateWindowNamesNoSpace() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const notice = findChild(privateBrowser.contentItem, "spaceNotice");
        verify(notice !== null);

        privateBrowser.windowBrowser.openInput("https://private-title.example", false);
        tryVerify(function () {
            return privateBrowser.windowBrowser.activeTitle.length > 0;
        });
        compare(privateBrowser.title, "Private — Omaweb");
        compare(privateBrowser.windowBrowser.createSpace("Private Space"), "");
        verify(!notice.visible);

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        tryVerify(function () {
            return window.privateWindows.length === 0;
        });
    }

    // A Private window's browser goes with it, and what the window still
    // asks of it on the way out is answered rather than thrown.
    function test_closingAPrivateWindowRaisesNoError() {
        failOnWarning(/TypeError/);
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        privateBrowser.windowBrowser.openInput("https://private-close.example", false);
        tryVerify(function () {
            return findChild(privateBrowser.contentItem, "engineLoader").item !== null;
        });

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        tryVerify(function () {
            return window.privateWindows.length === 0;
        });
        wait(50);
    }

    // The keyboard moves between the regions on screen by direction: the
    // outline, the page — a pane at a time while a split is on show — and the
    // inspector. A move with nothing that way leaves the keyboard where it
    // is, and a region that is not on screen is neither landed on nor shown.
    function test_focusMovesBetweenTheRegionsOnScreen() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const outline = findChild(window.contentItem, "sidebar");
        const dock = findChild(window.contentItem, "developerToolsDock");
        window.settingsOpen = false;
        window.sidebarCollapsed = false;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        openPage("https://regions.example/");
        const pageTabId = browser.activeTabId;
        settleMotion();
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        // Left of the page is the outline, under the keys the default table
        // binds.
        keyClick(Qt.Key_H, Qt.AltModifier);
        tryVerify(function () {
            return focusIsInside(outline);
        });

        // Nothing stands left of the outline, and nothing above or below any
        // of the regions.
        const landed = window.activeFocusItem;
        keyClick(Qt.Key_H, Qt.AltModifier);
        compare(window.activeFocusItem, landed);
        keyClick(Qt.Key_J, Qt.AltModifier);
        compare(window.activeFocusItem, landed);
        keyClick(Qt.Key_K, Qt.AltModifier);
        compare(window.activeFocusItem, landed);

        keyClick(Qt.Key_L, Qt.AltModifier);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        // The inspector is a region of its own for as long as it is docked.
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return dock.visible;
        });
        keyClick(Qt.Key_L, Qt.AltModifier);
        tryVerify(function () {
            return focusIsInside(dock);
        });
        keyClick(Qt.Key_L, Qt.AltModifier);
        verify(focusIsInside(dock));
        keyClick(Qt.Key_H, Qt.AltModifier);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        window.commands.run("developer-tools", -1);
        tryVerify(function () {
            return !dock.visible;
        });

        // A closed inspector and a hidden outline are not regions: the move
        // finds nothing that way and shows neither of them.
        keyClick(Qt.Key_L, Qt.AltModifier);
        verify(engineHost.item.activeFocus);
        window.sidebarCollapsed = true;
        keyClick(Qt.Key_H, Qt.AltModifier);
        verify(engineHost.item.activeFocus);
        compare(window.sidebarCollapsed, true);
        window.sidebarCollapsed = false;

        // A sheet standing in the page area takes it out of the row: a move
        // towards it finds nothing rather than landing on a page the reader
        // cannot see.
        window.historyOpen = true;
        window.commands.run("focus-sidebar", -1);
        tryVerify(function () {
            return focusIsInside(outline);
        });
        const inOutline = window.activeFocusItem;
        keyClick(Qt.Key_L, Qt.AltModifier);
        compare(window.activeFocusItem, inOutline);
        window.historyOpen = false;

        // Each pane of a split is a region: the move between them is the move
        // that focuses the tab beside, and the outline is still one step left
        // of the left pane rather than of whichever pane is focused.
        browser.openInput("https://regions-beside.example/", true);
        const besideTabId = browser.activeTabId;
        browser.activateTab(pageTabId);
        verify(browser.addSplit(besideTabId));
        tryCompare(browser, "splitOnShow", true);
        compare(browser.splitLeftTabId, pageTabId);
        settleMotion();
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        keyClick(Qt.Key_L, Qt.AltModifier);
        tryCompare(browser, "activeTabId", besideTabId);
        keyClick(Qt.Key_H, Qt.AltModifier);
        tryCompare(browser, "activeTabId", pageTabId);
        keyClick(Qt.Key_H, Qt.AltModifier);
        tryVerify(function () {
            return focusIsInside(outline);
        });
        compare(browser.activeTabId, pageTabId);

        // Right of the outline is the pane nearest it, not the one beside.
        keyClick(Qt.Key_L, Qt.AltModifier);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        compare(browser.activeTabId, pageTabId);

        // Right of the outline is the pane nearest it whichever pane the
        // keyboard left, and entering a pane is what makes its tab the active
        // one: the reader lands where they pressed towards.
        window.commands.run("focus-split-partner", -1);
        tryCompare(browser, "activeTabId", besideTabId);
        // The page that has just been focused takes the keyboard a turn
        // later, so the outline is asked for it once that has happened.
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        window.commands.run("focus-sidebar", -1);
        tryVerify(function () {
            return focusIsInside(outline);
        });
        compare(browser.activeTabId, besideTabId);
        keyClick(Qt.Key_L, Qt.AltModifier);
        tryCompare(browser, "activeTabId", pageTabId);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });

        browser.closeTab(besideTabId);
        browser.closeTab(pageTabId);
    }

    function test_pageHintsReceiveTheActiveThemeAndFont() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        tryVerify(function () {
            return engineHost.item !== null;
        });

        const hintTheme = engineHost.item.keyboardNavigationConfiguration.hintTheme;
        compare(String(hintTheme.surface), String(window.colors.surface));
        compare(String(hintTheme.text), String(window.colors.text));
        compare(String(hintTheme.accent), String(window.colors.accent));
        compare(hintTheme.font.family, window.colors.font.family);
        compare(hintTheme.font.size, window.colors.font.size);
    }

    function test_pageReceivesGgDespiteBrowserSequences() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        tryVerify(function () {
            return engineHost.item !== null;
        });
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return engineHost.item.activeFocus;
        });
        engineHost.item.keyboardInput = "";

        keyClick(Qt.Key_G);
        keyClick(Qt.Key_G);

        compare(engineHost.item.keyboardInput, "gg");
    }

    function test_switchingTabsPreservesPageLocalState() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        openPage("https://first.example");

        const firstTabId = browser.activeTabId;
        engineHost.item.pageLocalState = "edited form value";

        browser.openInput("https://second.example", true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item.currentUrl.toString()
                    === "https://second.example";
        });

        browser.activateTab(firstTabId);
        tryCompare(engineHost.item, "pageLocalState", "edited form value");
    }

    // A tab the reader is not looking at goes on holding its page and stops
    // spending on it: the renderer freezes rather than running timers,
    // animations and script behind the tab that replaced it. Selecting it
    // again continues the page rather than loading it a second time.
    function test_backgroundTabStopsRunningUntilItIsSelectedAgain() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        openPage("https://foreground.example");

        const firstTabId = browser.activeTabId;
        const firstEngine = engineHost.item;
        firstEngine.pageLocalState = "still here";
        compare(firstEngine.pageFrozen, false);

        browser.openInput("https://replacement.example", true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item !== firstEngine;
        });
        const secondTabId = browser.activeTabId;
        tryCompare(firstEngine, "pageFrozen", true);
        compare(engineHost.item.pageFrozen, false);

        browser.activateTab(firstTabId);
        tryCompare(firstEngine, "pageFrozen", false);
        tryCompare(engineHost.engines[secondTabId], "pageFrozen", true);
        // The same engine, with what the page held still in it.
        compare(engineHost.item, firstEngine);
        compare(firstEngine.pageLocalState, "still here");

        browser.closeTab(secondTabId);
        browser.closeTab(firstTabId);
    }

    // Two hidden pages are not idle. One is being watched through the
    // inspector, which is the whole reason it was kept, and Qt refuses to
    // freeze it anyway. The other is being heard.
    function test_watchedAndSoundingTabsKeepRunningWhileHidden() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        openPage("https://watched.example");
        const watchedTabId = browser.activeTabId;
        const watchedEngine = engineHost.item;
        browser.openDeveloperTools();

        browser.openInput("https://sounding.example", true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item !== watchedEngine;
        });
        const soundingTabId = browser.activeTabId;
        const soundingEngine = engineHost.item;
        soundingEngine.simulateAudible(true);

        browser.openInput("https://elsewhere.example", true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item !== soundingEngine;
        });
        const elsewhereTabId = browser.activeTabId;

        tryCompare(engineHost.item, "pageFrozen", false);
        compare(watchedEngine.pageFrozen, false);
        compare(soundingEngine.pageFrozen, false);

        // Neither exception outlives its reason.
        browser.closeDeveloperTools();
        soundingEngine.simulateAudible(false);
        tryCompare(watchedEngine, "pageFrozen", true);
        tryCompare(soundingEngine, "pageFrozen", true);

        browser.closeTab(elsewhereTabId);
        browser.closeTab(soundingTabId);
        browser.closeTab(watchedTabId);
    }

    // Keep active is the reader asking for a page to go on running while they
    // are looking at something else, so freezing never reaches it: not behind
    // another tab of its own Space, and not while its Space is away. Taking the
    // setting away hands the tab back to the ordinary rule.
    function test_keepActivePinGoesOnRunningWhereverItIs() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        const personalSpaceId = browser.activeSpaceId;

        openPage("https://kept-running.example");
        const keptTabId = browser.activeTabId;
        browser.toggleActivePinned();
        verify(browser.setTabKeepActive(keptTabId, true));
        const keptEngine = engineLoader.engines[keptTabId];
        verify(keptEngine !== undefined);

        // Behind another tab of its own Space.
        browser.openInput("https://beside-the-pin.example", true);
        tryVerify(function () {
            return engineLoader.item !== null && engineLoader.item !== keptEngine;
        });
        const elsewhereTabId = browser.activeTabId;
        tryCompare(engineLoader.item, "pageFrozen", false);
        wait(50);
        compare(keptEngine.pageFrozen, false);

        // And while its Space is away, which is what the setting names.
        const awaySpaceId = browser.createSpace("Away");
        verify(browser.switchSpace(awaySpaceId));
        verify(engineLoader.keepsEngineFor(keptTabId));
        wait(50);
        compare(keptEngine.pageFrozen, false);

        verify(browser.switchSpace(personalSpaceId));
        tryVerify(function () {
            return engineLoader.engines[keptTabId] === keptEngine;
        });
        wait(50);
        compare(keptEngine.pageFrozen, false);

        // Unticked, it is an ordinary background tab and stops like one.
        browser.activateTab(elsewhereTabId);
        verify(browser.setTabKeepActive(keptTabId, false));
        tryCompare(keptEngine, "pageFrozen", true);

        browser.activateTab(keptTabId);
        browser.toggleActivePinned();
        browser.closeTab(keptTabId);
        browser.closeTab(elsewhereTabId);
        verify(browser.deleteSpace(awaySpaceId, "Away"));
    }

    function test_primaryChromeIsAccessibleFromKeyboard() {
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        const addressButton = findChild(window.contentItem, "addressButton");
        const collapseButton = findChild(window.contentItem, "collapseButton");
        const reloadButton = findChild(window.contentItem, "reloadButton");
        const commandScopeButton = findChild(window.contentItem, "commandScopeButton");
        const newSpaceButton = findChild(window.contentItem, "newSpaceButton");
        const settingsButton = findChild(window.contentItem, "settingsButton");
        const materialSymbolsFont = findChild(window, "materialSymbolsFont");

        compare(settingsButton.accessibleName, "Browsing settings and downloads");
        compare(addressButton.accessibleName, "Search or enter address");
        compare(collapseButton.accessibleName, "Hide sidebar");
        compare(commandScopeButton.accessibleName, "Search commands");
        compare(newSpaceButton.label, "New Space");
        verify(iconFontSource.toString().endsWith("/material-symbols-rounded.ttf"));
        verify(materialSymbolsFont !== null);
        tryCompare(materialSymbolsFont, "status", FontLoader.Ready);
        verify(addressButton.activeFocusOnTab);
        verify(collapseButton.activeFocusOnTab);

        addressButton.forceActiveFocus();
        compare(window.activeFocusItem.objectName, "addressButton");

        let visitedTab = false;
        for (let step = 0; step < 20 && window.activeFocusItem.objectName
             !== "settingsButton"; ++step) {
            keyClick(Qt.Key_Tab);
            if (window.activeFocusItem.objectName.indexOf("tab-") === 0)
                visitedTab = true;
        }
        verify(visitedTab);
        compare(window.activeFocusItem.objectName, "settingsButton");
        keyClick(Qt.Key_Backtab);
        verify(window.activeFocusItem.objectName.indexOf("space-") === 0);
    }

    function test_everyBrowserCommandIsBoundAndSearchable() {
        const bindings = keyboardNavigation.browserBindings;
        verify(Object.keys(bindings).length > 0);
        compare(bindings.J, "next-tab");
        compare(bindings.K, "previous-tab");
        compare(bindings.X, "reopen-tab");
        compare(bindings.u, undefined);
        compare(keyboardNavigation.bindings.d, "scroll-half-page-down");
        compare(keyboardNavigation.bindings.u, "scroll-half-page-up");

        // The panel is the keymap: every action it lists carries its keys, and
        // every configured command is reachable from it.
        const actions = window.commands.actions();
        const listed = {};
        for (let index = 0; index < actions.length; ++index) {
            listed[actions[index].command] = actions[index];
        }
        for (const binding in bindings) {
            const command = bindings[binding];
            verify(listed[command] !== undefined);
            verify(listed[command].keys.length > 0);
        }

        const matches = window.commands.search("reopen");
        verify(matches.length > 0);
        compare(matches[0].command, "reopen-tab");
    }

    // The panel groups by where a command acts. Showing the extensions opens a
    // surface the window owns, the way Settings and Downloads do; it acts on no
    // tab, and under TABS a reader looking for it would be reading the wrong
    // list.
    function test_showingTheExtensionsIsGroupedWithTheWindowsOwnSurfaces() {
        const actions = window.commands.actions();
        const grouped = {};
        for (let index = 0; index < actions.length; ++index) {
            grouped[actions[index].command] = actions[index].group;
        }
        compare(grouped["extension-popup"], "interface");
        // Named beside the surfaces it belongs with, so a later move takes
        // these three together or fails here.
        compare(grouped["settings"], "interface");
        compare(grouped["downloads"], "interface");
        compare(grouped["history"], "interface");
    }

    function test_tabCommandsWalkTheModelInOrder() {
        const start = browser.activeTabId;
        const count = browser.tabs.rowCount();
        verify(count > 1);

        window.stepTab(1);
        verify(browser.activeTabId !== start);
        window.stepTab(-1);
        tryCompare(browser, "activeTabId", start);

        for (let step = 0; step < count; ++step) {
            window.stepTab(1);
        }
        tryCompare(browser, "activeTabId", start);
    }

    // Ctrl+O and Ctrl+I walk the Tab jump list from anywhere in the window.
    // Ctrl+I is the byte a terminal reads as Tab, and the window keeps the two
    // apart: a plain Tab moves focus and jumps nowhere, and Ctrl+I with nowhere
    // to jump leaves focus where it was.
    function test_jumpsBetweenTabsWithControlOAndControlI() {
        openPage("https://jump-first.example/");
        const firstTabId = browser.activeTabId;
        browser.openInput("https://jump-second.example/", true);
        const secondTabId = browser.activeTabId;
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });

        keyClick(Qt.Key_O, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", firstTabId);
        keyClick(Qt.Key_I, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", secondTabId);
        keyClick(Qt.Key_O, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", firstTabId);

        const addressButton = findChild(window.contentItem, "addressButton");
        addressButton.forceActiveFocus();
        keyClick(Qt.Key_Tab);
        verify(window.activeFocusItem !== addressButton);
        wait(50);
        compare(browser.activeTabId, firstTabId);

        keyClick(Qt.Key_I, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", secondTabId);
        addressButton.forceActiveFocus();
        keyClick(Qt.Key_I, Qt.ControlModifier);
        compare(window.activeFocusItem, addressButton);
        compare(browser.activeTabId, secondTabId);

        browser.closeTab(secondTabId);
    }

    // The shipped keymap is what the command panel and the shortcut sheet show
    // for the jumps, and opening a file is left in the panel without a key.
    function test_theJumpKeysReadTheSameInThePanelAndOnTheSheet() {
        const bindings = keyboardNavigation.browserBindings;
        compare(bindings["Primary+O"], "jump-back");
        compare(bindings["Primary+I"], "jump-forward");

        const listed = {};
        const actions = window.commands.actions();
        for (let index = 0; index < actions.length; ++index)
            listed[actions[index].command] = actions[index];
        compare(listed["jump-back"].group, "tabs");
        compare(listed["jump-back"].keys, "Ctrl+O");
        compare(listed["jump-forward"].group, "tabs");
        compare(listed["jump-forward"].keys, "Ctrl+I");
        verify(listed["open-file"] !== undefined);
        compare(listed["open-file"].keys, "");

        const sheet = findChild(window.contentItem, "shortcutSheet");
        const keysByTitle = {};
        const tabs = sheet.sections.filter(function (section) {
            return section.group === "tabs";
        })[0];
        for (let index = 0; index < tabs.entries.length; ++index)
            keysByTitle[tabs.entries[index].title] = tabs.entries[index].keys;
        compare(keysByTitle[listed["jump-back"].title], "Ctrl+O");
        compare(keysByTitle[listed["jump-forward"].title], "Ctrl+I");
    }

    // Only the Space on show keeps live pages. Putting one away takes its
    // renderers with it: coming back reloads its tabs from their addresses
    // rather than finding the very pages that were left. That is the memory
    // policy the browser is built on, not a shortcoming of the switch.
    // Putting a Space away stops its pages rather than taking them: every page
    // the reader opened in it freezes, and coming back finds each one where it
    // was rather than loading it again. What the Space costs while it is away
    // is a renderer per tab the reader actually opened in it.
    function test_spaceSuspensionStopsThePagesItKeeps() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        openPage("https://personal-background.example");
        const personalSpaceId = browser.activeSpaceId;
        const backgroundTabId = browser.activeTabId;
        const backgroundEngineView = engineLoader.item;
        backgroundEngineView.pageLocalState = "the tab behind";

        browser.openInput("https://personal-space.example", true);
        tryVerify(function () {
            return engineLoader.item !== null && engineLoader.item !== backgroundEngineView;
        });
        const personalTabId = browser.activeTabId;
        const personalEngineView = engineLoader.item;
        personalEngineView.pageLocalState = "where the reader was";
        const workSpaceId = browser.createSpace("Work");

        verify(browser.switchSpace(workSpaceId));
        // Both pages are stopped, neither is gone, and neither is being
        // retained: retention is a different question from existence.
        tryCompare(personalEngineView, "pageFrozen", true);
        tryCompare(backgroundEngineView, "pageFrozen", true);
        compare(engineLoader.engines[personalTabId], personalEngineView);
        compare(engineLoader.engines[backgroundTabId], backgroundEngineView);
        verify(!engineLoader.keepsEngineFor(personalTabId));
        verify(!engineLoader.keepsEngineFor(backgroundTabId));
        openPage("https://work-space.example");
        const workTabId = browser.activeTabId;

        verify(browser.switchSpace(personalSpaceId));
        // The same page, running again, with what it held still in it.
        tryVerify(function () {
            return engineLoader.item === personalEngineView;
        });
        tryCompare(personalEngineView, "pageFrozen", false);
        compare(personalEngineView.pageLocalState, "where the reader was");
        compare(engineLoader.item.profilePath, browser.activeProfilePath);

        // And the tab behind it is the same page too, still stopped until it is
        // the one being read.
        compare(engineLoader.engines[backgroundTabId], backgroundEngineView);
        compare(backgroundEngineView.pageFrozen, true);
        browser.activateTab(backgroundTabId);
        tryCompare(backgroundEngineView, "pageFrozen", false);
        compare(backgroundEngineView.pageLocalState, "the tab behind");

        verify(browser.deleteSpace(workSpaceId, "Work"));
        compare(browser.activeSpaceId, personalSpaceId);
        verify(engineLoader.engines[workTabId] === undefined);
        browser.closeTab(backgroundTabId);
    }

    // The core's Agent rules as the page area hears them: the tabs that are
    // Agent tabs, the page verbs to hand on, and the answers that come back.
    Component {
        id: agentControlComponent

        QtObject {
            property var agentTabIds: []
            property var tabs: ({})
            property var answers: ({})
            property var agentWindowIds: []
            property var closedWindows: []
            property var agentActivity: ({})
            property var windowOpeners: ({})
            signal agentTabsChanged
            signal pageRequested(int requestId, var request)
            signal pageRequestsCancelled
            signal pageRequestsCancelledIn(var targetIds)
            signal windowCloseRequested(string windowId)

            function agentTab(tabId) {
                return tabs[tabId] || ({});
            }
            function attachWindow(openerTabId) {
                if (agentTabIds.indexOf(openerTabId) < 0)
                    return "";
                const windowId = "window-" + (agentWindowIds.length + closedWindows.length + 1);
                windowOpeners[windowId] = openerTabId;
                agentWindowIds = agentWindowIds.concat([windowId]);
                return windowId;
            }
            function agentWindow(windowId) {
                return agentWindowIds.indexOf(windowId) >= 0 ? {
                                                                   "windowId": windowId,
                                                                   "openerTabId":
                                                                   windowOpeners[windowId],
                                                                   "connection": "claude-code",
                                                                   "downloadDirectory":
                                                                   "/downloads/Agents/test"
                                                               } : ({});
            }
            function windowClosed(windowId) {
                agentWindowIds = agentWindowIds.filter(function (id) {
                    return id !== windowId;
                });
                closedWindows = closedWindows.concat([windowId]);
            }
            // What the page area passed on from each page's console.
            property var consoleMessages: []
            property var consoleDocuments: []
            function recordConsoleMessage(tabId, document, level, message, source, line) {
                consoleMessages = consoleMessages.concat([
                                                             {
                                                                 "tabId": tabId,
                                                                 "document": document,
                                                                 "level": level,
                                                                 "message": message,
                                                                 "source": source,
                                                                 "line": line
                                                             }
                                                         ]);
            }
            function startConsoleDocument(tabId, document) {
                consoleDocuments = consoleDocuments.concat([
                                                               {
                                                                   "tabId": tabId,
                                                                   "document": document
                                                               }
                                                           ]);
            }
            // How many times each request was answered: the core hears the
            // first answer only, so a second is a page answering for another.
            property var answerCounts: ({})
            function answerPage(requestId, answer) {
                const next = Object.assign({}, answers);
                next[requestId] = answer;
                answers = next;
                const counts = Object.assign({}, answerCounts);
                counts[requestId] = (counts[requestId] || 0) + 1;
                answerCounts = counts;
            }
        }
    }

    // A window an Agent tab's page opens is the Agent's: it answers the page
    // verbs for the id the core gave it, keeps the reader's keyboard off its
    // page, and closes when the Agent closes it. A page asking for a tab opens
    // such a window too, since a tab would take the reader's view. A window
    // the reader's own page opens stays the reader's.
    function test_anAgentTabsWindowIsTheAgents() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const control = agentControlComponent.createObject(testCase);
        engineLoader.agentControl = control;
        const readerEngine = openPage("https://reader-opener.example/");
        const agentEngine = openPageInNewTab("https://agent-opener.example/");
        const agentTabId = browser.activeTabId;
        verify(agentEngine !== readerEngine);
        const tabs = {};
        tabs[agentTabId] = {
            "tabId": agentTabId,
            "spaceId": browser.activeSpaceId,
            "url": "https://agent-opener.example/",
            "downloadDirectory": "/downloads/Agents/test"
        };
        control.tabs = tabs;
        control.agentTabIds = [agentTabId];
        control.agentTabsChanged();
        compare(agentEngine.agentOwned, true);
        compare(agentEngine.agentDownloadDirectory, "/downloads/Agents/test");
        compare(readerEngine.agentOwned, false);
        // Another connection takes the tab over, and its downloads go to that
        // connection's directory from then on.
        tabs[agentTabId].downloadDirectory = "/downloads/Agents/other";
        control.agentTabsChanged();
        compare(agentEngine.agentDownloadDirectory, "/downloads/Agents/other");
        tabs[agentTabId].downloadDirectory = "/downloads/Agents/test";
        control.agentTabsChanged();

        // One window at a time, each gone before the next is opened.
        const openedWindow = function (windowId) {
            let found = null;
            tryVerify(function () {
                found = findChild(window, "auxiliaryWindow");
                return found !== null && found.visible;
            });
            compare(found.agentWindowId, windowId);
            const loader = findChild(found.contentItem, "auxiliaryEngineLoader");
            tryVerify(function () {
                return loader.item !== null;
            });
            return found;
        };
        const engineOf = function (auxiliary) {
            return findChild(auxiliary.contentItem, "auxiliaryEngineLoader").item;
        };
        const gone = function () {
            tryVerify(function () {
                return findChild(window, "auxiliaryWindow") === null;
            });
        };

        agentEngine.simulateNewWindowRequest("https://sign-in.example/", true);
        const opened = openedWindow("window-1");
        verify(opened.agentDriven);
        compare(engineOf(opened).agentOwned, true);
        compare(engineOf(opened).agentDownloadDirectory, "/downloads/Agents/test");
        compare(engineOf(opened).pageTakesFocus, false);
        // Its console is the Agent's too, under the window's id, and a new
        // document there starts again.
        engineOf(opened).pageConsoleMessage(2, "popup", 1, "", engineOf(opened).pageGeneration);
        const popupSaid = control.consoleMessages[control.consoleMessages.length - 1];
        compare(popupSaid.tabId, "window-1");
        compare(popupSaid.message, "popup");
        const documentsBefore = control.consoleDocuments.length;
        engineOf(opened).currentUrl = "https://sign-in.example/next";
        compare(control.consoleDocuments.length, documentsBefore + 1);
        compare(control.consoleDocuments[documentsBefore].tabId, "window-1");
        compare(control.consoleDocuments[documentsBefore].document, String(engineOf(
                                                                               opened).pageGeneration));

        control.pageRequested(21, {
                                  "verb": "look",
                                  "tabId": "window-1",
                                  "window": true,
                                  "downloadDirectory": "/downloads/Agents/test",
                                  "arguments": {}
                              });
        compare(engineOf(opened).agentRequests.length, 1);
        compare(agentEngine.agentRequests.length, 0);
        tryVerify(function () {
            return control.answers[21] !== undefined && control.answers[21].ok === true;
        });
        // Answered by the window alone, not by the tab that opened it too.
        wait(50);
        compare(control.answerCounts[21], 1);

        control.windowCloseRequested("window-1");
        gone();
        verify(control.closedWindows.indexOf("window-1") >= 0);

        // A tab asked for by the Agent's page is a window of the Agent's.
        const tabCount = browser.tabs.rowCount();
        agentEngine.simulateNewWindowRequest("https://checkout.example/", false);
        const second = openedWindow("window-2");
        compare(browser.tabs.rowCount(), tabCount);
        compare(browser.activeTabId, agentTabId);
        verify(second.agentDriven);
        engineOf(second).simulateWindowCloseRequest();
        gone();
        verify(control.closedWindows.indexOf("window-2") >= 0);

        // Once no Agent holds the tab, what its page opens is the reader's.
        control.agentTabIds = [];
        control.agentTabsChanged();
        compare(agentEngine.agentOwned, false);
        agentEngine.simulateNewWindowRequest("https://reader-window.example/", true);
        const readers = openedWindow("");
        verify(!readers.agentDriven);
        compare(engineOf(readers).agentOwned, false);
        compare(engineOf(readers).pageTakesFocus, true);
        engineOf(readers).simulateWindowCloseRequest();
        gone();

        engineLoader.agentControl = null;
        control.destroy();
    }

    // An Agent's window is marked as its tab is: framed in the Agent accent,
    // with who is driving it and what it did last, and plain again once the
    // Agent lets it go.
    function test_anAgentTabsWindowIsFramedLikeItsTab() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const control = agentControlComponent.createObject(testCase);
        engineLoader.agentControl = control;
        const agentEngine = openPageInNewTab("https://agent-framed-opener.example/");
        const agentTabId = browser.activeTabId;
        const tabs = {};
        tabs[agentTabId] = {
            "tabId": agentTabId,
            "spaceId": browser.activeSpaceId,
            "url": "https://agent-framed-opener.example/"
        };
        control.tabs = tabs;
        control.agentTabIds = [agentTabId];
        const report = function (act) {
            const activity = {};
            activity[agentTabId] = {
                "spaceId": browser.activeSpaceId,
                "name": "claude-code",
                "act": act,
                "busy": false
            };
            control.agentActivity = activity;
        };
        report("clicked \"Sign in\"");
        control.agentTabsChanged();

        agentEngine.simulateNewWindowRequest("https://sign-in.example/", true);
        let auxiliary = null;
        tryVerify(function () {
            auxiliary = findChild(window, "auxiliaryWindow");
            return auxiliary !== null && auxiliary.visible;
        });
        verify(auxiliary.agentDriven);
        const frame = findChild(auxiliary.contentItem, "auxiliaryAgentFrame");
        verify(frame !== null);
        verify(frame.visible);
        compare(frame.width, auxiliary.contentItem.width);
        compare(String(findChild(frame, "agentFrameBorder").border.color), String(
                    window.colors.agentAccent));
        compare(String(findChild(frame, "agentFrameLabel").color), String(
                    window.colors.agentAccent));
        compare(findChild(frame, "agentFrameCaption").text,
                "claude-code is driving · clicked \"Sign in\"");
        report("typed in \"Email\"");
        compare(findChild(frame, "agentFrameCaption").text,
                "claude-code is driving · typed in \"Email\"");

        // Let go, the window is the reader's and plain.
        control.agentWindowIds = [];
        verify(!auxiliary.agentDriven);
        verify(!frame.visible);
        auxiliary.close();
        tryVerify(function () {
            return findChild(window, "auxiliaryWindow") === null;
        });

        engineLoader.agentControl = window.agentControlSource;
        browser.closeTab(agentTabId);
        control.destroy();
    }

    // An Agent tab the reader is not looking at goes on running and stays
    // drawn, at no opacity and under the page on show, wherever its Space is,
    // and goes back to being an ordinary hidden page once no Agent uses it.
    function test_anAgentTabKeepsRunningBehindThePageOnShow() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const shield = findChild(window.contentItem, "agentTabShield");
        verify(engineLoader !== null);
        verify(shield !== null);
        const control = agentControlComponent.createObject(testCase);
        engineLoader.agentControl = control;

        const personalSpaceId = browser.activeSpaceId;
        const agentEngine = openPage("https://agent-tab.example/");
        const agentTabId = browser.activeTabId;
        browser.openInput("https://reader-tab.example/", true);
        tryVerify(function () {
            return engineLoader.item !== null && engineLoader.item !== agentEngine;
        });
        tryCompare(agentEngine, "pageFrozen", true);
        verify(!shield.visible);

        const tabs = {};
        tabs[agentTabId] = {
            "tabId": agentTabId,
            "spaceId": personalSpaceId,
            "url": "https://agent-tab.example/"
        };
        control.tabs = tabs;
        control.agentTabIds = [agentTabId];
        control.agentTabsChanged();
        compare(agentEngine.visible, true);
        compare(agentEngine.opacity, 0);
        verify(agentEngine.z < 0);
        verify(agentEngine.z < shield.z);
        tryCompare(agentEngine, "pageFrozen", false);
        verify(shield.visible);
        compare(engineLoader.item.opacity, 1);

        control.pageRequested(7, {
                                  "verb": "look",
                                  "tabId": agentTabId,
                                  "arguments": {}
                              });
        compare(agentEngine.agentRequests.length, 1);
        compare(agentEngine.agentRequests[0].verb, "look");
        tryVerify(function () {
            return control.answers[7] !== undefined && control.answers[7].ok === true;
        });

        // Its Space goes away, and it goes on running.
        const workSpaceId = browser.createSpace("Agent elsewhere");
        verify(browser.switchSpace(workSpaceId));
        compare(agentEngine.visible, true);
        compare(agentEngine.opacity, 0);
        tryCompare(agentEngine, "pageFrozen", false);

        // Its page is built again, as it is when its address changes while its
        // Space is away, and the new view goes on from the labels the old one
        // gave out, so a label an Agent still holds names nothing new.
        agentEngine.agentNextLabel = 9;
        control.pageRequested(8, {
                                  "verb": "look",
                                  "tabId": agentTabId,
                                  "arguments": {}
                              });
        tryVerify(function () {
            return control.answers[8] !== undefined;
        });
        engineLoader.discardEngine(agentTabId);
        tryVerify(function () {
            return engineLoader.engines[agentTabId] !== undefined;
        });
        const rebuilt = engineLoader.engines[agentTabId];
        control.pageRequested(9, {
                                  "verb": "look",
                                  "tabId": agentTabId,
                                  "arguments": {}
                              });
        verify(rebuilt.agentNextLabel >= 9);
        compare(rebuilt.opacity, 0);
        compare(rebuilt.visible, true);

        // Allow agents goes off with a verb still out: the pages stop.
        control.pageRequestsCancelled();
        verify(rebuilt.agentCancels >= 1);

        // No Agent uses it any more: hidden and stopped, as any other page.
        control.agentTabIds = [];
        control.agentTabsChanged();
        compare(rebuilt.visible, false);
        tryCompare(rebuilt, "pageFrozen", true);
        verify(!shield.visible);

        engineLoader.agentControl = null;
        verify(browser.switchSpace(personalSpaceId));
        verify(browser.deleteSpace(workSpaceId, "Agent elsewhere"));
        browser.activateTab(agentTabId);
        tryVerify(function () {
            return engineLoader.item === rebuilt;
        });
        compare(rebuilt.opacity, 1);
        tryCompare(rebuilt, "pageFrozen", false);
        control.destroy();
    }

    // The core's report of what each Agent tab's Agent is doing, with the
    // Agent rules the page area hears.
    Component {
        id: agentActivityComponent

        QtObject {
            property var agentTabIds: []
            property var tabs: ({})
            property var agentActivity: ({})
            signal agentTabsChanged
            signal pageRequested(int requestId, var request)
            signal pageRequestsCancelled

            function agentTab(tabId) {
                return tabs[tabId] || ({});
            }
            function answerPage(requestId, answer) {
            }
        }
    }

    // An Agent Space on show, holding a page an Agent is driving, which the
    // page area hears of through a stand-in for the core's report. `report`
    // says what the Agent did last and whether a command is in flight, and
    // `detach` is the connection closing.
    function driveAnAgentSpace(temporary) {
        const readersSpaceId = browser.activeSpaceId;
        const spaceId = agentSpaceProbe.create("Agent work", "claude-code", temporary);
        verify(spaceId.length > 0);
        verify(browser.switchSpace(spaceId));
        tryCompare(findChild(window.contentItem, "sidebar"), "arriving", false);
        openPage("https://agent-mark.example/");
        const tabId = browser.activeTabId;
        const control = agentActivityComponent.createObject(testCase);
        const tabs = {};
        tabs[tabId] = {
            "tabId": tabId,
            "spaceId": spaceId,
            "url": "https://agent-mark.example/"
        };
        control.tabs = tabs;
        const drive = {
            "readersSpaceId": readersSpaceId,
            "spaceId": spaceId,
            "tabId": tabId,
            "control": control,
            "report": function (busy, act) {
                const activity = {};
                activity[tabId] = {
                    "spaceId": spaceId,
                    "name": "claude-code",
                    "act": act,
                    "busy": busy
                };
                control.agentActivity = activity;
            },
            "detach": function () {
                control.agentActivity = ({});
                control.agentTabIds = [];
                control.agentTabsChanged();
            }
        };
        drive.report(false, "clicked \"Files changed\"");
        control.agentTabIds = [tabId];
        findChild(window.contentItem, "engineLoader").agentControl = control;
        control.agentTabsChanged();
        return drive;
    }

    function endAgentDrive(drive) {
        findChild(window.contentItem, "engineLoader").agentControl = window.agentControlSource;
        verify(browser.switchSpace(drive.readersSpaceId));
        tryCompare(findChild(window.contentItem, "sidebar"), "arriving", false);
        // The switch back names the Space for a moment, and the next test
        // starts from a page with nothing over it.
        tryCompare(findChild(window.contentItem, "spaceNotice"), "visible", false, 5000);
        for (let row = 0; row < browser.spaces.rowCount(); ++row) {
            const model = browser.spaces;
            if (model.data(model.index(row, 0), Qt.UserRole + 1) === drive.spaceId) {
                verify(browser.deleteSpace(drive.spaceId, model.data(model.index(row, 0),
                                                                     Qt.UserRole + 2)));
                break;
            }
        }
        drive.control.destroy();
    }

    // The row ends with the Agent mark in the Agent accent, which gives its
    // place to the close button on hover, and the row then says who is
    // driving and that the tab stays rendered.
    function test_anAgentTabsRowCarriesTheAgentMark() {
        const drive = driveAnAgentSpace(false);
        const sidebar = findChild(window.contentItem, "sidebar");
        const row = findChild(sidebar, "tab-" + drive.tabId);
        const mark = findChild(sidebar, "agentMark-" + drive.tabId);
        verify(mark !== null);
        verify(mark.visible);
        compare(mark.text, "smart_toy");
        compare(String(mark.color), String(window.colors.agentAccent));
        const spot = findChild(row, "agentSpot-" + drive.tabId);
        const close = findChild(row, "close-" + drive.tabId);
        compare(spot.mapToItem(row, spot.width / 2, 0).x, close.mapToItem(row, close.width / 2,
                                                                          0).x);

        mouseMove(row, row.width / 2, row.height / 2);
        tryCompare(mark, "visible", false);
        const note = findChild(spot, "agentNote-" + drive.tabId);
        compare(note.text, "claude-code is driving this tab. It stays rendered while attached.");
        tryCompare(note, "visible", true);
        compare(row.Accessible.description, note.text);
        mouseMove(window.contentItem, window.width - 10, window.height - 10);
        tryCompare(mark, "visible", true);

        drive.detach();
        verify(!mark.visible);
        endAgentDrive(drive);
    }

    // The marks draw frames only while one of the Agent's commands is in
    // flight, and hold still while the connection is idle.
    function test_theAgentMarksPulseOnlyWhileACommandIsInFlight() {
        const drive = driveAnAgentSpace(false);
        const sidebar = findChild(window.contentItem, "sidebar");
        const rowMark = findChild(sidebar, "agentMark-" + drive.tabId);
        const spaceMark = findChild(sidebar, "spaceAgentMark-" + drive.spaceId);
        compare(rowMark.opacity, 1);
        compare(spaceMark.opacity, 1);

        drive.report(true, "clicked \"Files changed\"");
        tryVerify(function () {
            return rowMark.opacity < 0.9 && spaceMark.opacity < 0.9;
        });
        drive.report(false, "looked at the page");
        compare(rowMark.opacity, 1);
        compare(spaceMark.opacity, 1);
        compare(rowMark.opacity, 1);
        compare(spaceMark.opacity, 1);
        endAgentDrive(drive);
    }

    // The page of an Agent tab on show is framed in the Agent accent, with a
    // label at its top-right corner naming the connection and its last act.
    // In a split, the pane showing the Agent tab is the one framed.
    function test_anAgentTabsPageIsFramedWithWhoIsDriving() {
        const drive = driveAnAgentSpace(false);
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const frame = findChild(window.contentItem, "agentFrame");
        const beside = findChild(window.contentItem, "besideAgentFrame");
        const bar = findChild(window.contentItem, "agentSpaceBar");
        verify(frame.visible);
        compare(frame.width, engineLoader.activePaneWidth);
        compare(String(findChild(frame, "agentFrameBorder").border.color), String(
                    window.colors.agentAccent));
        compare(findChild(frame, "agentFrameCaption").text,
                "claude-code is driving · clicked \"Files changed\"");
        const label = findChild(frame, "agentFrameLabel");
        compare(String(label.color), String(window.colors.agentAccent));
        compare(label.x + label.width, frame.width);
        compare(label.y, bar.height);
        drive.report(false, "looked at the page");
        compare(findChild(frame, "agentFrameCaption").text,
                "claude-code is driving · looked at the page");

        // Beside the reader's own page in a split.
        openPageInNewTab("https://reader-beside.example/");
        const readersTabId = browser.activeTabId;
        verify(readersTabId !== drive.tabId);
        browser.addSplit(drive.tabId);
        tryCompare(engineLoader, "splitOnShow", true);
        compare(engineLoader.tabBesideId, drive.tabId);
        verify(!frame.visible);
        verify(beside.visible);
        compare(beside.x, engineLoader.x + engineLoader.besidePaneX);
        compare(beside.width, engineLoader.besidePaneWidth);
        browser.separateSplit();
        browser.closeTab(readersTabId);
        browser.activateTab(drive.tabId);
        tryVerify(function () {
            return frame.visible;
        });

        drive.detach();
        verify(!frame.visible);
        endAgentDrive(drive);
    }

    // An Agent Space is the Agent's mark, in the Agent accent while an Agent
    // is attached, readable while it is away. Once the reader has taken it
    // over it is one of the reader's: while the Agent is still attached it is
    // the Agent's mark in the Space's own colour, in the square's place, and
    // the square comes back when the connection closes. The menu of the
    // Spaces left out draws it the same way.
    function test_aSpaceHoldingAnAgentTabWearsTheMark() {
        const drive = driveAnAgentSpace(false);
        const sidebar = findChild(window.contentItem, "sidebar");
        const button = findChild(sidebar, "space-" + drive.spaceId);
        const mark = findChild(sidebar, "spaceAgentMark-" + drive.spaceId);
        const square = findChild(sidebar, "spaceMark-" + drive.spaceId);
        verify(mark.visible);
        verify(!square.visible);
        compare(button.label, "");
        compare(String(mark.color), String(window.colors.agentAccent));
        compare(findChild(sidebar, "spaceAgentBadge-" + drive.spaceId), null);

        verify(browser.switchSpace(drive.readersSpaceId));
        tryCompare(sidebar, "arriving", false);
        verify(mark.visible);
        compare(String(mark.color), String(window.colors.agentAccent));
        verify(findChild(sidebar, "spaceMark-" + drive.readersSpaceId).visible);
        verify(!findChild(sidebar, "spaceAgentMark-" + drive.readersSpaceId).visible);

        // Taken over with the Agent still there.
        const before = button.x;
        verify(browser.takeOverSpace(drive.spaceId));
        verify(mark.visible);
        verify(!square.visible);
        const colour = window.colors.spaces[spaceColourName(drive.spaceId)];
        verify(Qt.colorEqual(mark.color, colour));
        tryVerify(function () {
            return button.x <= before;
        });
        drive.report(true, "clicked \"Files changed\"");
        tryVerify(function () {
            return mark.opacity < 0.9;
        });
        drive.report(false, "looked at the page");
        compare(mark.opacity, 1);

        window.openSpaceOverflowMenu([
                                         {
                                             "spaceId": drive.spaceId,
                                             "spaceName": "Agent work",
                                             "spaceColor": spaceColourName(drive.spaceId),
                                             "active": false,
                                             "agentMade": false,
                                             "attached": true
                                         }
                                     ], Qt.rect(0, window.height - 30, 20, 20));
        const row = findChild(window.contentItem, "spaceOverflowMenu").items[0];
        compare(row.glyph, "smart_toy");
        verify(Qt.colorEqual(row.glyphColor, colour));
        compare(row.swatch, undefined);
        window.spaceOverflowMenuOpen = false;

        drive.detach();
        verify(!mark.visible);
        verify(square.visible);
        endAgentDrive(drive);
    }

    // An Agent Space no Agent is using shows the mark muted and still, turns
    // to the Agent accent when an Agent attaches, and becomes a square in a
    // colour of its own when the reader takes it over.
    function test_anAgentSpaceNoAgentUsesIsMarkedMuted() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const idleSpaceId = agentSpaceProbe.create("Idle agent work", "claude-code", false);
        const mark = findChild(sidebar, "spaceAgentMark-" + idleSpaceId);
        const button = findChild(sidebar, "space-" + idleSpaceId);
        verify(mark.visible);
        compare(button.label, "");
        verify(!findChild(sidebar, "spaceMark-" + idleSpaceId).visible);
        compare(String(mark.color), String(window.colors.mutedText));
        compare(mark.opacity, 1);

        const control = agentActivityComponent.createObject(testCase);
        const activity = {};
        activity["elsewhere-tab"] = {
            "spaceId": idleSpaceId,
            "name": "claude-code",
            "act": "",
            "busy": false
        };
        control.agentActivity = activity;
        findChild(window.contentItem, "engineLoader").agentControl = control;
        compare(String(mark.color), String(window.colors.agentAccent));
        findChild(window.contentItem, "engineLoader").agentControl = window.agentControlSource;
        compare(String(mark.color), String(window.colors.mutedText));

        verify(browser.takeOverSpace(idleSpaceId));
        verify(!mark.visible);
        const square = findChild(sidebar, "spaceMark-" + idleSpaceId);
        verify(square.visible);
        verify(Qt.colorEqual(square.color, window.colors.spaces[spaceColourName(idleSpaceId)]));
        verify(browser.deleteSpace(idleSpaceId, "Idle agent work"));
        control.destroy();
    }

    // The palette name the core gave a Space.
    function spaceColourName(spaceId) {
        const spaces = browser.spaces;
        for (let row = 0; row < spaces.rowCount(); ++row) {
            const index = spaces.index(row, 0);
            if (spaces.data(index, Qt.UserRole + 1) === spaceId)
                return spaces.data(index, Qt.UserRole + 3);
        }
        return "";
    }

    // Each of the reader's Spaces is a small square in its colour, which the
    // theme resolves from the Space's palette name: no letter, and no plate,
    // border or fill around the one on show, which is the larger square. Two
    // Spaces made in turn differ, and both follow a theme change. Hovering a
    // square names its Space.
    function test_eachOfTheReadersSpacesIsASquareInItsColour() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const personalId = browser.activeSpaceId;
        const workId = browser.createSpace("Work");
        const personal = findChild(sidebar, "spaceMark-" + personalId);
        const work = findChild(sidebar, "spaceMark-" + workId);
        verify(personal.visible);
        verify(work.visible);
        const personalColour = spaceColourName(personalId);
        const workColour = spaceColourName(workId);
        verify(personalColour !== workColour);
        verify(Qt.colorEqual(personal.color, window.colors.spaces[personalColour]));
        verify(Qt.colorEqual(work.color, window.colors.spaces[workColour]));
        verify(!Qt.colorEqual(personal.color, work.color));
        compare(work.width, work.height);
        verify(work.radius > 0 && work.radius < work.width / 2);
        tryVerify(function () {
            return personal.width > work.width;
        });

        const workButton = findChild(sidebar, "space-" + workId);
        const personalButton = findChild(sidebar, "space-" + personalId);
        compare(workButton.label, "");
        compare(personalButton.label, "");
        compare(findChild(sidebar, "spacePlate"), null);
        verify(!personalButton.selected);
        verify(!personalButton.bordered);
        compare(String(personalButton.background), String(Qt.color("transparent")));

        // The one on show is the larger square, wherever the reader goes.
        verify(browser.switchSpace(workId));
        tryCompare(sidebar, "arriving", false);
        tryVerify(function () {
            return work.width > personal.width;
        });
        compare(workButton.accessibleName, "Current Space: Work");
        compare(personalButton.accessibleName, "Switch to Personal");
        verify(browser.switchSpace(personalId));
        tryCompare(sidebar, "arriving", false);

        const note = findChild(workButton, "spaceNote-" + workId);
        verify(!note.visible);
        mouseMove(workButton, workButton.width / 2, workButton.height / 2);
        tryCompare(note, "visible", true);
        compare(note.text, "Work");
        mouseMove(window.contentItem, window.width - 10, window.height - 10);
        tryCompare(note, "visible", false);

        // A theme change redraws both from the new theme.
        const changed = Object.assign({}, window.colors);
        changed.spaces = {
            "green": "#00aa44",
            "yellow": "#aa8800",
            "blue": "#0044aa",
            "bright_green": "#22cc66",
            "bright_yellow": "#ccaa22",
            "bright_blue": "#2266cc"
        };
        window.colors = changed;
        verify(Qt.colorEqual(personal.color, changed.spaces[personalColour]));
        verify(Qt.colorEqual(work.color, changed.spaces[workColour]));
        window.colors = Qt.binding(function () {
            return theme.palette;
        });

        // The reader's choice, from Settings or anywhere else.
        verify(browser.setSpaceColour(workId, "bright_blue"));
        verify(Qt.colorEqual(work.color, window.colors.spaces.bright_blue));
        verify(browser.deleteSpace(workId, "Work"));
    }

    // Agent Spaces follow the reader's squares, each the Agent mark, small:
    // muted while no Agent uses it, the Agent accent while one is attached,
    // and named on hover. One made before a Space of the reader's leaves that
    // Space its number.
    function test_agentSpacesFollowTheReadersSquares() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const personalId = browser.activeSpaceId;
        const agentId = agentSpaceProbe.create("Signup flow", "claude-code", false);
        const workId = browser.createSpace("Work");
        const agentButton = findChild(sidebar, "space-" + agentId);
        const mark = findChild(sidebar, "spaceAgentMark-" + agentId);
        verify(mark.visible);
        verify(mark.font.pixelSize < Style.font.iconLarge);
        verify(!findChild(sidebar, "spaceMark-" + agentId).visible);
        verify(Qt.colorEqual(mark.color, window.colors.mutedText));
        tryVerify(function () {
            return agentButton.x > findChild(sidebar, "space-" + workId).x;
        });

        const note = findChild(agentButton, "spaceNote-" + agentId);
        mouseMove(agentButton, agentButton.width / 2, agentButton.height / 2);
        tryCompare(note, "visible", true);
        compare(note.text, "Signup flow");
        mouseMove(window.contentItem, window.width - 10, window.height - 10);
        tryCompare(note, "visible", false);

        // The reader's Spaces keep 1 and 2, as their Key labels say, and the
        // Agent Space is 3.
        const workLabel = findChild(sidebar, "keyLabel-space-" + workId);
        keyPress(Qt.Key_Control);
        tryVerify(function () {
            return workLabel.visible;
        });
        const numberFor = function (position) {
            return bindingFor("select-space", position).slice("Primary+".length);
        };
        compare(findChild(sidebar, "keyLabel-space-" + personalId).text, numberFor(1));
        compare(workLabel.text, numberFor(2));
        compare(findChild(sidebar, "keyLabel-space-" + agentId).text, numberFor(3));
        keyRelease(Qt.Key_Control);
        window.activateSpaceAt(1);
        compare(browser.activeSpaceId, workId);
        tryCompare(sidebar, "arriving", false);
        window.activateSpaceAt(0);
        compare(browser.activeSpaceId, personalId);
        tryCompare(sidebar, "arriving", false);

        verify(browser.deleteSpace(workId, "Work"));
        verify(browser.deleteSpace(agentId, "Signup flow"));
    }

    // A footer too narrow for every Space leaves out the ones that do not fit
    // and counts them in plain muted text. Nothing scrolls and nothing lands
    // under the controls beside the row. The count opens a menu of the Spaces
    // left out, and the Space keys still reach them.
    function test_spacesThatDoNotFitAreCountedAndListed() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const switcher = findChild(sidebar, "spaceSwitcher");
        const overflow = findChild(sidebar, "spaceOverflow");
        const menu = findChild(window.contentItem, "spaceOverflowMenu");
        const personalId = browser.activeSpaceId;
        window.setSidebarWidth(window.sidebarMinimumWidth);
        verify(!overflow.visible);
        const names = ["Work", "Home", "Travel", "Reading", "Music", "Garden", "Kitchen", "Cars", "Signup flow",
                       "Crawler"];
        const made = [];
        for (const name of names.slice(0, 8))
            made.push(browser.createSpace(name));
        made.push(agentSpaceProbe.create("Signup flow", "claude-code", false));
        made.push(agentSpaceProbe.create("Crawler", "claude-code", false));
        tryVerify(function () {
            return overflow.visible;
        });

        // In footer order, with whatever Spaces the window had already.
        const ids = [];
        for (let row = 0; row < browser.spaces.rowCount(); ++row)
            ids.push(browser.spaces.data(browser.spaces.index(row, 0), Qt.UserRole + 1));
        compare(ids[0], personalId);
        compare(ids[ids.length - 1], made[made.length - 1]);
        let hidden = [];
        let rightmost = 0;
        const measure = function () {
            hidden = [];
            rightmost = 0;
            for (const id of ids) {
                const item = findChild(sidebar, "space-" + id);
                if (!item.visible) {
                    hidden.push(id);
                    continue;
                }
                rightmost = Math.max(rightmost, item.x + item.width);
            }
            return rightmost <= overflow.x;
        };
        tryVerify(measure);
        verify(hidden.length > 0);
        compare(overflow.text, "+" + hidden.length);
        verify(Qt.colorEqual(overflow.color, window.colors.mutedText));
        compare(overflow.Accessible.role, Accessible.Button);
        // The ones left out are the last in footer order.
        compare(hidden[hidden.length - 1], made[made.length - 1]);
        verify(findChild(sidebar, "space-" + personalId).visible);
        verify(overflow.x + overflow.width <= switcher.width);
        const settings = findChild(sidebar, "settingsButton");
        verify(switcher.mapToItem(sidebar, overflow.x + overflow.width, 0).x <= settings.mapToItem(sidebar,
                                                                                                   0, 0).x);
        verify(switcher.contentX === undefined);

        // A Space key reaches one that is left out, and the Space on show
        // always has a place in the row: it takes the last one shown, and
        // the Space that stood there joins the count.
        verify(hidden.indexOf(ids[8]) >= 0);
        const shownBefore = ids.filter(function (id) {
            return hidden.indexOf(id) < 0;
        });
        const displaced = shownBefore[shownBefore.length - 1];
        const hiddenBefore = hidden.length;
        window.activateSpaceAt(8);
        compare(browser.activeSpaceId, ids[8]);
        tryCompare(sidebar, "arriving", false);
        const onShow = findChild(sidebar, "space-" + ids[8]);
        tryVerify(function () {
            return onShow.visible && !findChild(sidebar, "space-" + displaced).visible && measure();
        });
        compare(hidden.length, hiddenBefore);
        compare(onShow.x + onShow.width, rightmost);
        compare(overflow.text, "+" + hidden.length);
        mouseClick(overflow, overflow.width / 2, overflow.height / 2);
        tryCompare(menu, "visible", true);
        verify(menu.items.some(function (item) {
            return item.spaceId === displaced;
        }));
        verify(!menu.items.some(function (item) {
            return item.spaceId === ids[8];
        }));
        keyClick(Qt.Key_Escape);
        tryCompare(menu, "visible", false);
        verify(browser.switchSpace(personalId));
        tryCompare(sidebar, "arriving", false);
        tryVerify(function () {
            return findChild(sidebar, "space-" + displaced).visible && !onShow.visible && measure();
        });

        // The menu lists exactly the Spaces left out, in footer order, and
        // choosing one switches to it.
        mouseClick(overflow, overflow.width / 2, overflow.height / 2);
        tryCompare(menu, "visible", true);
        compare(menu.items.map(function (item) {
            return item.spaceId;
        }), hidden);
        compare(menu.items.map(function (item) {
            return item.label;
        }), hidden.map(function (id) {
            return browser.spaces.data(browser.spaces.index(ids.indexOf(id), 0), Qt.UserRole + 2);
        }));
        const agentRow = menu.items[menu.items.length - 1];
        compare(agentRow.glyph, "smart_toy");
        verify(Qt.colorEqual(agentRow.glyphColor, window.colors.mutedText));
        const choice = findChild(menu, "chromeMenuItem" + (hidden.length - 1));
        mouseClick(choice, choice.width / 2, choice.height / 2);
        tryCompare(browser, "activeSpaceId", hidden[hidden.length - 1]);
        tryCompare(menu, "visible", false);
        tryCompare(sidebar, "arriving", false);

        verify(browser.switchSpace(personalId));
        tryCompare(sidebar, "arriving", false);
        for (let index = made.length - 1; index >= 0; --index)
            verify(browser.deleteSpace(made[index], names[index]));
        window.setSidebarWidth(window.sidebarDefaultWidth);
        tryVerify(function () {
            return !overflow.visible;
        });
    }

    // Opening an Agent Space says an Agent made it, with Take over and
    // Dismiss. The command scope takes it over too, which keeps a temporary
    // one after its connection closes.
    function test_openingAnAgentSpaceOffersToTakeItOver() {
        const bar = findChild(window.contentItem, "agentSpaceBar");
        const sidebar = findChild(window.contentItem, "sidebar");
        verify(!bar.open);
        verify(!window.commands.available("take-over-space"));
        const drive = driveAnAgentSpace(true);
        verify(bar.open);
        compare(bar.message, "claude-code made this Space");
        verify(bar.detail.indexOf("connection closes") >= 0);
        compare(bar.actions[0].label, "Take over");
        compare(bar.actions[1].label, "Dismiss");
        // The page under the notice is blurred rather than read through its
        // translucent ground, and the notice is drawn over the blur.
        const backdrop = findChild(bar, "pageBarBackdrop");
        verify(backdrop !== null);
        compare(backdrop.source, findChild(window.contentItem, "engineLoader"));
        verify(backdrop.sampling);
        compare(backdrop.width, bar.width);
        compare(backdrop.height, bar.height);
        verify(window.commands.available("take-over-space"));
        verify(window.commands.actions().some(function (action) {
            return action.command === "take-over-space" && action.title === "Take over Space";
        }));

        // Dismissed, it stays away until the Space is opened again.
        bar.actionTriggered(1);
        verify(!bar.open);
        verify(!backdrop.sampling);
        verify(browser.switchSpace(drive.readersSpaceId));
        tryCompare(sidebar, "arriving", false);
        verify(!bar.open);
        verify(browser.switchSpace(drive.spaceId));
        tryCompare(sidebar, "arriving", false);
        verify(bar.open);

        // Taken over from the command scope: the label goes, and the Space
        // is no longer temporary.
        verify(browser.temporarySpace(drive.spaceId));
        verify(window.commands.run("take-over-space", -1));
        verify(browser.agentSpaceIds.indexOf(drive.spaceId) < 0);
        verify(!browser.temporarySpace(drive.spaceId));
        verify(!bar.open);
        verify(!window.commands.available("take-over-space"));
        endAgentDrive(drive);
    }

    // The window reads the Agent activity log by its context name, so nothing
    // of the window's own may take that name: a property called
    // `agentActivity` would be what the window found instead of the log.
    function test_theActivityLogIsNotHiddenByTheWindowsOwnNames() {
        verify(agentActivityContext !== null && agentActivityContext !== undefined);
        verify(window.agentActivitySource === agentActivityContext);
        compare(window.agentActivity, undefined);
    }

    // Agents drive Spaces, and a Private window is not one: whatever the page
    // area hears, it marks nothing.
    function test_aPrivateWindowMarksNothing() {
        compare(windowManager.privateWindowCount, 0);
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        privateBrowser.windowBrowser.openInput("https://private-agent.example/", false);
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        const tabId = privateBrowser.windowBrowser.activeTabId;
        const control = agentActivityComponent.createObject(testCase);
        const activity = {};
        activity[tabId] = {
            "spaceId": "",
            "name": "claude-code",
            "act": "looked at the page",
            "busy": true
        };
        control.agentActivity = activity;
        privateEngine.agentControl = control;
        compare(Object.keys(privateBrowser.agentTabActivity).length, 0);
        verify(!findChild(privateBrowser.contentItem, "agentFrame").visible);
        const mark = findChild(privateBrowser.contentItem, "agentMark-" + tabId);
        verify(mark === null || !mark.visible);
        verify(!findChild(privateBrowser.contentItem, "agentSpaceBar").open);

        privateEngine.agentControl = null;
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        control.destroy();
    }

    // What an Agent tab's page says to its console reaches the core, named by
    // the tab and a document that a page built again does not share with the
    // one before. A new document is announced even when its page says
    // nothing, so the last page's lines are not read as the new one's, and a
    // reader's own page is not passed on.
    function test_anAgentTabsConsoleReachesTheCore() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const control = agentControlComponent.createObject(testCase);
        engineLoader.agentControl = control;
        const readerEngine = openPage("https://reader-console.example/");
        const agentEngine = openPageInNewTab("https://agent-console.example/");
        const agentTabId = browser.activeTabId;
        const tabs = {};
        tabs[agentTabId] = {
            "tabId": agentTabId,
            "spaceId": browser.activeSpaceId,
            "url": "https://agent-console.example/"
        };
        control.tabs = tabs;
        control.agentTabIds = [agentTabId];
        control.agentTabsChanged();
        try {
            readerEngine.pageConsoleMessage(2, "reader", 1, "https://reader-console.example/",
                                            readerEngine.pageGeneration);
            compare(control.consoleMessages.length, 0);

            agentEngine.pageConsoleMessage(2, "boom", 7, "https://agent-console.example/app.js",
                                           agentEngine.pageGeneration);
            compare(control.consoleMessages.length, 1);
            const said = control.consoleMessages[0];
            compare(said.tabId, agentTabId);
            compare(said.level, 2);
            compare(said.message, "boom");
            compare(said.source, "https://agent-console.example/app.js");
            compare(said.line, 7);
            const serial = said.document.split(":")[0];
            verify(serial.length > 0);
            compare(said.document, serial + ":" + agentEngine.pageGeneration);

            compare(control.consoleDocuments.length, 0);
            verify(openPage("https://agent-console.example/quiet") === agentEngine);
            verify(control.consoleDocuments.length > 0);
            const started = control.consoleDocuments[control.consoleDocuments.length - 1];
            compare(started.tabId, agentTabId);
            compare(started.document, serial + ":" + agentEngine.pageGeneration);

            engineLoader.discardEngine(agentTabId);
            tryVerify(function () {
                return engineLoader.engines[agentTabId] !== undefined;
            });
            const rebuilt = engineLoader.engines[agentTabId];
            verify(rebuilt !== agentEngine);
            rebuilt.pageConsoleMessage(1, "rebuilt", 1, "", rebuilt.pageGeneration);
            const last = control.consoleMessages[control.consoleMessages.length - 1];
            compare(last.message, "rebuilt");
            verify(last.document.split(":")[0] !== serial);
        } finally {
            control.agentTabIds = [];
            control.agentTabsChanged();
            engineLoader.agentControl = null;
            control.destroy();
            browser.closeTab(agentTabId);
        }
    }

    // The two exceptions to that policy, and nothing else: a Pinned tab the
    // reader marked Keep active, and the tab an inspector is attached to. Both
    // keep their page while their Space is away, both are named in the list of
    // what is being retained, and both are reported with what they cost.
    function test_suspensionKeepsOnlyTheTabsTheCoreRetains() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        const personalSpaceId = browser.activeSpaceId;

        openPage("https://kept.example/room");
        const keptTabId = browser.activeTabId;
        browser.toggleActivePinned();
        verify(browser.setTabKeepActive(keptTabId, true));
        const keptEngineView = engineLoader.engines[keptTabId];
        verify(keptEngineView !== undefined);

        browser.openInput("https://inspected.example", true);
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        const inspectedTabId = browser.activeTabId;
        browser.openDeveloperTools();

        browser.openInput("https://ordinary.example", true);
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        const ordinaryTabId = browser.activeTabId;

        const workSpaceId = browser.createSpace("Retained");
        verify(browser.switchSpace(workSpaceId));

        // Every page is kept, and only the named ones are retained: the
        // ordinary page is stopped and answers for nothing, while the retained
        // ones are identified and listed.
        tryVerify(function () {
            return engineLoader.engines[ordinaryTabId] !== undefined
                    && engineLoader.engines[ordinaryTabId].pageFrozen;
        });
        verify(engineLoader.keepsEngineFor(keptTabId));
        verify(engineLoader.keepsEngineFor(inspectedTabId));
        verify(!engineLoader.keepsEngineFor(ordinaryTabId));
        compare(engineLoader.engines[keptTabId], keptEngineView);

        // The reader can see the whole list and what each one holds.
        tryVerify(function () {
            return engineLoader.retainedTabReport().length === 2;
        });
        const report = engineLoader.retainedTabReport();
        let keptReport = null;
        for (let index = 0; index < report.length; ++index) {
            if (report[index].tabId === keptTabId)
                keptReport = report[index];
        }
        verify(keptReport !== null);
        compare(keptReport.spaceName, "Personal");
        verify(keptReport.running);

        // Coming back finds the retained page where it was left, and reloads
        // the ordinary one.
        verify(browser.switchSpace(personalSpaceId));
        tryVerify(function () {
            return engineLoader.engines[keptTabId] === keptEngineView;
        });
        tryVerify(function () {
            return !engineLoader.keepsEngineFor(keptTabId);
        });

        browser.closeDeveloperTools();
        browser.setTabKeepActive(keptTabId, false);
        browser.activateTab(keptTabId);
        browser.toggleActivePinned();
        browser.closeTab(keptTabId);
        browser.closeTab(inspectedTabId);
        browser.closeTab(ordinaryTabId);
        verify(browser.deleteSpace(workSpaceId, "Retained"));
    }

    // A retained page kept playing and kept its artwork while its Space was
    // away. The Space's tabs come back from a store that records neither, so
    // the row reads both off the page that outlived the switch.
    function test_retainedTabBringsBackItsIconAndItsSound() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        const personalSpaceId = browser.activeSpaceId;
        const keptEngineView = openPage("https://kept-sound.example");
        const keptTabId = browser.activeTabId;
        browser.toggleActivePinned();
        verify(browser.setTabKeepActive(keptTabId, true));

        const iconUrl = "https://kept-sound.example/favicon.ico";
        keptEngineView.pageIconUrl = iconUrl;
        keptEngineView.simulateAudible(true);
        const showsIcon = function () {
            const row = findChild(window.contentItem, "pinned-" + keptTabId);
            return row !== null && String(row.tabIconUrl) === iconUrl;
        };
        tryVerify(showsIcon);

        const workSpaceId = browser.createSpace("Sound");
        verify(browser.switchSpace(workSpaceId));
        verify(browser.switchSpace(personalSpaceId));

        tryVerify(function () {
            return engineLoader.engines[keptTabId] === keptEngineView;
        });
        tryVerify(showsIcon);
        const speaker = findChild(window.contentItem, "audio-" + keptTabId);
        verify(speaker !== null);
        tryVerify(function () {
            return speaker.visible;
        });

        keptEngineView.simulateAudible(false);
        browser.setTabKeepActive(keptTabId, false);
        browser.activateTab(keptTabId);
        browser.toggleActivePinned();
        browser.closeTab(keptTabId);
        verify(browser.deleteSpace(workSpaceId, "Sound"));
    }

    // Muting is the session's, not the page's: a tab the reader has never
    // opened has no renderer to hold it, so the engine that first draws it is
    // told what the session says — including after its Space has been put away
    // and brought back, which is where the tab's own record of itself would be
    // if it had one.
    function test_mutingComesBackFromTheSessionAfterSuspension() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const personalSpaceId = browser.activeSpaceId;
        const onShowEngine = openPage("https://muting-on-show.example");
        const onShowTabId = browser.activeTabId;

        // Opened behind the page on show and never selected, so it has no
        // engine at all: nothing here is being reloaded, it has yet to load.
        browser.openInputInBackground("https://muted.example");
        const tabIndex = browser.tabs.index(browser.tabs.rowCount() - 1, 0);
        const tabId = browser.tabs.data(tabIndex, Qt.UserRole + 1);
        verify(engineLoader.engines[tabId] === undefined);
        browser.toggleTabMuted(tabId);

        const workSpaceId = browser.createSpace("Muting");
        verify(browser.switchSpace(workSpaceId));
        verify(browser.switchSpace(personalSpaceId));
        tryVerify(function () {
            return engineLoader.item === onShowEngine;
        });
        browser.activateTab(tabId);

        // The engine drawing it for the first time, muted because the tab is.
        tryVerify(function () {
            const loaded = engineLoader.engines[tabId];
            return loaded !== undefined && loaded !== onShowEngine && loaded.audioMuted;
        });
        const row = findChild(window.contentItem, "tab-" + tabId);
        verify(row !== null);
        verify(row.tabMuted);

        browser.toggleTabMuted(tabId);
        browser.closeTab(tabId);
        browser.activateTab(onShowTabId);
        verify(browser.deleteSpace(workSpaceId, "Muting"));
    }

    // A page may start playing on its own; what waits for the reader is the
    // sound. Until they have dealt with the origin the tab is held silent, and
    // once they have, every tab on that origin is heard — including the one
    // they never touched.
    function test_soundWaitsForTheOriginToBeDealtWith() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const engineView = openPage("https://autoplay.example/clip");
        const tabId = browser.activeTabId;

        verify(browser.tabSoundSuppressed(tabId));
        tryVerify(function () {
            return engineView.autoplayAllowed && engineView.audioMuted;
        });
        // The page starts, and is not heard.
        verify(engineView.simulateAutoplay());
        verify(!engineView.pageAudible);

        // The row says so where it says muting, because that is what it is from
        // where the reader sits, and offers the sound back in the same place.
        const row = findChild(window.contentItem, "tab-" + tabId);
        verify(row !== null);
        verify(row.tabSoundSuppressed);
        verify(row.silenced);
        verify(!row.tabMuted);

        // A second tab on the same site is held silent too.
        browser.openInput("https://autoplay.example/other", true);
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        const secondTabId = browser.activeTabId;
        const secondEngineView = engineLoader.engines[secondTabId];
        verify(secondEngineView !== undefined);
        tryVerify(function () {
            return secondEngineView.audioMuted;
        });

        // A gesture on one page answers for the origin, so both are heard.
        engineView.simulateUserActivation();
        tryVerify(function () {
            return !engineView.audioMuted && !secondEngineView.audioMuted;
        });
        verify(!browser.tabSoundSuppressed(tabId));
        verify(secondEngineView.simulateAutoplay());
        verify(secondEngineView.pageAudible);

        // The reader's own muting is still theirs, and still says so.
        browser.toggleTabMuted(secondTabId);
        tryVerify(function () {
            return secondEngineView.audioMuted;
        });
        verify(!browser.tabSoundSuppressed(secondTabId));

        secondEngineView.simulateAudible(false);
        browser.closeTab(secondTabId);
        browser.closeTab(tabId);
    }

    // Asking a silenced row for its sound is the reader dealing with the
    // origin, not a muting decision of their own: the tab is heard, and the
    // next tab on that site is too.
    function test_theRowGivesBackASilencedTabsSound() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const engineView = openPage("https://granted.example/page");
        const tabId = browser.activeTabId;
        verify(browser.tabSoundSuppressed(tabId));

        const speaker = findChild(window.contentItem, "audio-" + tabId);
        verify(speaker !== null);
        // The row only shows the speaker once there is sound to give back.
        engineView.simulateAudible(true);
        tryVerify(function () {
            return speaker.visible;
        });
        compare(speaker.Accessible.name.indexOf("Allow sound from"), 0);

        sidebar_tabMuteToggled(tabId);
        tryVerify(function () {
            return !browser.tabSoundSuppressed(tabId);
        });
        tryVerify(function () {
            return !engineView.audioMuted;
        });
        // Not a muting: pressing it again now mutes, as it always did.
        sidebar_tabMuteToggled(tabId);
        tryVerify(function () {
            return engineView.audioMuted;
        });
        const row = findChild(window.contentItem, "tab-" + tabId);
        verify(row.tabMuted);

        browser.toggleTabMuted(tabId);
        engineView.simulateAudible(false);
        browser.closeTab(tabId);
    }

    // A notification names the origin and the Space before it says anything the
    // page wrote, and answering it goes to the tab that sent it.
    function test_notificationNamesOriginAndSpaceAndAnswersToItsTab() {
        const personalSpaceId = browser.activeSpaceId;
        openPage("https://chat.example/room");
        const chatTabId = browser.activeTabId;
        const host = window.profiles.hosts[personalSpaceId];
        verify(host !== undefined);

        const notificationId = host.simulateNotification("https://chat.example", "New message",
                                                         "Someone said something");
        const key = personalSpaceId + ":" + notificationId;
        tryVerify(function () {
            return window.notifications.pending[key] !== undefined;
        });
        const raised = window.notifications.pending[key];
        compare(raised.heading, "https://chat.example · Personal");
        compare(raised.detail, "New message — Someone said something");
        compare(raised.tabId, chatTabId);

        // Answering it takes the reader to the page that sent it, and the page
        // hears the click.
        browser.openInput("https://elsewhere.example", true);
        const elsewhereTabId = browser.activeTabId;
        window.notifications.answer(key, true);
        compare(browser.activeTabId, chatTabId);
        verify(host.activatedNotifications.indexOf(notificationId) >= 0);
        verify(window.notifications.pending[key] === undefined);

        // A page whose Space has been put away, and which nothing is keeping
        // running, is refused rather than reaching the desktop.
        const workSpaceId = browser.createSpace("Notifying");
        verify(browser.switchSpace(workSpaceId));
        const refusedId = host.simulateNotification("https://chat.example", "Ignored",
                                                    "Nobody should see this");
        tryVerify(function () {
            return host.dismissedNotifications.indexOf(refusedId) >= 0;
        });
        verify(window.notifications.pending[personalSpaceId + ":" + refusedId] === undefined);

        verify(browser.switchSpace(personalSpaceId));
        browser.closeTab(elsewhereTabId);
        browser.closeTab(chatTabId);
        verify(browser.deleteSpace(workSpaceId, "Notifying"));
    }

    // A row's menu is about that row. The ordinary rows are offered the two
    // sweeping closes and their own close; a pin is offered neither, and gets
    // Keep active instead.
    function test_tabMenuOffersClosesToOrdinaryRowsAndKeepActiveToPins() {
        openPage("https://menu.example/one");
        const ordinaryTabId = browser.activeTabId;
        const ordinaryLabels = window.tabMenuActionsFor(ordinaryTabId).map(function (action) {
            return action.label;
        });
        verify(ordinaryLabels.indexOf("Close other tabs") >= 0);
        verify(ordinaryLabels.indexOf("Close tabs below") >= 0);
        verify(ordinaryLabels.indexOf("Duplicate tab") >= 0);
        verify(ordinaryLabels.indexOf("Keep active") === -1);

        browser.toggleActivePinned();
        const pinnedLabels = window.tabMenuActionsFor(ordinaryTabId).map(function (action) {
            return action.label;
        });
        verify(pinnedLabels.indexOf("Keep active") >= 0);
        verify(pinnedLabels.indexOf("Close tab") === -1);
        verify(pinnedLabels.indexOf("Close other tabs") === -1);
        verify(pinnedLabels.indexOf("Close tabs below") === -1);

        // The row opens its own menu, by pointer and by keyboard, and the menu
        // runs against the row it was opened on.
        const row = findChild(window.contentItem, "pinned-" + ordinaryTabId);
        verify(row !== null);
        mouseClick(row, row.width / 2, row.height / 2, Qt.RightButton);
        tryVerify(function () {
            return window.tabMenuOpen;
        });
        compare(window.tabMenuTabId, ordinaryTabId);
        window.tabMenuOpen = false;

        // The keyboard reaches it through the command rather than a key of the
        // row's own: the page's context menu already owns Shift+F10.
        browser.activateTab(ordinaryTabId);
        verify(window.commands.run("tab-menu"));
        tryVerify(function () {
            return window.tabMenuOpen;
        });
        compare(window.tabMenuTabId, ordinaryTabId);
        window.tabMenuOpen = false;

        browser.activateTab(ordinaryTabId);
        browser.toggleActivePinned();
        browser.closeTab(ordinaryTabId);
    }

    // Every menu is the menu as it stands. The entries turn on whether the row
    // is pinned and kept active, which the core answers as calls rather than as
    // properties, so a menu bound to the tab id alone would hand back the list
    // it built the first time it was asked about that row: "Pin tab" on a pin,
    // and "Keep active" on a tab already kept active.
    function test_tabMenuAnswersWithTheRowAsItStands() {
        openPage("https://restated.example");
        const tabId = browser.activeTabId;

        const labels = function () {
            window.openTabMenu(tabId, 0, 0);
            window.tabMenuOpen = false;
            return window.tabMenuItems.map(function (action) {
                return action.label;
            });
        };

        verify(labels().indexOf("Pin tab") >= 0);

        browser.toggleActivePinned();
        const pinned = labels();
        verify(pinned.indexOf("Unpin tab") >= 0);
        verify(pinned.indexOf("Keep active") >= 0);
        verify(pinned.indexOf("Close tab") === -1);

        verify(browser.setTabKeepActive(tabId, true));
        verify(labels().indexOf("Stop keeping active") >= 0);

        // The row says so too, so the setting is legible without opening the
        // menu that made it. The mark is a pin's: unpinning gives Keep active
        // up, and the mark goes with it.
        const mark = findChild(window.contentItem, "keepActive-" + tabId);
        verify(mark !== null);
        tryVerify(function () {
            return mark.visible;
        });

        // Running the entry the menu is showing turns the setting off rather
        // than on again.
        window.openTabMenu(tabId, 0, 0);
        window.runTabMenu(window.tabMenuItems.map(function (action) {
            return action.label;
        }).indexOf("Stop keeping active"));
        verify(!browser.tabKeepActive(tabId));
        tryVerify(function () {
            return !mark.visible;
        });

        browser.activateTab(tabId);
        browser.toggleActivePinned();
        browser.closeTab(tabId);
    }

    // A pane coming beside the page on show, from here on. The nudge is set a
    // turn after the pairing and eases back from there, so a machine starved
    // of frames can draw none of it: both pages are heard as they are moved.
    function watchAPaneArrive(pane, page) {
        return {
            "pane": watchChanges(pane.transform[0], "x"),
            "pageX": watchChanges(page.transform[0], "x"),
            "pageY": watchChanges(page.transform[0], "y")
        };
    }

    // The pane set to the right of its place by the nudge, from its row on
    // the right.
    function waitForTheNudge(arrival) {
        tryVerify(function () {
            return arrival.pane.seen.some(function (x) {
                return x > 0;
            });
        });
    }

    // Once the nudge has settled: the pane moved by no more than the nudge,
    // and the page already on show did not move at all.
    function checkThePaneArrived(arrival) {
        tryCompare(findChild(window.contentItem, "engineLoader"), "tabNudgeX", 0);
        stopWatching(arrival.pane);
        stopWatching(arrival.pageX);
        stopWatching(arrival.pageY);
        for (let at = 0; at < arrival.pane.seen.length; ++at)
            verify(arrival.pane.seen[at] >= 0 && arrival.pane.seen[at] <= 10);
        compare(arrival.pageX.seen, []);
        compare(arrival.pageY.seen, []);
    }

    // Two tabs of one Space side by side, listed as one row of two. Both
    // pages are drawn, each in its half, and the row holds both halves on one
    // line with the active half marked as the active tab and the tab beside
    // marked as on show. The arriving pane comes from its row: to the right
    // of the active half, so from the right, by the page nudge.
    function test_splitShowsTwoPagesSideBySideAsOneRow() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const viewport = findChild(window.contentItem, "engineViewport");
        const outline = findChild(window.contentItem, "sidebar");
        const work = openPage("https://split-work.example/");
        const workTabId = browser.activeTabId;
        browser.openInput("https://split-reference.example/", true);
        const referenceTabId = browser.activeTabId;
        tryVerify(function () {
            return engineHost.engines[referenceTabId] !== undefined;
        });
        const reference = engineHost.engines[referenceTabId];
        work.pageBackgroundColor = "#ff0000";
        reference.pageBackgroundColor = "#0000ff";
        browser.activateTab(workTabId);
        tryCompare(engineHost, "item", work);
        tryCompare(reference, "visible", false);
        // The window is shared with every other test: whatever was still
        // arriving from the last one is let settle first.
        settleMotion();

        // The menu on the other row pairs it with the tab on show.
        window.openTabMenu(referenceTabId, 0, 0);
        const labels = window.tabMenuItems.map(function (action) {
            return action.label;
        });
        verify(labels.indexOf("Add split view") >= 0);
        const arrival = watchAPaneArrive(reference, work);
        window.runTabMenu(labels.indexOf("Add split view"));
        tryVerify(function () {
            return browser.splitOnShow;
        });
        compare(browser.activeTabId, workTabId);
        compare(browser.tabBesideId, referenceTabId);
        compare(engineHost.item, work);
        compare(engineHost.besideEngine, reference);

        // The arriving pane stands to the right of its place by the nudge and
        // the page already on show does not move.
        waitForTheNudge(arrival);
        compare(engineHost.tabNudgeX, reference.transform[0].x);

        // Both engines are drawn, one in each half of the page area, and
        // both run.
        verify(work.visible);
        verify(reference.visible);
        compare(work.pageFrozen, false);
        compare(reference.pageFrozen, false);
        compare(work.x, 0);
        compare(Math.round(work.width), Math.round(engineHost.width / 2));
        compare(Math.round(reference.x), Math.round(engineHost.width / 2) + 1);
        compare(Math.round(reference.x + reference.width), Math.round(engineHost.width));
        checkThePaneArrived(arrival);
        const painted = grabImage(viewport);
        const middle = Math.round(viewport.height / 2);
        verify(Qt.colorEqual(painted.pixel(Math.round(engineHost.width / 4), middle), "#ff0000"));
        verify(Qt.colorEqual(painted.pixel(Math.round(engineHost.width * 3 / 4), middle),
                             "#0000ff"));
        // Separating reverses the arrival: the pane leaves towards its row,
        // over the page that has already taken the whole width, and is hidden
        // once it has gone. The control: the same pixels with the split
        // separated are the one page's.
        // Heard as it moves, since a starved machine can draw none of a 120 ms
        // departure.
        const departure = watchSignal(reference.transform[0].xChanged, function () {
            return {
                "x": reference.transform[0].x,
                "visible": reference.visible,
                "z": reference.z,
                "narrow": reference.width < engineHost.width
            };
        });
        verify(browser.separateSplit());
        tryCompare(reference, "visible", false);
        stopWatching(departure);
        const leaving = departure.seen.filter(function (step) {
            return step.x > 0;
        });
        verify(leaving.length > 0);
        for (let at = 0; at < leaving.length; ++at) {
            verify(leaving[at].visible);
            compare(leaving[at].z, 2);
            verify(leaving[at].narrow);
        }
        compare(reference.transform[0].x, 0);
        compare(reference.z, 0);
        tryVerify(function () {
            return Math.round(work.width) === Math.round(engineHost.width);
        });
        const alone = grabImage(viewport);
        verify(Qt.colorEqual(alone.pixel(Math.round(engineHost.width / 4), middle), "#ff0000"));
        verify(Qt.colorEqual(alone.pixel(Math.round(engineHost.width * 3 / 4), middle), "#ff0000"));

        // The row: two halves on one line, the active one filled and the tab
        // beside bordered as on show.
        verify(browser.addSplit(referenceTabId));
        const workRow = findChild(window.contentItem, "tab-" + workTabId);
        const referenceRow = findChild(window.contentItem, "tab-" + referenceTabId);
        verify(workRow !== null);
        verify(referenceRow !== null);
        tryVerify(function () {
            return workRow.width < outline.width / 2 && referenceRow.width === workRow.width;
        });
        settleRow(referenceRow);
        compare(workRow.mapToItem(outline, 0, 0).y, referenceRow.mapToItem(outline, 0, 0).y);
        verify(workRow.mapToItem(outline, 0, 0).x < referenceRow.mapToItem(outline, 0, 0).x);
        verify(workRow.active);
        verify(!workRow.tabBeside);
        verify(referenceRow.tabBeside);
        verify(!referenceRow.active);
        verify(referenceRow.inSplit);

        // The address field, and everything the chrome answers, is the active
        // half's.
        compare(String(browser.activeUrl), "https://split-work.example/");

        browser.closeTab(referenceTabId);
        browser.closeTab(workTabId);
    }

    // A Split separated earlier leaves both its tabs open, and pairing one of
    // them again moves rows, so the list builds the row on show again and the
    // new row says it is active before it has been placed. It is the same page
    // and it stays where it is, and the pane arriving beside it still comes
    // from its row by the nudge (#507).
    function test_aPaneArrivesBesideARowTheListBuiltAgain() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        openPage("https://rebuilt-first.example/");
        const firstTabId = browser.activeTabId;
        const work = openPageInNewTab("https://rebuilt-work.example/");
        const workTabId = browser.activeTabId;
        verify(browser.addSplit(firstTabId));
        tryVerify(function () {
            return browser.splitOnShow;
        });
        verify(browser.separateSplit());
        browser.openInput("https://rebuilt-beside.example/", true);
        const besideTabId = browser.activeTabId;
        tryVerify(function () {
            return engineHost.engines[besideTabId] !== undefined;
        });
        const beside = engineHost.engines[besideTabId];
        browser.activateTab(workTabId);
        tryCompare(engineHost, "item", work);
        settleMotion();

        const arrival = watchAPaneArrive(beside, work);
        verify(browser.addSplit(besideTabId));
        waitForTheNudge(arrival);
        checkThePaneArrived(arrival);

        browser.closeTab(besideTabId);
        browser.closeTab(workTabId);
        browser.closeTab(firstTabId);
    }

    // Focus moves to the other pane by its key and by a press in the pane,
    // and the chrome follows it: the address field, the find bar and reload
    // all answer for the focused pane. The pages stay where they are.
    function test_splitFocusFollowsTheKeyAndThePointer() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const left = openPage("https://focus-left.example/");
        const leftTabId = browser.activeTabId;
        browser.openInput("https://focus-right.example/", true);
        const rightTabId = browser.activeTabId;
        const right = engineHost.engines[rightTabId];
        browser.activateTab(leftTabId);
        verify(browser.addSplit(rightTabId));
        tryCompare(engineHost, "besideEngine", right);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        settleMotion();

        keyClick(Qt.Key_Semicolon, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", rightTabId);
        compare(engineHost.item, right);
        compare(engineHost.besideEngine, left);
        compare(String(browser.activeUrl), "https://focus-right.example/");
        verify(left.visible);
        verify(right.visible);
        compare(left.x, 0);
        verify(right.x > left.width);
        // The engines are not arriving: focus moved, nothing came on show.
        compare(engineHost.tabNudgeX, 0);
        compare(engineHost.tabNudgeY, 0);

        // Each pane reports its own loading, over its own half.
        const besideMark = findChild(window.contentItem, "besideLoadingIndicator");
        const marks = findChild(window.contentItem, "engineViewport").children.filter(function (
            child) {
            return child.objectName === "pageLoadingIndicator";
        });
        compare(marks.length, 1);
        const activeMark = marks[0];
        left.loading = true;
        tryCompare(besideMark, "opacity", 1);
        compare(activeMark.opacity, 0);
        verify(besideMark.x + besideMark.width / 2 < engineHost.x + left.width);
        left.loading = false;
        right.loading = true;
        tryCompare(activeMark, "opacity", 1);
        tryCompare(besideMark, "opacity", 0);
        verify(activeMark.x + activeMark.width / 2 > engineHost.x + right.x);
        right.loading = false;
        tryCompare(activeMark, "opacity", 0);

        // Reload and find go to the focused pane.
        const generationBefore = right.pageGeneration;
        const leftGeneration = left.pageGeneration;
        window.commands.run("reload", -1);
        compare(right.pageGeneration, generationBefore + 1);
        compare(left.pageGeneration, leftGeneration);
        window.openFind();
        tryVerify(function () {
            return window.findOpen;
        });
        window.closeFind();

        // A press in the other pane focuses it, and the page still gets the
        // press. The Start page is given time to drop away first.
        const pane = findChild(window.contentItem, "tabBesidePane");
        verify(pane.visible);
        compare(pane.x, left.x);
        settleMotion();
        mouseClick(engineHost, left.x + left.width / 2, engineHost.height / 2);
        tryCompare(browser, "activeTabId", leftTabId);
        compare(engineHost.item, left);
        compare(String(browser.activeUrl), "https://focus-left.example/");

        keyClick(Qt.Key_Semicolon, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", rightTabId);

        // A question the tab beside asks waits until the pane is focused.
        const permissionBar = findChild(window.contentItem, "sitePermissionBar");
        verify(permissionBar !== null);
        const requestId = left.simulateSitePermission("https://focus-left.example", "camera");
        verify(requestId.length > 0);
        wait(20);
        verify(!window.permissionOpen);
        compare(window.heldPermissionRequests.length, 1);
        keyClick(Qt.Key_Semicolon, Qt.ControlModifier);
        tryCompare(browser, "activeTabId", leftTabId);
        tryCompare(window, "permissionOpen", true);
        compare(window.pendingPermissionResponder, left);
        compare(window.pendingPermissionRequest, requestId);
        compare(window.heldPermissionRequests.length, 0);
        window.respondToPermission(BrowserController.Block);
        verify(!window.permissionOpen);

        browser.closeTab(rightTabId);
        browser.closeTab(leftTabId);
    }

    // The menu and the chooser offer a split only where one can be made: on
    // any unpaired ordinary tab, never on a pin or a tab already paired. A
    // paired tab is offered Separate split view instead of Pin tab. The
    // chooser lists the unpaired ordinary tabs but the active one, headed by
    // a blank tab, which confirming with nothing picked gives.
    function test_splitMenuEntriesAndChooserOfferOnlyWhatCanBePaired() {
        openPage("https://chooser-active.example/");
        const activeTabId = browser.activeTabId;
        browser.openInput("https://chooser-other.example/", true);
        const otherTabId = browser.activeTabId;
        browser.openInput("https://chooser-pin.example/", true);
        const pinTabId = browser.activeTabId;
        browser.toggleActivePinned();
        browser.activateTab(activeTabId);

        const labelsOf = function (tabId) {
            return window.tabMenuActionsFor(tabId).map(function (action) {
                return action.label;
            });
        };
        verify(labelsOf(otherTabId).indexOf("Add split view") >= 0);
        verify(labelsOf(activeTabId).indexOf("Add split view") >= 0);
        verify(labelsOf(pinTabId).indexOf("Add split view") === -1);

        // The chooser: the blank tab first, then the tabs a split could take,
        // which other tests may have left in the window too: never the
        // active tab, and never a pin.
        verify(window.commands.run("add-split", -1));
        tryCompare(window, "dialogMode", "split");
        const offered = window.splitTargets.map(function (row) {
            return row.id;
        });
        compare(offered[0], "");
        verify(offered.indexOf(otherTabId) > 0);
        verify(offered.indexOf(activeTabId) === -1);
        verify(offered.indexOf(pinTabId) === -1);
        const dialog = findChild(window.contentItem, "spaceDialog");
        verify(dialog !== null);
        tryVerify(function () {
            return dialog.open;
        });
        compare(dialog.selected, 0);
        dialog.accept();
        tryVerify(function () {
            return browser.splitOnShow;
        });
        compare(window.dialogMode, "");
        // A blank tab beside, focused, so the next address lands in it.
        verify(browser.activeTabBlank);
        compare(browser.tabBesideId, activeTabId);
        const blankTabId = browser.activeTabId;
        browser.openInput("https://chooser-landed.example/", false);
        compare(browser.activeTabId, blankTabId);
        compare(String(browser.activeUrl), "https://chooser-landed.example/");

        // Paired tabs are offered separation and not a pin; the command
        // panel says the same.
        const pairedLabels = labelsOf(blankTabId);
        verify(pairedLabels.indexOf("Separate split view") >= 0);
        verify(pairedLabels.indexOf("Add split view") === -1);
        verify(pairedLabels.indexOf("Pin tab") === -1);
        verify(!window.commands.available("pin-tab"));
        verify(!window.commands.available("add-split"));
        verify(window.commands.available("separate-split"));
        verify(window.commands.available("focus-split-partner"));
        window.commands.run("pin-tab", -1);
        verify(!browser.activeTabPinned);

        // The chooser lists nothing that is paired, and the entry on an
        // unpaired row is there but unavailable while the active tab cannot
        // take a partner.
        browser.activateTab(otherTabId);
        verify(window.commands.available("add-split"));
        verify(window.commands.run("add-split", -1));
        tryCompare(window, "dialogMode", "split");
        const paired = window.splitTargets.map(function (row) {
            return row.id;
        });
        compare(paired[0], "");
        verify(paired.indexOf(activeTabId) === -1);
        verify(paired.indexOf(blankTabId) === -1);
        verify(paired.indexOf(otherTabId) === -1);
        window.dialogMode = "";
        browser.activateTab(blankTabId);
        const otherEntry = window.tabMenuActionsFor(otherTabId).filter(function (action) {
            return action.label === "Add split view";
        })[0];
        verify(otherEntry !== undefined);
        compare(otherEntry.enabled, false);

        // Separate split view from the menu puts two ordinary rows back.
        window.openTabMenu(activeTabId, 0, 0);
        window.runTabMenu(window.tabMenuItems.map(function (action) {
            return action.label;
        }).indexOf("Separate split view"));
        tryVerify(function () {
            return !browser.splitOnShow;
        });
        verify(!browser.tabInSplit(activeTabId));
        verify(!browser.tabInSplit(blankTabId));

        browser.activateTab(pinTabId);
        browser.toggleActivePinned();
        browser.closeTab(pinTabId);
        browser.closeTab(blankTabId);
        browser.closeTab(otherTabId);
        browser.closeTab(activeTabId);
    }

    // The divider drags with the same handle the sidebar's seam uses, and
    // where it was left is remembered for the split while the window lives:
    // another tab on show, or the Space away and back, finds it where it was.
    // The tab beside runs while on show and freezes with the Space when it
    // is put away, and comes back as it was left.
    function test_splitDividerAndPanesFollowTheSpace() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const left = openPage("https://divider-left.example/");
        const leftTabId = browser.activeTabId;
        browser.openInput("https://divider-right.example/", true);
        const rightTabId = browser.activeTabId;
        const right = engineHost.engines[rightTabId];
        browser.openInput("https://divider-third.example/", true);
        const thirdTabId = browser.activeTabId;
        browser.activateTab(leftTabId);
        verify(browser.addSplit(rightTabId));
        tryCompare(engineHost, "besideEngine", right);
        const resizer = findChild(window.contentItem, "splitResizer");
        verify(resizer !== null);
        tryVerify(function () {
            return resizer.visible;
        });
        verify(Math.abs(resizer.x + resizer.width / 2 - engineHost.width / 2) <= 1);
        // The Start page still dropping away from the page that replaced it
        // would take the press.
        settleMotion();

        mousePress(resizer, resizer.width / 2, 200);
        mouseMove(resizer, resizer.width / 2 + 80, 200);
        mouseRelease(resizer, resizer.width / 2 + 80, 200);
        tryVerify(function () {
            return Math.abs(engineHost.leftPaneWidth - (engineHost.width / 2 + 80)) <= 1;
        });
        const leftWidth = engineHost.leftPaneWidth;
        compare(Math.round(left.width), Math.round(leftWidth));
        compare(Math.round(right.x), Math.round(leftWidth) + 1);

        // Another tab shows alone; both panes go off screen and stop.
        browser.activateTab(thirdTabId);
        tryCompare(left, "visible", false);
        tryCompare(right, "visible", false);
        tryCompare(left, "pageFrozen", true);
        tryCompare(right, "pageFrozen", true);
        verify(!resizer.visible);

        // Back on either half, the split returns with the divider where it
        // was left.
        browser.activateTab(rightTabId);
        tryCompare(right, "visible", true);
        tryCompare(left, "visible", true);
        compare(left.pageFrozen, false);
        compare(right.pageFrozen, false);
        compare(engineHost.leftPaneWidth, leftWidth);
        compare(engineHost.item, right);

        // Away in another Space the split is frozen like any of its pages,
        // and comes back as it was left.
        const personalSpaceId = browser.activeSpaceId;
        const workSpaceId = browser.createSpace("Split away");
        verify(browser.switchSpace(workSpaceId));
        tryCompare(left, "pageFrozen", true);
        tryCompare(right, "pageFrozen", true);
        verify(!left.visible);
        verify(!right.visible);
        verify(browser.switchSpace(personalSpaceId));
        tryCompare(browser, "activeTabId", rightTabId);
        tryVerify(function () {
            return browser.splitOnShow && engineHost.engines[leftTabId] === left
                    && engineHost.engines[rightTabId] === right;
        });
        tryCompare(left, "visible", true);
        tryCompare(right, "visible", true);
        tryCompare(left, "pageFrozen", false);
        tryCompare(right, "pageFrozen", false);
        compare(engineHost.leftPaneWidth, leftWidth);

        verify(browser.deleteSpace(workSpaceId, "Split away"));
        browser.closeTab(thirdTabId);
        browser.closeTab(rightTabId);
        browser.closeTab(leftTabId);
    }

    // Order is the reader's, within one section. A drag down the ordinary list
    // moves a row past its neighbour and no further than the section's end,
    // and the keyboard does the same a step at a time.
    function test_tabsReorderByPointerAndKeyboardWithinTheirSection() {
        openPage("https://order.example/one");
        const firstTabId = browser.activeTabId;
        browser.openInput("https://order.example/two", true);
        const secondTabId = browser.activeTabId;
        browser.openInput("https://order.example/three", true);
        const thirdTabId = browser.activeTabId;
        // A pin above them, which no ordinary row may be dragged into.
        browser.activateTab(firstTabId);
        browser.toggleActivePinned();

        // The window is shared with every other test, so the places are read
        // relative to where these two rows start rather than from the top.
        const secondPlace = browser.tabSectionIndex(secondTabId);
        compare(browser.tabSectionIndex(thirdTabId), secondPlace + 1);

        // The pointer asks the core for the same step the keyboard does.
        verify(browser.moveTab(secondTabId, secondPlace + 1));
        compare(browser.tabSectionIndex(secondTabId), secondPlace + 1);
        compare(browser.tabSectionIndex(thirdTabId), secondPlace);

        // The keyboard steps it back, and stops at the section's edge rather
        // than carrying it into the pins.
        browser.activateTab(secondTabId);
        verify(window.commands.run("move-tab-up"));
        compare(browser.tabSectionIndex(secondTabId), secondPlace);
        for (let step = 0; step < secondPlace + 2; ++step)
            window.commands.run("move-tab-up");
        compare(browser.tabSectionIndex(secondTabId), 0);
        verify(browser.tabPinned(firstTabId));

        browser.activateTab(firstTabId);
        browser.toggleActivePinned();
        browser.closeTab(thirdTabId);
        browser.closeTab(secondTabId);
        browser.closeTab(firstTabId);
    }

    // A dragged row leaves the list and follows the hand, the rows it passes
    // open the place it would drop into, and one drag can carry it the whole
    // length of its section. Nothing is reordered until it is let go.
    function test_draggingARowCarriesItToAnyPlaceInItsSection() {
        openPage("https://carried.example/one");
        const firstTabId = browser.activeTabId;
        browser.openInput("https://carried.example/two", true);
        const secondTabId = browser.activeTabId;
        browser.openInput("https://carried.example/three", true);
        const thirdTabId = browser.activeTabId;
        const outline = findChild(window.contentItem, "sidebar");
        verify(outline !== null);

        // The list is still filling in behind the tabs just opened, and a row
        // that is about to be placed somewhere else cannot be dragged from
        // where it currently looks to be. The arrangement replaces the rows
        // showing it, so a row is asked for again after every settle.
        settleRow(findChild(window.contentItem, "tab-" + thirdTabId));
        const lastRow = findChild(window.contentItem, "tab-" + thirdTabId);
        verify(lastRow !== null);

        const place = browser.tabSectionIndex(thirdTabId);
        const rowHeight = lastRow.height;
        verify(place >= 2);
        compare(browser.tabSectionIndex(secondTabId), place - 1);
        const grabY = rowHeight / 2;
        // Two whole places in one gesture, which is what the step-at-a-time
        // reorder this replaced could not do.
        const travel = rowHeight * 2;
        const grabbed = lastRow.mapToItem(window.contentItem, lastRow.width / 2, grabY);

        // A press alone is not a drag: a row is not lifted by the tremor in a
        // click, and nothing is asked of the list.
        mousePress(lastRow, lastRow.width / 2, grabY);
        mouseMove(window.contentItem, grabbed.x, grabbed.y + 2);
        wait(1);
        verify(!lastRow.lifted);

        // Carried up past both of its neighbours in one gesture. The row goes
        // with the hand rather than staying where the list put it, and the
        // rows it passes open the place it would drop into.
        dragRowBy(grabbed, -travel);
        verify(lastRow.lifted);
        verify(lastRow.carry.y < -rowHeight);
        compare(outline.dropDestination, place - 2);
        const passedRow = findChild(window.contentItem, "tab-" + secondTabId);
        verify(passedRow !== null);
        // The rows it passed settle into the places the arrangement would give
        // them rather than jumping, so the gap opens over a frame or two.
        tryVerify(function () {
            return passedRow.carry.y > 0;
        });
        // And nothing has actually moved yet.
        compare(browser.tabSectionIndex(thirdTabId), place);

        mouseRelease(window.contentItem, grabbed.x, grabbed.y - travel);
        // Two places up, and the rows it passed have each moved down one.
        compare(browser.tabSectionIndex(thirdTabId), place - 2);
        compare(browser.tabSectionIndex(secondTabId), place);

        // The arrangement replaces the rows that were showing it, so the row is
        // asked for again rather than remembered.
        const carried = findChild(window.contentItem, "tab-" + thirdTabId);
        verify(carried !== null);
        verify(!carried.lifted);
        compare(carried.carry.y, 0);

        // And the whole length of the section, from wherever it now sits to
        // the very first place.
        settleRow(carried);
        const regrabbed = carried.mapToItem(window.contentItem, carried.width / 2, grabY);
        // Well past the first row rather than exactly onto it: what is being
        // asked is that a drag off the top of the section lands at the top of
        // it, not that a particular pixel does.
        const toTheTop = rowHeight * (browser.tabSectionIndex(thirdTabId) + 4);
        mousePress(carried, carried.width / 2, grabY);
        dragRowBy(regrabbed, -toTheTop);
        compare(outline.dropDestination, 0);
        mouseRelease(window.contentItem, regrabbed.x, regrabbed.y - toTheTop);
        compare(browser.tabSectionIndex(thirdTabId), 0);

        browser.closeTab(thirdTabId);
        browser.closeTab(secondTabId);
        browser.closeTab(firstTabId);
    }

    // The outline's empty space below the rows is the largest surface the
    // chrome has, and a reader looking for somewhere to grab the window reaches
    // for it first (#179).
    function test_theOutlinesEmptySpaceMovesTheWindow() {
        openPage("https://empty-space.example/one");
        const tabId = browser.activeTabId;
        const outline = findChild(window.contentItem, "sidebar");
        const list = findChild(window.contentItem, "ordinaryList");
        const tabScroll = findChild(window.contentItem, "tabScroll");
        verify(outline !== null && list !== null && tabScroll !== null);
        settleRow(findChild(window.contentItem, "tab-" + tabId));

        // Halfway between the last row and the bottom of the list, which a
        // test sharing the window with others still leaves room for.
        const listBottom = list.mapToItem(window.contentItem, 0, list.height).y;
        const scrollBottom = tabScroll.mapToItem(window.contentItem, 0, tabScroll.height).y;
        verify(scrollBottom - listBottom > 40, "the rows leave no empty space to grab");
        const at = tabScroll.mapToItem(window.contentItem, tabScroll.width / 2, 0);
        at.y = (listBottom + scrollBottom) / 2;

        moveSpy.target = outline;
        moveSpy.clear();
        // A press that does not travel is not a move.
        mouseClick(window.contentItem, at.x, at.y);
        compare(moveSpy.count, 0);

        mousePress(window.contentItem, at.x, at.y);
        dragRowBy(at, 60);
        mouseRelease(window.contentItem, at.x, at.y + 60);
        compare(moveSpy.count, 1);

        browser.closeTab(tabId);
    }

    // The space around the rows moves the window, and the rows keep the drag
    // that reorders them: the handler under them cannot take it.
    function test_draggingARowLeavesTheWindowWhereItIs() {
        openPage("https://row-stays.example/one");
        const firstTabId = browser.activeTabId;
        browser.openInput("https://row-stays.example/two", true);
        const secondTabId = browser.activeTabId;
        const outline = findChild(window.contentItem, "sidebar");
        verify(outline !== null);
        const row = findChild(window.contentItem, "tab-" + secondTabId);
        settleRow(row);
        const place = browser.tabSectionIndex(secondTabId);
        verify(place >= 1);

        moveSpy.target = outline;
        moveSpy.clear();
        const grabbed = row.mapToItem(window.contentItem, row.width / 2, row.height / 2);
        mousePress(row, row.width / 2, row.height / 2);
        dragRowBy(grabbed, -row.height);
        verify(row.lifted);
        mouseRelease(window.contentItem, grabbed.x, grabbed.y - row.height);
        compare(browser.tabSectionIndex(secondTabId), place - 1);
        compare(moveSpy.count, 0);

        browser.closeTab(secondTabId);
        browser.closeTab(firstTabId);
    }

    // Pins are laid out across the section as well as down it, so a pin is
    // carried in both directions and the place it would take is read off where
    // the list put the other pins rather than from a row height.
    function test_draggingAPinCarriesItAcrossThePinnedSection() {
        openPage("https://pin-order.example/one");
        const firstTabId = browser.activeTabId;
        browser.toggleActivePinned();
        browser.openInput("https://pin-order.example/two", true);
        const secondTabId = browser.activeTabId;
        browser.toggleActivePinned();
        const outline = findChild(window.contentItem, "sidebar");

        settleRow(findChild(window.contentItem, "pinned-" + secondTabId));
        const secondPin = findChild(window.contentItem, "pinned-" + secondTabId);
        verify(secondPin !== null);
        compare(browser.tabSectionIndex(secondTabId), 1);

        const grabbed = secondPin.mapToItem(window.contentItem, secondPin.width / 2,
                                            secondPin.height / 2);
        mousePress(secondPin, secondPin.width / 2, secondPin.height / 2);
        const steps = 6;
        for (let step = 1; step <= steps; ++step) {
            mouseMove(window.contentItem, grabbed.x - secondPin.width * step / steps, grabbed.y);
            wait(1);
        }
        verify(secondPin.lifted);
        // Carried sideways, which is the axis its section runs in.
        verify(secondPin.carry.x < 0);
        compare(outline.dropDestination, 0);
        mouseRelease(window.contentItem, grabbed.x - secondPin.width, grabbed.y);
        compare(browser.tabSectionIndex(secondTabId), 0);
        compare(browser.tabSectionIndex(firstTabId), 1);

        browser.activateTab(secondTabId);
        browser.toggleActivePinned();
        browser.closeTab(secondTabId);
        browser.activateTab(firstTabId);
        browser.toggleActivePinned();
        browser.closeTab(firstTabId);
    }

    // The right button opens the menu; it never carries the row.
    function test_theRightButtonNeverCarriesARow() {
        openPage("https://menu-only.example");
        const tabId = browser.activeTabId;
        const row = findChild(window.contentItem, "tab-" + tabId);
        verify(row !== null);

        mousePress(row, row.width / 2, row.height / 2, Qt.RightButton);
        mouseMove(row, row.width / 2, row.height / 2 + row.height * 2);
        wait(1);
        verify(!row.lifted);
        compare(row.carry.y, 0);
        mouseRelease(row, row.width / 2, row.height / 2 + row.height * 2, Qt.RightButton);
        window.tabMenuOpen = false;

        browser.closeTab(tabId);
    }

    // Duplicate opens the address again in a new ordinary tab, with its own
    // engine: no history, no form state, and no share of the page it came from.
    function test_duplicateOpensTheAddressInItsOwnNewTab() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const engineView = openPage("https://duplicated.example/page");
        const sourceTabId = browser.activeTabId;
        engineView.pageLocalState = "typed into a form";

        verify(window.commands.run("duplicate-tab"));
        const duplicateTabId = browser.activeTabId;
        verify(duplicateTabId !== sourceTabId);
        tryVerify(function () {
            const copy = engineLoader.engines[duplicateTabId];
            return copy !== undefined && copy !== engineView && String(copy.currentUrl)
                    === "https://duplicated.example/page" && copy.pageLocalState === "";
        });
        verify(!browser.tabPinned(duplicateTabId));

        browser.closeTab(duplicateTabId);
        browser.closeTab(sourceTabId);
    }

    // Settings names every retained tab, which Space it belongs to, why it is
    // running and what it holds — and lets the reader stop one from there.
    function test_settingsListEveryRetainedTabAndItsCost() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const personalSpaceId = browser.activeSpaceId;
        openPage("https://listed.example");
        const keptTabId = browser.activeTabId;
        browser.toggleActivePinned();
        verify(browser.setTabKeepActive(keptTabId, true));

        const workSpaceId = browser.createSpace("Listed");
        verify(browser.switchSpace(workSpaceId));
        window.settingsOpen = true;
        window.refreshRetainedTabs();

        const listed = findChild(window.contentItem, "retainedTab-" + keptTabId);
        verify(listed !== null);
        verify(listed.note.indexOf("Personal") >= 0);
        verify(listed.note.indexOf("Keep active") >= 0);

        // Stopping it from the list is the same decision as the row's own, made
        // about a tab in a Space that is not on show.
        window.releaseRetainedTab(keptTabId);
        tryVerify(function () {
            return browser.retainedTabs.length === 0;
        });
        tryVerify(function () {
            return engineLoader.engines[keptTabId] === undefined;
        });
        window.settingsOpen = false;

        verify(browser.switchSpace(personalSpaceId));
        browser.activateTab(keptTabId);
        browser.toggleActivePinned();
        browser.closeTab(keptTabId);
        verify(browser.deleteSpace(workSpaceId, "Listed"));
    }

    // Moving a tab to another Space is not the same as switching to one. A
    // Space switch puts its pages aside; a move takes the tab away from the
    // Space that held its engine, so the page is discarded and the moved tab
    // arrives without one. It therefore shows its lettered tile until its page
    // is loaded again and reports its own artwork — which is the contract
    // `BrowserController::setTabIcon` states, rather than a second instance of
    // the Space-switch bug. What has to survive the move is the wiring: the
    // tab's new engine still has to reach the tab it belongs to.
    //
    // Losing the page is the intended cost of a move, not a gap left to close:
    // a moved tab gives up its scroll position, form state and history along
    // with its artwork, and arrives in its new Space as a tab to be loaded.
    // Preserving any of that across a move is deliberately not a goal, so this
    // test asserts the discard rather than tolerating it.
    function test_movingATabToAnotherSpaceLeavesItsPageAndIconBehind() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        openPage("https://moved.example/page");

        const sourceSpaceId = browser.activeSpaceId;
        const tabId = browser.activeTabId;
        const iconUrl = "https://moved.example/favicon.ico";
        const tabIcon = function () {
            const row = findChild(window.contentItem, "tab-" + tabId);
            return row === null ? null : String(row.tabIconUrl);
        };

        engineLoader.item.pageIconUrl = iconUrl;
        tryVerify(function () {
            return tabIcon() === iconUrl;
        });

        const workSpaceId = browser.createSpace("Work");
        verify(browser.requestTabMoveToSpace(tabId, workSpaceId, false));
        verify(browser.switchSpace(workSpaceId));
        compare(browser.activeTabId, tabId);

        // The page did not come with the tab, so neither did its artwork: the
        // tab asks its new Space's store, which never saw this site.
        verify(engineLoader.engines[tabId] === undefined);
        tryVerify(function () {
            return tabIcon() === String(browser.storedFavicon("https://moved.example/page"));
        });

        // The tab is served by a new engine, and that engine still reports to
        // the right tab.
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        const reloadedIconUrl = "https://moved.example/reloaded.ico";
        engineLoader.item.pageIconUrl = reloadedIconUrl;
        tryVerify(function () {
            return tabIcon() === reloadedIconUrl;
        });

        // Leave the suite in the Space it started in.
        verify(browser.switchSpace(sourceSpaceId));
    }

    function test_privateWindowUsesTemporaryIdentityAndDistinctChrome() {
        compare(windowManager.privateWindowCount, 0);
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);

        const privateBrowser = window.privateWindows[0];
        verify(privateBrowser !== undefined && privateBrowser !== null);
        compare(privateBrowser.objectName, "privateBrowserWindow");
        verify(privateBrowser.visible);
        verifyApplicationWindowFlags(privateBrowser);
        // A window of its own: no transient parent for a tiling compositor to
        // read as "float this next to its opener".
        verify(!privateBrowser.transientParent);
        compare(privateBrowser.colors.accent, privateBrowser.colors.privateAccent);
        compare(privateBrowser.colors.overlay, theme.palette.privateOverlay);
        compare(privateBrowser.colors.mutedText, theme.palette.privateMutedText);
        compare(privateBrowser.colors.border, theme.palette.privateBorder);

        const spaceSwitcher = findChild(privateBrowser.contentItem, "spaceSwitcher");
        const pinnedList = findChild(privateBrowser.contentItem, "pinnedList");
        const newSpaceButton = findChild(privateBrowser.contentItem, "newSpaceButton");
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        const privateBadge = findChild(privateBrowser.contentItem, "privateBadge");
        verify(spaceSwitcher !== null);
        verify(pinnedList !== null);
        verify(newSpaceButton !== null);
        verify(privateEngine !== null);
        verify(!spaceSwitcher.visible);
        verify(!pinnedList.visible);
        verify(!newSpaceButton.visible);
        // Nor a menu of Spaces left out of a row it does not have.
        findChild(privateBrowser.contentItem, "sidebar").openHiddenSpaces();
        verify(!findChild(privateBrowser.contentItem, "spaceOverflowMenu").visible);
        // The window names itself with its palette and the mask in the footer.
        // Nothing is drawn over the page to say it.
        verify(findChild(privateBrowser.contentItem, "privateIndicator") === null);
        verify(privateBadge !== null);
        verify(privateBadge.visible);
        compare(privateBadge.text, "domino_mask");
        compare(String(privateBadge.color), String(privateBrowser.colors.privateAccent));
        privateBrowser.windowBrowser.openInput("https://private.example", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        compare(privateEngine.item.profilePath, windowManager.privateProfilePath);
        compare(privateEngine.item.browserProfile, window.privateProfileHost.profile);

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        tryVerify(function () {
            return window.privateWindows.length === 0;
        });
    }

    // A Space remembers an answer and a Private window only holds it until it
    // closes, and the prompt says which the reader is giving.
    function test_aPermissionPromptSaysHowLongItsAnswerLasts() {
        const engine = openPage("https://asks-space.example/");
        const spaceBar = findChild(window.contentItem, "sitePermissionBar");
        verify(engine.simulateSitePermission("https://asks-space.example", "notifications").length
               > 0);
        tryCompare(window, "permissionOpen", true);
        compare(spaceBar.detail, "notifications · remembered for this Space only");
        verify(spaceBar.actions[1].enabled);
        window.respondToPermission(BrowserController.Block);

        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://asks-private.example", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        verify(privateEngine.item.simulateSitePermission("https://asks-private.example",
                                                         "notifications").length > 0);
        tryCompare(privateBrowser, "permissionOpen", true);
        const privateBar = findChild(privateBrowser.contentItem, "sitePermissionBar");
        compare(privateBar.detail, "notifications · kept until the last Private window closes");
        verify(!privateBar.actions[1].enabled);
        privateBrowser.respondToPermission(BrowserController.Block);

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        // The Private window took the keyboard, and the tests after this one
        // press keys in the main window.
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    // With the Glance refused, a page's new-tab request is a tab, as it was
    // before there was a Glance to open it in.
    function test_newWindowRequestsRouteToTabsOrAuxiliaryWindows() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        window.setGlanceEnabled(false);
        const previousTabCount = browser.tabs.rowCount();
        const previousActiveTabId = browser.activeTabId;

        engineLoader.item.simulateBackgroundTabRequest("https://example.com/background");
        tryVerify(function () {
            return browser.tabs.rowCount() === previousTabCount + 1;
        });
        compare(browser.activeTabId, previousActiveTabId);

        engineLoader.item.simulateNewWindowRequest("https://example.com/tab", false);
        tryVerify(function () {
            return browser.tabs.rowCount() === previousTabCount + 2;
        });
        compare(browser.activeUrl.toString(), "https://example.com/tab");

        engineLoader.item.simulateNewWindowRequest("https://example.com/dialog", true);
        const auxiliary = findChild(window, "auxiliaryWindow");
        tryVerify(function () {
            return auxiliary !== null && auxiliary.visible;
        });
        verify(!Boolean(auxiliary.flags & Qt.FramelessWindowHint));

        const auxiliaryEngine = findChild(auxiliary.contentItem, "auxiliaryEngineLoader");
        verify(auxiliaryEngine !== null);
        tryVerify(function () {
            return auxiliaryEngine.item !== null;
        });
        compare(auxiliaryEngine.item.sharedProfile, engineLoader.item.browserProfile);
        compare(auxiliaryEngine.item.currentUrl.toString(), "https://example.com/dialog");
        auxiliaryEngine.item.simulateWindowCloseRequest();
        tryVerify(function () {
            return !auxiliary.visible;
        });
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        window.setGlanceEnabled(true);
    }

    // A page names no address at all while a navigation is in flight, and a
    // page adopted from a new-window request passes through that gap on its way
    // to the address the link asked for. The tab has not lost its page there,
    // and must not be blanked for it: a blank tab is given no engine, so
    // believing the gap would take the renderer down mid-load and leave the
    // Start page standing where the opened page belongs.
    function test_aPageBetweenAddressesKeepsItsTabAndItsEngine() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const startPage = findChild(window.contentItem, "startPage");
        verify(engineLoader !== null);
        verify(startPage !== null);
        window.setGlanceEnabled(false);

        openPage("https://opener.example");
        engineLoader.item.simulateNewWindowRequest("https://opened.example/page", false);
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://opened.example/page";
        });
        const openedTabId = browser.activeTabId;
        const openedEngine = engineLoader.item;
        verify(openedEngine !== null);

        openedEngine.currentUrl = "";
        compare(browser.activeUrl.toString(), "https://opened.example/page");
        compare(engineLoader.engines[openedTabId], openedEngine);
        compare(engineLoader.item, openedEngine);
        tryVerify(function () {
            return !startPage.visible;
        });

        // The address the navigation commits to is the one the tab takes.
        openedEngine.currentUrl = "https://opened.example/next";
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://opened.example/next";
        });
        verify(!startPage.visible);
        window.setGlanceEnabled(true);
    }

    // Every engine the host holds answers for a tab that exists, and the tab
    // showing is the only one drawing. An engine keyed to no tab would be a
    // page nothing can show, hide or take away: it would sit over the page the
    // reader came back to, which is what the tab they left looks like from
    // behind it.
    function test_everyEngineAnswersForATabThatExists() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        window.setGlanceEnabled(false);

        const openerEngine = openPage("https://opener.example");
        const openerTabId = browser.activeTabId;
        engineLoader.item.simulateNewWindowRequest("https://opened.example/page", false);
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://opened.example/page";
        });
        const openedTabId = browser.activeTabId;

        // Both tabs hold a page, so both are listed: an engine keyed to
        // anything else answers for no tab in the Space.
        for (const tabId in engineLoader.engines) {
            verify(tabId.length > 0);
            verify(findChild(window.contentItem, "tab-" + tabId) !== null);
            compare(engineLoader.engines[tabId].visible, tabId === openedTabId);
        }

        // The tab the link was clicked from still draws the page it left.
        browser.activateTab(openerTabId);
        tryVerify(function () {
            return engineLoader.item === openerEngine;
        });
        compare(engineLoader.engines[openerTabId], openerEngine);
        verify(openerEngine.visible);
        verify(!engineLoader.engines[openedTabId].visible);
        window.setGlanceEnabled(true);
    }

    // A page's new-tab request, with the Glance on, and the Glance it opens:
    // over the page, with the requested address, adding no tab.
    // Fails where an item rests between pixels: there its edges and text are
    // drawn soft, and a page's texture splits along its diagonal (#567). The
    // pixels are the display's: under a scale of 1.25 or 1.6 a whole logical
    // pixel can fall between two of them (#571).
    function verifyOnWholePixels(item, name, axes) {
        const corner = item.mapToItem(null, 0, 0);
        const ratio = window.devicePixelRatio;
        if (axes !== "y")
            verifyOnDevicePixel(corner.x * ratio, name + " rests between pixels across");
        if (axes !== "x")
            verifyOnDevicePixel(corner.y * ratio, name + " rests between pixels down");
    }

    // Qt Quick maps a position through single-precision matrices, so one on a
    // pixel can read a hair off it, and the further from the origin, the more:
    // 325.99998 for 326 at a scale of 1.25, and 1003.9978 for 1004 in a GCC
    // build. A hundredth of a pixel allows for that and is still far below the
    // smallest offset these tests guard, 0.2 pixels at a scale of 1.6.
    function verifyOnDevicePixel(position, message) {
        verify(Math.abs(position - Math.round(position)) < 0.01, message + ": " + position);
    }

    function openGlance(requestedUrl) {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const glance = findChild(window.contentItem, "glance");
        verify(engineLoader !== null);
        verify(glance !== null);
        window.setGlanceEnabled(true);
        const tabCount = browser.tabs.rowCount();
        engineLoader.item.simulateNewWindowRequest(requestedUrl, false);
        tryVerify(function () {
            return window.glanceEngine !== null && String(window.glanceEngine.currentUrl)
                    === requestedUrl;
        });
        tryVerify(function () {
            return glance.visible;
        });
        compare(browser.tabs.rowCount(), tabCount);
        return glance;
    }

    function test_aPagesNewTabRequestOpensAGlanceOverThePage() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const opener = openPage("https://opener.example");
        const openerTabId = browser.activeTabId;
        const tabCount = browser.tabs.rowCount();
        const glance = openGlance("https://glanced.example/page");
        const engine = window.glanceEngine;

        // The Glance's page is a page of the Space on show, drawn in the
        // Glance rather than in the host, and answering for no tab.
        compare(engine.sharedProfile, opener.browserProfile);
        compare(engine.parent, glance.pageHost);
        verify(engine.visible);
        for (const tabId in engineLoader.engines)
            verify(engineLoader.engines[tabId] !== engine);
        compare(engineLoader.item, opener);

        // The chrome goes on answering for the tab beneath; the Glance names
        // its own page.
        compare(browser.activeTabId, openerTabId);
        compare(browser.activeUrl.toString(), "https://opener.example");
        const address = findChild(glance, "glanceAddress");
        verify(address !== null);
        compare(address.text, "https://glanced.example/page");
        verify(window.commands.available("glance-to-tab"));

        // Escape closes it whatever the page does with the key, the engine goes
        // with it, and the keyboard returns to the page beneath.
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return window.glanceEngine === null;
        });
        tryVerify(function () {
            return !glance.visible;
        });
        compare(browser.tabs.rowCount(), tabCount);
        compare(browser.activeTabId, openerTabId);
        verify(!window.commands.available("glance-to-tab"));
        tryVerify(function () {
            return opener.activeFocus;
        });
    }

    // Keeping a Glance makes it a tab, engine and all: the tab is listed after
    // the one the Glance stood over, holds the same engine, and reports the
    // Glance's address.
    function test_aGlanceBecomesATabWithItsEngine() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const opener = openPage("https://opener.example");
        const openerTabId = browser.activeTabId;
        const tabCount = browser.tabs.rowCount();
        const glance = openGlance("https://kept.example/page");
        const engine = window.glanceEngine;

        verify(window.commands.run("glance-to-tab", -1));
        tryVerify(function () {
            return browser.tabs.rowCount() === tabCount + 1;
        });
        const keptTabId = browser.activeTabId;
        verify(keptTabId !== openerTabId);
        tryVerify(function () {
            return browser.activeUrl.toString() === "https://kept.example/page";
        });
        compare(engineLoader.engines[keptTabId], engine);
        tryVerify(function () {
            return engineLoader.item === engine;
        });
        compare(engine.parent, engineLoader);
        verify(engine.visible);
        compare(window.glanceEngine, null);
        tryVerify(function () {
            return !glance.visible;
        });

        // The tab it stood over still draws the page it left.
        browser.activateTab(openerTabId);
        tryVerify(function () {
            return engineLoader.item === opener;
        });
        verify(!engine.visible);
        browser.closeTab(keptTabId);
    }

    // The close-tab key closes what is in front, which is the Glance, and the
    // tab it stood over is untouched. Every other way the tab on show changes
    // ends the Glance too: a tab switch, a Space switch, and a sheet taking the
    // page area.
    function test_aGlanceEndsWithTheTabItStandsOver() {
        openPage("https://opener.example");
        const openerTabId = browser.activeTabId;
        const tabCount = browser.tabs.rowCount();

        openGlance("https://first.example/page");
        verify(window.commands.run("close-tab", -1));
        tryVerify(function () {
            return window.glanceEngine === null;
        });
        compare(browser.tabs.rowCount(), tabCount);
        compare(browser.activeTabId, openerTabId);

        openGlance("https://second.example/page");
        openPageInNewTab("https://other.example");
        const otherTabId = browser.activeTabId;
        tryVerify(function () {
            return window.glanceEngine === null;
        });
        browser.activateTab(openerTabId);
        tryVerify(function () {
            return browser.activeTabId === openerTabId;
        });

        openGlance("https://third.example/page");
        window.requestSettings();
        tryVerify(function () {
            return window.glanceEngine === null;
        });
        window.settingsOpen = false;
        browser.closeTab(otherTabId);
    }

    // A link followed from inside a Glance that asks for a new tab gets one,
    // which is the tab on show changing, so the Glance ends. A second request
    // from the page beneath replaces the Glance rather than stacking one over
    // another.
    function test_aGlanceOpensNoGlanceOfItsOwn() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        openPage("https://opener.example");
        const tabCount = browser.tabs.rowCount();

        openGlance("https://first.example/page");
        const first = window.glanceEngine;
        engineLoader.item.simulateNewWindowRequest("https://second.example/page", false);
        tryVerify(function () {
            return window.glanceEngine !== null && window.glanceEngine !== first;
        });
        compare(String(window.glanceEngine.currentUrl), "https://second.example/page");
        compare(browser.tabs.rowCount(), tabCount);

        window.glanceEngine.simulateNewWindowRequest("https://opened.example/page", false);
        tryVerify(function () {
            return browser.tabs.rowCount() === tabCount + 1;
        });
        compare(browser.activeUrl.toString(), "https://opened.example/page");
        tryVerify(function () {
            return window.glanceEngine === null;
        });
        browser.closeActiveTab();
    }

    // The Glance grows out of the link the reader pressed and retreats into
    // it, so it is that link opened rather than a panel that appeared. What is
    // inside keeps its resting size the whole way: the page is revealed, never
    // laid out again for the movement. A page that named no press gets a
    // sheet's lift instead.
    function test_aGlanceGrowsOutOfTheLinkThatAskedForIt() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        SystemMotion.reduced = true;
        const opener = openPage("https://opener.example");
        opener.simulatePress(120, 300, 160, 20);
        const glance = openGlance("https://linked.example/page");
        const panel = findChild(glance, "glancePanel");
        const pageHost = findChild(glance, "glancePageHost");
        verify(panel !== null);
        verify(pageHost !== null);

        const expected = opener.mapToItem(glance, Qt.rect(120, 300, 160, 20));
        compare(glance.origin, expected);
        compare(glance.arrival, 1);
        compare(Math.round(panel.x), 40);
        compare(Math.round(panel.y), 40);
        compare(Math.round(panel.width), Math.round(glance.width - 80));
        const restPageHeight = pageHost.height;

        glance.arrival = 0;
        compare(Math.round(panel.x), Math.round(expected.x));
        compare(Math.round(panel.y), Math.round(expected.y));
        compare(Math.round(panel.width), Math.round(expected.width));
        compare(Math.round(panel.height), Math.round(expected.height));
        compare(pageHost.height, restPageHeight);
        compare(panel.opacity, 1);
        glance.arrival = 1;
        window.closeGlance();
        tryVerify(function () {
            return !glance.visible;
        });

        // A press the page did not name: the panel lifts from below its place.
        opener.simulatePress(0, 0, 0, 0);
        openGlance("https://unplaced.example/page");
        verify(!glance.fromOrigin);
        glance.arrival = 0;
        compare(Math.round(panel.x), 40);
        compare(Math.round(panel.y), 64);
        compare(panel.opacity, 0);
        glance.arrival = 1;
        window.closeGlance();
        SystemMotion.reduced = false;
    }

    // A popup-sized Glance stands in the middle of the page area on whole
    // pixels, whether the room around it is odd or even, at two page-area
    // widths: on half a pixel the page is drawn split along its diagonal.
    function test_aPopupSizedGlanceRestsOnWholePixels() {
        openPage("https://popup-room.example");
        const glance = openGlance("https://popup.example/page");
        const panel = findChild(glance, "glancePanel");
        const pageHost = findChild(glance, "glancePageHost");
        tryCompare(glance, "arrival", 1);
        for (const oddPageArea of [0, 1]) {
            window.setSidebarWidth(window.sidebarDefaultWidth + oddPageArea);
            for (const size of [Qt.size(400, 600), Qt.size(401, 601)]) {
                glance.preferredSize = size;
                verifyOnWholePixels(pageHost, "the popup's page");
                fuzzyCompare(panel.x + panel.width / 2, glance.width / 2, 0.5);
                fuzzyCompare(panel.y + panel.height / 2, glance.height / 2, 0.5);
            }
        }
        glance.preferredSize = Qt.size(0, 0);
        window.closeGlance();
    }

    // What stands in the middle of the page area rests on whole pixels whether
    // the page area is odd or even across.
    function test_centredChromeRestsOnWholePixels() {
        const frame = findChild(window.contentItem, "omnibarFrame");
        const omnibar = findChild(window.contentItem, "omnibar");
        const sheet = findChild(window.contentItem, "shortcutSheet");
        const column = findChild(sheet, "shortcutSheetColumn");
        const notice = findChild(window.contentItem, "spaceNotice");
        const mark = findChild(window.contentItem, "engineViewport").children.filter(function (
            child) {
            return child.objectName === "pageLoadingIndicator";
        })[0];
        const engine = openPage("https://centred.example/");
        const homeId = browser.activeSpaceId;
        const otherId = browser.createSpace("Centred Space");
        for (const oddPageArea of [0, 1]) {
            window.setSidebarWidth(window.sidebarDefaultWidth + oddPageArea);

            window.openOmnibar(false);
            tryCompare(frame, "y", omnibar.restY);
            verifyOnWholePixels(frame, "the Omnibar");
            window.closeOmnibar();
            tryCompare(omnibar, "visible", false);

            // The sheet's height on the page is its lift, which is motion.
            window.requestShortcuts();
            tryCompare(sheet, "visible", true);
            verifyOnWholePixels(column, "the Shortcut sheet", "x");
            window.shortcutsOpen = false;
            tryCompare(sheet, "visible", false);

            engine.loading = true;
            tryCompare(mark, "opacity", 1);
            tryCompare(mark, "scale", 1);
            verifyOnWholePixels(mark, "the loading mark", "x");
            engine.loading = false;
            tryCompare(mark, "opacity", 0);

            // The notice drops in, so only where it stands across is at rest.
            verify(browser.switchSpace(otherId));
            tryVerify(function () {
                return notice.visible;
            });
            verifyOnWholePixels(notice, "the Space notice", "x");
            verify(browser.switchSpace(homeId));
            tryVerify(function () {
                return !notice.visible;
            }, 4000);
        }
        verify(browser.deleteSpace(otherId, "Centred Space"));
    }

    // The split's handle is centred on a one-pixel divider, so its middle is
    // half a pixel in, and it rests on the pixel beside that instead. The
    // loading mark over the pane beside stands in the middle of that pane.
    function test_aSplitsHandleAndMarkRestOnWholePixels() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        openPage("https://handle-left.example/");
        const leftTabId = browser.activeTabId;
        browser.openInput("https://handle-right.example/", true);
        const rightTabId = browser.activeTabId;
        browser.activateTab(leftTabId);
        verify(browser.addSplit(rightTabId));
        tryCompare(engineHost, "besideEngine", engineHost.engines[rightTabId]);
        const resizer = findChild(window.contentItem, "splitResizer");
        tryCompare(resizer, "visible", true);
        verifyOnWholePixels(resizer, "the split's handle", "x");
        const besideMark = findChild(window.contentItem, "besideLoadingIndicator");
        const beside = engineHost.besideEngine;
        beside.loading = true;
        tryCompare(besideMark, "opacity", 1);
        tryCompare(besideMark, "scale", 1);
        verifyOnWholePixels(besideMark, "the loading mark beside", "x");
        beside.loading = false;
        browser.closeTab(rightTabId);
        browser.closeTab(leftTabId);
    }

    // The page area rests on whole pixels of the display beside the sidebar at
    // its default width and at an odd one, while the width the reader chose
    // stays the whole number that is written down.
    function test_thePageAreaRestsOnWholePixelsBesideTheSidebar() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        openPage("https://page-area-pixels.example/");
        compare(window.sidebarWidth, 292);
        verifyOnWholePixels(engineHost, "the page area beside the default sidebar", "x");
        window.setSidebarWidth(261);
        verifyOnWholePixels(engineHost, "the page area beside an odd sidebar", "x");
        compare(window.sidebarWidth, 261);
        tryVerify(function () {
            return browser.preference("sidebar-width", "") === "261";
        });
        window.setSidebarWidth(window.sidebarDefaultWidth);
    }

    // A right sidebar starts on a whole pixel of the display, and the page
    // beside it ends on one, in a window whose width is not a whole number of
    // the display's pixels at a fractional scale.
    function test_aRightSidebarRestsOnWholePixels() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const viewport = findChild(window.contentItem, "engineViewport");
        openPage("https://right-pixels.example/");
        const width = window.width;
        window.setSidebarSide("right");
        window.width = width + 1;
        try {
            tryCompare(window.contentItem, "width", width + 1);
            for (const sidebarWidth of [window.sidebarDefaultWidth, 261]) {
                window.setSidebarWidth(sidebarWidth);
                tryVerify(function () {
                    return Math.abs(sidebar.x + sidebar.width - window.contentItem.width) < 1;
                });
                verifyOnWholePixels(sidebar, "a right sidebar " + sidebarWidth + " wide", "x");
                const pageEnd = viewport.mapToItem(null, viewport.width, 0).x;
                verifyOnDevicePixel(pageEnd * window.devicePixelRatio,
                                    "the page beside a right sidebar ends between pixels");
                compare(pageEnd, sidebar.mapToItem(null, 0, 0).x);
            }
        } finally {
            window.width = width;
            window.setSidebarWidth(window.sidebarDefaultWidth);
        }
    }

    // A split's divider, its handle and the pane right of it rest on whole
    // pixels of the display wherever the reader leaves the divider, here at an
    // odd width for the left pane.
    function test_aSplitsPanesRestOnWholePixelsAtAnOddDivider() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        openPage("https://odd-left.example/");
        const leftTabId = browser.activeTabId;
        browser.openInput("https://odd-right.example/", true);
        const rightTabId = browser.activeTabId;
        browser.activateTab(leftTabId);
        verify(browser.addSplit(rightTabId));
        tryCompare(engineHost, "besideEngine", engineHost.engines[rightTabId]);
        engineHost.setLeftPaneWidth(401);
        verifyOnWholePixels(findChild(engineHost, "splitDivider"), "the split's divider", "x");
        verifyOnWholePixels(findChild(engineHost, "splitResizer"), "the split's handle", "x");
        verifyOnWholePixels(engineHost.engines[rightTabId], "the right pane", "x");
        browser.closeTab(rightTabId);
        browser.closeTab(leftTabId);
    }

    // Developer tools at an odd width start on a whole pixel of the display,
    // so the page beside them ends on one, while the width the reader chose
    // stays the whole number that is written down.
    function test_thePageEndsOnWholePixelsBesideDeveloperTools() {
        openPage("https://dock-pixels.example/");
        window.commands.run("developer-tools", -1);
        const dock = findChild(window.contentItem, "developerToolsDock");
        tryVerify(function () {
            return dock.visible;
        });
        window.setDeveloperToolsWidth(333);
        verifyOnWholePixels(dock, "developer tools at an odd width", "x");
        compare(window.developerToolsWidth, 333);
        tryVerify(function () {
            return browser.preference("developer-tools-width", "") === "333";
        });
        window.setDeveloperToolsWidth(window.developerToolsDefaultWidth);
        browser.closeDeveloperTools();
        tryVerify(function () {
            return !dock.visible;
        });
    }

    // A pinned tab's icon stands in the middle of its tile, at two sidebar
    // widths a pixel apart.
    function test_aPinnedTabsIconRestsOnWholePixels() {
        openPage("https://pinned-pixels.example");
        const tabId = browser.activeTabId;
        browser.toggleActivePinned();
        tryVerify(function () {
            return findChild(window.contentItem, "pinned-" + tabId) !== null;
        });
        const tile = findChild(findChild(window.contentItem, "pinned-" + tabId), "siteTile-"
                               + tabId);
        for (const oddSidebar of [0, 1]) {
            window.setSidebarWidth(window.sidebarDefaultWidth + oddSidebar);
            verifyOnWholePixels(tile, "a pinned tab's icon", "x");
        }
        browser.toggleActivePinned();
        browser.closeTab(tabId);
    }

    // A suggestion without a second line stands in the middle of its row,
    // whichever way the font's height leaves the room around it.
    function test_aSuggestionsTextRestsOnWholePixels() {
        const engine = openPage("https://forms-pixels.example/");
        submitForm(engine, "pixel-field", ["alpha"]);
        engine.simulateFormFieldFocus("pixel-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        const text = findChild(findChild(list, "suggestionRow0"), "suggestionText");
        // A font's line is an odd or an even number of pixels tall.
        for (const taller of [0, 1]) {
            text.height = text.implicitHeight + taller;
            verifyOnWholePixels(text, "a suggestion's text", "y");
        }
        engine.simulateFormFieldBlur();
    }

    // The Glance is the reader's to refuse, from Settings, and the refusal
    // survives a restart because it is a preference like the others there.
    function test_theGlanceIsRefusedFromSettings() {
        window.setGlanceEnabled(true);
        window.requestSettings();
        const toggle = findChild(window.contentItem, "glanceEnabled");
        verify(toggle !== null);
        compare(window.glanceEnabled, true);
        toggle.clicked();
        compare(window.glanceEnabled, false);
        compare(browser.preference("glance", "true"), "false");
        toggle.clicked();
        compare(window.glanceEnabled, true);
        compare(browser.preference("glance", "false"), "true");
        window.settingsOpen = false;
    }

    function test_omnibarShowsOnlyActiveSpaceHistory() {
        const personalSpaceId = browser.activeSpaceId;
        browser.recordVisit("https://personal.example/docs", "Personal docs");
        window.openOmnibar(true);
        const input = findChild(window.contentItem, "omnibarInput");
        const suggestions = findChild(window.contentItem, "omnibarRowList");
        verify(input !== null);
        verify(suggestions !== null);
        input.text = "docs";
        tryCompare(suggestions, "count", 1);

        // The typed text is the selection until the user steps into the list,
        // so the first Down lands on the first suggestion and Up leaves again.
        const panel = findChild(window.contentItem, "omnibar");
        verify(panel !== null);
        compare(panel.selected, -1);
        panel.step(1);
        compare(panel.selected, 0);
        panel.step(1);
        compare(panel.selected, -1);
        panel.step(-1);
        compare(panel.selected, 0);

        window.closeOmnibar();
        const workSpaceId = browser.createSpace("History Work");
        verify(browser.switchSpace(workSpaceId));
        window.openOmnibar(true);
        input.text = "docs";
        tryCompare(suggestions, "count", 0);
        window.closeOmnibar();
        verify(browser.switchSpace(personalSpaceId));
        verify(browser.deleteSpace(workSpaceId, "History Work"));
    }

    // Suggestions arrive from the search thread, so the rows on show are the
    // answer to the last thing typed rather than to whichever search finished
    // last, and closing the Omnibar leaves nothing to arrive.
    function test_omnibarSuggestionsFollowTheLastKeystroke() {
        browser.recordVisit("https://alpha-omni.example/one", "Alpha omni");
        browser.recordVisit("https://omega-omni.example/two", "Omega omni");
        browser.setHistorySearchDelayForTests(40);
        window.openOmnibar(true);
        const input = findChild(window.contentItem, "omnibarInput");
        const suggestions = findChild(window.contentItem, "omnibarRowList");
        verify(input !== null);
        verify(suggestions !== null);

        input.text = "alpha-omni";
        input.text = "no-such-history";
        input.text = "omega-omni";
        tryCompare(suggestions, "count", 1);
        compare(window.omnibarSuggestions[0].url.toString(), "https://omega-omni.example/two");
        // The replaced searches never reach the panel.
        wait(120);
        compare(suggestions.count, 1);

        // A row the reader stepped onto is a different destination once the
        // next answer lands, so the selection goes back to the typed text.
        const panel = findChild(window.contentItem, "omnibar");
        verify(panel !== null);
        panel.step(1);
        compare(panel.selected, 0);
        input.text = "omni";
        panel.step(1);
        compare(panel.selected, 0);
        tryCompare(suggestions, "count", 2);
        compare(panel.selected, -1);

        input.text = "alpha-omni";
        window.closeOmnibar();
        wait(120);
        compare(window.omnibarSuggestions.length, 0);
        browser.setHistorySearchDelayForTests(0);
        verify(browser.deleteHistoryOrigin("https://alpha-omni.example/one"));
        verify(browser.deleteHistoryOrigin("https://omega-omni.example/two"));
    }

    // The engine a keyword selects is named while it is typed, so where the
    // search goes is read off the panel rather than off the page that loads.
    function test_omnibarNamesTheEngineAKeywordSelects() {
        // A Space with no history, so the rows are the keywords alone.
        const homeSpaceId = browser.activeSpaceId;
        const keywordSpaceId = browser.createSpace("Keyword Space");
        verify(browser.switchSpace(keywordSpaceId));
        window.openOmnibar(true);
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const chip = findChild(window.contentItem, "omnibarEngineChip");
        const rows = findChild(window.contentItem, "omnibarRowList");
        verify(panel !== null);
        verify(input !== null);
        verify(chip !== null);
        verify(rows !== null);
        compare(chip.visible, false);
        compare(panel.destination, "");

        // The space after the keyword is what enters the mode.
        input.text = "g";
        compare(chip.visible, false);
        compare(panel.destination, "Open Google");
        input.text = "g ";
        compare(chip.visible, true);
        compare(chip.Accessible.name, "Google");
        compare(input.text, "");
        compare(panel.destination, "Open Google");
        input.text = "rust lifetimes";
        compare(panel.destination, "Search Google for rust lifetimes");
        compare(input.Accessible.name, "Search Google");
        compare(input.Accessible.description, "Search Google for rust lifetimes");
        compare(panel.selected, -1);
        panel.accept();
        compare(browser.activeUrl.toString(), "https://www.google.com/search?q=rust lifetimes");
        tryCompare(window, "omnibarShown", false);

        // Backspace on empty terms is the key that undoes entering the mode.
        window.openOmnibar(true);
        input.text = "g ";
        compare(chip.visible, true);
        input.text = "";
        input.forceActiveFocus();
        keyClick(Qt.Key_Backspace);
        compare(chip.visible, false);
        compare(input.text, "g");

        // A mistyped keyword is visibly a default-engine search.
        input.text = "gg rust";
        compare(chip.visible, false);
        compare(panel.destination, "Search DuckDuckGo for gg rust");
        // An address is not a search, so the row says it opens.
        input.text = "example.com";
        compare(panel.destination, "Open example.com");

        // A prefix offers the keywords it could become, ahead of the commands
        // it starts as strongly.
        input.text = "b";
        tryVerify(function () {
            return rows.count > 2;
        });
        tryVerify(function () {
            return rows.itemAtIndex(1) !== null;
        });
        compare(rows.itemAtIndex(0).Accessible.name, "Search Bing");
        compare(rows.itemAtIndex(1).Accessible.name, "Search Brave Search");
        panel.step(1);
        panel.step(1);
        compare(panel.selected, 1);
        panel.accept();
        compare(chip.visible, true);
        compare(chip.Accessible.name, "Brave Search");
        compare(input.text, "");
        compare(window.omnibarShown, true);
        compare(rows.count, 0);

        // Return with no terms is the engine's front page.
        panel.accept();
        compare(browser.activeUrl.toString(), "https://search.brave.com/");
        tryCompare(window, "omnibarShown", false);

        verify(browser.switchSpace(homeSpaceId));
        verify(browser.deleteSpace(keywordSpaceId, "Keyword Space"));
    }

    function omnibarRowsOf(panel, kind) {
        return panel.rows.filter(function (row) {
            return row.kind === kind;
        });
    }

    // The Omnibar is one field over every kind of row. Text that starts an
    // open tab's title or host names that tab, so Return goes to it; text that
    // only appears inside one is still an address or a search.
    function test_omnibarSelectsTheOpenTabTheTextStarts() {
        const startTabId = browser.activeTabId;
        openPageInNewTab("https://ranking-tab.example/one");
        const quarterlyTabId = browser.activeTabId;
        browser.reportTabPageState(quarterlyTabId, "https://ranking-tab.example/one",
                                   "Quarterly figures", "", false, false);
        openPageInNewTab("https://www.notes-site.example/two");
        const notesTabId = browser.activeTabId;
        browser.reportTabPageState(notesTabId, "https://www.notes-site.example/two",
                                   "Notes on the quarterly figures", "", false, false);
        const tabCount = browser.tabs.rowCount();
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");

        // The tab on show is never a row, whatever the text.
        window.openOmnibar(false);
        input.text = "notes";
        verify(omnibarRowsOf(panel, "tab").every(function (row) {
            return row.argument !== notesTabId;
        }));
        compare(panel.selected, -1);

        input.text = "quar";
        verify(panel.selected >= 0);
        compare(panel.rows[panel.selected].kind, "tab");
        compare(panel.rows[panel.selected].argument, quarterlyTabId);
        panel.accept();
        tryCompare(window, "omnibarShown", false);
        compare(browser.activeTabId, quarterlyTabId);
        compare(browser.tabs.rowCount(), tabCount);

        // The host starts it as well, without the www a reader never types.
        window.openOmnibar(false);
        input.text = "notes-s";
        compare(panel.rows[panel.selected].argument, notesTabId);

        // Inside a title is listed, and the typed text stays the selection,
        // for a tab and for a visit alike.
        browser.recordVisit("https://annual-report.example/", "Annual figures");
        input.text = "figures";
        verify(omnibarRowsOf(panel, "tab").some(function (row) {
            return row.argument === notesTabId;
        }));
        tryVerify(function () {
            return omnibarRowsOf(panel, "history").some(function (row) {
                return row.url === "https://annual-report.example/";
            });
        });
        compare(panel.selected, -1);

        // The unedited preset is the page on show, and Return goes there
        // again rather than to anything listed: not even to a second tab of
        // the same address, whose untitled page the preset starts.
        window.closeOmnibar();
        openPageInNewTab("https://ranking-tab.example/one");
        const twinTabId = browser.activeTabId;
        browser.reportTabPageState(twinTabId, "https://ranking-tab.example/one",
                                   "https://ranking-tab.example/one", "", false, false);
        browser.activateTab(quarterlyTabId);
        window.openOmnibar(false);
        compare(input.text, "https://ranking-tab.example/one");
        compare(omnibarRowsOf(panel, "tab").length, 0);
        compare(panel.selected, -1);
        panel.accept();
        compare(browser.activeTabId, quarterlyTabId);
        compare(browser.tabs.rowCount(), tabCount + 1);
        compare(browser.activeUrl.toString(), "https://ranking-tab.example/one");

        browser.closeTab(twinTabId);
        browser.closeTab(notesTabId);
        browser.closeTab(quarterlyTabId);
        browser.activateTab(startTabId);
        verify(browser.deleteHistoryOrigin("https://annual-report.example/"));
    }

    // A new tab exists only once something is committed, and choosing a tab
    // that is already open commits nothing.
    function test_aNewTabOmnibarSwitchingToAnOpenTabCreatesNone() {
        const startTabId = browser.activeTabId;
        openPageInNewTab("https://gamma-open.example/");
        const gammaTabId = browser.activeTabId;
        browser.reportTabPageState(gammaTabId, "https://gamma-open.example/", "Gamma open page", "",
                                   false, false);
        openPageInNewTab("https://delta-open.example/");
        const deltaTabId = browser.activeTabId;
        const tabCount = browser.tabs.rowCount();
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");

        window.openOmnibar(true);
        input.text = "gamma";
        compare(panel.rows[panel.selected].argument, gammaTabId);
        panel.accept();
        compare(browser.activeTabId, gammaTabId);
        compare(browser.tabs.rowCount(), tabCount);

        window.openOmnibar(true);
        input.text = "https://epsilon-open.example/";
        panel.accept();
        compare(browser.tabs.rowCount(), tabCount + 1);
        const epsilonTabId = browser.activeTabId;
        verify(epsilonTabId !== gammaTabId);

        browser.closeTab(epsilonTabId);
        browser.closeTab(deltaTabId);
        browser.closeTab(gammaTabId);
        browser.activateTab(startTabId);
    }

    // Two Spaces away from the one on show, each with a page the reader
    // opened and then left, and a tab of the Space on show beside the start.
    // Long enough that its row has to give way for the Space's name.
    readonly property string alphaTitle: "The orbit plan for boards, with every milestone, owner "
                                         + "and date the team agreed on over the spring"

    function openTabsInOtherSpaces() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const homeSpaceId = browser.activeSpaceId;
        const startTabId = browser.activeTabId;
        openPageInNewTab("https://notes.example/orbit");
        const localTabId = browser.activeTabId;
        browser.reportTabPageState(localTabId, "https://notes.example/orbit", "Notes on orbit", "",
                                   false, false);
        browser.activateTab(startTabId);
        const alphaSpaceId = browser.createSpace("Alpha");
        verify(browser.switchSpace(alphaSpaceId));
        openPage("https://plans.example/orbit");
        const alphaTabId = browser.activeTabId;
        browser.reportTabPageState(alphaTabId, "https://plans.example/orbit", alphaTitle, "", false,
                                   false);
        const alphaEngine = engineHost.item;
        const betaSpaceId = browser.createSpace("Beta");
        verify(browser.switchSpace(betaSpaceId));
        openPage("https://boards.example/orbit");
        const betaTabId = browser.activeTabId;
        browser.reportTabPageState(betaTabId, "https://boards.example/orbit", "Orbit board", "",
                                   false, false);
        const betaEngine = engineHost.item;
        verify(browser.switchSpace(homeSpaceId));
        tryCompare(alphaEngine, "pageFrozen", true);
        tryCompare(betaEngine, "pageFrozen", true);
        return {
            homeSpaceId: homeSpaceId,
            startTabId: startTabId,
            localTabId: localTabId,
            alphaSpaceId: alphaSpaceId,
            alphaTabId: alphaTabId,
            alphaEngine: alphaEngine,
            betaSpaceId: betaSpaceId,
            betaTabId: betaTabId,
            betaEngine: betaEngine
        };
    }

    // An Agent Space has no palette colour on screen, so the Omnibar names it
    // as the footer draws it: muted while no Agent uses it, in the Agent
    // accent while one is attached.
    function test_omnibarNamesAnAgentSpaceInTheAgentsColours() {
        const homeSpaceId = browser.activeSpaceId;
        const agentId = agentSpaceProbe.create("Crawler", "claude-code", false);
        verify(browser.switchSpace(agentId));
        openPage("https://crawl.example/quasar");
        browser.reportTabPageState(browser.activeTabId, "https://crawl.example/quasar",
                                   "Quasar crawl", "", false, false);
        verify(browser.switchSpace(homeSpaceId));
        tryCompare(findChild(window.contentItem, "sidebar"), "arriving", false);
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");

        window.openOmnibar(false);
        input.text = "quasar";
        const tabRow = function () {
            return omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "tab")[0]));
        };
        tryVerify(function () {
            return omnibarRowsOf(panel, "tab").length === 1;
        });
        // The label is the Space's colour until the row is the selected one.
        panel.selected = -1;
        const suffix = findChild(tabRow(), "omnibarRowSpace");
        compare(suffix.text, "Crawler");
        verify(Qt.colorEqual(suffix.color, window.colors.mutedText));
        input.text = "crawler";
        tryVerify(function () {
            return omnibarRowsOf(panel, "space").length === 1;
        });
        const spaceRow = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "space")[0]));
        verify(Qt.colorEqual(findChild(spaceRow, "omnibarRowSpaceColor").color,
                             window.colors.mutedText));

        const control = agentActivityComponent.createObject(testCase);
        const activity = {};
        activity["elsewhere-tab"] = {
            "spaceId": agentId,
            "name": "claude-code",
            "act": "",
            "busy": false
        };
        control.agentActivity = activity;
        findChild(window.contentItem, "engineLoader").agentControl = control;
        verify(Qt.colorEqual(findChild(spaceRow, "omnibarRowSpaceColor").color,
                             window.colors.agentAccent));
        input.text = "quasar";
        tryVerify(function () {
            return omnibarRowsOf(panel, "tab").length === 1;
        });
        panel.selected = -1;
        verify(Qt.colorEqual(findChild(tabRow(), "omnibarRowSpace").color,
                             window.colors.agentAccent));
        panel.selected = panel.rows.indexOf(omnibarRowsOf(panel, "tab")[0]);
        verify(Qt.colorEqual(findChild(tabRow(), "omnibarRowSpace").color, window.colors.text));
        findChild(window.contentItem, "engineLoader").agentControl = window.agentControlSource;
        control.destroy();
        window.closeOmnibar();
        verify(browser.deleteSpace(agentId, "Crawler"));
    }

    function closeTabsInOtherSpaces(opened) {
        window.closeOmnibar();
        verify(browser.switchSpace(opened.homeSpaceId));
        verify(browser.deleteSpace(opened.alphaSpaceId, "Alpha"));
        verify(browser.deleteSpace(opened.betaSpaceId, "Beta"));
        browser.closeTab(opened.localTabId);
        browser.activateTab(opened.startTabId);
    }

    // Every Space's open tabs are one list: the Space on show's first,
    // however weakly they hold the text, then each other Space's in Space
    // order, each naming its Space. Listing them wakes no page.
    function test_omnibarListsTheTabsOfEverySpace() {
        const opened = openTabsInOtherSpaces();
        const engineHost = findChild(window.contentItem, "engineLoader");
        const engineCount = Object.keys(engineHost.engines).length;
        awayPageSpy.clear();
        awayPageSpy.target = opened.alphaEngine;
        otherAwayPageSpy.clear();
        otherAwayPageSpy.target = opened.betaEngine;
        listingSpaceSpy.clear();
        listingSpaceSpy.target = browser;
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");

        window.openOmnibar(false);
        input.text = "orbit";
        compare(omnibarRowsOf(panel, "tab").map(function (row) {
            return row.argument;
        }), [opened.localTabId, opened.alphaTabId, opened.betaTabId]);

        const local = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "tab")[0]));
        compare(findChild(local, "omnibarRowSpace").visible, false);
        compare(local.Accessible.name, "Switch to tab Notes on orbit");

        const away = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "tab")[1]));
        panel.selected = -1;
        const suffix = findChild(away, "omnibarRowSpace");
        compare(suffix.visible, true);
        compare(suffix.text, "Alpha");
        const spaces = browser.spaces;
        const alphaRow = spaces.index(spaces.rowCount() - 2, 0);
        compare(spaces.data(alphaRow, Qt.UserRole + 2), "Alpha");
        // In the colour the theme gives the Space's palette name, which
        // differs from the next Space's, and both follow a theme change.
        const alphaColour = spaces.data(alphaRow, Qt.UserRole + 3);
        const betaColour = spaces.data(spaces.index(spaces.rowCount() - 1, 0), Qt.UserRole + 3);
        verify(Qt.colorEqual(suffix.color, window.colors.spaces[alphaColour]));
        const beta = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "tab")[2]));
        const betaSuffix = findChild(beta, "omnibarRowSpace");
        compare(betaSuffix.text, "Beta");
        verify(Qt.colorEqual(betaSuffix.color, window.colors.spaces[betaColour]));
        verify(!Qt.colorEqual(betaSuffix.color, suffix.color));
        const changed = Object.assign({}, window.colors);
        changed.spaces = {
            "green": "#00aa44",
            "yellow": "#aa8800",
            "blue": "#0044aa",
            "bright_green": "#22cc66",
            "bright_yellow": "#ccaa22",
            "bright_blue": "#2266cc"
        };
        window.colors = changed;
        verify(Qt.colorEqual(suffix.color, changed.spaces[alphaColour]));
        verify(Qt.colorEqual(betaSuffix.color, changed.spaces[betaColour]));
        window.colors = Qt.binding(function () {
            return theme.palette;
        });
        compare(findChild(away, "omnibarRowTitle").text, alphaTitle);
        compare(findChild(away, "omnibarRowHost").text, "plans.example");
        compare(away.action, "Switch to Tab");
        // The title gives way before the host, and the Space's name, the
        // row's label at its right edge, is drawn whole: it is said there
        // and not again beside the host, and the word it replaces is gone.
        const awayTitle = findChild(away, "omnibarRowTitle");
        verify(awayTitle.truncated);
        verify(!findChild(away, "omnibarRowHost").truncated);
        compare(suffix.font.pixelSize, findChild(away, "omnibarRowAction").font.pixelSize);
        compare(suffix.width, suffix.implicitWidth);
        verify(!findChild(away, "omnibarRowAction").visible);
        verify(suffix.mapToItem(away, 0, 0).x >= awayTitle.mapToItem(away, awayTitle.width, 0).x);
        verify(suffix.mapToItem(away, suffix.width, 0).x <= away.width);
        verify(findChild(away, "omnibarRowGo").visible);
        panel.selected = panel.rows.indexOf(omnibarRowsOf(panel, "tab")[1]);
        verify(Qt.colorEqual(suffix.color, window.colors.text));
        panel.selected = -1;
        compare(away.Accessible.name, "Switch to tab " + alphaTitle + " in Alpha");

        compare(Object.keys(engineHost.engines).length, engineCount);
        compare(awayPageSpy.count, 0);
        awayPageSpy.target = null;
        compare(otherAwayPageSpy.count, 0);
        otherAwayPageSpy.target = null;
        compare(listingSpaceSpy.count, 0);
        listingSpaceSpy.target = null;
        compare(opened.alphaEngine.pageFrozen, true);
        compare(opened.betaEngine.pageFrozen, true);
        compare(browser.activeSpaceId, opened.homeSpaceId);

        closeTabsInOtherSpaces(opened);
    }

    // Committing another Space's tab switches to that Space with the tab on
    // show, in one step, and the page it wakes is the one that was left.
    // Text that starts its title names it for Return when no tab of the Space
    // on show holds the text.
    function test_omnibarSwitchesSpaceAndSelectsTheTab() {
        const opened = openTabsInOtherSpaces();
        const engineHost = findChild(window.contentItem, "engineLoader");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");

        // A tab of the Space on show that holds the text keeps Return on the
        // typed text, whatever another Space's tab holds.
        window.openOmnibar(false);
        input.text = "orbit";
        compare(panel.selected, -1);

        // The tab the text starts is the one named, even where another
        // Space's weaker match stands above it in Space order.
        input.text = "board";
        compare(omnibarRowsOf(panel, "tab").map(function (row) {
            return row.argument;
        }), [opened.alphaTabId, opened.betaTabId]);
        verify(panel.selected >= 0);
        compare(panel.rows[panel.selected].argument, opened.betaTabId);

        input.text = "orbit b";
        compare(omnibarRowsOf(panel, "tab").length, 1);
        verify(panel.selected >= 0);
        compare(panel.rows[panel.selected].argument, opened.betaTabId);
        panel.accept();
        tryCompare(window, "omnibarShown", false);
        compare(browser.activeSpaceId, opened.betaSpaceId);
        compare(browser.activeTabId, opened.betaTabId);
        tryVerify(function () {
            return engineHost.item === opened.betaEngine;
        });
        tryCompare(opened.betaEngine, "pageFrozen", false);
        compare(opened.alphaEngine.pageFrozen, true);

        // The Space it came from is now one of the others, whether the
        // Omnibar opens after the switch or was open through it.
        window.openOmnibar(false);
        input.text = "orbit";
        compare(omnibarRowsOf(panel, "tab").map(function (row) {
            return row.argument;
        }), [opened.localTabId, opened.alphaTabId]);
        verify(browser.switchSpace(opened.alphaSpaceId));
        compare(window.omnibarShown, true);
        compare(omnibarRowsOf(panel, "tab").map(function (row) {
            return row.argument;
        }), [opened.localTabId, opened.betaTabId]);

        closeTabsInOtherSpaces(opened);
    }

    // Spaces and commands are named in the same list, each row saying what
    // committing it does.
    function test_omnibarListsSpacesAndCommandsBesideTheAddress() {
        const homeSpaceId = browser.activeSpaceId;
        const zephyrSpaceId = browser.createSpace("Zephyr reading");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");

        window.openOmnibar(false);
        input.text = "zephyr";
        const spaces = omnibarRowsOf(panel, "space");
        compare(spaces.length, 1);
        compare(spaces[0].argument, zephyrSpaceId);
        compare(panel.selected, -1);
        const spaceRow = panel.rows.indexOf(spaces[0]);
        tryVerify(function () {
            return rows.itemAtIndex(spaceRow) !== null;
        });
        compare(rows.itemAtIndex(spaceRow).Accessible.name, "Switch to Space Zephyr reading");
        compare(rows.itemAtIndex(spaceRow).action, "Switch to Space");
        compare(findChild(rows.itemAtIndex(spaceRow), "omnibarRowSpaceColor").visible, true);

        input.text = "zoom in";
        const commands = omnibarRowsOf(panel, "command");
        verify(commands.length > 0);
        compare(commands[0].command, "zoom-in");
        compare(panel.selected, -1);

        input.text = "zephyr";
        panel.selected = panel.rows.indexOf(omnibarRowsOf(panel, "space")[0]);
        panel.accept();
        compare(browser.activeSpaceId, zephyrSpaceId);
        // The Space it switched to is at rest, so what shows is its Start page
        // and the overlay has gone.
        compare(window.omnibarOpen, false);
        tryVerify(function () {
            return window.startPageShown;
        });

        verify(browser.switchSpace(homeSpaceId));
        verify(browser.deleteSpace(zephyrSpaceId, "Zephyr reading"));
    }

    // Answers a suggest request from the loopback stub the harness runs,
    // through an engine the test adds and makes the default.
    function useStubSuggestions(body) {
        suggestServer.answer(body);
        verify(browser.addSearchEngine("Stub", "https://stub.example/?q={query}", "",
                                       suggestServer.suggestUrl()));
        engineSuggestions.enabled = true;
    }

    function stopStubSuggestions() {
        engineSuggestions.enabled = false;
        browser.deleteSearchEngine("stub");
        browser.setDefaultSearchEngine("duckduckgo");
    }

    // With the setting on, typed search terms go to the default engine and
    // its proposals list under every local row: four at most, never the terms
    // again, each a search of the engine that proposed it.
    function test_engineSuggestionsListBelowEveryLocalRow() {
        const startTabId = browser.activeTabId;
        openPageInNewTab("https://weather-start.example/");
        const commitTabId = browser.activeTabId;
        browser.recordVisit("https://weathered.example/log", "Weathered log");
        useStubSuggestions('["weath", ["weather", "WEATH", "weather.com", "Weather <img src=x>",'
                           + ' "weather today", "weather week"]]');
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");
        try {
            const asked = suggestServer.requestCount();
            window.openOmnibar(false);
            input.text = "weath";
            tryVerify(function () {
                return omnibarRowsOf(panel, "suggestion").length === 4;
            });
            compare(suggestServer.requestCount(), asked + 1);
            compare(suggestServer.lastTarget(), "/suggest?q=weath");
            tryVerify(function () {
                return omnibarRowsOf(panel, "history").length === 1;
            });
            const first = panel.rows.indexOf(omnibarRowsOf(panel, "suggestion")[0]);
            compare(first, panel.rows.length - 4);
            compare(omnibarRowsOf(panel, "suggestion").map(function (row) {
                return row.title;
            }), ["weather", "weather.com", "Weather <img src=x>", "weather today"]);
            // Nothing is ever selected for the reader: Return on the typed
            // text still searches the typed text.
            compare(panel.selected, -1);

            let row = omnibarRowItem(rows, first);
            compare(row.action, "Search");
            compare(row.keys, "");
            compare(row.Accessible.name, "Search Stub for weather");
            compare(findChild(row, "omnibarRowHost").visible, false);
            compare(findChild(row, "omnibarRowTitle").text, "weath<b>er</b>");
            const tile = findChild(row, "omnibarRowTile");
            compare(tile.visible, true);
            compare(tile.siteUrl.toString(), "https://stub.example/");
            // What the engine sent is text, never markup the row would draw.
            row = omnibarRowItem(rows, first + 2);
            compare(findChild(row, "omnibarRowTitle").text, "Weath<b>er &lt;img src=x&gt;</b>");

            // Typing on past the asked terms keeps the answer listed, and
            // leaving them drops it at once, before any new answer arrives.
            input.text = "weathe";
            compare(omnibarRowsOf(panel, "suggestion").length, 4);
            compare(findChild(omnibarRowItem(rows, first), "omnibarRowTitle").text,
                    "weathe<b>r</b>");
            suggestServer.answer('["wind", ["windy"]]');
            input.text = "wind";
            compare(omnibarRowsOf(panel, "suggestion").length, 0);
            tryVerify(function () {
                return omnibarRowsOf(panel, "suggestion").length === 1;
            });
            suggestServer.answer(
                        '["weath", ["weather", "WEATH", "weather.com", "Weather <img src=x>",'
                        + ' "weather today", "weather week"]]');
            input.text = "weath";
            tryVerify(function () {
                return omnibarRowsOf(panel, "suggestion").length === 4
                        && panel.engineSuggestions.terms === "weath";
            });

            // An address-looking proposal is still a search of its engine.
            panel.selected = first + 1;
            panel.accept();
            tryVerify(function () {
                return browser.activeUrl.toString() === "https://stub.example/?q=weather.com";
            });
            compare(browser.activeTabId, commitTabId);
        } finally {
            window.closeOmnibar();
            stopStubSuggestions();
            browser.deleteHistoryOrigin("https://weathered.example/log");
            browser.closeTab(commitTabId);
            browser.activateTab(startTabId);
        }
    }

    // Off, typing sends nothing; on, command scope and an address send
    // nothing either. Closing the Omnibar leaves no proposal behind.
    function test_engineSuggestionsAskOnlyForSearchTerms() {
        useStubSuggestions('["weath", ["weather"]]');
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        try {
            const asked = suggestServer.requestCount();
            engineSuggestions.enabled = false;
            window.openOmnibar(false);
            input.text = "weath";
            wait(400);
            compare(suggestServer.requestCount(), asked);
            compare(omnibarRowsOf(panel, "suggestion").length, 0);
            window.closeOmnibar();

            engineSuggestions.enabled = true;
            window.openOmnibar(false);
            input.text = "";
            input.text = ":";
            verify(panel.commandScope);
            input.text = "weath";
            // Past the pause, so a request that was going to go has gone.
            wait(400);
            compare(suggestServer.requestCount(), asked);
            input.text = "";
            input.text = "weath";
            tryVerify(function () {
                return omnibarRowsOf(panel, "suggestion").length === 0 && panel.commandScope;
            });
            window.closeOmnibar();

            window.openOmnibar(false);
            input.text = "github.com/weath";
            wait(400);
            compare(suggestServer.requestCount(), asked);
            input.text = "weath";
            tryVerify(function () {
                return omnibarRowsOf(panel, "suggestion").length === 1;
            });
            window.closeOmnibar();
            window.openOmnibar(false);
            compare(omnibarRowsOf(panel, "suggestion").length, 0);
        } finally {
            window.closeOmnibar();
            stopStubSuggestions();
        }
    }

    // The add-engine form takes a suggest URL, and leaving it empty adds an
    // engine that offers none.
    function test_addEngineFormTakesAnOptionalSuggestUrl() {
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("search");
        const name = findChild(settings, "engineName");
        const queryUrl = findChild(settings, "engineQueryUrl");
        const suggestUrl = findChild(settings, "engineSuggestUrl");
        const keyword = findChild(settings, "engineKeyword");
        const add = findChild(settings, "addSearchEngineButton");
        const caption = findChild(settings, "engineSuggestUrlCaption");
        try {
            verify(suggestUrl !== null);
            compare(suggestUrl.placeholderText, "optional suggest URL with {query}");
            compare(caption.text, "Answers in OpenSearch suggestions JSON. Leave empty if the "
                    + "engine offers none.");
            // Between the query URL and the keyword, with its caption under it.
            const top = function (item) {
                return item.mapToItem(settings, 0, 0).y;
            };
            verify(top(queryUrl) < top(suggestUrl));
            verify(top(suggestUrl) < top(caption));
            verify(top(caption) < top(keyword));

            name.text = "Proposing";
            queryUrl.text = "https://proposing.example/?q={query}";
            suggestUrl.text = "https://proposing.example/ac?q={query}";
            add.clicked();
            compare(browser.searchEngine("proposing").suggestUrl,
                    "https://proposing.example/ac?q={query}");
            compare(suggestUrl.text, "");

            name.text = "Quiet";
            queryUrl.text = "https://quiet.example/?q={query}";
            add.clicked();
            compare(browser.searchEngine("quiet").suggestUrl, "");

            // A suggest URL with nowhere to put the terms is refused, as a
            // query URL would be.
            name.text = "Broken";
            queryUrl.text = "https://broken.example/?q={query}";
            suggestUrl.text = "https://broken.example/ac";
            verify(!add.enabled);
        } finally {
            name.text = "";
            queryUrl.text = "";
            suggestUrl.text = "";
            browser.deleteSearchEngine("proposing");
            browser.deleteSearchEngine("quiet");
            browser.setDefaultSearchEngine("duckduckgo");
            window.settingsOpen = false;
        }
    }

    function omnibarRowItem(rows, index) {
        tryVerify(function () {
            return rows.itemAtIndex(index) !== null;
        });
        return rows.itemAtIndex(index);
    }

    // A site the Space loaded keeps its favicon after its tab closes, and a
    // history row for it draws that icon with no tab of the site open.
    function test_omnibarHistoryRowDrawsTheSpacesStoredFavicon() {
        const startTabId = browser.activeTabId;
        openPageInNewTab("https://stored-icon.example/page");
        const storedTabId = browser.activeTabId;
        browser.reportTabPageState(storedTabId, "https://stored-icon.example/page",
                                   "Stored icon page", "image://omawebtesticon/#d04040", false,
                                   false);
        browser.recordVisit("https://stored-icon.example/page", "Stored icon page");
        const address = browser.storedFavicon("https://stored-icon.example/page");
        browser.closeTab(storedTabId);
        browser.activateTab(startTabId);

        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");
        window.openOmnibar(false);
        input.text = "stored-icon";
        tryVerify(function () {
            return omnibarRowsOf(panel, "history").length === 1;
        });
        compare(omnibarRowsOf(panel, "tab").length, 0);
        const row = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "history")[0]));
        const tile = findChild(row, "omnibarRowTile");
        compare(tile.iconUrl, address);
        tryVerify(function () {
            return tile.showsArtwork;
        });
        window.closeOmnibar();
    }

    // A row leads with a picture of what it names and ends with what
    // committing it does, bright only on the row Return would commit.
    function test_omnibarRowsLeadWithAPictureAndEndWithTheirAction() {
        const homeSpaceId = browser.activeSpaceId;
        const startTabId = browser.activeTabId;
        openPageInNewTab("https://edge-tab.example/one");
        const edgeTabId = browser.activeTabId;
        browser.reportTabPageState(edgeTabId, "https://edge-tab.example/one", "Edge tab page",
                                   "image://favicon/https://edge-tab.example/favicon.ico", false,
                                   false);
        browser.activateTab(startTabId);
        browser.recordVisit("https://www.edge-history.example/deep/page", "Edge history page");
        const edgeSpaceId = browser.createSpace("Edge Space");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");

        window.openOmnibar(false);
        input.text = "edge-tab";
        let row = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "tab")[0]));
        compare(row.action, "Switch to Tab");
        compare(row.keys, "");
        compare(row.Accessible.name, "Switch to tab Edge tab page");
        compare(findChild(row, "omnibarRowTitle").text, "Edge tab page");
        compare(findChild(row, "omnibarRowHost").text, "edge-tab.example");
        let tile = findChild(row, "omnibarRowTile");
        compare(tile.visible, true);
        compare(tile.iconUrl.toString(), "image://favicon/https://edge-tab.example/favicon.ico");
        compare(tile.useArtwork, true);

        // The tile follows the sidebar's favicon setting.
        window.setUseFavicons(false);
        compare(tile.useArtwork, false);
        compare(tile.code, "ED");
        window.setUseFavicons(true);

        input.text = "edge-history";
        tryVerify(function () {
            return omnibarRowsOf(panel, "history").length === 1;
        });
        row = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "history")[0]));
        compare(row.action, "Open");
        compare(row.keys, "");
        compare(row.Accessible.name, "Open history result Edge history page");
        compare(row.Accessible.description, "https://www.edge-history.example/deep/page");
        compare(findChild(row, "omnibarRowHost").text, "edge-history.example");
        compare(findChild(row, "omnibarRowTile").siteUrl.toString(),
                "https://www.edge-history.example/deep/page");
        // No tab of this site is open and the Space stored no icon for it, so
        // the tile asks the store and draws the host code.
        tile = findChild(row, "omnibarRowTile");
        compare(tile.visible, true);
        compare(tile.iconUrl, browser.storedFavicon("https://www.edge-history.example/deep/page"));
        wait(50);
        compare(tile.showsArtwork, false);
        compare(tile.code, "ED");
        window.setUseFavicons(false);
        compare(tile.useArtwork, false);
        compare(tile.code, "ED");
        window.setUseFavicons(true);

        // A history result on a site with an open tab takes that tab's icon.
        browser.recordVisit("https://edge-tab.example/older", "Edge tab older page");
        input.text = "older";
        let older = [];
        tryVerify(function () {
            older = omnibarRowsOf(panel, "history").filter(function (entry) {
                return entry.url === "https://edge-tab.example/older";
            });
            return older.length === 1;
        });
        row = omnibarRowItem(rows, panel.rows.indexOf(older[0]));
        compare(findChild(row, "omnibarRowTile").iconUrl.toString(),
                "image://favicon/https://edge-tab.example/favicon.ico");

        input.text = "edge space";
        row = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "space")[0]));
        compare(row.action, "Switch to Space");
        compare(row.keys, "");
        compare(findChild(row, "omnibarRowTile").visible, false);
        const spaceColor = findChild(row, "omnibarRowSpaceColor");
        compare(spaceColor.visible, true);
        let edgeSpaceColor = "";
        for (let space = 0; space < browser.spaces.rowCount(); ++space) {
            const index = browser.spaces.index(space, 0);
            if (browser.spaces.data(index, Qt.UserRole + 1) === edgeSpaceId)
                edgeSpaceColor = browser.spaces.data(index, Qt.UserRole + 3);
        }
        verify(window.colors.spaces[edgeSpaceColor] !== undefined);
        verify(Qt.colorEqual(spaceColor.color, window.colors.spaces[edgeSpaceColor]));

        // A keyword's tile is its engine's own site.
        input.text = "br";
        tryVerify(function () {
            return omnibarRowsOf(panel, "keyword").length === 1;
        });
        // The Space never loaded the engine's site, so the tile draws its host
        // code, and still does with favicons off.
        wait(50);
        row = omnibarRowItem(rows, panel.rows.indexOf(omnibarRowsOf(panel, "keyword")[0]));
        compare(row.action, "Search");
        compare(row.keys, "br");
        tile = findChild(row, "omnibarRowTile");
        compare(tile.visible, true);
        compare(tile.siteUrl.toString(), "https://search.brave.com/");
        compare(tile.code, "BR");
        compare(tile.showsArtwork, false);
        window.setUseFavicons(false);
        compare(tile.useArtwork, false);
        compare(tile.code, "BR");
        window.setUseFavicons(true);

        // A command keeps its keys at the edge and says nothing more there.
        input.text = "zoom in";
        const zoomIndex = panel.rows.indexOf(omnibarRowsOf(panel, "command")[0]);
        row = omnibarRowItem(rows, zoomIndex);
        compare(row.action, "");
        compare(row.keys, panel.rows[zoomIndex].keys);
        verify(row.keys.length > 0);
        compare(findChild(row, "omnibarRowSymbol").text, window.commands.groupSymbols.page);
        compare(findChild(row, "omnibarRowTile").visible, false);

        // Only the selected row's action is bright.
        // "edge" lists both history pages once the search answers, so the
        // test waits for the settled rows rather than counting them.
        input.text = "edge";
        tryVerify(function () {
            return omnibarRowsOf(panel, "history").length === 2;
        });
        const tabIndex = panel.rows.indexOf(omnibarRowsOf(panel, "tab")[0]);
        const edgeHistory = omnibarRowsOf(panel, "history").filter(function (entry) {
            return entry.url === "https://www.edge-history.example/deep/page";
        });
        const historyIndex = panel.rows.indexOf(edgeHistory[0]);
        panel.selected = tabIndex;
        compare(findChild(omnibarRowItem(rows, tabIndex), "omnibarRowAction").color,
                window.colors.text);
        compare(findChild(omnibarRowItem(rows, historyIndex), "omnibarRowAction").color,
                window.colors.mutedText);
        panel.selected = historyIndex;
        compare(findChild(omnibarRowItem(rows, tabIndex), "omnibarRowAction").color,
                window.colors.mutedText);
        compare(findChild(omnibarRowItem(rows, historyIndex), "omnibarRowAction").color,
                window.colors.text);

        window.closeOmnibar();
        verify(browser.deleteSpace(edgeSpaceId, "Edge Space"));
        verify(browser.deleteHistoryOrigin("https://www.edge-history.example/deep/page"));
        verify(browser.deleteHistoryOrigin("https://edge-tab.example/older"));
        browser.closeTab(edgeTabId);
        compare(browser.activeSpaceId, homeSpaceId);
    }

    // In command scope every row carries its group's symbol and no group
    // name, so its rows read like the widened Omnibar's.
    function test_commandScopeRowsCarryTheirGroupSymbol() {
        const panel = findChild(window.contentItem, "omnibar");
        const rows = findChild(window.contentItem, "omnibarRowList");
        window.openCommandScope();
        verify(panel.rows.length > 1);
        for (let index = 0; index < Math.min(panel.rows.length, 8); ++index) {
            const row = omnibarRowItem(rows, index);
            compare(findChild(row, "omnibarRowSymbol").text,
                    window.commands.groupSymbols[panel.rows[index].group]);
            const picture = findChild(row, "omnibarRowPicture");
            compare(findChild(row, "omnibarRowTitle").mapToItem(row, 0, 0).x, picture.x
                    + picture.width + 10);
            compare(row.action, "");
        }
        window.closeOmnibar();
    }

    // `:` is the command scope: the prompt takes it, backspacing it asks the
    // same text of everything, and text never starts with it.
    function test_theCommandScopeIsALeadingColonInBothDirections() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const prompt = findChild(window.contentItem, "omnibarPrompt");
        const mark = findChild(window.contentItem, "omnibarMark");
        const colon = findChild(window.contentItem, "omnibarColon");
        const go = findChild(window.contentItem, "omnibarGo");
        const accent = String(window.colors.accent);
        const markFill = mark.data.filter(function (child) {
            return child.fillColor !== undefined;
        })[0];
        browser.recordVisit("https://reopen-notes.example/", "Reopen notes");
        openPage("https://command-scope.example/");
        activateWindow();

        keyClick(Qt.Key_K, Qt.ControlModifier);
        compare(panel.commandScope, true);
        compare(prompt.text, ":");
        verify(colon.visible);
        verify(!mark.visible);
        compare(String(colon.color), accent);
        compare(String(go.color), accent);
        compare(input.text, "");
        tryVerify(function () {
            return input.activeFocus;
        });
        input.text = "reopen";
        verify(panel.rows.length > 0);
        verify(panel.rows.every(function (row) {
            return row.kind === "command";
        }));
        compare(panel.rows[0].command, "reopen-tab");
        compare(panel.selected, 0);

        input.cursorPosition = 0;
        keyClick(Qt.Key_Backspace);
        compare(panel.commandScope, false);
        compare(prompt.text, "mark");
        verify(mark.visible);
        verify(!colon.visible);
        compare(String(markFill.fillColor), accent);
        compare(input.text, "reopen");
        compare(panel.selected, -1);
        verify(omnibarRowsOf(panel, "command").some(function (row) {
            return row.command === "reopen-tab";
        }));
        tryVerify(function () {
            return omnibarRowsOf(panel, "history").some(function (row) {
                return row.url === "https://reopen-notes.example/";
            });
        });

        // Backspace inside the text is only a Backspace.
        input.text = ":reopen";
        compare(panel.commandScope, true);
        compare(input.text, "reopen");
        input.cursorPosition = 3;
        keyClick(Qt.Key_Backspace);
        compare(panel.commandScope, true);
        compare(input.text, "repen");

        // Typing `:` into an empty field narrows it.
        window.closeOmnibar();
        window.openOmnibar(true);
        compare(panel.commandScope, false);
        input.text = ":";
        compare(panel.commandScope, true);
        compare(input.text, "");
        compare(prompt.text, ":");
        window.closeOmnibar();
        verify(browser.deleteHistoryOrigin("https://reopen-notes.example/"));
    }

    // The go mark at the end of the field is Return, for the pointer.
    function test_theGoMarkCommitsAsReturnDoes() {
        const startTabId = browser.activeTabId;
        const input = findChild(window.contentItem, "omnibarInput");
        const go = findChild(window.contentItem, "omnibarGo");
        window.openOmnibar(true);
        compare(go.Accessible.name, "Go");
        input.text = "https://go-mark.example/";
        mouseClick(go);
        tryCompare(window, "omnibarShown", false);
        compare(browser.activeUrl.toString(), "https://go-mark.example/");
        browser.closeTab(browser.activeTabId);
        browser.activateTab(startTabId);
    }

    // The Omnibar drops into its own place from just above it, whichever key
    // opened it: it never starts on the sidebar's address field and crosses
    // the window from there.
    function test_theOmnibarArrivesInItsOwnPlace() {
        // Over a page: on the Start page the Omnibar is already at rest.
        openPage("https://arrival.example/");
        const panel = findChild(window.contentItem, "omnibar");
        const frame = findChild(window.contentItem, "omnibarFrame");
        verify(!window.sidebarCollapsed);
        window.openOmnibar(false);
        compare(frame.x, panel.restX);
        compare(frame.width, panel.restWidth);
        verify(frame.y <= panel.restY);
        tryCompare(frame, "y", panel.restY);
        // Over a page it stands where the Start page rests it: centred in the
        // page area, the horizon 50 px below its top as on the website.
        const startPage = findChild(window.contentItem, "startPage");
        const top = frame.mapToItem(startPage, frame.width / 2, 0);
        fuzzyCompare(top.x, startPage.width / 2, 1);
        fuzzyCompare(top.y + panel.horizonBelowTop, startPage.horizonY, 1);
        window.closeOmnibar();
        tryCompare(panel, "visible", false);

        window.openCommandScope();
        compare(frame.x, panel.restX);
        compare(frame.width, panel.restWidth);
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    function test_aPrivateOmnibarListsNoSpacesAndNoHistory() {
        browser.recordVisit("https://private-probe.example/", "Private probe history");
        const probeSpaceId = browser.createSpace("Private probe space");
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const panel = findChild(privateBrowser.contentItem, "omnibar");
        const input = findChild(privateBrowser.contentItem, "omnibarInput");

        privateBrowser.openOmnibar(false);
        input.text = "private probe";
        wait(50);
        compare(omnibarRowsOf(panel, "space").length, 0);
        compare(omnibarRowsOf(panel, "history").length, 0);
        privateBrowser.closeOmnibar();

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        verify(browser.deleteSpace(probeSpaceId, "Private probe space"));
        verify(browser.deleteHistoryOrigin("https://private-probe.example/"));
    }

    // A Private window's own tabs are the only ones its Omnibar lists; the
    // ordinary Spaces' stay in the ordinary window.
    function test_aPrivateOmnibarListsOnlyItsOwnTabs() {
        const opened = openTabsInOtherSpaces();
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateSession = privateBrowser.windowBrowser;
        privateSession.openInput("https://orbit-private.example/one", false);
        const privateTabId = privateSession.activeTabId;
        privateSession.openInput("https://elsewhere-private.example/two", true);
        const panel = findChild(privateBrowser.contentItem, "omnibar");
        const input = findChild(privateBrowser.contentItem, "omnibarInput");

        privateBrowser.openOmnibar(false);
        input.text = "orbit";
        compare(omnibarRowsOf(panel, "tab").map(function (row) {
            return row.argument;
        }), [privateTabId]);
        privateBrowser.closeOmnibar();

        privateSession.closeActiveTab();
        privateSession.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        closeTabsInOtherSpaces(opened);
    }

    // A Private window's Start page drives with the lights off, under the
    // glass, and the window beside it keeps its own lit.
    function test_aPrivateWindowsRoadHasItsLightsOff() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        tryVerify(function () {
            return findChild(privateBrowser.contentItem, "startPage").visible;
        });
        const road = findChild(privateBrowser.contentItem, "nightRoad");
        const scene = findChild(privateBrowser.contentItem, "startPageScene");
        verify(scene.visible);
        verify(road.unlit);
        verify(!road.sunShown);
        compare(road.stars, 0);
        compare(road.centreMarks, 0);
        // Under the glass all the same.
        verify(findChild(scene, "crtGlass").visible);
        verify(!findChild(window.contentItem, "nightRoad").unlit);

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    // A Private window shows the Scene the reader chose, and its sky has its
    // lights out: its planet stays, without the glow on its limb, and there
    // are no stars and no comets, under the glass all the same.
    function test_aPrivateWindowsSkyHasItsLightsOut() {
        window.setStartPageScene("night-sky");
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        tryVerify(function () {
            return findChild(privateBrowser.contentItem, "startPage").visible;
        });
        const sky = findChild(privateBrowser.contentItem, "nightSky");
        const scene = findChild(privateBrowser.contentItem, "startPageScene");
        verify(sky !== null);
        verify(scene.visible);
        verify(sky.unlit);
        compare(sky.stars, 0);
        verify(!sky.cometShown);
        verify(!sky.limbGlows);
        verify(findChild(scene, "crtGlass").visible);
        verify(!findChild(window.contentItem, "nightSky").unlit);

        // A choice made while it is open reaches it.
        window.setStartPageScene("none");
        verify(!scene.visible);

        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    function test_historyIsAFilteredBrowserOwnedSheet() {
        browser.recordVisit("https://history-sheet.example/first", "History sheet first");
        browser.recordVisit("https://other-sheet.example/second", "Other sheet");
        window.requestHistory();

        const surface = findChild(window.contentItem, "historySurface");
        const search = findChild(window.contentItem, "historySearch");
        const list = findChild(window.contentItem, "historyList");
        verify(surface !== null);
        verify(search !== null);
        verify(list !== null);
        tryVerify(function () {
            return surface.visible;
        });
        search.text = "history-sheet.example";
        tryCompare(list, "count", 1);
        compare(String(list.model[0].url), "https://history-sheet.example/first");

        const personalSpaceId = browser.activeSpaceId;
        const workSpaceId = browser.createSpace("History sheet work");
        verify(browser.switchSpace(workSpaceId));
        tryCompare(list, "count", 0);
        verify(browser.switchSpace(personalSpaceId));
        tryCompare(list, "count", 1);
        verify(browser.deleteSpace(workSpaceId, "History sheet work"));

        findChild(window.contentItem, "closeHistoryButton").clicked();
        tryVerify(function () {
            return !surface.visible;
        });
    }

    // Puts one tab away, in a Space of its own. The tab's clock is moved back
    // a week so that no tab another test left behind is old enough to go with
    // it, and everything unstamped is stamped first at the real time. Answers
    // the Space it made and the one it came from, which `endPutAway` returns
    // to.
    function putAwayInASpaceOfItsOwn(name, address) {
        browser.putAwayUnusedTabs();
        const home = browser.activeSpaceId;
        const spaceId = browser.createSpace(name);
        verify(browser.switchSpace(spaceId));
        browser.openInput(address, false);
        const lastWeek = Date.now() - 7 * 24 * 3600 * 1000;
        browser.setNowForTests(lastWeek);
        browser.openInput("https://put-away-reading.example/", true);
        // Its page was on show a moment ago and focuses on the next turn; a
        // reader's tab is never put away that soon after.
        wait(0);
        browser.setNowForTests(lastWeek + 24 * 3600 * 1000);
        browser.putAwayUnusedTabs();
        browser.setNowForTests(0);
        compare(browser.putAwayTabs.length, 1);
        return {
            "home": home,
            "spaceId": spaceId,
            "name": name
        };
    }

    function endPutAway(put) {
        // A tab just reopened focuses its page on the next turn, which has to
        // find the page still there.
        wait(0);
        browser.dismissPutAwayNotice();
        verify(browser.switchSpace(put.home));
        wait(0);
        verify(browser.deleteSpace(put.spaceId, put.name));
    }

    // The first time Omaweb puts tabs away it says so, once, and the notice is
    // the way to the setting that chose when.
    function test_putAwayNoticeSaysSoOnceAndLeadsToTheSetting() {
        // As an installation that has never put a tab away.
        verify(browser.setPreference("put-away-notice-given", ""));
        const put = putAwayInASpaceOfItsOwn("Put away notice", "https://put-away-notice.example/");
        const bar = findChild(window.contentItem, "putAwayNoticeBar");
        verify(bar !== null);
        tryCompare(bar, "open", true);
        compare(bar.message, "Omaweb puts away tabs you have not shown for a while");
        // It never takes the keyboard from the page.
        verify(!bar.activeFocus);

        bar.actionTriggered(0);
        tryCompare(window, "settingsOpen", true);
        const settings = findChild(window.contentItem, "settingsSurface");
        compare(settings.sections[settings.section], "interface");
        tryCompare(findChild(window.contentItem, "putAwayAfter"), "visible", true);
        tryCompare(bar, "open", false);
        verify(!browser.putAwayNotice);
        findChild(window.contentItem, "settingsSection0").Accessible.pressAction();
        findChild(window.contentItem, "closeSettingsButton").clicked();

        // Once for the installation: the next tab put away says nothing.
        browser.openInput("https://put-away-notice-again.example/", true);
        const lastWeek = Date.now() - 7 * 24 * 3600 * 1000;
        browser.setNowForTests(lastWeek);
        browser.openInput("https://put-away-reading-again.example/", true);
        wait(0);
        browser.setNowForTests(lastWeek + 24 * 3600 * 1000);
        browser.putAwayUnusedTabs();
        browser.setNowForTests(0);
        compare(browser.putAwayTabs.length, 2);
        verify(!browser.putAwayNotice);
        verify(!bar.open);
        endPutAway(put);
    }

    // The History sheet lists the Space's put-away tabs at the top, newest
    // first, by title, host and age, and a row opens its tab again.
    function test_historySheetReopensAPutAwayTab() {
        const put = putAwayInASpaceOfItsOwn("Put away history",
                                            "https://put-away-history.example/page");
        browser.dismissPutAwayNotice();
        // Visits under the group, as a Space in use has.
        for (let visit = 0; visit < 12; ++visit)
            browser.recordVisit("https://put-away-visit.example/" + visit, "Visit " + visit);
        window.requestHistory();
        const surface = findChild(window.contentItem, "historySurface");
        tryVerify(function () {
            return surface.visible;
        });
        const group = findChild(window.contentItem, "historyPutAwayGroup");
        verify(group !== null);
        tryCompare(group, "visible", true);
        // At the top of the sheet where it can be seen, not scrolled out of
        // the list above its first visit.
        const historyList = findChild(window.contentItem, "historyList");
        tryVerify(function () {
            const top = group.mapToItem(historyList, 0, 0).y;
            return top >= 0 && top < historyList.height;
        });
        compare(findChild(group, "historyPutAwayHeading").text, "put away");
        const row = findChild(group, "historyPutAwayRow");
        verify(row !== null);
        compare(findChild(row, "putAwayHost").text, "put-away-history.example");
        compare(findChild(row, "putAwayAge").text, "6 days ago");

        // The search narrows them as it narrows the visits.
        const search = findChild(window.contentItem, "historySearch");
        search.text = "nothing-put-away-matches";
        tryCompare(group, "visible", false);
        search.text = "put-away-history";
        tryCompare(group, "visible", true);

        // The search built the rows again.
        const reopen = findChild(findChild(group, "historyPutAwayRow"), "reopenPutAwayButton");
        settleActions(reopen);
        const miss = clickReportingAMiss(reopen, function () {
            return !window.historyOpen;
        });
        compare(miss, "");
        tryCompare(window, "historyOpen", false);
        compare(String(browser.activeUrl), "https://put-away-history.example/page");
        compare(browser.putAwayTabs.length, 0);
        endPutAway(put);
    }

    // The Omnibar finds a put-away tab as it finds History, and choosing it
    // opens the tab again.
    function test_omnibarReopensAPutAwayTab() {
        const put = putAwayInASpaceOfItsOwn("Put away omnibar",
                                            "https://put-away-omnibar.example/");
        browser.dismissPutAwayNotice();
        window.openOmnibar(true);
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        input.text = "put-away-omni";
        function putAwayRow() {
            for (let index = 0; index < panel.rows.length; ++index) {
                if (panel.rows[index].kind === "putaway")
                    return index;
            }
            return -1;
        }
        tryVerify(function () {
            return putAwayRow() >= 0;
        });
        compare(panel.rows[putAwayRow()].url, "https://put-away-omnibar.example/");
        panel.selected = putAwayRow();
        panel.accept();
        tryCompare(panel, "open", false);
        compare(String(browser.activeUrl), "https://put-away-omnibar.example/");
        compare(browser.putAwayTabs.length, 0);
        endPutAway(put);
    }

    // A row in the hand is in use, however long its tab has gone unshown, and
    // it goes once the hand lets it go.
    function test_draggedRowIsNotPutAway() {
        browser.putAwayUnusedTabs();
        const home = browser.activeSpaceId;
        const spaceId = browser.createSpace("Put away drag");
        verify(browser.switchSpace(spaceId));
        browser.openInput("https://put-away-dragged.example/", false);
        const draggedId = browser.activeTabId;
        const lastWeek = Date.now() - 7 * 24 * 3600 * 1000;
        browser.setNowForTests(lastWeek);
        browser.openInput("https://put-away-held-reading.example/", true);
        wait(0);
        browser.setNowForTests(0);

        settleRow(findChild(window.contentItem, "tab-" + draggedId));
        const row = findChild(window.contentItem, "tab-" + draggedId);
        verify(row !== null);
        const grabbed = row.mapToItem(window.contentItem, row.width / 2, row.height / 2);
        mousePress(row, row.width / 2, row.height / 2);
        dragRowBy(grabbed, row.height / 2);
        verify(row.lifted);

        browser.setNowForTests(lastWeek + 24 * 3600 * 1000);
        browser.putAwayUnusedTabs();
        compare(browser.putAwayTabs.length, 0);

        mouseRelease(window.contentItem, grabbed.x, grabbed.y + row.height / 2);
        // Carrying a row is not choosing it.
        verify(browser.activeTabId !== draggedId);
        browser.putAwayUnusedTabs();
        browser.setNowForTests(0);
        compare(browser.putAwayTabs.length, 1);
        compare(String(browser.putAwayTabs[0].url), "https://put-away-dragged.example/");

        browser.dismissPutAwayNotice();
        verify(browser.switchSpace(home));
        wait(0);
        verify(browser.deleteSpace(spaceId, "Put away drag"));
    }

    // Settings offers when unused tabs go, from never to a week, and twelve
    // hours until the reader chooses.
    function test_settingsChoosesWhenUnusedTabsArePutAway() {
        window.requestSettings();
        railSection("interface").Accessible.pressAction();
        const choice = findChild(window.contentItem, "putAwayAfter");
        verify(choice !== null);
        tryCompare(choice, "visible", true);
        compare(choice.value, "43200");
        compare(choice.options.map(function (option) {
            return option.label;
        }), ["Off", "1 hour", "12 hours", "1 day", "1 week"]);
        choice.changed("3600");
        compare(browser.putAwayAfterSeconds, 3600);
        tryCompare(choice, "value", "3600");
        choice.changed("43200");
        compare(browser.putAwayAfterSeconds, 43200);
        findChild(window.contentItem, "settingsSection0").Accessible.pressAction();
        findChild(window.contentItem, "closeSettingsButton").clicked();
    }

    // Show agent activity opens the log as a page of its own, in a new tab,
    // newest first and filtered by Agent and by Space.
    function test_showAgentActivityOpensTheLogInANewTab() {
        const before = browser.activeTabId;
        verify(window.commands.run("agent-activity", -1));
        verify(browser.activeTabId !== before);
        compare(String(browser.activeUrl), "omaweb:agent-activity");

        const surface = findChild(window.contentItem, "agentActivitySurface");
        const list = findChild(window.contentItem, "agentActivityList");
        const agents = findChild(window.contentItem, "agentActivityAgentFilter");
        const spaces = findChild(window.contentItem, "agentActivitySpaceFilter");
        verify(surface !== null);
        tryVerify(function () {
            return surface.visible;
        });
        tryCompare(list, "count", 2);
        compare(list.model[0].agent, "script");
        compare(list.model[1].target, "fill 7, click 9");

        agents.value = "claude";
        tryCompare(list, "count", 1);
        compare(list.model[0].space, "Research");
        spaces.value = "errands-space";
        tryCompare(list, "count", 0);
        agents.value = "";
        tryCompare(list, "count", 1);
        compare(list.model[0].agent, "script");
        spaces.value = "";

        browser.closeActiveTab();
        tryVerify(function () {
            return !surface.visible;
        });
    }

    // The rail names its sections; their order is the page's to change. A test
    // that wants one asks for it by name, so adding a section never sends a
    // test to the page next door.
    function railSection(name) {
        const surface = findChild(window.contentItem, "settingsSurface");
        return findChild(window.contentItem, "settingsSection" + surface.sections.indexOf(name));
    }

    function test_settingsAboutNamesTheVersion() {
        window.requestSettings();
        const aboutSection = railSection("about");
        verify(aboutSection !== null);
        compare(aboutSection.text.toLowerCase(), "about");

        aboutSection.Accessible.pressAction();
        const name = findChild(window.contentItem, "aboutName");
        const version = findChild(window.contentItem, "aboutVersion");
        const links = findChild(window.contentItem, "aboutLinks");
        verify(name !== null);
        verify(version !== null);
        verify(links !== null);
        compare(name.text, "Omaweb");

        // The build carries a version, and the page shows that one rather than
        // a hardcoded string that would rot at the next release.
        verify(Qt.application.version.length > 0);
        compare(version.text, "Version " + Qt.application.version);
        verify(links.text.indexOf("omaweb.app") >= 0);

        // Test functions share one window and run in name order, so hand the
        // page back on the section the later settings tests open it expecting.
        findChild(window.contentItem, "settingsSection0").Accessible.pressAction();
        findChild(window.contentItem, "closeSettingsButton").clicked();
    }

    function test_settingsReceivesSyncLauncher() {
        const settings = findChild(window.contentItem, "settingsSurface");
        verify(settings !== null);
        compare(settings.syncLauncher, syncLauncherContext);
    }

    function test_settingsSyncCopyShowsNotice() {
        const settings = findChild(window.contentItem, "settingsSurface");
        const notice = findChild(window.contentItem, "pageNotice");
        settings.syncCodeCopied("Forge code copied");
        tryCompare(notice, "message", "Forge code copied");
        compare(notice.detail, "Paste it into the authorization page");
        notice.dismiss();
    }

    function test_settingsSyncConsentOpensAForegroundTab() {
        const settings = findChild(window.contentItem, "settingsSurface");
        const target = "https://github.example/new?name=omaweb-sync";

        settings.syncConsentRequested(target);

        tryVerify(function () {
            return browser.activeUrl.toString() === target;
        });
    }

    function test_settingsOwnSearchAndBrowsingDataControls() {
        window.requestSettings();
        const searchSection = railSection("search");
        const dataSection = railSection("privacy");
        verify(searchSection !== null);
        verify(dataSection !== null);
        compare(searchSection.text.toLowerCase(), "search");
        compare(dataSection.text.toLowerCase(), "privacy");

        searchSection.Accessible.pressAction();
        const engines = findChild(window.contentItem, "searchEngineList");
        verify(engines !== null);
        compare(engines.count, 7);
        verify(String(engines.model[0].name).indexOf("DuckDuckGo") >= 0);
        const providerPicker = findChild(window.contentItem, "searchProviderPreset");
        const addProvider = findChild(window.contentItem, "addSearchProviderButton");
        verify(providerPicker !== null);
        verify(addProvider !== null);
        verify(providerPicker.count >= 5);

        // Privacy is one row and one button. The five category switches and the
        // scope switch that used to stand here were arguments to that button,
        // and a switch promises the browser changed when it was flipped (#84).
        dataSection.Accessible.pressAction();
        verify(findChild(window.contentItem, "clearBrowsingDataRow") !== null);
        verify(findChild(window.contentItem, "clearBrowsingDataButton") !== null);
        verify(findChild(window.contentItem, "clearCookies") === null);
        verify(findChild(window.contentItem, "clearStorage") === null);
        verify(findChild(window.contentItem, "clearCache") === null);
        verify(findChild(window.contentItem, "clearPermissions") === null);
        verify(findChild(window.contentItem, "clearHistory") === null);
        verify(findChild(window.contentItem, "clearEverySpace") === null);

        findChild(window.contentItem, "closeSettingsButton").clicked();
    }

    // The dialog's own contract — its tab order, its keys, what it refuses —
    // is asserted in `tst_clearbrowsingdata.qml`, against a dialog standing on
    // its own in that test case's window. It cannot be asserted here: this
    // window is shared by a hundred tests, `activeFocus` is false throughout a
    // window the platform does not call active, and which window that is gets
    // decided by whichever test opened one last. What is left here is what
    // needs the browser around it.
    //
    // The dialog is reset first either way, because it disables the page under
    // it: a test inheriting an open one from a failure above would fail for
    // that reason rather than its own.
    function readyForBrowsingData() {
        findChild(window.contentItem, "settingsSurface").clearDataOpen = false;
        window.requestSettings();
        railSection("privacy").Accessible.pressAction();
    }

    function leaveBrowsingData() {
        findChild(window.contentItem, "settingsSurface").clearDataOpen = false;
        findChild(window.contentItem, "closeSettingsButton").clicked();
    }

    // The contract the switches could not state: the arguments are composed in
    // the dialog, and only confirming it takes anything.
    function test_browsingDataIsTakenOnlyByConfirmingItsDialog() {
        // The engine profile is what counts a clear, so there has to be one.
        openPage("https://browsing-data.example/page");
        readyForBrowsingData();
        const dialog = findChild(window.contentItem, "clearBrowsingDataDialog");
        const openButton = findChild(window.contentItem, "clearBrowsingDataButton");
        verify(dialog !== null);
        verify(openButton !== null);
        verify(!dialog.visible);

        const cleared = window.spaceProfileHost.browsingDataClearCount;
        openButton.clicked();
        tryVerify(function () {
            return dialog.visible;
        });
        // Opening composes; it does not act.
        compare(window.spaceProfileHost.browsingDataClearCount, cleared);

        // A dialog is modal, so the page under it takes neither a click nor a
        // Tab while it stands. Qt has no tab fence a QML item can raise, so
        // taking the page out of the ring is what keeps the ring in the dialog.
        compare(findChild(window.contentItem, "settingsSection0").enabled, false);
        compare(findChild(window.contentItem, "closeSettingsButton").enabled, false);

        // Neither does dismissing it.
        dialog.dismissed();
        tryVerify(function () {
            return !dialog.visible;
        });
        compare(window.spaceProfileHost.browsingDataClearCount, cleared);
        compare(findChild(window.contentItem, "settingsSection0").enabled, true);

        openButton.clicked();
        tryVerify(function () {
            return dialog.visible;
        });
        findChild(window.contentItem, "clearBrowsingDataConfirm").clicked();
        tryVerify(function () {
            return window.spaceProfileHost.browsingDataClearCount === cleared + 1;
        });
        // Confirming is the whole act: nothing asks a second time.
        verify(!dialog.visible);

        leaveBrowsingData();
    }

    // The kit's section label leans low on purpose: in a scrolling pane it
    // belongs to the rows that follow it. A dialog head is a bar with one line
    // in it, where that lean only drops the name below the hint beside it. Both
    // dialogs share this head, so a lean here is a lean in every question the
    // browser asks.
    function test_theDialogHeadCentresItsNameAgainstItsHint() {
        readyForBrowsingData();
        findChild(window.contentItem, "clearBrowsingDataButton").clicked();
        const dialog = findChild(window.contentItem, "clearBrowsingDataDialog");
        tryVerify(function () {
            return dialog.visible;
        });

        const name = findChild(dialog, "dialogPanelLabel");
        const hint = findChild(dialog, "dialogPanelCancelHint");
        verify(name !== null);
        verify(hint !== null);
        compare(name.text, "clear browsing data");

        // The centre is the glyphs' own rather than a padded box's, which is
        // the whole of the fix: `verticalCenter` centres what it is given.
        compare(name.topPadding, name.bottomPadding);
        const nameCentre = name.mapToItem(dialog, 0, name.height / 2).y;
        const hintCentre = hint.mapToItem(dialog, 0, hint.height / 2).y;
        verify(Math.abs(nameCentre - hintCentre) <= 1);

        leaveBrowsingData();
    }

    // A selection is remembered because a reader who took the same things last
    // month means the same things now. Scope is not: it is the one argument
    // with no prior art to borrow, so it is asked for every time.
    function test_theSelectionSurvivesARestartAndTheScopeDoesNot() {
        readyForBrowsingData();
        findChild(window.contentItem, "clearBrowsingDataButton").clicked();
        const dialog = findChild(window.contentItem, "clearBrowsingDataDialog");
        tryVerify(function () {
            return dialog.visible;
        });

        findChild(window.contentItem, "clearCategory-cache").clicked();
        findChild(window.contentItem, "clearTimeRange").changed("0");
        dialog.everySpace = true;
        compare(browser.preference("clear-data-categories", ""),
                "cookies,storage,permissions,history,forms");
        compare(browser.preference("clear-data-range", ""), "0");

        dialog.dismissed();
        tryVerify(function () {
            return !dialog.visible;
        });
        leaveBrowsingData();

        // A second launch of the page against the same preferences.
        const restarted = restartedSettingsComponent.createObject(testCase);
        verify(restarted !== null);
        compare(restarted.clearCategories.join(","), "cookies,storage,permissions,history,forms");
        compare(restarted.clearRange, "0");
        const restartedDialog = findChild(restarted, "clearBrowsingDataDialog");
        compare(restartedDialog.everySpace, false);
        restarted.destroy();

        // Leave the store as the rest of the suite expects to find it.
        findChild(window.contentItem, "settingsSurface").toggleClearCategory("cache");
        findChild(window.contentItem, "settingsSurface").chooseClearRange("86400000");
    }

    function test_theSidebarOpensAndClosesSettings() {
        const settingsButton = findChild(window.contentItem, "settingsButton");
        const settingsSurface = findChild(window.contentItem, "settingsSurface");
        verify(settingsButton !== null);
        verify(settingsSurface !== null);
        tryVerify(function () {
            return !settingsSurface.visible;
        });
        // A page to cover, so the backdrop below has something to blur.
        openPage("https://under-settings.example");

        settingsButton.clicked();
        tryVerify(function () {
            return settingsSurface.visible;
        });

        // Settings is a place over the page, not instead of it: the same
        // translucency the sidebar has, over the page it covers, blurred.
        const settingsBackdrop = findChild(window.contentItem, "settingsBackdrop");
        verify(settingsBackdrop !== null);
        compare(String(settingsBackdrop.tint), String(window.colors.sheet));
        tryVerify(function () {
            return settingsBackdrop.sampling;
        });

        const closeButton = findChild(window.contentItem, "closeSettingsButton");
        verify(closeButton !== null);
        closeButton.clicked();
        tryVerify(function () {
            return !settingsSurface.visible;
        });
    }

    function test_authenticatedAvatarReplacesSettingsAndOpensIt() {
        const outline = authenticatedOutlineComponent.createObject(window.contentItem);
        verify(outline !== null);
        avatarSettingsSpy.target = outline;
        avatarSettingsSpy.clear();
        try {
            const avatar = findChild(outline, "syncAvatarButton");
            const avatarClip = findChild(outline, "syncAvatarClip");
            const avatarMonogram = findChild(outline, "syncAvatarMonogram");
            const avatarEffect = findChild(outline, "syncAvatarEffect");
            const activityDot = findChild(outline, "syncActivityDot");
            const settings = findChild(outline, "settingsButton");
            verify(avatar !== null);
            verify(avatarClip !== null);
            verify(avatarMonogram !== null);
            verify(avatarEffect !== null);
            verify(activityDot !== null);
            verify(settings !== null);
            verify(avatar.visible);
            verify(!settings.visible);
            compare(avatarClip.width, avatarClip.height);
            compare(avatarClip.radius, avatarClip.width / 2);
            compare(avatarClip.opacity, 1);
            compare(avatarMonogram.text, "O");
            verify(avatarMonogram.visible);
            compare(avatarEffect.saturation, 0);
            compare(String(activityDot.color), String(outline.colors.accent));

            avatar.clicked();
            compare(avatarSettingsSpy.count, 1);

            outline.sync.enabled = false;
            verify(avatar.visible);
            verify(!settings.visible);
            compare(avatarClip.opacity, 1);
            compare(avatarEffect.saturation, -1);
            compare(String(activityDot.color), String(outline.colors.mutedText));
        } finally {
            avatarSettingsSpy.target = null;
            outline.destroy();
            window.requestActivate();
        }
    }

    // The rail is as wide as the longest section name it draws, measured in the
    // bold face the current section takes. A pixel count was right at one theme
    // font size and clipped "Content Blocking" at the next, and a rail that
    // widened when the selection moved would shift the pane beside it.
    function test_settingsRailIsAsWideAsTheNamesItDraws() {
        const settingsSurface = findChild(window.contentItem, "settingsSurface");
        verify(settingsSurface !== null);
        window.requestSettings();
        tryVerify(function () {
            return settingsSurface.visible;
        });

        let longest = 0;
        for (let index = 0; index < settingsSurface.sections.length; ++index) {
            const entry = findChild(window.contentItem, "settingsSection" + index);
            verify(entry !== null);
            verify(entry.implicitWidth <= settingsSurface.railWidth);
            if (entry.implicitWidth > longest) {
                longest = entry.implicitWidth;
                settingsSurface.section = index;
            }
        }
        verify(longest > 0);

        // The longest name is now the current one, so it is drawn bold. The
        // rail was measured in that face, so it still fits — in a proportional
        // family bold is wider than the regular the loop above measured, and in
        // a monospace one it is the same. Either way the pane does not shift.
        const current = findChild(window.contentItem, "settingsSection" + settingsSurface.section);
        verify(current.font.bold);
        verify(current.implicitWidth >= longest);
        verify(current.implicitWidth <= settingsSurface.railWidth);

        settingsSurface.section = 0;
        window.settingsOpen = false;
        tryVerify(function () {
            return !settingsSurface.visible;
        });
    }

    function test_theKeymapOpensSettings() {
        const settingsSurface = findChild(window.contentItem, "settingsSurface");
        verify(settingsSurface !== null);
        tryVerify(function () {
            return !settingsSurface.visible;
        });
        compare(keyboardNavigation.browserBindings["Primary+,"], "settings");

        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        keyClick(Qt.Key_Comma, Qt.ControlModifier);
        tryVerify(function () {
            return settingsSurface.visible;
        });

        findChild(window.contentItem, "closeSettingsButton").clicked();
        tryVerify(function () {
            return !settingsSurface.visible;
        });
    }

    function test_settingsOwnsTheKeyboardUntilItCloses() {
        browser.openInput("about:blank", true);
        const settings = findChild(window.contentItem, "settingsSurface");
        const activeTab = browser.activeTabId;
        verify(settings !== null);

        try {
            window.requestSettings();
            tryVerify(function () {
                return settings.visible && settings.activeFocus;
            });

            keyClick(Qt.Key_J, Qt.ShiftModifier);
            keyClick(Qt.Key_Escape);

            tryVerify(function () {
                return !settings.visible;
            });
            compare(browser.activeTabId, activeTab);
        } finally {
            window.settingsOpen = false;
            browser.closeTab(activeTab);
        }
    }

    function test_settingsRegainsTheKeyboardAfterAPointerChangesTab() {
        const targetEngine = openPage("https://settings-focus.example/one");
        const targetTab = browser.activeTabId;
        browser.openInput("https://settings-focus.example/two", true);
        const addedTab = browser.activeTabId;
        const engineHost = findChild(window.contentItem, "engineLoader");
        const targetPointer = findChild(window.contentItem, "tabPointer-" + targetTab);
        const settings = findChild(window.contentItem, "settingsSurface");
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item !== targetEngine;
        });
        verify(targetPointer !== null);
        verify(settings !== null);
        settleRow(targetPointer.parent);

        try {
            window.requestSettings();
            tryVerify(function () {
                return settings.visible && settings.activeFocus;
            });

            mouseClick(targetPointer, targetPointer.width / 2, targetPointer.height / 2);

            compare(browser.activeTabId, targetTab);
            wait(50);
            verify(settings.activeFocus);
            keyClick(Qt.Key_Escape);
            tryVerify(function () {
                return !settings.visible;
            });
        } finally {
            window.settingsOpen = false;
            browser.closeTab(addedTab);
        }
    }

    function test_settingsExposeNetworkAndDownloadPolicy() {
        const settingsButton = findChild(window.contentItem, "settingsButton");
        const engineSuggestionsSwitch = findChild(window.contentItem, "engineSuggestions");
        const automaticRequestsStatus = findChild(window.contentItem, "automaticRequestsStatus");
        const keyboardNavigationEnabled = findChild(window.contentItem,
                                                    "keyboardNavigationEnabled");
        verify(settingsButton !== null);
        verify(engineSuggestionsSwitch !== null);
        verify(automaticRequestsStatus !== null);
        verify(keyboardNavigationEnabled !== null);
        // Off until the reader turns it on, and bound to the browser's one
        // setting.
        compare(engineSuggestionsSwitch.checked, false);
        compare(engineSuggestionsSwitch.checked, engineSuggestions.enabled);
        verify(automaticRequestsStatus.text.indexOf("automatic network requests") >= 0);
        // A keyboard-driven browser ships with its keymap live.
        compare(keyboardNavigationEnabled.checked, true);
        // The kit's Toggle is stateless about the value: it reports the click
        // and the settings page flips the setting, which flows back through
        // the `checked` binding. Drive it from the keyboard, since that is how
        // this browser is meant to be reached, and on both of the kit's
        // activation keys.
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        keyboardNavigationEnabled.forceActiveFocus();
        verify(keyboardNavigationEnabled.activeFocus);
        keyClick(Qt.Key_Space);
        compare(keyboardNavigation.enabled, false);
        compare(keyboardNavigationEnabled.checked, false);
        keyClick(Qt.Key_Return);
        compare(keyboardNavigation.enabled, true);
        compare(keyboardNavigationEnabled.checked, true);
    }

    function activateWindow() {
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    // A Space at rest, switched to and waiting for its Start page's field.
    function enterRestingSpace(name) {
        const spaceId = browser.createSpace(name);
        verify(browser.switchSpace(spaceId));
        tryVerify(function () {
            return browser.atRest && window.pagelessViewport;
        });
        activateWindow();
        const input = findChild(window.contentItem, "omnibarInput");
        tryVerify(function () {
            return input.activeFocus;
        });
        return spaceId;
    }

    // Back to the Space the test started in, and the notice that switch shows
    // gone again, so the next test starts where this one did.
    function leaveSpace(homeSpaceId, spaceId, name) {
        verify(browser.switchSpace(homeSpaceId));
        verify(browser.deleteSpace(spaceId, name));
        const notice = findChild(window.contentItem, "spaceNotice");
        tryVerify(function () {
            return !notice.visible;
        }, 4000);
    }

    Component {
        id: otherWindowComponent

        Window {
            width: 120
            height: 80
        }
    }

    // WCAG's contrast between two opaque colours.
    function contrastRatio(one, other) {
        const luminance = function (colour) {
            const c = Qt.color(colour);
            const channel = function (value) {
                return value <= 0.04045 ? value / 12.92 : Math.pow((value + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
        };
        const a = luminance(one);
        const b = luminance(other);
        return (Math.max(a, b) + 0.05) / (Math.min(a, b) + 0.05);
    }

    // A see-through colour laid over an opaque one.
    function over(colour, ground) {
        const c = Qt.color(colour);
        const g = Qt.color(ground);
        return Qt.rgba(c.r * c.a + g.r * (1 - c.a), c.g * c.a + g.g * (1 - c.a), c.b * c.a + g.b * (1 - c.a),
                       1);

    }

    // Where the road's sun is, in an item's coordinates.
    function sunIn(item) {
        const scene = findChild(window.contentItem, "startPageScene");
        return scene.mapToItem(item, scene.light.centre.x, scene.light.centre.y);
    }

    // At rest on the Start page the Omnibar is glass over the road, as the
    // website's is: the road blurred behind a see-through plate, and its rim
    // lit by the road's sun from where the sun stands. Over a page there is no
    // sun: the glass stays, and the rim is the plain edge.
    function test_theOmnibarIsGlassLitByTheSunOverTheRoad() {
        const scene = findChild(window.contentItem, "startPageScene");
        const panel = findChild(window.contentItem, "omnibar");
        const rim = findChild(window.contentItem, "omnibarRim");
        const glass = findChild(window.contentItem, "omnibarGlass");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Sunlit rim");
        tryVerify(function () {
            return panel.shownResting && scene.light !== null;
        });
        tryCompare(panel, "arrival", 1);
        tryCompare(findChild(window.contentItem, "sidebar"), "arriving", false);

        verify(rim.visible);
        compare(rim.light, scene.light);
        const sun = sunIn(rim.parent);
        fuzzyCompare(rim.sun.x, sun.x, 1);
        fuzzyCompare(rim.sun.y, sun.y, 1);
        compare(glass.source, scene);

        openPage("https://glass-over-a-page.example/");
        settleMotion();
        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        verify(!rim.visible);
        verify(glass.source !== null);
        verify(glass.source !== scene);
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
        leaveSpace(homeSpaceId, restingSpaceId, "Sunlit rim");
    }

    // The Omnibar's text stays readable whatever the glass lets through: over
    // the road and over a page, light or dark, its plate over black or white
    // holds the text at the theme's contrast floor for text, 4.5, in the
    // theme on show and in a light one.
    function test_theOmnibarTextKeepsItsContrastOnTheGlass_data() {
        return [
                    {
                        tag: "theme"
                    },
                    {
                        tag: "light",
                        text: "#1c1b22",
                        overlay: Qt.rgba(0.957, 0.953, 0.973, 0.96)
                    }
                ];
    }

    function test_theOmnibarTextKeepsItsContrastOnTheGlass(data) {
        const panel = findChild(window.contentItem, "omnibar");
        const plate = findChild(window.contentItem, "omnibarGlass");
        const input = findChild(window.contentItem, "omnibarInput");
        if (data.text) {
            const changed = Object.assign({}, window.colors);
            changed.text = data.text;
            // What ThemeController derives from the text for the field.
            changed.fieldText = data.text;
            changed.overlay = data.overlay;
            window.colors = changed;
        }
        const check = function (where) {
            verify(plate.visible, where);
            for (const ground of ["black", "white"]) {
                const ratio = contrastRatio(input.color, over(plate.tint, ground));
                verify(ratio >= 4.5, where + " over " + ground + ": " + ratio.toFixed(2));
            }
        };

        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Readable rim");
        tryVerify(function () {
            return panel.shownResting;
        });
        check("over the road");

        openPage("https://readable-glass.example/");
        settleMotion();
        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        check("over a page");
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
        leaveSpace(homeSpaceId, restingSpaceId, "Readable rim");
        window.colors = Qt.binding(function () {
            return theme.palette;
        });
    }

    // The Start page's field is the website's dash, measured from
    // website/styles.css: a panel `min(viewport - 32px, 720px)` wide with its
    // top 50 px above the horizon, a 50 px field with 16 px of padding, the
    // mark drawn 32 px wide in the viewBox's 38.2 by 18.8, 12 px before the
    // 17 px text.
    function test_theFieldIsTheWebsitesDash() {
        const startPage = findChild(window.contentItem, "startPage");
        const frame = findChild(window.contentItem, "omnibarFrame");
        const field = findChild(window.contentItem, "omnibarField");
        const prompt = findChild(window.contentItem, "omnibarPrompt");
        const mark = findChild(window.contentItem, "omnibarMark");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting dash");
        tryCompare(findChild(window.contentItem, "omnibar"), "arrival", 1);
        tryCompare(findChild(window.contentItem, "sidebar"), "arriving", false);

        compare(frame.width, Math.min(startPage.width - 32, 720));
        const top = frame.mapToItem(startPage, frame.width / 2, 0);
        fuzzyCompare(top.x, startPage.width / 2, 1);
        fuzzyCompare(startPage.horizonY - top.y, 50, 0.5);
        compare(field.height, 50);
        compare(prompt.x, 16);
        compare(prompt.width, 32);
        fuzzyCompare(mark.width * mark.scale, 31.7, 0.1);
        fuzzyCompare(mark.height * mark.scale, 15.4, 0.1);
        compare(input.x, prompt.x + prompt.width + 12);
        compare(input.font.pixelSize, 17);

        leaveSpace(homeSpaceId, restingSpaceId, "Resting dash");
    }

    // The glass over the road is a heavy one, so the results read cleanly
    // over it: the road blurred well past a sliver and under a tint of the
    // overlay that is nearly opaque, never less than the 0.8 the text's
    // contrast is floored against. Both are the Omnibar's own settings, so
    // they can be tuned in one place.
    function test_theGlassOverTheRoadIsBlurredAndNearlyOpaque() {
        const panel = findChild(window.contentItem, "omnibar");
        const glass = findChild(window.contentItem, "omnibarGlass");
        const scene = findChild(window.contentItem, "startPageScene");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Heavy glass");
        tryVerify(function () {
            return panel.shownResting && scene.light !== null;
        });
        tryCompare(panel, "arrival", 1);

        verify(glass.overRoad);
        compare(glass.blur, panel.glassBlurOverRoad);
        verify(panel.glassBlurOverRoad >= 32);
        verify(panel.glassTintOverRoad >= 0.8);
        fuzzyCompare(glass.tint.a, Math.min(glass.overlay.a, panel.glassTintOverRoad), 0.01);
        verify(glass.tint.a >= 0.9);
        compare(glass.source, scene);

        leaveSpace(homeSpaceId, restingSpaceId, "Heavy glass");
    }

    // The sun's light on the field's edge, the rim and the bloom either side
    // of it, is drawn at one strength, the Omnibar's `rimStrength`, so it can
    // be tuned in one place. It is half what the website draws.
    function test_theRimLightIsDrawnAtOneToneDownStrength() {
        const panel = findChild(window.contentItem, "omnibar");
        const rim = findChild(window.contentItem, "omnibarRim");
        const bloom = findChild(window.contentItem, "omnibarInnerBloom");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Rim strength");
        tryCompare(panel, "arrival", 1);
        tryVerify(function () {
            return rim.light !== null;
        });

        compare(panel.rimStrength, 0.5);
        compare(rim.strength, panel.rimStrength);
        compare(bloom.strength, panel.rimStrength);
        fuzzyCompare(rim.opacity, panel.opacity * panel.rimStrength, 0.001);
        compare(bloom.opacity, panel.rimStrength);

        leaveSpace(homeSpaceId, restingSpaceId, "Rim strength");
    }

    // The text the reader types and the caret are drawn plain: in the
    // theme's field text, which clears 4.5:1 on the glass, with no layer
    // effect, so no glow, blur or shadow; and the bloom of the rim light on
    // the field's edge stops short of the glyphs.
    function test_theTypedTextAndCaretHaveNoGlow() {
        const input = findChild(window.contentItem, "omnibarInput");
        const mark = findChild(window.contentItem, "omnibarMark");
        const field = findChild(window.contentItem, "omnibarField");
        const bloom = findChild(window.contentItem, "omnibarInnerBloom");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting plain");
        tryCompare(findChild(window.contentItem, "omnibar"), "arrival", 1);
        tryVerify(function () {
            return bloom.light !== null;
        });

        compare(String(input.color), String(window.colors.fieldText));
        verify(!input.layer.enabled);
        verify(!mark.layer.enabled);
        const caret = findChild(input, "omnibarCaret");
        verify(caret === null || !caret.layer.enabled);
        // The bloom reaches no further into the field than the glyphs'
        // margin above and below the 17 px text.
        verify(bloom.reach <= (field.height - input.font.pixelSize) / 2,
               "the bloom reaches the text");

        leaveSpace(homeSpaceId, restingSpaceId, "Resting plain");
    }

    // The field asks as the website's dash does, "Where to?", wherever the
    // Omnibar opens, while a mode keeps its own prompt. What a screen reader
    // speaks names what the field takes, never only the prompt.
    function test_theFieldAsksWhereTo() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        openPage("https://where-to.example/");
        activateWindow();

        window.openOmnibar(false);
        compare(input.placeholderText, "Where to?");
        compare(input.Accessible.name, "Address, search, tabs and Spaces");
        window.closeOmnibar();
        tryCompare(panel, "visible", false);

        window.openOmnibar(true);
        compare(input.placeholderText, "Where to? \u00b7 opens in a new tab");
        compare(input.Accessible.name, "Address, search, tabs and Spaces");
        window.closeOmnibar();
        tryCompare(panel, "visible", false);

        window.openCommandScope();
        compare(input.placeholderText, "search every action");
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // Every item under `item` named `name`, delegates and content items
    // included.
    function childrenNamed(item, name) {
        let found = [];
        const below = item.contentItem ? [item.contentItem].concat(Array.from(item.children)) :
                                         Array.from(item.children);
        for (const child of below) {
            if (child.objectName === name)
                found.push(child);
            found = found.concat(childrenNamed(child, name));
        }
        return found;
    }

    // Under the results the Omnibar names the keys that work its list, as the
    // website's dash does: `select` for the arrows and `go` for Return as key
    // caps in 11 px dim text over a rule, the keys read from the key map, and
    // `run` in place of `go` in command scope. Nothing is shown with no
    // results.
    function test_theOmnibarNamesItsKeysUnderTheResults() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const frame = findChild(window.contentItem, "omnibarFrame");
        const hints = findChild(window.contentItem, "omnibarHints");
        browser.recordVisit("https://hint-row.example/", "Hint row");
        openPage("https://hint-row-page.example/");
        activateWindow();

        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        verify(!hints.visible);
        input.text = "hint row";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        verify(hints.visible);
        compare(hints.keys.select.join(), panel.keymap.omnibarKeys.select.join());
        compare(hints.keys.go.join(), panel.keymap.omnibarKeys.go.join());
        const words = childrenNamed(hints, "omnibarHintWord").map(function (word) {
            return word.text;
        });
        compare(words.join(), "select,go");
        verify(findChild(hints, "omnibarShortcutsHint") !== null);
        const capsOf = function (group) {
            return childrenNamed(group, "keycap").filter(function (cap) {
                return cap.text.length > 0;
            }).map(function (cap) {
                return cap.text;
            });
        };
        compare(capsOf(hints).join(), "Up,Down,Return,?");
        const arrow = childrenNamed(hints, "keycap").filter(function (cap) {
            return cap.visible && cap.text === "Up";
        })[0];
        compare(findChild(arrow, "keycapLabel").text, "arrow_upward");
        compare(findChild(arrow, "keycapLabel").font.family, panel.iconFontFamily);
        const word = childrenNamed(hints, "omnibarHintWord")[0];
        compare(word.font.pixelSize, Style.font.bodySmall);
        compare(String(word.color), String(window.colors.mutedText));
        compare(findChild(hints, "omnibarHintRule").height, 1);
        compare(frame.height, panel.restHeight);
        verify(hints.y >= findChild(window.contentItem, "omnibarRowList").y);
        window.closeOmnibar();
        tryCompare(panel, "visible", false);

        window.openCommandScope();
        input.text = "reopen";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        compare(childrenNamed(hints, "omnibarHintWord").map(function (word) {
            return word.text;
        }).join(), "select,run,back");
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // The selected result is the website's selected row: the accent at 14%
    // over the panel, so the glass still shows through it, with the accent bar
    // at its left edge. A row that is not selected has no tint.
    function test_theSelectedRowIsATranslucentAccentTint() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const list = findChild(window.contentItem, "omnibarRowList");
        browser.recordVisit("https://selected-tint.example/", "Selected tint");
        openPage("https://selected-tint-page.example/");
        activateWindow();
        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        input.text = "selected tint";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        keyClick(Qt.Key_Down);
        tryVerify(function () {
            return panel.selected >= 0;
        });
        const row = omnibarRowItem(list, panel.selected);
        const tint = findChild(row, "omnibarRowTint");
        const accent = Qt.color(String(window.colors.accent));
        fuzzyCompare(tint.color.r, accent.r, 0.01);
        fuzzyCompare(tint.color.g, accent.g, 0.01);
        fuzzyCompare(tint.color.b, accent.b, 0.01);
        fuzzyCompare(tint.color.a, 0.14, 0.01);
        compare(String(findChild(row, "omnibarRowBar").color), String(accent));
        compare(findChild(row, "omnibarRowBar").width, 2);

        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // The keys at a command row's right edge are key caps, one to a key, the
    // bare binding before the chord with a quiet gap between bindings rather
    // than a dot, and they are right-aligned so they line up down the list.
    // The title elides before the caps clip.
    function test_aCommandRowsKeysAreKeyCapsRightAligned() {
        const panel = findChild(window.contentItem, "omnibar");
        const list = findChild(window.contentItem, "omnibarRowList");
        openPage("https://command-caps-page.example/");
        activateWindow();
        window.openCommandScope();
        tryCompare(panel, "arrival", 1);
        tryVerify(function () {
            return panel.rows.length > 5;
        });
        let two = -1;
        let one = -1;
        for (let i = 0; i < panel.rows.length; ++i) {
            const row = panel.rows[i];
            if (row.kind !== "command" || !row.keys)
                continue;
            if (two < 0 && row.keys.indexOf("\u00b7") >= 0)
                two = i;
            if (one < 0 && row.keys.indexOf("\u00b7") < 0)
                one = i;
        }
        verify(two >= 0 && one >= 0, "the list has a command with two bindings and one with one");

        const capsOf = function (index) {
            const row = omnibarRowItem(list, index);
            const keys = findChild(row, "omnibarRowKeys");
            verify(keys !== null, "no keys on row " + index);
            return {
                "row": row,
                "keys": keys,
                "caps": childrenNamed(keys, "keycap").filter(function (cap) {
                    return cap.visible && cap.text.length > 0;
                })
            };
        };
        const withTwo = capsOf(two);
        // No dot anywhere, and one cap for each key of each binding, the
        // shorter binding first.
        const gaps = childrenNamed(withTwo.keys, "keycapSeparator").filter(function (gap) {
            return gap.visible;
        });
        compare(gaps.length, 1);
        compare(gaps[0].text, "");
        const bindings = panel.rows[two].keys.split(/\s+\u00b7\s+/);
        const expected = bindings.map(function (binding) {
            return binding.split(/\+(?=.)/);
        }).sort(function (a, b) {
            return a.length - b.length;
        });
        compare(withTwo.caps.map(function (cap) {
            return cap.text;
        }).join(), [].concat.apply([], expected).join());
        // Caps of a chord are 4 px apart, bindings further apart than that.
        const edge = function (cap) {
            return cap.mapToItem(withTwo.keys, cap.width, 0).x;
        };
        const start = function (cap) {
            return cap.mapToItem(withTwo.keys, 0, 0).x;
        };
        const chordFrom = expected[0].length;
        if (withTwo.caps.length > chordFrom + 1)
            compare(start(withTwo.caps[chordFrom + 1]) - edge(withTwo.caps[chordFrom]), 4);
        verify(start(withTwo.caps[chordFrom]) - edge(withTwo.caps[chordFrom - 1]) > 4);

        // The last cap of every row stands at the same distance from the
        // row's edge, so the caps line up down the list.
        const withOne = capsOf(one);
        const rightGap = function (found) {
            const last = found.caps[found.caps.length - 1];
            return found.row.width - last.mapToItem(found.row, last.width, 0).x;
        };
        compare(rightGap(withTwo), rightGap(withOne));
        compare(rightGap(withOne), 14);

        // The title gives way before the caps do.
        const title = findChild(withTwo.row, "omnibarRowTitle");
        verify(title.mapToItem(withTwo.row, title.width, 0).x <= withTwo.keys.mapToItem(withTwo.row,
                                                                                        0, 0).x);

        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // A row that is not a command ends in a muted word for what Return does and
    // an arrow in a key cap, as Arc's command bar does. On the selected row the
    // word turns to the text colour and the cap fills with the accent, its
    // arrow in the panel's ground colour. A command row has no arrow.
    function test_aRowEndsInALabelAndAnArrowCap() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const list = findChild(window.contentItem, "omnibarRowList");
        browser.recordVisit("https://arrow-cap.example/", "Arrow cap");
        openPage("https://arrow-cap-page.example/");
        activateWindow();
        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        input.text = "arrow cap";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        const first = omnibarRowItem(list, 0);
        const label = findChild(first, "omnibarRowAction");
        const cap = findChild(first, "omnibarRowGo");
        verify(cap !== null && cap.visible);
        compare(cap.text, "Right");
        compare(findChild(cap, "keycapLabel").text, "arrow_forward");
        compare(label.text, "Open");
        // At the row's own size, as the muted host is.
        compare(label.font.pixelSize, Style.font.body);
        compare(label.font.pixelSize, findChild(first, "omnibarRowHost").font.pixelSize);
        verify(label.text.indexOf("\u2192") < 0);
        compare(String(label.color), String(window.colors.mutedText));
        verify(!cap.accented);

        panel.selected = 0;
        compare(String(label.color), String(window.colors.text));
        verify(cap.accented);
        const accent = Qt.color(String(window.colors.accent));
        compare(String(cap.color), String(accent));
        compare(String(findChild(cap, "keycapLabel").color), String(window.colors.overlayOpaque));
        // Right-aligned like every other cap on a row.
        compare(first.width - cap.mapToItem(first, cap.width, 0).x, 14);

        window.closeOmnibar();
        tryCompare(panel, "visible", false);
        window.openCommandScope();
        tryCompare(panel, "arrival", 1);
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        verify(!findChild(omnibarRowItem(list, 0), "omnibarRowGo").visible);
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // The label at the field's right end names where Return goes, in title case
    // and muted at the size of the rows' own labels, with the arrow kept. It is
    // not the spaced capitals of a section label.
    function test_theFieldsRightLabelIsTitleCaseAndMuted() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        openPage("https://mode-label-page.example/");
        activateWindow();
        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        const mode = findChild(window.contentItem, "omnibarMode");
        compare(mode.text, "This Tab");
        compare(mode.font.capitalization, Font.MixedCase);
        compare(String(mode.color), String(window.colors.mutedText));
        compare(mode.font.pixelSize, Style.font.body);
        verify(findChild(window.contentItem, "omnibarGo").visible);
        window.closeOmnibar();
        tryCompare(panel, "visible", false);

        window.openCommandScope();
        tryCompare(panel, "arrival", 1);
        compare(mode.text, "Command");
        window.closeOmnibar();
        tryCompare(panel, "visible", false);

        window.openOmnibar(true);
        tryCompare(panel, "arrival", 1);
        tryVerify(function () {
            return panel.newTabIntent;
        });
        compare(mode.text, "New Tab");
        window.dismissStartPage();
        tryVerify(function () {
            return !window.startPageSummoned;
        });
    }

    // A command row's symbol is drawn in the accent: full on the selected row,
    // softer on the others. The picture of a tab, history or Space row is a
    // favicon or a colour and is left as it is.
    function test_aCommandRowsSymbolIsDrawnInTheAccent() {
        const panel = findChild(window.contentItem, "omnibar");
        const list = findChild(window.contentItem, "omnibarRowList");
        openPage("https://command-symbols-page.example/");
        activateWindow();
        window.openCommandScope();
        tryCompare(panel, "arrival", 1);
        tryVerify(function () {
            return panel.rows.length > 1;
        });
        tryVerify(function () {
            return panel.selected >= 0;
        });
        const accent = Qt.color(String(window.colors.accent));
        const symbolOf = function (index) {
            return findChild(omnibarRowItem(list, index), "omnibarRowSymbol");
        };
        const selected = symbolOf(panel.selected);
        compare(String(selected.color), String(accent));
        const other = symbolOf(panel.selected + 1);
        fuzzyCompare(other.color.r, accent.r, 0.01);
        fuzzyCompare(other.color.g, accent.g, 0.01);
        fuzzyCompare(other.color.b, accent.b, 0.01);
        fuzzyCompare(other.color.a, 0.6, 0.01);

        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // The block caret stands before the placeholder in an empty field, as the
    // website's does, and never over its first letter. Typing puts it after
    // the text.
    function test_theCaretStandsBeforeThePlaceholder() {
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting caret");
        tryCompare(findChild(window.contentItem, "omnibar"), "arrival", 1);
        compare(input.text, "");
        const caret = findChild(input, "omnibarCaret");
        verify(caret !== null);
        const textLeft = input.mapToItem(window.contentItem, input.leftPadding, 0).x;
        const caretRight = caret.mapToItem(window.contentItem, caret.width, 0).x;
        verify(caretRight <= textLeft, "the caret stands over the placeholder");
        const prompt = findChild(window.contentItem, "omnibarPrompt");
        verify(caret.mapToItem(window.contentItem, 0, 0).x >= prompt.mapToItem(window.contentItem,
                                                                               prompt.width, 0).x,
               "the caret stands over the mark");
        input.text = "x";
        verify(caret.mapToItem(window.contentItem, 0, 0).x >= textLeft);
        input.text = "";
        leaveSpace(homeSpaceId, restingSpaceId, "Resting caret");
    }

    // The Omnibar's field answers the keys its key map names, and the hint row
    // names the same keys: rebinding the arrows turns the selection the other
    // way and the row follows.
    function test_theFieldAnswersTheKeysTheHintRowNames() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const hints = findChild(window.contentItem, "omnibarHints");
        const keymap = panel.keymap;
        const original = keymap.omnibarBindings;
        browser.recordVisit("https://rebind-one.example/", "Rebind one");
        browser.recordVisit("https://rebind-two.example/", "Rebind two");
        openPage("https://rebind-page.example/");
        activateWindow();
        window.openOmnibar(false);
        tryCompare(panel, "arrival", 1);
        input.text = "rebind";
        tryVerify(function () {
            return panel.rows.length > 1;
        });
        panel.selected = 0;
        keyClick(Qt.Key_Down);
        compare(panel.selected, 1);
        keyClick(Qt.Key_Up);
        compare(panel.selected, 0);

        keymap.omnibarBindings = {
            "Up": "next",
            "Down": "previous",
            "Return": "go",
            "Backspace": "leave"
        };
        keyClick(Qt.Key_Up);
        compare(panel.selected, 1);
        keyClick(Qt.Key_Down);
        compare(panel.selected, 0);
        compare(hints.keys.select.join(), keymap.omnibarKeys.select.join());

        keymap.omnibarBindings = {
            "Left": "next",
            "Return": "go",
            "Backspace": "leave"
        };
        compare(hints.keys.select.join(), "Left");
        keyClick(Qt.Key_Down);
        compare(panel.selected, 0);
        keyClick(Qt.Key_Left);
        compare(panel.selected, 1);

        keymap.omnibarBindings = original;
        window.closeOmnibar();
        tryCompare(panel, "visible", false);
    }

    // The Shortcut sheet holds a row for every command whether it is open or
    // not. A closed sheet draws none of their key caps, which kept the rest of
    // the window slow enough to miss clicks, and an open one draws them.
    function test_aClosedShortcutSheetDrawsNoKeyCaps() {
        const sheet = findChild(window.contentItem, "shortcutSheet");
        const capsIn = function () {
            return childrenNamed(sheet, "keycap").filter(function (cap) {
                return cap.text.length > 0;
            }).length;
        };
        verify(!window.shortcutsOpen);
        compare(capsIn(), 0);
        window.requestShortcuts();
        tryVerify(function () {
            return window.shortcutsOpen && capsIn() > 0;
        });
        window.shortcutsOpen = false;
        tryVerify(function () {
            return capsIn() === 0;
        });
    }

    // A Space with nothing open in it has no page to show and no ordinary tab
    // to list. The Start page stands in: the Omnibar at rest over the road,
    // focused, and no renderer spent on the blank tab behind it.
    function test_aSpaceAtRestShowsTheOmnibarAtRestOverTheRoad() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const startPage = findChild(window.contentItem, "startPage");
        const road = findChild(window.contentItem, "nightRoad");
        const panel = findChild(window.contentItem, "omnibar");
        const frame = findChild(window.contentItem, "omnibarFrame");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting");

        // No row stands for a page nobody opened.
        const restingTabId = browser.activeTabId;
        tryVerify(function () {
            return findChild(window.contentItem, "tab-" + restingTabId) === null;
        });
        tryVerify(function () {
            return startPage.visible;
        });
        verify(road.visible);
        verify(panel.resting);
        verify(!window.omnibarOpen);
        // Nothing is listed until something is typed.
        browser.recordVisit("https://resting-history.example/", "Resting history");
        input.text = "resting";
        tryVerify(function () {
            return panel.rows.length > 0;
        });
        input.text = "";
        compare(panel.rows.length, 0);
        compare(engineLoader.engines[restingTabId], undefined);
        compare(engineLoader.item, null);

        // The field rests above the horizon, in the middle of the page area,
        // once the Space has arrived.
        tryCompare(panel, "arrival", 1);
        tryCompare(findChild(window.contentItem, "sidebar"), "arriving", false);
        const top = frame.mapToItem(startPage, frame.width / 2, 0);
        fuzzyCompare(top.x, startPage.width / 2, 1);
        verify(top.y < startPage.horizonY);
        fuzzyCompare(top.y + panel.horizonBelowTop, startPage.horizonY, 1);

        // Escape has no page to give back: it releases the field to the
        // browser and leaves what is typed alone.
        input.text = "half typed";
        keyClick(Qt.Key_Escape);
        verify(startPage.open);
        compare(input.text, "half typed");
        verify(!input.activeFocus);

        // Asking for the address focuses the field and keeps the text.
        window.sidebarCollapsed = false;
        findChild(window.contentItem, "sidebar").focusOutline();
        verify(!input.activeFocus);
        window.commands.run("open-address", -1);
        tryVerify(function () {
            return input.activeFocus;
        });
        compare(input.text, "half typed");

        // Return loads the address into the tab that was resting, and its row
        // arrives with it.
        input.text = "https://resting.example";
        const restingY = frame.y;
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return !startPage.open;
        });
        // The Omnibar leaves from where it rested, as it rested: no jump to
        // the overlay's place and no key hints on the way out.
        while (panel.visible) {
            verify(panel.shownResting);
            verify(frame.y <= restingY && frame.y >= restingY - 8);
            wait(10);
        }
        compare(browser.activeTabId, restingTabId);
        tryVerify(function () {
            return engineLoader.item !== null && engineLoader.item.currentUrl.toString()
                    === "https://resting.example";
        });
        tryVerify(function () {
            return findChild(window.contentItem, "tab-" + restingTabId) !== null;
        });

        leaveSpace(homeSpaceId, restingSpaceId, "Resting");
    }

    // The Start page's field belongs to the tab it stands in for. Moving on to
    // another Space at rest, from a row in that field or from anywhere else,
    // leaves what was typed and what the last Space's history answered behind.
    function test_aSpaceSwitchLeavesTheStartPageFieldBehind() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const secondSpaceId = browser.createSpace("Resting second");
        const firstSpaceId = enterRestingSpace("Resting first");

        input.text = "resting sec";
        const spaces = omnibarRowsOf(panel, "space");
        compare(spaces.length, 1);
        panel.selected = panel.rows.indexOf(spaces[0]);
        panel.accept();
        compare(browser.activeSpaceId, secondSpaceId);
        verify(window.startPageShown);
        compare(input.text, "");
        compare(panel.rows.length, 0);

        verify(browser.switchSpace(firstSpaceId));
        browser.recordVisit("https://first-space-only.example/", "First space only");
        input.text = "first space";
        tryVerify(function () {
            return omnibarRowsOf(panel, "history").length > 0;
        });
        verify(browser.switchSpace(secondSpaceId));
        verify(window.startPageShown);
        compare(input.text, "");
        compare(panel.rows.length, 0);

        verify(browser.switchSpace(homeSpaceId));
        verify(browser.deleteSpace(secondSpaceId, "Resting second"));
        leaveSpace(homeSpaceId, firstSpaceId, "Resting first");
    }

    // A new tab is the Start page over the page on show, and the tab exists
    // only once a destination is committed. Escape gives the page back.
    function test_aNewTabShowsTheStartPageAndCreatesItsTabOnCommit() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const startPage = findChild(window.contentItem, "startPage");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const engine = openPage("https://before-new-tab.example");
        settleMotion();
        activateWindow();
        const pageTabId = browser.activeTabId;
        const tabCount = browser.tabs.rowCount();

        keyClick(Qt.Key_T, Qt.ControlModifier);
        tryVerify(function () {
            return startPage.open && input.activeFocus;
        });
        verify(panel.resting);
        verify(panel.newTabIntent);
        compare(browser.tabs.rowCount(), tabCount);
        compare(browser.activeTabId, pageTabId);

        // Asking for the address focuses the Omnibar already there.
        window.commands.run("open-address", -1);
        verify(!window.omnibarOpen);
        verify(panel.resting);
        tryVerify(function () {
            return input.activeFocus;
        });

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !startPage.open;
        });
        compare(browser.activeTabId, pageTabId);
        compare(engineLoader.item, engine);
        compare(browser.tabs.rowCount(), tabCount);

        window.commands.run("new-tab", -1);
        tryVerify(function () {
            return input.activeFocus;
        });
        input.text = "https://after-new-tab.example";
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return browser.tabs.rowCount() === tabCount + 1;
        });
        verify(browser.activeTabId !== pageTabId);
        tryVerify(function () {
            return !startPage.open;
        });

        browser.closeActiveTab();
        browser.activateTab(pageTabId);
    }

    // Choosing an open tab from the Start page switches to it and opens no
    // tab of its own.
    function test_choosingAnOpenTabFromTheStartPageSwitchesToIt() {
        const startPage = findChild(window.contentItem, "startPage");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const firstTabId = browser.activeTabId;
        openPageInNewTab("https://kestrel-open.example/");
        const kestrelTabId = browser.activeTabId;
        browser.reportTabPageState(kestrelTabId, "https://kestrel-open.example/", "Kestrel page", "",
                                   false, false);
        openPageInNewTab("https://wren-open.example/");
        const wrenTabId = browser.activeTabId;
        const tabCount = browser.tabs.rowCount();
        activateWindow();

        window.commands.run("new-tab", -1);
        tryVerify(function () {
            return startPage.open && input.activeFocus;
        });
        input.text = "kestrel";
        compare(panel.rows[panel.selected].argument, kestrelTabId);
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return !startPage.open;
        });
        compare(browser.activeTabId, kestrelTabId);
        compare(browser.tabs.rowCount(), tabCount);
        verify(!window.startPageDriving);

        browser.closeTab(wrenTabId);
        browser.closeTab(kestrelTabId);
        browser.activateTab(firstTabId);
    }

    // Over a split the Start page covers both panes, which is where the tab it
    // opens will land, and Escape brings the split back as it was.
    function test_aNewTabOverASplitCoversBothPanes() {
        const startPage = findChild(window.contentItem, "startPage");
        const input = findChild(window.contentItem, "omnibarInput");
        const firstTabId = browser.activeTabId;
        openPageInNewTab("https://split-left.example");
        const leftTabId = browser.activeTabId;
        openPageInNewTab("https://split-right.example");
        const rightTabId = browser.activeTabId;
        verify(browser.addSplit(leftTabId));
        tryCompare(browser, "splitOnShow", true);
        activateWindow();

        window.commands.run("new-tab", -1);
        tryVerify(function () {
            return startPage.open;
        });
        tryVerify(function () {
            return input.activeFocus;
        }, 5000, "focus is on " + window.activeFocusItem);
        compare(startPage.x, 0);
        compare(startPage.width, startPage.parent.width);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !startPage.open;
        });
        verify(browser.splitOnShow);

        browser.closeTab(rightTabId);
        browser.closeTab(leftTabId);
        browser.activateTab(firstTabId);
    }

    // The Start page's road fills the page area and nothing else: it is
    // drawn at the page area's own size, as the website draws it in a
    // viewport, centred on it, and never runs under the sidebar. Over a Start
    // page the sidebar stands on its own fill, as with the road off.
    function test_theRoadFillsThePageAreaAndNotTheSidebar() {
        const sidebar = findChild(window.contentItem, "sidebar");
        const startPage = findChild(window.contentItem, "startPage");
        const scene = findChild(window.contentItem, "startPageScene");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting in the page area");
        tryCompare(sidebar, "arriving", false);
        tryVerify(function () {
            return startPage.visible && sidebar.visible && sidebar.width > 0;
        });

        const widths = [window.sidebarMinimumWidth + 40, window.sidebarMinimumWidth + 120];
        for (const width of widths) {
            window.sidebarWidth = width;
            tryCompare(startPage, "width", window.width - width);
            checkRoadFillsPageArea(startPage, scene);
            // Nothing between the road and the window cuts it short of the
            // page area's own edges, and the page area starts at the seam.
            verify(startPage.clip);
            compare(startPage.mapToItem(window.contentItem, 0, 0).x, width);
        }

        window.sidebarCollapsed = true;
        tryCompare(startPage, "width", window.width);
        checkRoadFillsPageArea(startPage, scene);
        window.sidebarCollapsed = false;
        tryCompare(startPage, "width", window.width - window.sidebarWidth);

        // Drawn over the road's side of the window, not under it.
        verify(sidebar.z <= findChild(window.contentItem, "engineViewport").z);
        compare(String(sidebar.color), String(window.colors.sidebar));

        const input = findChild(window.contentItem, "omnibarInput");
        input.text = "https://under-the-sidebar.example/";
        keyClick(Qt.Key_Return);
        tryVerify(function () {
            return findChild(window.contentItem, "engineLoader").item !== null;
        });
        findChild(window.contentItem, "engineLoader").item.simulateFirstPaint();
        tryVerify(function () {
            return !startPage.visible;
        });
        verify(!scene.visible);

        leaveSpace(homeSpaceId, restingSpaceId, "Resting in the page area");
    }

    // Dragging the sidebar's seam changes the page area at every frame, and
    // the road is drawn again for a size, not for a frame: it stays at the
    // size it was drawn at, stretched to the page area, with its sun on the
    // page area's middle, and is drawn again once the seam has settled.
    function test_theRoadIsDrawnAgainOnlyOnceTheSeamHasSettled() {
        const startPage = findChild(window.contentItem, "startPage");
        const scene = findChild(window.contentItem, "startPageScene");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting while dragged");
        tryVerify(function () {
            return startPage.visible && scene.light !== null;
        });
        window.sidebarWidth = window.sidebarMinimumWidth + 40;
        tryCompare(scene, "drawnWidth", scene.width);
        const drawn = scene.drawnWidth;

        for (let step = 1; step <= 6; ++step) {
            window.sidebarWidth = window.sidebarMinimumWidth + 40 + step * 12;
            tryCompare(startPage, "width", window.width - window.sidebarWidth);
            compare(scene.drawnWidth, drawn);
            compare(scene.width, startPage.width);
            verify(Math.abs(scene.light.centre.x - scene.width / 2) <= 2,
                   "the sun left the middle");

        }
        tryCompare(scene, "drawnWidth", scene.width);

        window.sidebarWidth = window.sidebarMinimumWidth + 40;
        leaveSpace(homeSpaceId, restingSpaceId, "Resting while dragged");
    }

    // The road is as wide as the page area, centred on it, and the glass is
    // framed by the road itself.
    function checkRoadFillsPageArea(startPage, scene) {
        compare(scene.width, startPage.width);
        compare(scene.x, 0);
        compare(scene.frame.x, 0);
        compare(scene.frame.width, startPage.width);
        const origin = scene.mapToItem(window.contentItem, 0, 0);
        const page = startPage.mapToItem(window.contentItem, 0, 0);
        compare(origin.x, page.x);
    }

    // The Scenes the reader can choose, for the tests that hold every one of
    // them to the road's rules.
    function test_theStartPageDrawsNoFramesHiddenOrUnfocused_data() {
        return [
                    {
                        tag: "road",
                        scene: "crt-road"
                    },
                    {
                        tag: "sky",
                        scene: "night-sky"
                    }
                ];
    }

    // A Scene moves only while the reader could see it: a window that has
    // lost the keyboard or gone from the screen schedules no frame for it.
    function test_theStartPageDrawsNoFramesHiddenOrUnfocused(data) {
        const startPage = findChild(window.contentItem, "startPage");
        const homeSpaceId = browser.activeSpaceId;
        window.setStartPageScene(data.scene);
        const restingSpaceId = enterRestingSpace("Resting frames");
        tryVerify(function () {
            return startPage.sceneRunning;
        });
        let frames = startPage.sceneFrames;
        tryVerify(function () {
            return startPage.sceneFrames > frames + 2;
        });

        const other = createTemporaryObject(otherWindowComponent, testCase);
        other.show();
        other.requestActivate();
        tryVerify(function () {
            return !window.active;
        });
        verify(!startPage.sceneRunning);
        frames = startPage.sceneFrames;
        wait(250);
        compare(startPage.sceneFrames, frames);
        other.close();

        activateWindow();
        tryVerify(function () {
            return startPage.sceneRunning;
        });

        window.hide();
        tryVerify(function () {
            return !startPage.sceneRunning;
        });
        frames = startPage.sceneFrames;
        wait(250);
        compare(startPage.sceneFrames, frames);
        window.show();
        activateWindow();
        tryVerify(function () {
            return startPage.sceneRunning;
        });

        leaveSpace(homeSpaceId, restingSpaceId, "Resting frames");
    }

    function test_reducedMotionHoldsTheSceneStill_data() {
        return test_theStartPageDrawsNoFramesHiddenOrUnfocused_data();
    }

    // A reader who asked for less motion gets a Scene that holds still: no
    // clock, so no frame is drawn for it, and the glass without its band or
    // flicker. The desktop's setting reaches the Scene through the window.
    function test_reducedMotionHoldsTheSceneStill(data) {
        const startPage = findChild(window.contentItem, "startPage");
        const glass = findChild(window.contentItem, "crtGlass");
        const homeSpaceId = browser.activeSpaceId;
        window.setStartPageScene(data.scene);
        const restingSpaceId = enterRestingSpace("Resting still");
        tryVerify(function () {
            return startPage.sceneRunning;
        });

        SystemMotion.reduced = true;
        verify(window.reducedMotion);
        verify(!startPage.sceneRunning);
        const frames = startPage.sceneFrames;
        wait(250);
        compare(startPage.sceneFrames, frames);
        compare(glass.flicker, 0);
        compare(glass.bandStrength, 0);

        SystemMotion.reduced = false;
        tryVerify(function () {
            return startPage.sceneRunning;
        });
        leaveSpace(homeSpaceId, restingSpaceId, "Resting still");
    }

    // `omaweb dev` before the dev server is up: the project's Space shows the
    // road driving and loads nothing, and Escape stops waiting. Once the
    // address answers, the road drives on into the page.
    function test_theRoadDrivesUntilTheProjectsAddressAnswers() {
        const startPage = findChild(window.contentItem, "startPage");
        const road = findChild(window.contentItem, "nightRoad");
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const homeSpaceId = browser.activeSpaceId;
        const project = "/home/reader/omaweb-ui-dev-shop";
        const waited = agentSocket.ask({
                                           verb: "dev",
                                           name: "zsh",
                                           directory: project,
                                           address: "127.0.0.1:1"
                                       });
        verify(waited.ok, waited.error);
        const spaceId = waited.space;
        compare(browser.activeSpaceId, spaceId);
        verify(browser.activeSpaceAwaitsAddress);
        tryVerify(function () {
            return startPage.open;
        });
        compare(road.navigating, 1);
        wait(300);
        compare(road.navigating, 1);
        verify(browser.activeTabBlank);
        compare(engineLoader.item, null);

        activateWindow();
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !browser.activeSpaceAwaitsAddress;
        });
        compare(road.navigating, 0);
        verify(browser.activeTabBlank);

        const served = String(suggestServer.suggestUrl()).match(/^http:\/\/([^/]+)/);
        const answered = agentSocket.ask({
                                             verb: "dev",
                                             name: "zsh",
                                             directory: project,
                                             address: served[1]
                                         });
        verify(answered.ok, answered.error);
        compare(answered.space, spaceId);
        tryVerify(function () {
            return String(browser.activeUrl).indexOf(served[0]) === 0;
        });
        verify(window.startPageDriving);
        compare(road.navigating, 1);
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        engineLoader.item.simulateFirstPaint();
        tryVerify(function () {
            return !window.startPageDriving;
        });
        leaveSpace(homeSpaceId, spaceId, waited.spaceName);
    }

    // After a commit the road drives until the page first paints, for two
    // seconds at most, and a failure ends it at once.
    function test_theRoadDrivesUntilFirstPaintAndStopsOnAnError() {
        const panel = findChild(window.contentItem, "omnibar");
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const startPage = findChild(window.contentItem, "startPage");
        const road = findChild(window.contentItem, "nightRoad");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting drive");

        input.text = "https://slow-paint.example/one";
        // The Omnibar stays open from Return to the drive: it never closes
        // and opens again in between.
        const openings = [];
        const noteOpening = function () {
            openings.push(panel.open);
        };
        panel.openChanged.connect(noteOpening);
        keyClick(Qt.Key_Return);
        panel.openChanged.disconnect(noteOpening);
        compare(openings.length, 0);
        verify(window.startPageDriving);
        compare(road.navigating, 1);
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        wait(300);
        verify(window.startPageDriving);
        verify(startPage.open);
        // The first paint ends it then, long before the limit would.
        engineLoader.item.simulateFirstPaint();
        tryVerify(function () {
            return !window.startPageDriving && !startPage.open;
        }, 400);
        // Navigating falls at the first paint, and the road eases out of it,
        // still lit, as it fades into the page.
        verify(startPage.visible);
        compare(road.navigating, 0);
        verify(road.sunUp > 0);
        tryVerify(function () {
            return !startPage.visible;
        });

        // A failed load has its own page to show, straight away.
        window.commands.run("new-tab", -1);
        tryVerify(function () {
            return input.activeFocus;
        });
        input.text = "https://slow-paint.example/two";
        keyClick(Qt.Key_Return);
        verify(window.startPageDriving);
        tryVerify(function () {
            return engineLoader.item !== null && engineLoader.item.currentUrl.toString()
                    === "https://slow-paint.example/two";
        });
        engineLoader.item.lastLoadFailed = true;
        tryVerify(function () {
            return !window.startPageDriving;
        }, 400);
        browser.closeActiveTab();

        // So has an upgrade HTTPS-only mode could not complete, and a
        // certificate the engine refused.
        const failures = [function (engine) {
            engine.simulateHttpsUpgradeFailure("http://slow-paint.example/upgrade", "unreachable",
                                               "");
        }, function (engine) {
            engine.certificateErrorOrigin = "https://slow-paint.example";
        }];
        for (let index = 0; index < failures.length; ++index) {
            window.commands.run("new-tab", -1);
            tryVerify(function () {
                return input.activeFocus;
            });
            const address = "https://slow-paint.example/failure-" + index;
            input.text = address;
            keyClick(Qt.Key_Return);
            verify(window.startPageDriving);
            tryVerify(function () {
                return engineLoader.item !== null && engineLoader.item.currentUrl.toString()
                        === address;
            });
            failures[index](engineLoader.item);
            tryVerify(function () {
                return !window.startPageDriving;
            }, 400);
            browser.closeActiveTab();
        }

        // A page that has not painted in two seconds is left to the page's own
        // loading indicator.
        window.commands.run("new-tab", -1);
        tryVerify(function () {
            return input.activeFocus;
        });
        const committed = Date.now();
        input.text = "https://slow-paint.example/three";
        keyClick(Qt.Key_Return);
        verify(window.startPageDriving);
        tryVerify(function () {
            return !window.startPageDriving;
        }, 4000);
        verify(Date.now() - committed >= window.startPageDriveLimit - 100);
        verify(!startPage.open);
        browser.closeActiveTab();

        leaveSpace(homeSpaceId, restingSpaceId, "Resting drive");
    }

    // Asking for a new tab while the road still drives to the last one is
    // being done with waiting: that page takes over and a new Start page
    // stands over it.
    function test_aNewTabWhileTheRoadDrivesLetsThePageTakeOver() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const startPage = findChild(window.contentItem, "startPage");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting impatient");

        input.text = "https://slow-paint.example/impatient";
        keyClick(Qt.Key_Return);
        verify(window.startPageDriving);
        const drivenTabId = browser.activeTabId;

        window.commands.run("new-tab", -1);
        verify(!window.startPageDriving);
        verify(window.startPageSummoned);
        tryVerify(function () {
            return startPage.open && input.activeFocus;
        });
        compare(browser.activeTabId, drivenTabId);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !startPage.open;
        });
        compare(engineLoader.item.currentUrl.toString(), "https://slow-paint.example/impatient");

        leaveSpace(homeSpaceId, restingSpaceId, "Resting impatient");
    }

    // In a Space at rest there is no page to go back to, so Escape releases
    // the field: the caret goes, the field is drawn unfocused, and the keyboard
    // is the browser's, so a bare key from the key map runs its command as it
    // does over a page. `o`, a click on the field and Primary+L give the field
    // back.
    function test_escapeInASpaceAtRestReleasesTheFieldToTheBareKeys() {
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting release");
        tryCompare(panel, "arrival", 1);
        verify(panel.keymap.pageCommandsEnabled);
        verify(input.activeFocus);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !input.activeFocus;
        });
        verify(!input.cursorVisible);
        verify(panel.shownResting);
        compare(window.focusedRegionName(), window.pageRegionName());

        keyClick(":");
        tryVerify(function () {
            return panel.commandScope;
        });
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !panel.commandScope;
        });

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !input.activeFocus;
        });
        keyClick("o");
        tryVerify(function () {
            return input.activeFocus;
        });
        compare(input.text, "");

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !input.activeFocus;
        });
        mouseClick(input);
        tryVerify(function () {
            return input.activeFocus;
        });

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !input.activeFocus;
        });
        keyClick(Qt.Key_L, Qt.ControlModifier);
        tryVerify(function () {
            return input.activeFocus;
        });

        leaveSpace(homeSpaceId, restingSpaceId, "Resting release");
    }

    // The Shortcut sheet is summoned, not shown: `?` in the Start page's empty
    // field brings it up over the road, and Escape goes back to the field.
    function test_questionMarkSummonsTheShortcutSheetFromTheStartPage() {
        const startPage = findChild(window.contentItem, "startPage");
        const sheet = findChild(window.contentItem, "shortcutSheet");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting sheet");
        tryVerify(function () {
            return findChild(window.contentItem, "omnibarShortcutsHint").visible;
        });

        keyClick("?");
        tryVerify(function () {
            return window.shortcutsOpen && sheet.visible;
        });
        compare(input.text, "");
        compare(findChild(sheet, "shortcutsBackdrop").source, startPage);
        verify(startPage.open);
        // The sheet covers the Omnibar, which waits under it.
        const panel = findChild(window.contentItem, "omnibar");
        compare(panel.opacity, 0);
        verify(panel.open);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !window.shortcutsOpen;
        });
        tryVerify(function () {
            return input.activeFocus;
        });
        verify(startPage.open);
        compare(panel.opacity, 1);

        leaveSpace(homeSpaceId, restingSpaceId, "Resting sheet");
    }

    // The reader chooses the Start page's Scene in Settings' interface
    // section, on this installation alone: Night road, Night sky, or None,
    // which leaves the Omnibar over the sidebar's fill. The choice takes effect
    // at once.
    function test_settingsChoosesTheStartPageScene() {
        const startPage = findChild(window.contentItem, "startPage");
        const scene = findChild(window.contentItem, "startPageScene");
        const backdrop = findChild(window.contentItem, "startPageBackdrop");
        const settings = findChild(window.contentItem, "settingsSurface");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting scene");
        tryVerify(function () {
            return scene.visible && findChild(scene, "nightRoad") !== null;
        });
        verify(!backdrop.visible);

        window.settingsOpen = true;
        settings.section = settings.sections.indexOf("interface");
        const picker = findChild(settings, "startPageScenePicker");
        verify(picker !== null);
        compare(picker.value, "crt-road");
        const sky = findChild(picker, "sceneThumbnail-night-sky");
        // Settings has arrived, so a click lands on it.
        tryVerify(function () {
            return sky.visible && sky.width > 0 && settings.opacity === 1 && settings.lift === 0;
        });
        const miss = clickReportingAMiss(sky, function () {
            return browser.preference("start-page-scene", "") === "night-sky";
        });
        verify(miss === "", miss);
        compare(picker.value, "night-sky");

        // The keyboard moves between the thumbnails, and Return chooses.
        picker.forceActiveFocus();
        keyClick(Qt.Key_Right);
        compare(browser.preference("start-page-scene", ""), "night-sky");
        keyClick(Qt.Key_Return);
        compare(browser.preference("start-page-scene", ""), "none");
        keyClick(Qt.Key_Left);
        keyClick(Qt.Key_Left);
        keyClick(Qt.Key_Space);
        compare(browser.preference("start-page-scene", ""), "crt-road");
        const again = clickReportingAMiss(sky, function () {
            return browser.preference("start-page-scene", "") === "night-sky";
        });
        verify(again === "", again);
        window.settingsOpen = false;

        tryVerify(function () {
            return startPage.open;
        });
        verify(findChild(scene, "nightSky") !== null);
        // The road the sky replaced is let go once the event loop turns.
        tryVerify(function () {
            return findChild(scene, "nightRoad") === null;
        });
        // Shown once the Start page has begun to fade in.
        tryVerify(function () {
            return scene.visible && startPage.sceneRunning;
        });

        window.setStartPageScene("none");
        compare(browser.preference("start-page-scene", ""), "none");
        verify(!scene.visible);
        tryVerify(function () {
            return backdrop.visible;
        });
        verify(!startPage.sceneRunning);

        // The stored choice is what the window follows, however it was made.
        browser.setPreference("start-page-scene", "night-sky");
        verify(scene.visible);
        verify(findChild(scene, "nightSky") !== null);
        browser.setPreference("start-page-scene", "crt-road");
        verify(findChild(scene, "nightRoad") !== null);
        tryVerify(function () {
            return findChild(scene, "nightSky") === null;
        });
        leaveSpace(homeSpaceId, restingSpaceId, "Resting scene");
    }

    function test_theSkysPlanetStaysBelowTheOmnibar_data() {
        return [
                    {
                        tag: "1360 x 860",
                        width: 1360,
                        height: 860,
                        larger: 0
                    },
                    {
                        tag: "1000 x 640",
                        width: 1000,
                        height: 640,
                        larger: 0
                    },
                    {
                        tag: "1800 x 1100",
                        width: 1800,
                        height: 1100,
                        larger: 0
                    },
                    {
                        tag: "larger type",
                        width: 1360,
                        height: 860,
                        larger: 6
                    }
                ];
    }

    // The sky's planet lies wholly under the resting Omnibar: its top, the
    // glow along its limb included, stands a small gap below the hint row's
    // bottom edge, at any window size and type size, so its curve shows whole.
    // The Omnibar keeps the place it has over the road.
    function test_theSkysPlanetStaysBelowTheOmnibar(data) {
        const width = window.width;
        const height = window.height;
        fontSettings.setInterfaceFontSize(fontSettings.themeFontSize + data.larger);
        const panel = findChild(window.contentItem, "omnibar");
        const scene = findChild(window.contentItem, "startPageScene");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting planet");
        try {
            window.width = data.width;
            window.height = data.height;
            tryCompare(window.contentItem, "height", data.height);
            const restY = panel.restY;
            window.setStartPageScene("night-sky");
            const sky = findChild(scene, "nightSky");
            verify(sky !== null);
            compare(panel.restY, restY);
            const bottom = function () {
                return panel.restY + panel.restHeight;
            };
            const top = function () {
                return scene.mapToItem(panel, 0, sky.planetTop).y;
            };
            tryVerify(function () {
                return top() > bottom() + 4;
            }, 1000, "planet's top " + top() + ", Omnibar's bottom " + bottom());
            verify(top() < bottom() + 24, "planet's top " + top() + ", Omnibar's bottom " + bottom(
                       ));
        } finally {
            leaveSpace(homeSpaceId, restingSpaceId, "Resting planet");
            fontSettings.resetInterfaceFontSize();
            window.width = width;
            window.height = height;
        }
    }

    // A reader who turned the road off before there was a choice arrives on
    // None, and one who left it on, or never touched it, on the road.
    function test_theRoadSwitchCarriesOverToTheScene_data() {
        return [
                    {
                        tag: "switched off",
                        road: "false",
                        scene: "none"
                    },
                    {
                        tag: "switched on",
                        road: "true",
                        scene: "crt-road"
                    },
                    {
                        tag: "never switched",
                        road: "",
                        scene: "crt-road"
                    }
                ];
    }

    function test_theRoadSwitchCarriesOverToTheScene(data) {
        browser.setPreference("start-page-scene", "");
        browser.setPreference("start-page-road", data.road);
        window.restoreChromeAppearance();
        compare(window.startPageScene, data.scene);
        // A choice made since is the one that holds.
        window.setStartPageScene("night-sky");
        window.restoreChromeAppearance();
        compare(window.startPageScene, "night-sky");
        browser.setPreference("start-page-road", "");
    }

    // A commit brings the sky's streaks until the page first paints, as it
    // drives the road.
    function test_theSkyStreaksUntilFirstPaint() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        const startPage = findChild(window.contentItem, "startPage");
        const input = findChild(window.contentItem, "omnibarInput");
        const homeSpaceId = browser.activeSpaceId;
        window.setStartPageScene("night-sky");
        const restingSpaceId = enterRestingSpace("Resting streaks");
        const sky = findChild(window.contentItem, "nightSky");
        verify(sky !== null);

        input.text = "https://slow-paint.example/sky";
        keyClick(Qt.Key_Return);
        verify(window.startPageDriving);
        compare(sky.navigating, 1);
        tryVerify(function () {
            return sky.streaks > 0;
        });
        tryVerify(function () {
            return engineLoader.item !== null;
        });
        verify(startPage.open);
        engineLoader.item.simulateFirstPaint();
        tryVerify(function () {
            return !window.startPageDriving && !startPage.open;
        }, 400);
        compare(sky.navigating, 0);
        tryVerify(function () {
            return !startPage.visible;
        });
        browser.closeActiveTab();
        leaveSpace(homeSpaceId, restingSpaceId, "Resting streaks");
    }

    // The CRT glass is on unless the reader turns it off, on this installation
    // alone; off, the road is its plain pixels.
    function test_settingsTurnsTheGlassOffLocally() {
        const scene = findChild(window.contentItem, "startPageScene");
        const glass = findChild(scene, "crtGlass");
        const settings = findChild(window.contentItem, "settingsSurface");
        const homeSpaceId = browser.activeSpaceId;
        const restingSpaceId = enterRestingSpace("Resting glass");
        tryVerify(function () {
            return scene.visible;
        });
        verify(scene.glass);
        verify(glass.visible);

        window.settingsOpen = true;
        settings.section = settings.sections.indexOf("interface");
        const toggle = findChild(settings, "startPageGlass");
        verify(toggle !== null);
        verify(toggle.checked);
        toggle.clicked();
        compare(browser.preference("start-page-glass", "true"), "false");
        window.settingsOpen = false;
        tryVerify(function () {
            return scene.visible;
        });
        verify(!scene.glass);
        verify(!glass.visible);
        verify(findChild(scene, "sceneDisplay").visible);

        // The stored choice is what the window follows, however it was made.
        browser.setPreference("start-page-glass", "true");
        verify(scene.glass);
        browser.setPreference("start-page-glass", "false");
        verify(!scene.glass);
        window.setStartPageGlass(true);
        compare(browser.preference("start-page-glass", "false"), "true");
        verify(glass.visible);

        // The glass is over whichever Scene stands there, and with None there
        // is no Scene for it, so the setting is not offered.
        compare(toggle.title, "CRT glass over the Scene");
        window.setStartPageScene("night-sky");
        verify(findChild(scene, "nightSky") !== null);
        verify(glass.visible);
        verify(toggle.visible);
        window.setStartPageScene("none");
        verify(!toggle.visible);
        leaveSpace(homeSpaceId, restingSpaceId, "Resting glass");
    }

    // A blank address is not the same thing as a resting Space, and it must not
    // be an empty viewport either: there is no page, so the Start page stands
    // in. The tab itself stays listed, because the reader put it there and has
    // to be able to close it.
    function test_blankAddressShowsTheStartPageRatherThanAnEmptyViewport() {
        const startPage = findChild(window.contentItem, "startPage");
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(startPage !== null);
        openPage("https://not-blank.example");
        browser.openInputInBackground("https://beside.example");
        verify(browser.tabs.rowCount() > 1);
        tryVerify(function () {
            return !startPage.visible;
        });

        const tabId = browser.activeTabId;
        browser.openInput("about:blank", false);
        tryVerify(function () {
            return startPage.visible;
        });

        // The Space is not at rest, so the tab keeps its row and its close
        // button, and the Start page stands in without pretending otherwise.
        verify(!browser.atRest);
        verify(findChild(window.contentItem, "tab-" + tabId) !== null);
        verify(!window.startPageSummoned);
        tryVerify(function () {
            return engineLoader.engines[tabId] === undefined;
        });

        openPage("https://not-blank.example/again");
        tryVerify(function () {
            return !startPage.visible;
        });
    }

    // Leaving a blank tab for a page and coming back has to bring the Start
    // page back with it. The host names one active engine, and a tab that has
    // none has to clear that name rather than leave the last tab's page
    // standing in as the answer to "is a page up?".
    function test_returningToABlankTabBringsTheStartPageBack() {
        const startPage = findChild(window.contentItem, "startPage");
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(startPage !== null);

        openPage("https://neighbour.example");
        const blankTabId = browser.activeTabId;
        browser.openInput("about:blank", false);
        tryVerify(function () {
            return startPage.visible;
        });

        browser.openInput("https://elsewhere.example", true);
        const pageTabId = browser.activeTabId;
        tryVerify(function () {
            return !startPage.visible && engineLoader.item !== null;
        });

        browser.activateTab(blankTabId);
        tryVerify(function () {
            return startPage.visible;
        });
        // Nothing is drawing a page, and the window is told so.
        compare(engineLoader.item, null);

        browser.activateTab(pageTabId);
        tryVerify(function () {
            return !startPage.visible;
        });
        verify(engineLoader.item !== null);
    }

    // The sheet is a command like any other, so it answers on demand over a
    // live page, blurs that page, and closes back to it.
    function test_shortcutSheetAnswersOnDemandOverAPage() {
        const sheet = findChild(window.contentItem, "shortcutSheet");
        verify(sheet !== null);
        openPage("https://busy.example");
        verify(!sheet.visible);

        window.commands.run("shortcuts", -1);
        tryVerify(function () {
            return sheet.visible;
        });

        // Over a page the sheet keeps the sidebar's colour and blurs that page
        // behind itself, so what the reader left is still legible as a place
        // without being readable as a page.
        const sheetBackdrop = findChild(sheet, "shortcutsBackdrop");
        verify(sheetBackdrop !== null);
        // A sheet over a page lets more through than the sidebar does: at the
        // sidebar's own value a dark page shows as nothing.
        compare(String(sheetBackdrop.tint), String(window.colors.sheet));
        verify(Qt.color(window.colors.sheet).a < Qt.color(window.colors.sidebar).a);
        tryVerify(function () {
            return sheetBackdrop.sampling;
        });
        compare(sheetBackdrop.source, findChild(window.contentItem, "engineLoader"));

        // Every command the sheet lists carries the keys the window answers to,
        // and it names them exactly as the Omnibar does.
        verify(sheet.sections.length > 0);
        let openAddressKeys = "";
        for (let group = 0; group < sheet.sections.length; ++group) {
            const entries = sheet.sections[group].entries;
            for (let index = 0; index < entries.length; ++index) {
                verify(entries[index].keys.length > 0);
                if (entries[index].title === "Open address")
                    openAddressKeys = entries[index].keys;
            }
        }
        compare(openAddressKeys, window.commands.keymap.keysFor("open-address"));

        const closeButton = findChild(window.contentItem, "closeShortcutsButton");
        verify(closeButton !== null);
        verify(closeButton.visible);

        // The same command takes it away again.
        window.commands.run("shortcuts", -1);
        tryVerify(function () {
            return !sheet.visible;
        });

        // So does Escape, and so does the close button.
        window.commands.run("shortcuts", -1);
        tryVerify(function () {
            return sheet.visible;
        });
        activateWindow();
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !sheet.visible;
        });

        window.commands.run("shortcuts", -1);
        tryVerify(function () {
            return sheet.visible;
        });
        closeButton.clicked();
        tryVerify(function () {
            return !sheet.visible;
        });

        // Being in the registry is what makes it searchable and bindable.
        const matches = window.commands.search("Keyboard shortcuts");
        verify(matches.length > 0);
        compare(matches[0].command, "shortcuts");
        verify(matches[0].keys.length > 0);
    }

    // A binding this build cannot honour is dropped rather than taking the
    // keymap with it, so a notice is the only thing that says a configured key
    // is missing. It waits in the settings section it belongs to, and the
    // sidebar's settings button carries a mark so it is findable from outside.
    function test_settingsMarksItselfWhenTheKeymapReportsIgnoredBindings() {
        const settings = findChild(window.contentItem, "settingsSurface");
        const notice = findChild(window.contentItem, "keyboardBindingNotice");
        const dot = findChild(window.contentItem, "settingsAttentionDot");
        verify(settings !== null);
        verify(notice !== null);
        verify(dot !== null);

        // This build knows every command in its own default file, so nothing is
        // waiting and neither the notice nor the mark is drawn.
        compare(keyboardNavigation.errorMessage, "");
        verify(!settings.needsAttention);
        verify(!notice.visible);
        verify(!dot.visible);

        // The notice and the mark are drawn in the one colour the theme names
        // for something being wrong, so the two read as one thing.
        compare(String(dot.color), String(window.colors.urgent));
        compare(String(notice.urgent), String(window.colors.urgent));
        verify(String(window.colors.urgent) !== String(window.colors.privateAccent));

        // The notice states what the keymap reported, wherever that came from.
        compare(notice.detail, settings.keyboardReport);

        // A keymap that did report something: the notice states it, and the
        // section it belongs to says the reader is wanted.
        const reported = "Ignored bindings this build does not know: "
              + "Primary+Shift+D (debug-current-tab)";
        const reporting = reportingSettingsComponent.createObject(window.contentItem, {
                                                                      "report": reported
                                                                  });
        verify(reporting !== null);
        compare(reporting.keyboardReport, reported);
        verify(reporting.needsAttention);
        const reportingNotice = findChild(reporting, "keyboardBindingNotice");
        verify(reportingNotice !== null);
        tryVerify(function () {
            return reportingNotice.visible;
        });
        compare(reportingNotice.detail, reported);
        verify(reportingNotice.height > 0);
        // It is not on the section it does not belong to.
        reporting.section = 0;
        tryVerify(function () {
            return !reportingNotice.visible;
        });
        reporting.destroy();

        // And the mark is drawn once the outline is told.
        const marked = attentionOutlineComponent.createObject(window.contentItem);
        verify(marked !== null);
        const markedDot = findChild(marked, "settingsAttentionDot");
        verify(markedDot !== null);
        tryVerify(function () {
            return markedDot.visible;
        });
        compare(String(markedDot.color), String(window.colors.urgent));
        marked.destroy();
    }

    // A tab's chip stands in for artwork that is not being drawn, so it takes
    // the site's own colour: the hue of the favicon, at the theme's saturation
    // and lightness. An icon with no hue to give leaves the chip neutral
    // instead of falling back to a colour the site never chose.
    function test_chipTakesItsColourFromTheFaviconWhenArtworkIsOff() {
        const engineLoader = findChild(window.contentItem, "engineLoader");
        verify(engineLoader !== null);
        openPage("https://chip.example");

        const tabId = browser.activeTabId;
        const tile = findChild(window.contentItem, "siteTile-" + tabId);
        verify(tile !== null);

        const hostTint = tile.hostTint;
        engineLoader.item.pageIconUrl = colouredFaviconUrl;
        tryCompare(tile, "iconUrl", colouredFaviconUrl);

        // Artwork on: the chip's colour is still the host's, because the
        // artwork itself is what carries the site's colour.
        compare(window.useFavicons, true);
        compare(String(tile.chipTint), String(hostTint));

        const useFavicons = findChild(window.contentItem, "useFavicons");
        verify(useFavicons !== null);
        useFavicons.clicked();
        compare(window.useFavicons, false);

        const blue = Qt.color("#2f5ce6");
        tryVerify(function () {
            return String(tile.chipTint) !== String(hostTint);
        });
        fuzzyCompare(tile.chipTint.hsvHue, blue.hsvHue, 0.03);
        // The theme still owns how strong a chip may be.
        fuzzyCompare(tile.chipTint.hslSaturation, tile.tintSaturation, 0.02);
        fuzzyCompare(tile.chipTint.hslLightness, tile.tintLightness, 0.02);

        // A white icon names no colour, so the chip goes neutral rather than
        // borrowing one from the host's name.
        engineLoader.item.pageIconUrl = colourlessFaviconUrl;
        tryCompare(tile, "iconUrl", colourlessFaviconUrl);
        tryVerify(function () {
            return String(tile.chipTint) === String(Qt.color(window.colors.mutedText));
        });

        useFavicons.clicked();
        compare(window.useFavicons, true);
        engineLoader.item.pageIconUrl = "";
    }

    function test_tabArtworkSettingsAreLiveAndSaved() {
        const useFavicons = findChild(window.contentItem, "useFavicons");
        const tintFavicons = findChild(window.contentItem, "tintFavicons");
        verify(useFavicons !== null);
        verify(tintFavicons !== null);
        compare(window.useFavicons, true);
        // A fresh profile draws every favicon as its site drew it, so tinting
        // is off until the reader asks for it.
        compare(window.tintFavicons, false);

        useFavicons.clicked();
        compare(window.useFavicons, false);
        compare(useFavicons.checked, false);
        compare(tintFavicons.enabled, false);
        compare(browser.preference("use-favicons", "true"), "false");

        useFavicons.clicked();
        tintFavicons.clicked();
        compare(window.useFavicons, true);
        compare(window.tintFavicons, true);
        compare(browser.preference("tint-favicons", "false"), "true");

        tintFavicons.clicked();
        compare(window.tintFavicons, false);
        compare(browser.preference("tint-favicons", "false"), "false");
    }

    // The strip stands in for the sidebar by default, and a reader who wants
    // the page to have the window keeps the keys that hide the sidebar either
    // way: refusing the strip refuses the strip, not the state it belongs to.
    function test_theFloatingControlsCanBeRefused() {
        window.settingsOpen = false;
        window.sidebarCollapsed = false;
        const cluster = findChild(window.contentItem, "navigationCluster");
        const floatingControls = findChild(window.contentItem, "floatingControls");
        verify(cluster !== null);
        verify(floatingControls !== null);
        compare(window.floatingControls, true);

        window.sidebarCollapsed = true;
        tryVerify(function () {
            return cluster.visible;
        });

        floatingControls.clicked();
        compare(window.floatingControls, false);
        compare(browser.preference("floating-controls", "true"), "false");
        verify(!cluster.visible);
        // The sidebar still answers, so the reader is not shut out of the
        // state they are in.
        verify(window.commands.run("toggle-sidebar", -1));
        tryVerify(function () {
            return !window.sidebarCollapsed;
        });

        floatingControls.clicked();
        compare(window.floatingControls, true);
        compare(browser.preference("floating-controls", "true"), "true");
    }

    // The kit's choice draws one button per option and names none of them, so
    // a button is found by what it says.
    function chipLabelled(group, label) {
        for (let index = 0; index < group.children.length; ++index) {
            if (group.children[index].text === label)
                return group.children[index];
        }
        return null;
    }

    // The side is the reader's, chosen in Interface. It moves the sidebar at
    // once, without a restart, and comes back on the next start.
    function test_interfaceChoosesTheSidebarsSide() {
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("interface");
        const side = findChild(settings, "sidebarSide");
        verify(side !== null);
        verify(side.visible);
        compare(side.value, "left");
        const sidebar = findChild(window.contentItem, "sidebar");

        const right = chipLabelled(side, "Right");
        verify(right !== null);
        settleActions(right);
        const missed = clickReportingAMiss(right, function () {
            return window.sidebarSide === "right";
        });
        verify(missed.length === 0, missed);
        compare(side.value, "right");
        compare(browser.preference("sidebar-side", "left"), "right");
        tryCompare(sidebar, "x", window.width - sidebar.width);

        // What the next start reads.
        window.sidebarSide = "left";
        window.restoreChromeAppearance();
        compare(window.sidebarSide, "right");

        const left = chipLabelled(side, "Left");
        mouseClick(left, left.width / 2, left.height / 2);
        compare(window.sidebarSide, "left");
        compare(browser.preference("sidebar-side", "right"), "left");
        tryCompare(sidebar, "x", 0);
        window.settingsOpen = false;

        // Another window choosing a side moves this one's sidebar as well.
        browser.setPreference("sidebar-side", "right");
        compare(window.sidebarSide, "right");
        browser.setPreference("sidebar-side", "left");
        compare(window.sidebarSide, "left");
    }

    // On the right the sidebar is the left one turned round: the page starts at
    // the window's left edge and ends on the seam, the divider and the handle
    // stand on the sidebar's inner edge, and every way of resizing reads the
    // width from the right edge.
    function test_aRightSidebarResizesFromItsInnerEdge() {
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        window.setSidebarSide("right");
        const sidebar = findChild(window.contentItem, "sidebar");
        const viewport = findChild(window.contentItem, "engineViewport");
        const resizer = findChild(window.contentItem, "sidebarResizer");
        const divider = findChild(sidebar, "sidebarDivider");
        verify(divider !== null);
        const row = window.contentItem.width;
        const restsAt = function (width) {
            return Math.round(sidebar.x) === row - width && Math.round(viewport.x) === 0
                    && Math.round(viewport.width) === row - width;
        };
        tryVerify(function () {
            return restsAt(window.sidebarDefaultWidth);
        });
        compare(Math.round(resizer.x + resizer.width / 2), Math.round(sidebar.x));
        compare(Math.round(divider.mapToItem(window.contentItem, 0, 0).x), Math.round(sidebar.x));
        // The Space notice stands over the middle of the page, not of the window.
        const notice = findChild(window.contentItem, "spaceNotice");
        verify(Math.abs(notice.x + notice.width / 2 - (row - window.sidebarDefaultWidth) / 2) <= 1);

        resizer.forceActiveFocus();
        keyClick(Qt.Key_Left);
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 16);
        keyClick(Qt.Key_Right);
        compare(window.sidebarWidth, window.sidebarDefaultWidth);

        // A drag toward the page widens the sidebar by the distance travelled.
        mousePress(resizer, resizer.width / 2, 300);
        mouseMove(resizer, resizer.width / 2 - 60, 300);
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 60);
        mouseRelease(resizer, resizer.width / 2, 300);
        tryVerify(function () {
            return restsAt(window.sidebarDefaultWidth + 60);
        });
        compare(Math.round(resizer.x + resizer.width / 2), Math.round(sidebar.x));

        window.commands.run("reset-sidebar", -1);
        window.commands.run("widen-sidebar", -1);
        tryVerify(function () {
            return restsAt(window.sidebarDefaultWidth + 24);
        });
        window.commands.run("narrow-sidebar", -1);
        tryVerify(function () {
            return restsAt(window.sidebarDefaultWidth);
        });

        // The sidebar is to the right of the page now, so that is the way the
        // keyboard goes to reach it.
        window.focusPage();
        window.commands.run("move-focus-left", -1);
        compare(window.focusedRegionName(), "page");
        window.commands.run("move-focus-right", -1);
        compare(window.focusedRegionName(), "sidebar");
        window.commands.run("move-focus-left", -1);
        compare(window.focusedRegionName(), "page");
        window.commands.run("focus-sidebar", -1);
        compare(window.focusedRegionName(), "sidebar");
        window.commands.run("focus-page", -1);
        compare(window.focusedRegionName(), "page");
    }

    // A right sidebar hides toward the right edge and comes back from it. In
    // every frame the page stays at the window's left edge, so its content
    // does not jump, and the part of it on show ends on the sidebar's leading
    // edge, so the slide opens no gap and the page is not drawn over the
    // sidebar. Hidden, it leaves the floating controls and the edge that peeks
    // at it on the right.
    function test_aRightSidebarHidesTowardItsOwnEdge() {
        window.setSidebarSide("right");
        const sidebar = findChild(window.contentItem, "sidebar");
        const viewport = findChild(window.contentItem, "engineViewport");
        const cluster = findChild(window.contentItem, "navigationCluster");
        const revealEdge = findChild(window.contentItem, "sidebarRevealEdge");
        const backdrop = findChild(window.contentItem, "sidebarBackdrop");
        const pageClip = findChild(window.contentItem, "pageClip");
        const row = window.contentItem.width;
        const shownAt = row - window.sidebarDefaultWidth;
        tryVerify(function () {
            return Math.round(sidebar.x) === shownAt;
        });

        const watchTheRightSeam = function () {
            return watchFrames(function () {
                const pageEnd = viewport.mapToItem(window.contentItem, viewport.width, 0).x;
                return {
                    "sidebarStart": Math.round(sidebar.x),
                    "pageStart": Math.round(viewport.mapToItem(window.contentItem, 0, 0).x),
                    "pageShownEnd": Math.round(pageClip.clip ? Math.min(pageEnd, pageClip.width) :
                                                               pageEnd),
                    "sidebarShown": sidebar.visible,
                    "strip": cluster.visible
                };
            });
        };
        const checkTheRightSeam = function (watch) {
            stopWatching(watch);
            verify(watch.seen.length > 0);
            for (let at = 0; at < watch.seen.length; ++at) {
                const frame = watch.seen[at];
                compare(frame.pageStart, 0);
                compare(frame.pageShownEnd, frame.sidebarStart);
                if (frame.strip)
                    verify(!frame.sidebarShown);
            }
        };

        let seam = watchTheRightSeam();
        let slide = watchChanges(sidebar, "x");
        sidebar.sidebarToggled();
        verify(passedBetween(slide, shownAt, row));
        tryVerify(function () {
            return !sidebar.visible;
        });
        checkTheRightSeam(seam);
        compare(Math.round(viewport.x), 0);
        compare(Math.round(viewport.width), row);

        // The strip stands where the sidebar's top was, against the right edge.
        verify(cluster.visible);
        const strip = cluster.mapToItem(window.contentItem, 0, 0);
        compare(Math.round(strip.x + cluster.width), row - 16);
        // The find bar moves across the page, out of the strip's way.
        const findBar = findChild(window.contentItem, "findBar");
        verify(findBar.mapToItem(window.contentItem, 0, 0).x + findBar.width < strip.x);
        compare(Math.round(revealEdge.mapToItem(window.contentItem, 0, 0).x + revealEdge.width),
                row);

        // Holding the pointer at the right edge peeks from there.
        mouseMove(window.contentItem, row - 2, window.height / 2);
        tryCompare(sidebar, "visible", true);
        compare(sidebar.floating, true);
        tryVerify(function () {
            return Math.round(sidebar.x) === shownAt;
        });
        compare(backdrop.sourceRect.x, sidebar.x);
        mouseMove(window.contentItem, row / 2, window.height / 2);
        tryCompare(sidebar, "visible", false);
        compare(window.sidebarCollapsed, true);

        seam = watchTheRightSeam();
        slide = watchChanges(sidebar, "x");
        cluster.sidebarToggled();
        verify(passedBetween(slide, row, shownAt));
        tryVerify(function () {
            return Math.round(viewport.width) === shownAt;
        });
        checkTheRightSeam(seam);
        compare(Math.round(sidebar.x), shownAt);
        compare(Math.round(viewport.x), 0);
    }

    // With developer tools docked and a right sidebar hidden, the strip still
    // stands against the window's right edge, over the dock's top corner,
    // where the sidebar's controls were.
    function test_aRightSidebarsStripStandsAtTheWindowsEdgeBesideTheDock() {
        openPage("https://dock-strip.example/");
        window.setSidebarSide("right");
        window.commands.run("developer-tools", -1);
        const dock = findChild(window.contentItem, "developerToolsDock");
        const cluster = findChild(window.contentItem, "navigationCluster");
        tryVerify(function () {
            return dock.visible;
        });
        try {
            window.sidebarCollapsed = true;
            tryVerify(function () {
                return cluster.visible;
            });
            const strip = cluster.mapToItem(window.contentItem, 0, 0);
            compare(Math.round(strip.x + cluster.width), window.contentItem.width - 16);
        } finally {
            browser.closeDeveloperTools();
            tryVerify(function () {
                return !dock.visible;
            });
        }
    }

    // What stands over the page area beside a right sidebar stands over the
    // page and not over the sidebar: the Start page's resting Omnibar and its
    // road, and the area a click in puts Site information away.
    function test_aRightSidebarLeavesTheOverlaysOverThePage() {
        // Summoned over a page, which is where the Start page is summoned.
        openPage("https://overlays.example/");
        window.setSidebarSide("right");
        const sidebar = findChild(window.contentItem, "sidebar");
        const omnibar = findChild(window.contentItem, "omnibar");
        const startPage = findChild(window.contentItem, "startPage");
        tryVerify(function () {
            return Math.round(sidebar.x) === window.contentItem.width - window.sidebarDefaultWidth;
        });
        window.startPageSummoned = true;
        try {
            tryVerify(function () {
                return window.startPageShown && omnibar.resting;
            });
            const page = startPage.mapToItem(window.contentItem, 0, 0);
            compare(omnibar.restArea.x, page.x);
            verify(omnibar.restArea.x + omnibar.restArea.width <= sidebar.x);
            verify(startPage.scene !== null);
            const road = startPage.scene.mapToItem(window.contentItem, 0, 0);
            compare(omnibar.roadOrigin.x, road.x);
        } finally {
            window.startPageSummoned = false;
            tryCompare(startPage, "visible", false);
            tryCompare(omnibar, "visible", false);
        }

        window.openSiteInformation("");
        const dismiss = findChild(window.contentItem, "siteInformationDismissArea");
        verify(dismiss !== null);
        tryVerify(function () {
            return dismiss.visible;
        });
        compare(dismiss.x, 0);
        compare(dismiss.x + dismiss.width, sidebar.x);
        window.closeSiteInformation();
        // The card hangs over the seam, so the next test's handle waits for it.
        tryCompare(siteCard(), "visible", false);
    }

    // A key hides the sidebar in one step: the seam is where it settles in the
    // frame the sidebar was hidden in rather than somewhere along the way, and
    // the page is laid out once as it always is. The pointer's press eases it.
    function test_theSidebarSettlesFromAKeyAndEasesFromThePointer() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        window.setSidebarWidth(window.sidebarDefaultWidth);
        openPage("https://seam.example/");
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const viewport = findChild(window.contentItem, "engineViewport");
        window.requestActivate();
        tryVerify(function () {
            return window.active && Math.round(sidebar.x) === 0;
        });
        const row = Math.round(sidebar.x + sidebar.width + viewport.width);

        // The key the reader pressed is already a decision: no frame of the
        // seam on its way, and the page laid out once.
        viewportWidthSpy.target = viewport;
        viewportWidthSpy.clear();
        keyClick(Qt.Key_B, Qt.ControlModifier);
        compare(window.sidebarCollapsed, true);
        verify(!sidebar.visible);
        compare(Math.round(viewport.x), 0);
        compare(Math.round(viewport.width), row);
        compare(viewportWidthSpy.count, 1);
        keyClick(Qt.Key_B, Qt.ControlModifier);
        verify(sidebar.visible);
        compare(Math.round(sidebar.x), 0);
        compare(Math.round(viewport.x), window.sidebarDefaultWidth);
        viewportWidthSpy.target = null;

        // The pointer's press slides it, so the eye can follow where it went.
        const hide = findChild(sidebar, "collapseButton");
        settleActions(hide);
        let slide = watchChanges(sidebar, "x");
        mouseClick(hide);
        compare(window.sidebarCollapsed, true);
        verify(passedBetween(slide, 0, -sidebar.width));
        tryVerify(function () {
            return !sidebar.visible;
        });
        const show = findChild(findChild(window.contentItem, "navigationCluster"),
                               "collapseButton");
        tryVerify(function () {
            return show.visible;
        });
        settleActions(show);
        slide = watchChanges(sidebar, "x");
        mouseClick(show);
        compare(window.sidebarCollapsed, false);
        verify(passedBetween(slide, -sidebar.width, 0));
        tryVerify(function () {
            return Math.round(sidebar.x) === 0;
        });

        // A tap is the pointer too, whatever was pressed before it.
        keyClick(Qt.Key_Shift);
        settleActions(hide);
        slide = watchChanges(sidebar, "x");
        const tap = touchEvent(hide);
        tap.press(0, hide, hide.width / 2, hide.height / 2).commit();
        tap.release(0, hide, hide.width / 2, hide.height / 2).commit();
        compare(window.sidebarCollapsed, true);
        verify(passedBetween(slide, 0, -sidebar.width));
        window.sidebarCollapsed = false;
    }

    // A Space switched to by its key is there at once: the list, the page
    // and the lit Space stand where they end. A click on its square moves
    // them, so the eye can follow the Space it went to.
    function test_aSpaceSettlesFromItsKeyAndSlidesFromThePointer() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        openPage("https://space-key.example/");
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const viewport = findChild(window.contentItem, "engineViewport");
        const homeId = browser.activeSpaceId;
        const otherId = browser.createSpace("Keyed Space");
        const spaces = browser.spaces;
        let position = -1;
        for (let row = 0; row < spaces.rowCount(); ++row) {
            if (spaces.data(spaces.index(row, 0), Qt.UserRole + 1) === otherId)
                position = row;
        }
        verify(position >= 0 && position < 9);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });

        const otherMark = findChild(sidebar, "spaceMark-" + otherId);
        const restingSide = otherMark.width;
        keyClick(Qt.Key_1 + position, Qt.ControlModifier);
        compare(browser.activeSpaceId, otherId);
        verify(!sidebar.arriving);
        compare(viewport.transform[0].x, 0);
        verify(otherMark.width > restingSide);
        const litSide = otherMark.width;

        const homeButton = findChild(sidebar, "space-" + homeId);
        settleActions(homeButton);
        const shrink = watchChanges(otherMark, "width");
        mouseClick(homeButton);
        compare(browser.activeSpaceId, homeId);
        verify(sidebar.arriving);
        verify(passedBetween(shrink, litSide, restingSide));
        tryCompare(sidebar, "arriving", false);

        verify(browser.deleteSpace(otherId, "Keyed Space"));
        tryCompare(findChild(window.contentItem, "spaceNotice"), "visible", false, 5000);
    }

    // A tab chosen by its key is on show where it rests. A click on its row
    // nudges the page in from the row's side.
    function test_aTabSettlesFromItsKeyAndArrivesFromThePointer() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        const engineHost = findChild(window.contentItem, "engineLoader");
        openPage("https://tab-key-first.example/");
        const firstId = browser.activeTabId;
        openPageInNewTab("https://tab-key-second.example/");
        const secondId = browser.activeTabId;
        settleMotion();
        const order = sidebarOrder();
        verify(order.indexOf(firstId) < 9);
        window.requestActivate();
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return window.active && engineHost.item.activeFocus;
        });

        keyClick(Qt.Key_1 + order.indexOf(firstId));
        compare(browser.activeTabId, firstId);
        let nudged = false;
        for (let sample = 0; sample < 40; ++sample) {
            nudged = nudged || engineHost.tabNudgeX !== 0 || engineHost.tabNudgeY !== 0;
            wait(5);
        }
        verify(!nudged);

        const secondRow = findChild(window.contentItem, "tab-" + secondId);
        settleActions(secondRow);
        mouseClick(secondRow);
        compare(browser.activeTabId, secondId);
        tryVerify(function () {
            return engineHost.tabNudgeY !== 0;
        }, 500);
        settleMotion();

        browser.closeTab(secondId);
    }

    // The Omnibar a key opens is there at once, and Escape takes it away at
    // once. A click on the address opens it with its drop, and a click past
    // it sends it back with its rise. A command the command scope runs is the
    // reader's typed decision even when its row is clicked: the Omnibar goes
    // at once and what the command does arrives settled.
    function test_theOmnibarSettlesFromAKeyAndMovesFromThePointer() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        openPage("https://omnibar-key.example/");
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const panel = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        const rows = findChild(window.contentItem, "omnibarRowList");
        window.requestActivate();
        window.commands.run("focus-page", -1);
        tryVerify(function () {
            return window.active;
        });

        keyClick(Qt.Key_L, Qt.ControlModifier);
        verify(window.omnibarOpen);
        compare(panel.arrival, 1);
        tryVerify(function () {
            return input.activeFocus;
        });
        keyClick(Qt.Key_Escape);
        verify(!window.omnibarOpen);
        verify(!panel.retreating);
        verify(!panel.visible);

        const address = findChild(sidebar, "addressButton");
        settleActions(address);
        mouseClick(address);
        verify(window.omnibarOpen);
        verify(panel.arrival < 1);
        tryCompare(panel, "arrival", 1);
        mouseClick(window.contentItem, window.width - 20, window.height - 20);
        verify(!window.omnibarOpen);
        verify(panel.retreating);
        tryVerify(function () {
            return !panel.visible;
        });

        keyClick(Qt.Key_K, Qt.ControlModifier);
        verify(window.omnibarOpen && panel.commandScope);
        input.text = "Hide or show the sidebar";
        const commandRow = function () {
            return panel.rows.findIndex(function (row) {
                return row.command === "toggle-sidebar";
            });
        };
        tryVerify(function () {
            return commandRow() >= 0;
        });
        const row = omnibarRowItem(rows, commandRow());
        settleActions(row);
        mouseClick(row);
        verify(!window.omnibarOpen);
        verify(!panel.retreating);
        compare(window.sidebarCollapsed, true);
        verify(!sidebar.visible);
        window.sidebarCollapsed = false;
    }

    // Motion that tells the reader something moves whatever started it: the
    // notice naming the Space a key or a click arrived in comes down from
    // above its place either way, and an Agent mark pulses while its command
    // is in flight after a key as after a click.
    function test_theSpaceNoticeAndTheAgentPulseMoveForAKeyAndThePointer() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        const drive = driveAnAgentSpace(false);
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const notice = findChild(window.contentItem, "spaceNotice");
        tryCompare(notice, "visible", false, 5000);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        const spaces = browser.spaces;
        let readersPosition = -1;
        for (let row = 0; row < spaces.rowCount(); ++row) {
            if (spaces.data(spaces.index(row, 0), Qt.UserRole + 1) === drive.readersSpaceId)
                readersPosition = row;
        }
        verify(readersPosition >= 0 && readersPosition < 9);

        let drop = watchChanges(notice, "drop");
        keyClick(Qt.Key_1 + readersPosition, Qt.ControlModifier);
        compare(browser.activeSpaceId, drive.readersSpaceId);
        verify(passedBetween(drop, -8, 0));
        tryCompare(notice, "visible", false, 5000);

        const agentButton = findChild(sidebar, "space-" + drive.spaceId);
        settleActions(agentButton);
        drop = watchChanges(notice, "drop");
        mouseClick(agentButton);
        compare(browser.activeSpaceId, drive.spaceId);
        verify(passedBetween(drop, -8, 0));
        tryCompare(notice, "visible", false, 5000);
        settleMotion();

        // The switches built the Space's rows again.
        const rowMark = findChild(sidebar, "agentMark-" + drive.tabId);
        verify(rowMark.visible);
        keyClick(Qt.Key_Shift);
        let pulse = watchChanges(rowMark, "opacity");
        drive.report(true, "clicked \"Files changed\"");
        verify(passedBetween(pulse, 0.3, 1));
        drive.report(false, "looked at the page");
        mouseClick(findChild(window.contentItem, "spaceHeading"));
        pulse = watchChanges(rowMark, "opacity");
        drive.report(true, "clicked \"Files changed\"");
        verify(passedBetween(pulse, 0.3, 1));
        drive.report(false, "looked at the page");
        endAgentDrive(drive);
    }

    // A desktop that asks for reduced motion stills the chrome whatever
    // started it: from a click, the sidebar, a Space, a tab and the Omnibar
    // arrive settled, a peek is there at once, the Space notice appears where
    // it stands, and the page loading indicator holds still.
    function test_reducedMotionStillsThePointersMotionToo() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        const engineHost = findChild(window.contentItem, "engineLoader");
        const firstEngine = openPage("https://reduced-first.example/");
        const firstId = browser.activeTabId;
        openPageInNewTab("https://reduced-second.example/");
        const secondId = browser.activeTabId;
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const notice = findChild(window.contentItem, "spaceNotice");
        const panel = findChild(window.contentItem, "omnibar");
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        const homeId = browser.activeSpaceId;
        const otherId = browser.createSpace("Still Space");
        tryCompare(notice, "visible", false, 5000);
        SystemMotion.reduced = true;
        try {
            compare(window.reducedMotion, true);

            const firstRow = findChild(window.contentItem, "tab-" + firstId);
            settleActions(firstRow);
            mouseClick(firstRow);
            compare(browser.activeTabId, firstId);
            let nudged = false;
            for (let sample = 0; sample < 40; ++sample) {
                nudged = nudged || engineHost.tabNudgeX !== 0 || engineHost.tabNudgeY !== 0;
                wait(5);
            }
            verify(!nudged);

            firstEngine.loading = true;
            tryVerify(function () {
                return indicator.visible;
            });
            compare(indicator.motionEnabled, false);
            firstEngine.stopLoading();
            tryCompare(indicator, "visible", false);

            const otherButton = findChild(sidebar, "space-" + otherId);
            settleActions(otherButton);
            mouseClick(otherButton);
            compare(browser.activeSpaceId, otherId);
            verify(!sidebar.arriving);
            tryCompare(notice, "visible", true);
            compare(notice.drop, 0);
            tryCompare(notice, "visible", false, 5000);
            const homeButton = findChild(sidebar, "space-" + homeId);
            settleActions(homeButton);
            mouseClick(homeButton);
            compare(browser.activeSpaceId, homeId);
            verify(!sidebar.arriving);
            tryCompare(notice, "visible", false, 5000);

            const address = findChild(sidebar, "addressButton");
            settleActions(address);
            mouseClick(address);
            verify(window.omnibarOpen);
            compare(panel.arrival, 1);
            mouseClick(window.contentItem, window.width - 20, window.height - 20);
            verify(!window.omnibarOpen);
            verify(!panel.retreating);

            const hide = findChild(sidebar, "collapseButton");
            settleActions(hide);
            mouseClick(hide);
            compare(window.sidebarCollapsed, true);
            verify(!sidebar.visible);

            mouseMove(window.contentItem, 2, window.height / 2);
            tryVerify(function () {
                return sidebar.visible;
            });
            compare(Math.round(sidebar.x), 0);
            mouseMove(window.contentItem, window.width / 2, window.height / 2);
            tryVerify(function () {
                return !sidebar.visible;
            });
            window.sidebarCollapsed = false;
        } finally {
            SystemMotion.reduced = false;
        }
        verify(browser.deleteSpace(otherId, "Still Space"));
        tryCompare(notice, "visible", false, 5000);
        browser.closeTab(secondId);
    }

    // Under reduced motion a page notice fades in where it stands rather than
    // coming down from the edge, and the rows a dragged row passes step into
    // their opened places rather than sliding.
    function test_reducedMotionStillsANoticeAndTheRowsADragPasses() {
        const notice = findChild(window.contentItem, "pageNotice");
        const surface = findChild(notice, "pageNoticeSurface");
        const surfaceY = surface.transform[0];
        let drop = watchChanges(surfaceY, "y");
        notice.show("info", "Moving notice");
        verify(passedBetween(drop, -8, 0));
        notice.dismiss();
        tryVerify(function () {
            return !notice.visible;
        });

        openPage("https://still-drag.example/one");
        browser.openInput("https://still-drag.example/two", true);
        const secondTabId = browser.activeTabId;
        browser.openInput("https://still-drag.example/three", true);
        const thirdTabId = browser.activeTabId;
        settleRow(findChild(window.contentItem, "tab-" + thirdTabId));
        SystemMotion.reduced = true;
        try {
            drop = watchChanges(surfaceY, "y");
            notice.show("info", "Still notice");
            verify(!passedBetween(drop, -8, 0));
            compare(surfaceY.y, 0);
            notice.dismiss();

            const lastRow = findChild(window.contentItem, "tab-" + thirdTabId);
            const rowHeight = lastRow.height;
            const grabbed = lastRow.mapToItem(window.contentItem, lastRow.width / 2, rowHeight / 2);
            mousePress(lastRow, lastRow.width / 2, rowHeight / 2);
            dragRowBy(grabbed, -rowHeight * 1.5);
            verify(lastRow.lifted);
            const passedRow = findChild(window.contentItem, "tab-" + secondTabId);
            tryVerify(function () {
                return passedRow.carry.y > 0;
            }, 500);
            // Already where it opens to, with no frames on the way.
            const opened = passedRow.carry.y;
            verify(opened >= rowHeight);
            for (let sample = 0; sample < 30; ++sample) {
                compare(passedRow.carry.y, opened);
                wait(5);
            }
            mouseRelease(window.contentItem, grabbed.x, grabbed.y - rowHeight * 1.5);
        } finally {
            SystemMotion.reduced = false;
        }
        browser.closeTab(thirdTabId);
        browser.closeTab(secondTabId);
    }

    // A busy Agent mark holds still under reduced motion, dimmed so it still
    // says a command is in flight, and comes back to full strength when it
    // ends.
    function test_reducedMotionHoldsABusyAgentMarkStill() {
        const drive = driveAnAgentSpace(false);
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        const rowMark = findChild(sidebar, "agentMark-" + drive.tabId);
        const spaceMark = findChild(sidebar, "spaceAgentMark-" + drive.spaceId);
        // A pulse already under way when the desktop asks stops where it is
        // asked to.
        drive.report(true, "clicked \"Files changed\"");
        tryVerify(function () {
            return rowMark.opacity < 0.9;
        });
        SystemMotion.reduced = true;
        try {
            verify(rowMark.opacity < 1);
            verify(spaceMark.opacity < 1);
            const held = rowMark.opacity;
            for (let sample = 0; sample < 40; ++sample) {
                compare(rowMark.opacity, held);
                wait(10);
            }
            drive.report(false, "looked at the page");
            compare(rowMark.opacity, 1);
            compare(spaceMark.opacity, 1);
        } finally {
            SystemMotion.reduced = false;
        }
        endAgentDrive(drive);
    }

    // The chrome's ease is no longer a setting: Settings offers no switch for
    // it, and a value stored by an earlier version, or carried in by Sync,
    // changes nothing. The pointer's press still eases the sidebar.
    function test_aStoredChromeEaseIsIgnored() {
        window.settingsOpen = false;
        window.historyOpen = false;
        window.sidebarCollapsed = false;
        openPage("https://stored-ease.example/");
        settleMotion();
        const sidebar = findChild(window.contentItem, "sidebar");
        browser.setPreference("ease-sidebar", "false");
        window.restoreChromeAppearance();
        try {
            window.requestSettings();
            const settings = findChild(window.contentItem, "settingsSurface");
            settings.section = settings.sections.indexOf("interface");
            compare(findChild(settings, "easeChrome"), null);
            window.settingsOpen = false;
            tryVerify(function () {
                return !settings.visible;
            });

            const hide = findChild(sidebar, "collapseButton");
            settleActions(hide);
            const slide = watchChanges(sidebar, "x");
            mouseClick(hide);
            compare(window.sidebarCollapsed, true);
            verify(passedBetween(slide, 0, -sidebar.width));
        } finally {
            window.sidebarCollapsed = false;
        }
    }

    // Changing a default must not change an answer someone already gave. A
    // reader who turned tinting on while it was still the default comes back to
    // it on, and the stored answer is what the window reads rather than the
    // property's own value.
    function test_aStoredTintAnswerOutlivesTheDefault() {
        const stored = browser.preference("tint-favicons", "");

        browser.setPreference("tint-favicons", "true");
        window.restoreTabAppearance();
        compare(window.tintFavicons, true);

        browser.setPreference("tint-favicons", "false");
        window.restoreTabAppearance();
        compare(window.tintFavicons, false);

        browser.setPreference("tint-favicons", stored);
        window.restoreTabAppearance();
    }

    // A row's chip and a pin's mark share the same size, and the
    // size is the theme's smallest type rather than a number written into the
    // row: a theme with a larger font takes a larger chip with it instead of
    // clipping the two letters the chip stands in with.
    function test_theChipSizesAreDerivedFromTheThemesSmallestType() {
        openPage("https://chip-size.example");
        const row = findChild(window.contentItem, "tab-" + browser.activeTabId);
        verify(row !== null);
        const tile = findChild(window.contentItem, "siteTile-" + browser.activeTabId);
        verify(tile !== null);

        const ordinary = row.chipSize;
        verify(ordinary > 0);
        // Small enough to sit beside a title rather than to carry the row.
        verify(ordinary < row.height);
        tryCompare(tile, "implicitWidth", ordinary);
        tryCompare(tile, "implicitHeight", ordinary);

        themeAxis.useTypeTokens(1.6);
        tryVerify(function () {
            return row.chipSize > ordinary;
        });
        tryCompare(tile, "implicitWidth", row.chipSize);
        themeAxis.useStatedTheme();
        tryCompare(row, "chipSize", ordinary);

        // Pinning replaces the row, so the pin is found again rather than
        // asked of the row that has just been destroyed.
        browser.toggleActivePinned();
        const pinnedRow = findChild(window.contentItem, "pinned-" + browser.activeTabId);
        verify(pinnedRow !== null);
        const pinnedTile = findChild(window.contentItem, "siteTile-" + browser.activeTabId);
        verify(pinnedTile !== null);
        tryCompare(pinnedTile, "implicitWidth", ordinary);
        tryCompare(pinnedTile, "implicitHeight", ordinary);
        themeAxis.useTypeTokens(1.6);
        tryVerify(function () {
            return pinnedRow.chipSize > ordinary;
        });
        tryCompare(pinnedTile, "implicitWidth", pinnedRow.chipSize);
        tryCompare(pinnedTile, "implicitHeight", pinnedRow.chipSize);
        themeAxis.useStatedTheme();
        tryCompare(pinnedTile, "implicitWidth", ordinary);
        tryCompare(pinnedTile, "implicitHeight", ordinary);
        browser.toggleActivePinned();

        browser.closeActiveTab();
    }

    // A pin is a square with no title, so Omaweb paints it in the site's own
    // colour — which is artwork colour, and therefore the tint setting's to
    // give. Switched off, the pin is chrome: no wash, no site-coloured mark.
    function test_pinnedSiteColourFollowsTheTintSetting() {
        const tintFavicons = findChild(window.contentItem, "tintFavicons");
        verify(tintFavicons !== null);
        compare(window.tintFavicons, false);

        browser.toggleActivePinned();
        const pinnedRow = findChild(window.contentItem, "pinned-" + browser.activeTabId);
        verify(pinnedRow !== null);
        tryVerify(function () {
            return pinnedRow.visible;
        });
        compare(pinnedRow.siteColored, false);
        const tile = findChild(window.contentItem, "siteTile-" + browser.activeTabId);
        verify(tile !== null);
        compare(tile.siteColoredMark, false);

        tintFavicons.clicked();
        compare(window.tintFavicons, true);
        tryCompare(pinnedRow, "tintFavicons", true);
        compare(pinnedRow.siteColored, true);
        tryCompare(tile, "siteColoredMark", true);

        tintFavicons.clicked();
        compare(window.tintFavicons, false);
        tryCompare(pinnedRow, "siteColored", false);
        browser.toggleActivePinned();
    }

    // ---- Everyday page commands ----------------------------------------

    function openPageInNewTab(url) {
        const engineHost = findChild(window.contentItem, "engineLoader");
        browser.openInput(url, true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item.currentUrl.toString() === url;
        });
        return engineHost.item;
    }

    function commandEnabled(command) {
        const actions = window.commands.actions();
        for (let index = 0; index < actions.length; ++index) {
            if (actions[index].command === command)
                return actions[index].enabled;
        }
        return undefined;
    }

    // Find belongs to one tab: hiding it keeps the query and the match the tab
    // had reached, another tab has a search of its own, and a navigation takes
    // the matches without taking the query.
    function test_findBelongsToOneTabAndKeepsItsQueryWhileHidden() {
        const first = openPage("https://find-one.example/page");
        first.pageText = "alpha beta alpha gamma alpha";
        const firstTabId = browser.activeTabId;
        const bar = findChild(window.contentItem, "findBar");
        verify(bar !== null);
        verify(!bar.open);

        window.commands.run("find", -1);
        tryVerify(function () {
            return bar.open;
        });
        const input = findChild(bar, "findInput");
        verify(input !== null);
        // Asking to find puts the keyboard in the field: a bar that opens and
        // leaves the reader typing into the page has not answered the command.
        tryVerify(function () {
            return input.activeFocus;
        });
        // One row, and everything in it has room: the field, the tally, the
        // two match steps and the close.
        const closeButton = findChild(bar, "findCloseButton");
        verify(closeButton !== null);
        verify(input.width > 100);
        verify(input.height <= bar.height);
        verify(closeButton.x + closeButton.width <= bar.width);
        verify(input.x + input.width <= closeButton.x);

        input.text = "alpha";
        tryCompare(bar, "matchCount", 3);
        compare(bar.activeMatch, 1);
        compare(first.findQuery, "alpha");

        window.commands.run("find-next", -1);
        compare(bar.activeMatch, 2);
        window.commands.run("find-previous", -1);
        compare(bar.activeMatch, 1);

        // Hidden, not forgotten — and the keyboard goes back to the page.
        window.closeFind();
        compare(bar.open, false);
        tryVerify(function () {
            return first.pageHasFocus;
        });
        compare(first.findQuery, "alpha");
        compare(first.findActiveMatch, 1);

        window.commands.run("find", -1);
        tryVerify(function () {
            return bar.open;
        });
        compare(bar.text, "alpha");

        // The tab beside it is searching for nothing, and says so by not
        // offering the bar at all.
        const second = openPageInNewTab("https://find-two.example/page");
        const secondTabId = browser.activeTabId;
        compare(bar.open, false);
        compare(second.findQuery, "");

        // A search of its own, which goes away with the tab rather than
        // outliving it in the window's map.
        window.commands.run("find", -1);
        tryVerify(function () {
            return bar.open;
        });
        verify(window.tabsShowingFind[secondTabId] === true);

        browser.activateTab(firstTabId);
        tryVerify(function () {
            return bar.open;
        });
        compare(bar.text, "alpha");

        // A navigation invalidates where the search had reached. What the
        // reader was looking for is still theirs.
        browser.openInput("https://find-one.example/other", false);
        tryCompare(first, "findMatchCount", 0);
        compare(first.findQuery, "alpha");

        browser.closeTab(secondTabId);
        window.refreshFindOpen();
        compare(window.tabsShowingFind[secondTabId], undefined);

        window.closeFind();
    }

    // Zoom belongs to one tab, reaches that tab's engine, and leaves every
    // other tab at the size it was.
    function test_zoomBelongsToOneTabAndReachesItsEngine() {
        const first = openPage("https://zoom-one.example");
        const firstTabId = browser.activeTabId;
        compare(browser.activeTabZoom, 1.0);
        compare(first.zoomFactor, 1.0);

        window.commands.run("zoom-in", -1);
        compare(browser.activeTabZoom, 1.1);
        tryCompare(first, "zoomFactor", 1.1);

        const notice = findChild(window.contentItem, "pageNotice");
        verify(notice !== null);
        compare(notice.message, "Page zoom 110%");

        const second = openPageInNewTab("https://zoom-two.example");
        const secondTabId = browser.activeTabId;
        compare(browser.activeTabZoom, 1.0);
        compare(second.zoomFactor, 1.0);

        browser.activateTab(firstTabId);
        compare(browser.activeTabZoom, 1.1);
        window.commands.run("zoom-out", -1);
        compare(browser.activeTabZoom, 1.0);
        window.commands.run("zoom-in", -1);
        window.commands.run("zoom-reset", -1);
        compare(browser.activeTabZoom, 1.0);
        tryCompare(first, "zoomFactor", 1.0);

        browser.closeTab(secondTabId);
    }

    // The command registry sends navigation straight to the active engine host
    // rather than passing it through browser state that it does not change.
    function test_navigationCommandsReachTheActiveEngine() {
        const engine = openPage("https://navigation.example/page");
        const backBefore = engine.backCount;
        const forwardBefore = engine.forwardCount;

        window.commands.run("back", -1);
        window.commands.run("forward", -1);

        compare(engine.backCount, backBefore + 1);
        compare(engine.forwardCount, forwardBefore + 1);
    }

    // Three asks that look alike from outside are three operations inside:
    // read the page again, read it again from the network, stop reading it.
    function test_reloadStopAndBypassingCacheAreSeparateOperations() {
        const engine = openPage("https://reload.example/page");
        const bypassedBefore = engine.bypassedCacheCount;
        const stoppedBefore = engine.stoppedLoadCount;

        window.commands.run("reload", -1);
        compare(engine.bypassedCacheCount, bypassedBefore);
        compare(engine.stoppedLoadCount, stoppedBefore);
        verify(engine.loading);

        window.commands.run("reload-bypassing-cache", -1);
        compare(engine.bypassedCacheCount, bypassedBefore + 1);

        window.commands.run("stop-loading", -1);
        compare(engine.stoppedLoadCount, stoppedBefore + 1);
        compare(engine.loading, false);

        // And the page is still the page: stopping a load clears nothing.
        compare(engine.currentUrl.toString(), "https://reload.example/page");
    }

    function test_pageLoadingIndicatorFollowsTheActiveEngine() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const engine = openPage("https://loading.example/page");
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        verify(indicator !== null);
        verify(!indicator.visible);

        engine.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });
        compare(Math.round(indicator.x + indicator.width / 2), Math.round(engineHost.x
                                                                          + engineHost.width / 2));
        compare(indicator.y, 8);

        engine.loading = false;
        tryVerify(function () {
            return !indicator.visible;
        });

        engine.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });
        engine.lastLoadFailed = true;
        engine.loading = false;
        tryVerify(function () {
            return !indicator.visible;
        });
    }

    function test_pageLoadingIndicatorBelongsToThePageOnShow() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        const first = openPage("https://loading-first.example/page");
        const firstTabId = browser.activeTabId;
        first.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });

        browser.openInput("https://loading-second.example/page", true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item !== first;
        });
        const second = engineHost.item;
        const secondTabId = browser.activeTabId;
        tryCompare(indicator, "visible", false);

        second.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });
        browser.activateTab(firstTabId);
        tryVerify(function () {
            return engineHost.item === first && indicator.visible;
        });

        first.loading = false;
        browser.activateTab(secondTabId);
        tryVerify(function () {
            return engineHost.item === second && indicator.visible;
        });
        second.loading = false;
        browser.closeTab(secondTabId);
    }

    function test_pageLoadingIndicatorFollowsTheSpaceOnShow() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        const first = openPage("https://loading-personal.example/page");
        const personalSpaceId = browser.activeSpaceId;
        first.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });

        const otherSpaceId = browser.createSpace("Loading indicator");
        verify(browser.switchSpace(otherSpaceId));
        tryVerify(function () {
            return engineHost.item === null && !indicator.visible;
        });

        const second = openPage("https://loading-work.example/page");
        verify(!indicator.visible);
        verify(browser.switchSpace(personalSpaceId));
        tryVerify(function () {
            return engineHost.item === first && indicator.visible;
        });

        first.loading = false;
        verify(browser.switchSpace(otherSpaceId));
        second.loading = false;
        browser.closeTab(browser.activeTabId);
        verify(browser.switchSpace(personalSpaceId));
        verify(browser.deleteSpace(otherSpaceId, "Loading indicator"));
    }

    function test_pageLoadingIndicatorStaysOffBrowserSurfacesAndSiteFullscreen() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        const engine = openPage("https://loading-visibility.example/page");
        engine.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });

        window.settingsOpen = true;
        verify(!indicator.visible);
        window.settingsOpen = false;
        window.shortcutsOpen = true;
        verify(!indicator.visible);
        window.shortcutsOpen = false;

        window.visibility = Window.FullScreen;
        tryCompare(window, "browserFullscreen", true);
        verify(indicator.visible);
        engine.simulateSiteFullscreen("loading-visibility.example");
        tryVerify(function () {
            return engineHost.siteFullscreenActive && !indicator.visible;
        });
        engine.exitSiteFullscreen();
        tryVerify(function () {
            return !engineHost.siteFullscreenActive && indicator.visible;
        });
        window.visibility = Window.Windowed;
        tryCompare(window, "browserFullscreen", false);

        engine.loading = false;
        browser.openInput("about:blank", false);
        tryVerify(function () {
            return window.pagelessViewport && !indicator.visible;
        });
    }

    // The indicator reports a load, not something the reader did, so it moves
    // after a key as after a click.
    function test_pageLoadingIndicatorMovesWhateverTheReaderPressed() {
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        const engine = openPage("https://loading-steady.example/page");
        InputOrigin.pointer = false;
        engine.loading = true;
        tryVerify(function () {
            return indicator.visible;
        });

        compare(indicator.motionEnabled, true);
        compare(indicator.moving, true);
        compare(indicator.enabled, false);
        compare(indicator.Accessible.ignored, true);
        compare(indicator.color, Qt.alpha(window.colors.accent, 0.45));

        engine.stopLoading();
        tryCompare(indicator, "visible", false);
    }

    function test_pageLoadingIndicatorFadesAndCanResumeDuringExit() {
        const indicator = findChild(window.contentItem, "pageLoadingIndicator");
        const engine = openPage("https://loading-motion.example/page");
        engine.loading = true;
        tryCompare(indicator, "opacity", 1);
        tryCompare(indicator, "scale", 1);

        engine.loading = false;
        verify(indicator.visible);
        tryVerify(function () {
            return indicator.opacity < 1 && indicator.opacity > 0;
        });
        engine.loading = true;
        tryCompare(indicator, "opacity", 1);
        tryCompare(indicator, "scale", 1);

        engine.loading = false;
        tryCompare(indicator, "visible", false);
        compare(indicator.opacity, 0);
        tryCompare(indicator, "scale", 0.92);
        verify(!indicator.moving);
    }

    // A site holding the screen is not the reader holding it. The notice names
    // the origin, Escape hands the screen back, and the reader's own fullscreen
    // is untouched throughout.
    function test_siteFullscreenIsDistinctFromBrowserFullscreenAndLeavesWithEscape() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const engine = openPage("https://cinema.example/watch");
        const notice = findChild(window.contentItem, "pageNotice");
        compare(window.browserFullscreen, false);
        compare(window.sidebarCollapsed, false);

        const floatingControls = findChild(window.contentItem, "navigationCluster");
        verify(floatingControls !== null);

        engine.simulateSiteFullscreen("cinema.example");
        tryVerify(function () {
            return engineHost.siteFullscreenActive;
        });
        compare(window.browserFullscreen, false);
        compare(window.sidebarCollapsed, true);
        verify(notice.message.indexOf("cinema.example") !== -1);
        // The page has the whole window: standing the outline aside must not
        // put the floating strip over the page in its place.
        verify(!floatingControls.visible);
        verify(!findChild(window.contentItem, "sidebarRevealEdge").visible);
        mouseMove(window.contentItem, 2, window.height / 2);
        wait(200);
        compare(window.sidebarCollapsed, true);

        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return !engineHost.siteFullscreenActive;
        });
        compare(window.sidebarCollapsed, false);
        compare(window.browserFullscreen, false);

        window.commands.run("fullscreen", -1);
        compare(window.browserFullscreen, true);
        compare(engineHost.siteFullscreenActive, false);
        window.commands.run("fullscreen", -1);
        compare(window.browserFullscreen, false);
    }

    // The window is the desktop's to move too — a menu command or a keyboard
    // shortcut of the platform's own takes it in and out of fullscreen without
    // asking Omaweb. What Omaweb believes has to follow the window, or the next
    // fullscreen command toggles the wrong way and appears to do nothing.
    function test_fullscreenFollowsTheWindowWhateverMovedIt() {
        const engineHost = findChild(window.contentItem, "engineLoader");
        const engine = openPage("https://desktop.example/page");
        compare(window.browserFullscreen, false);

        window.visibility = Window.FullScreen;
        tryCompare(window, "browserFullscreen", true);

        window.visibility = Window.Windowed;
        tryCompare(window, "browserFullscreen", false);

        // And the next command still works from there.
        window.commands.run("fullscreen", -1);
        compare(window.browserFullscreen, true);
        window.commands.run("fullscreen", -1);
        compare(window.browserFullscreen, false);

        // A site holding the screen is told when the screen is taken back by a
        // route it knows nothing about, and the outline comes back with it.
        engine.simulateSiteFullscreen("desktop.example");
        tryVerify(function () {
            return engineHost.siteFullscreenActive;
        });
        compare(window.sidebarCollapsed, true);

        window.visibility = Window.Windowed;
        tryVerify(function () {
            return !engineHost.siteFullscreenActive;
        });
        compare(window.sidebarCollapsed, false);
        compare(window.browserFullscreen, false);
    }

    // A command this engine cannot carry out is listed, unavailable, and says
    // so when it is run. Doing nothing at all would leave the reader to guess
    // whether the key reached the browser.
    function test_everyPageOperationReportsAnEngineThatCannotDoIt() {
        const engine = openPage("https://limited.example/page");
        const notice = findChild(window.contentItem, "pageNotice");
        const bar = findChild(window.contentItem, "findBar");

        compare(testCase.commandEnabled("find"), true);
        compare(testCase.commandEnabled("zoom-in"), true);

        engine.findAvailable = false;
        engine.zoomAvailable = false;
        compare(window.findAvailable, false);
        compare(window.zoomAvailable, false);
        compare(testCase.commandEnabled("find"), false);
        compare(testCase.commandEnabled("find-next"), false);
        compare(testCase.commandEnabled("zoom-out"), false);

        window.commands.run("find", -1);
        compare(bar.open, false);
        tryCompare(notice, "message", "Find is not available");

        window.commands.run("zoom-in", -1);
        compare(browser.activeTabZoom, 1.0);
        tryCompare(notice, "message", "Zoom is not available");

        // Printing needs an engine that can render the page and a desktop with
        // a dialog to answer. The test session has no dialog, so the command is
        // unavailable and names that half.
        compare(window.printingAvailable, false);
        compare(testCase.commandEnabled("print"), false);
        window.commands.run("print", -1);
        tryCompare(notice, "message", "Print is not available");
        compare(notice.detail, "This desktop has no print dialog to answer");

        engine.findAvailable = true;
        engine.zoomAvailable = true;

        // A tab with no page at all is not an engine that lacks something, and
        // the notice does not say it is.
        browser.openInput("about:blank", true);
        const blankTabId = browser.activeTabId;
        tryVerify(function () {
            return window.pagelessViewport;
        });
        compare(testCase.commandEnabled("stop-loading"), false);
        window.commands.run("stop-loading", -1);
        tryCompare(notice, "message", "Stop loading is not available");
        compare(notice.detail, "There is no page here");

        browser.closeTab(blankTabId);
        notice.dismiss();
    }

    // An upgrade HTTPS-only mode could not complete is Omaweb's own page over
    // the engine's error, naming the host and why. Going back is the first
    // answer; loading the plain address once takes the reader there, and
    // "always" also keeps the choice for the site in this Space.
    function test_aFailedHttpsUpgradeIsAskedAboutOnOmawebsOwnPage() {
        const page = findChild(window.contentItem, "httpsOnlyPage");
        verify(page !== null);
        const engine = openPage("https://plain.example/page");
        verify(!page.visible);

        engine.simulateHttpsUpgradeFailure("http://plain.example/page", "unreachable",
                                           "The connection was refused");
        tryVerify(function () {
            return page.visible;
        });
        compare(findChild(page, "httpsOnlyHeadline").text,
                "plain.example could not be reached over HTTPS");
        verify(findChild(page, "httpsOnlyDetail").text.startsWith("The connection was refused. "));
        verify(findChild(page, "httpsOnlyLoadAlways").visible);

        engine.simulateHttpsUpgradeFailure("http://plain.example/page", "downgrade", "");
        compare(findChild(page, "httpsOnlyHeadline").text,
                "plain.example sent this page back to plain HTTP");

        page.loadOnce();
        tryCompare(browser, "activeUrl", "http://plain.example/page");
        tryVerify(function () {
            return !page.visible;
        });
        verify(!browser.plainHttpRemembered(browser.activeSpaceId, "http://plain.example"));

        const again = openPage("https://always.example/");
        again.simulateHttpsUpgradeFailure("http://always.example/", "form", "");
        compare(findChild(page, "httpsOnlyHeadline").text,
                "always.example asked for a form to be sent over plain HTTP");
        page.loadAlways();
        tryCompare(browser, "activeUrl", "http://always.example/");
        verify(browser.plainHttpRemembered(browser.activeSpaceId, "http://always.example"));
    }

    // "Load once" is a load HTTPS-only mode lets through, not a plain address
    // it sends straight back over HTTPS.
    function test_loadingAFailedUpgradeOnceLetsItThrough() {
        const page = findChild(window.contentItem, "httpsOnlyPage");
        const engine = openPage("https://once.example/page");
        verify(httpsOnly.sendsOverHttps(engine.spaceId, "http://once.example/page"));
        engine.simulateHttpsUpgradeFailure("http://once.example/page", "unreachable", "");
        tryVerify(function () {
            return page.visible;
        });
        page.loadOnce();
        tryCompare(browser, "activeUrl", "http://once.example/page");
        verify(!httpsOnly.sendsOverHttps(engine.spaceId, "http://once.example/page"));
    }

    // Retrying a page over plain HTTP from its menu is the reader's choice
    // for that load, which HTTPS-only mode lets through like "Load once".
    function test_retryingOverPlainHttpIsLetThroughHttpsOnlyMode() {
        const engine = openPage("https://retry.example/page");
        const menu = findChild(window.contentItem, "pageMenu");
        verify(httpsOnly.sendsOverHttps(engine.spaceId, "http://retry.example/page"));
        engine.simulateContextMenu({});
        tryVerify(function () {
            return menu.visible;
        });
        const retry = window.pageMenuActions.findIndex(function (row) {
            return row.run === "retry-insecure";
        });
        verify(retry >= 0);
        window.runPageMenu(retry);
        tryCompare(browser, "activeUrl", "http://retry.example/page");
        verify(!httpsOnly.sendsOverHttps(engine.spaceId, "http://retry.example/page"));
    }

    // Site information says when a page came over HTTPS because the mode
    // sent it there.
    function test_siteInformationSaysHttpsOnlyModeUpgradedThePage() {
        const engine = openPage("https://upgraded.example/page");
        engine.arrivedThroughHttpsUpgrade = true;
        const card = openSiteCard("");
        const upgraded = findChild(card, "siteInformationVerdictDetail").text;
        closeSiteCard();
        engine.arrivedThroughHttpsUpgrade = false;
        compare(upgraded, "Encrypted · upgraded from HTTP by HTTPS-only mode");
    }

    // A Private window remembers nothing, so its page offers the plain
    // address for this load and not for good.
    function test_aPrivateWindowsFailedUpgradeOffersOnlyThisLoad() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://private-plain.example/", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        privateEngine.item.simulateHttpsUpgradeFailure("http://private-plain.example/",
                                                       "unreachable", "");
        const page = findChild(privateBrowser.contentItem, "httpsOnlyPage");
        tryVerify(function () {
            return page.visible;
        });
        const offered = [findChild(page, "httpsOnlyLoadOnce").visible, findChild(page,
                                                                                 "httpsOnlyLoadAlways").visible];
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        compare(offered, [true, false]);
    }

    // Screenshot page writes the page area, and nothing of Omaweb's own, into
    // the downloads location at the display's pixel density, and lists it as
    // a finished download. The stand-in page is drawn in stripes 96 points
    // wide, so a probe on each side of the first edge tells the page apart
    // from anything else that could have been captured.
    function test_screenshotPageSavesThePageAreaAsAFinishedDownload() {
        const directory = imageProbe.directory("screenshots");
        const previous = browser.downloadDirectory;
        verify(browser.setDownloadDirectory(directory));
        const engine = openPage("https://capture.example/article");
        // The file is named for the page's title, so the page names itself
        // rather than leaving the tab with whatever it held before.
        engine.pageTitle = "Capture: the article";
        tryCompare(browser, "activeTitle", "Capture: the article");
        engine.blurReviewPattern = true;
        const notice = findChild(window.contentItem, "pageNotice");
        const listed = window.downloads.count;

        verify(window.commands.available("screenshot-page"));
        window.commands.run("screenshot-page", -1);
        tryVerify(function () {
            return imageProbe.files(directory).length === 1;
        });
        const name = imageProbe.files(directory)[0];
        // Named for the page's title, less what a file name cannot carry.
        verify(name.startsWith("Capture the article 2") && name.endsWith(".png"), name);
        const path = directory + "/" + name;
        tryCompare(notice, "message", "Saved " + name);
        compare(window.downloads.count, listed + 1);

        const ratio = engine.Screen.devicePixelRatio > 0 ? engine.Screen.devicePixelRatio : 1;
        const size = imageProbe.size(path);
        compare(size.width, Math.round(engine.width * ratio));
        compare(size.height, Math.round(engine.height * ratio));
        verify(Qt.colorEqual(imageProbe.pixel(path, Math.round(48 * ratio), 4), "#f4f0ff"));
        verify(Qt.colorEqual(imageProbe.pixel(path, Math.round(144 * ratio), 4), "#241832"));

        // Its row in the downloads list shows where it landed.
        window.settingsOpen = true;
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("downloads");
        compare(downloadRole(0, Downloads.PathRole), path);
        compare(downloadRole(0, Downloads.StateRole), "completed");
        const reveal = findChild(settings, "revealDownload-0");
        verify(reveal !== null && reveal.visible);
        reveal.clicked();
        compare(desktopProbe.opened()[desktopProbe.opened().length - 1], directory);
        window.settingsOpen = false;

        engine.blurReviewPattern = false;
        notice.dismiss();
        // The default location need not exist on the machine running this,
        // in which case it cannot be chosen again and the test's stays.
        browser.setDownloadDirectory(previous);
    }

    // Copy screenshot puts the same image on the clipboard and leaves nothing
    // behind in the downloads location.
    function test_copyScreenshotPutsThePageAreaOnTheClipboard() {
        const directory = imageProbe.directory("copied");
        const previous = browser.downloadDirectory;
        verify(browser.setDownloadDirectory(directory));
        const engine = openPage("https://capture.example/copied");
        const notice = findChild(window.contentItem, "pageNotice");
        SystemClipboard.copyText("something the reader already had");

        window.commands.run("copy-screenshot", -1);
        tryCompare(notice, "message", "Screenshot copied");
        const ratio = engine.Screen.devicePixelRatio > 0 ? engine.Screen.devicePixelRatio : 1;
        compare(SystemClipboard.imageSize().width, Math.round(engine.width * ratio));
        compare(SystemClipboard.imageSize().height, Math.round(engine.height * ratio));
        compare(imageProbe.files(directory).length, 0);

        notice.dismiss();
        // The default location need not exist on the machine running this,
        // in which case it cannot be chosen again and the test's stays.
        browser.setDownloadDirectory(previous);
    }

    // Screenshot full page takes the whole document, top to bottom, including
    // what was below the fold, and leaves the reader where they were. The
    // stand-in document is drawn in bands a hundred points tall that alternate
    // colour, so a probe well below the first screenful tells which band it
    // landed in.
    function test_screenshotFullPageTakesWhatWasBelowTheFold() {
        const directory = imageProbe.directory("full-page");
        const previous = browser.downloadDirectory;
        verify(browser.setDownloadDirectory(directory));
        const engine = openPage("https://capture.example/long");
        const notice = findChild(window.contentItem, "pageNotice");
        engine.documentReview = true;
        engine.pageScrollLength = Math.ceil(engine.height * 2.5 / 100) * 100;
        engine.pageScrollOffset = 37;

        window.commands.run("screenshot-full-page", -1);
        tryVerify(function () {
            return imageProbe.files(directory).length === 1;
        });
        const path = directory + "/" + imageProbe.files(directory)[0];
        tryCompare(notice, "message", "Saved " + imageProbe.files(directory)[0]);
        compare(engine.pageScrollOffset, 37);

        const ratio = engine.Screen.devicePixelRatio > 0 ? engine.Screen.devicePixelRatio : 1;
        const size = imageProbe.size(path);
        compare(size.width, Math.round(engine.width * ratio));
        compare(size.height, Math.round(engine.pageScrollLength * ratio));
        // The last band starts below everything the first screenful showed.
        const lastBand = engine.pageScrollLength / 100 - 1;
        verify(lastBand * 100 > engine.height);
        const below = Math.round((lastBand * 100 + 50) * ratio);
        verify(Qt.colorEqual(imageProbe.pixel(path, 4, below), lastBand % 2 === 0 ? "#d9f2e6" :
                                                                                    "#3a1d4f"));
        verify(Qt.colorEqual(imageProbe.pixel(path, 4, below - Math.round(100 * ratio)), lastBand
                             % 2 === 0 ? "#3a1d4f" : "#d9f2e6"));
        verify(Qt.colorEqual(imageProbe.pixel(path, 4, Math.round(50 * ratio)), "#d9f2e6"));

        engine.documentReview = false;
        engine.pageScrollLength = 0;
        engine.pageScrollOffset = 0;
        notice.dismiss();
        browser.setDownloadDirectory(previous);
    }

    // A page taller than a screenshot can hold is reported, naming the limit,
    // and nothing is left behind in the downloads location.
    function test_aPageTooTallForAScreenshotIsReported() {
        const directory = imageProbe.directory("too-tall");
        const previous = browser.downloadDirectory;
        verify(browser.setDownloadDirectory(directory));
        const engine = openPage("https://capture.example/endless");
        const notice = findChild(window.contentItem, "pageNotice");
        engine.pageScrollLength = 40000;

        window.commands.run("screenshot-full-page", -1);
        tryCompare(notice, "message", "Screenshot failed");
        verify(notice.detail.indexOf(String(PageImages.heightLimit)) >= 0, notice.detail);
        compare(imageProbe.files(directory).length, 0);

        engine.pageScrollLength = 0;
        notice.dismiss();
        browser.setDownloadDirectory(previous);
    }

    // In a split the tab the reader is working in is the one captured, at its
    // own pane's size: the striped page while it is active, and the plain one
    // once focus moves across.
    function test_aScreenshotInASplitTakesTheActiveTab() {
        const directory = imageProbe.directory("split");
        const previous = browser.downloadDirectory;
        verify(browser.setDownloadDirectory(directory));
        const engineHost = findChild(window.contentItem, "engineLoader");
        const notice = findChild(window.contentItem, "pageNotice");
        const striped = openPage("https://split-capture.example/striped");
        const stripedTabId = browser.activeTabId;
        browser.openInput("https://split-capture.example/plain", true);
        const plainTabId = browser.activeTabId;
        const plain = engineHost.engines[plainTabId];
        browser.activateTab(stripedTabId);
        verify(browser.addSplit(plainTabId));
        tryCompare(engineHost, "besideEngine", plain);
        compare(engineHost.item, striped);
        striped.blurReviewPattern = true;
        const ratio = striped.Screen.devicePixelRatio > 0 ? striped.Screen.devicePixelRatio : 1;

        window.commands.run("screenshot-page", -1);
        tryVerify(function () {
            return imageProbe.files(directory).length === 1;
        });
        const first = directory + "/" + imageProbe.files(directory)[0];
        tryCompare(notice, "message", "Saved " + imageProbe.files(directory)[0]);
        compare(imageProbe.size(first).width, Math.round(striped.width * ratio));
        verify(Qt.colorEqual(imageProbe.pixel(first, Math.round(48 * ratio), 4), "#f4f0ff"));
        verify(Qt.colorEqual(imageProbe.pixel(first, Math.round(144 * ratio), 4), "#241832"));
        notice.dismiss();

        verify(browser.focusSplitPartner());
        tryCompare(engineHost, "item", plain);
        window.commands.run("screenshot-page", -1);
        tryVerify(function () {
            return imageProbe.files(directory).length === 2;
        });
        const second = imageProbe.files(directory).map(function (name) {
            return directory + "/" + name;
        }).filter(function (path) {
            return path !== first;
        })[0];
        compare(imageProbe.size(second).width, Math.round(plain.width * ratio));
        verify(!Qt.colorEqual(imageProbe.pixel(second, Math.round(48 * ratio), 4), "#f4f0ff") ||
               !Qt.colorEqual(imageProbe.pixel(second, Math.round(144 * ratio), 4), "#241832"));

        striped.blurReviewPattern = false;
        tryVerify(function () {
            return notice.message.indexOf("Saved ") === 0;
        });
        notice.dismiss();
        verify(browser.separateSplit());
        browser.closeTab(plainTabId);
        browser.setDownloadDirectory(previous);
    }

    // A tab with no page has nothing to capture, and the reader is told so
    // rather than handed a picture of Omaweb's own Start page.
    function test_aScreenshotOfNoPageSaysWhy() {
        const notice = findChild(window.contentItem, "pageNotice");
        const startPageReason = "The Start page is not a page to capture";
        browser.openInput("about:blank", true);
        const blankTabId = browser.activeTabId;
        tryVerify(function () {
            return window.pagelessViewport;
        });
        window.commands.run("screenshot-page", -1);
        tryCompare(notice, "message", "Screenshot page is not available");
        compare(notice.detail, startPageReason);
        window.commands.run("copy-screenshot", -1);
        tryCompare(notice, "message", "Copy screenshot is not available");
        compare(notice.detail, startPageReason);
        window.commands.run("screenshot-full-page", -1);
        tryCompare(notice, "message", "Screenshot full page is not available");
        compare(notice.detail, startPageReason);
        window.commands.run("copy-full-page-screenshot", -1);
        tryCompare(notice, "message", "Copy full-page screenshot is not available");
        compare(notice.detail, startPageReason);
        browser.closeTab(blankTabId);
        notice.dismiss();

        // A Space at rest shows the Start page too. With Settings over it
        // there is no page at all.
        const homeSpaceId = browser.activeSpaceId;
        const spaceId = enterRestingSpace("Nothing to capture");
        window.commands.run("screenshot-page", -1);
        tryCompare(notice, "message", "Screenshot page is not available");
        compare(notice.detail, startPageReason);
        notice.dismiss();
        window.settingsOpen = true;
        window.commands.run("screenshot-page", -1);
        tryCompare(notice, "detail", "There is no page here");
        compare(notice.message, "Screenshot page is not available");
        window.settingsOpen = false;
        notice.dismiss();
        leaveSpace(homeSpaceId, spaceId, "Nothing to capture");
    }

    // A render that produced nothing is a failure the reader hears about,
    // rather than a print that quietly never happened.
    function test_printReportsARenderThatProducedNothing() {
        const engine = openPage("https://print.example/invoice");
        const notice = findChild(window.contentItem, "pageNotice");
        engine.printPage("");
        tryCompare(notice, "message", "Printing failed");
        notice.dismiss();
    }

    // Where the engine has no sandboxed PDF viewer the document is downloaded
    // instead, and the missing capability is reported rather than left to be
    // inferred from a page that never appeared.
    function test_pdfWithoutASandboxedViewerIsDownloadedAndReported() {
        const engine = openPage("https://docs.example/start");
        const notice = findChild(window.contentItem, "pageNotice");
        compare(window.inlinePdfViewingAvailable, true);

        browser.openInput("https://docs.example/inline.pdf", false);
        tryVerify(function () {
            return String(browser.activeUrl) === "https://docs.example/inline.pdf";
        });
        compare(notice.showing, false);

        engine.inlinePdfViewingAvailable = false;
        compare(window.inlinePdfViewingAvailable, false);
        browser.openInput("https://docs.example/manual.pdf", false);
        tryCompare(notice, "message", "This engine cannot show PDFs");
        compare(notice.detail, "The document was downloaded instead");

        engine.inlinePdfViewingAvailable = true;
        notice.dismiss();
    }

    function test_aProgramIsNotWrittenDownUntilTheReaderSaysSo() {
        openPage("https://tools.example/releases");
        const host = window.spaceProfileHost;
        clearDownloads();
        const question = findChild(window.contentItem, "downloadQuestionBar");
        verify(question !== null);
        browser.recordOriginInteraction("https://tools.example/releases");

        const started = host.simulateDownloadRequest("https://tools.example/releases",
                                                     "https://tools.example/install.sh",
                                                     "omaweb-test-install.sh", "text/plain");
        compare(started, "");
        tryCompare(question, "open", true);
        verify(question.message.indexOf("https://tools.example") === 0);
        verify(question.message.indexOf("omaweb-test-install.sh") > 0);
        verify(question.detail.indexOf("script") === 0);
        verify(question.detail.indexOf("never runs") > 0);
        compare(question.actions.length, 2);
        compare(question.actions[0].label, "Download");
        compare(question.actions[1].label, "Discard");

        const notice = findChild(window.contentItem, "pageNotice");
        question.actionTriggered(1);
        tryCompare(question, "open", false);
        tryCompare(notice, "message", "Download discarded");
        compare(window.downloads.running, 0);
        notice.dismiss();

        compare(host.simulateDownloadRequest("https://tools.example/releases",
                                             "https://tools.example/install.sh",
                                             "omaweb-test-install.sh", "text/plain"), "");
        tryCompare(question, "open", true);
        question.actionTriggered(0);
        tryCompare(question, "open", false);
        tryCompare(window.downloads, "running", 1);
        const runtimeId = downloadRole(0, Downloads.RuntimeIdRole);
        verify(String(downloadRole(0, Downloads.PathRole)).indexOf("omaweb-test-install.sh") > 0);
        host.simulateDownloadFinished(runtimeId);
        tryVerify(function () {
            return notice.message === "Saved omaweb-test-install.sh";
        });
        notice.dismiss();
    }

    // Private windows forget a download answer when the last of them closes,
    // and the question says so rather than promising a Space's memory.
    function test_aPrivateWindowsDownloadQuestionSaysHowLongItsAnswerLasts() {
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://auto-private.example/page", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        const question = findChild(privateBrowser.contentItem, "downloadQuestionBar");
        compare(window.privateProfileHost.simulateDownloadRequest(
                    "https://auto-private.example/page", "https://auto-private.example/tracker.pdf",
                    "omaweb-test-private.pdf", "application/pdf"), "");
        tryCompare(question, "open", true);
        const detail = question.detail;
        const alwaysEnabled = question.actions[1].enabled !== false;
        question.actionTriggered(2);
        tryCompare(question, "open", false);
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        compare(detail, "omaweb-test-private.pdf · kept until the last Private window closes");
        verify(!alwaysEnabled);
    }

    function test_aPageThatDownloadsByItselfTakesASitePermission() {
        openPage("https://auto.example/page");
        const host = window.spaceProfileHost;
        clearDownloads();
        const question = findChild(window.contentItem, "downloadQuestionBar");
        const notice = findChild(window.contentItem, "pageNotice");

        compare(host.simulateDownloadRequest("https://auto.example/page",
                                             "https://auto.example/tracker.pdf",
                                             "omaweb-test-tracker.pdf", "application/pdf"), "");
        tryCompare(question, "open", true);
        verify(question.message.indexOf("by itself") > 0);
        compare(question.actions.length, 3);
        compare(question.actions[0].label, "Allow once");
        compare(question.actions[1].label, "Always allow");
        compare(question.actions[2].label, "Block");
        compare(question.detail, "omaweb-test-tracker.pdf · remembered for this Space only");

        question.actionTriggered(2);
        tryCompare(question, "open", false);
        compare(window.downloads.running, 0);
        const decisions = browser.sitePermissions("https://auto.example/page");
        let blocked = false;
        for (let index = 0; index < decisions.length; ++index)
            blocked = blocked || (decisions[index].permission === "automatic-downloads"
                                  && decisions[index].decision === 3);
        verify(blocked);

        compare(host.simulateDownloadRequest("https://auto.example/page",
                                             "https://auto.example/tracker.pdf",
                                             "omaweb-test-tracker.pdf", "application/pdf"), "");
        compare(question.open, false);
        tryCompare(notice, "message", "Download refused");
        notice.dismiss();
        verify(browser.resetSitePermissions("https://auto.example/page"));
    }

    function test_anOrdinaryDownloadIsListedWithWhatCanStillBeDoneToIt() {
        openPage("https://files.example/library");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://files.example/library");
        compare(host.downloadDirectory, browser.downloadDirectory);

        const runtimeId = host.simulateDownloadRequest("https://files.example/library",
                                                       "https://files.example/notes.pdf",
                                                       "omaweb-test-notes.pdf", "application/pdf");
        verify(runtimeId.length > 0);
        window.settingsOpen = true;
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("downloads");
        // The list is the model itself, so the row is there without anyone
        // asking for it.
        tryCompare(window.downloads, "count", 1);
        compare(downloadRole(0, Downloads.RuntimeIdRole), runtimeId);
        compare(downloadRole(0, Downloads.StateRole), "in-progress");
        compare(downloadRole(0, Downloads.RunningRole), true);

        const row = findChild(settings, "recordedDownload-0");
        verify(row !== null);
        verify(findChild(settings, "cancelDownload-0").visible);
        verify(!findChild(settings, "revealDownload-0").visible);

        const cancelledBefore = host.cancelledDownloads.length;
        findChild(settings, "cancelDownload-0").clicked();
        tryVerify(function () {
            return host.cancelledDownloads.length === cancelledBefore + 1;
        });
        compare(host.cancelledDownloads[cancelledBefore], runtimeId);

        // Cancelled, the row keeps the Download record its Space made of it.
        tryCompare(window.downloads, "running", 0);
        compare(window.downloads.count, 1);
        verify(String(downloadRole(0, Downloads.RecordIdRole)).length > 0);
        findChild(settings, "forgetDownload-0").clicked();
        tryCompare(window.downloads, "count", 0);

        const interrupted = host.simulateDownloadRequest("https://files.example/library",
                                                         "https://files.example/interrupted.pdf",
                                                         "omaweb-test-interrupted.pdf",
                                                         "application/pdf");
        verify(interrupted.length > 0);
        host.downloadUpdated(interrupted, "interrupted", 5, 100, "network changed");
        tryVerify(function () {
            return downloadRole(0, Downloads.StateRole) === "interrupted";
        });
        verify(findChild(settings, "retryDownload-0").visible);
        const retriedBefore = host.retriedDownloads.length;
        findChild(settings, "retryDownload-0").clicked();
        tryVerify(function () {
            return host.retriedDownloads.length === retriedBefore + 1;
        });
        compare(host.retriedDownloads[retriedBefore], interrupted);

        host.cancelDownload(interrupted);
        tryVerify(function () {
            return String(downloadRole(0, Downloads.RecordIdRole)).length > 0;
        });
        window.downloads.forget(0);
        tryCompare(window.downloads, "count", 0);
        window.settingsOpen = false;
    }

    // A retry with no engine left behind it asks the page for the address
    // again, which is the only thing a Download record can still do.
    function test_aRetriedRecordWithNoEngineBehindItReopensTheAddress() {
        openPage("https://gone.example/library");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://gone.example/library");
        const runtimeId = host.simulateDownloadRequest("https://gone.example/library",
                                                       "https://gone.example/omaweb-test-lost.pdf",
                                                       "omaweb-test-lost.pdf", "application/pdf");
        verify(runtimeId.length > 0);
        host.downloadUpdated(runtimeId, "interrupted", 5, 100, "network changed");
        tryVerify(function () {
            return downloadRole(0, Downloads.StateRole) === "interrupted";
        });

        const table = window.downloadHostsByNamespace;
        delete table[String(host.downloadNamespace)];
        window.downloadHostsByNamespace = table;
        window.downloads.retry(0);
        tryVerify(function () {
            return String(browser.activeUrl) === "https://gone.example/omaweb-test-lost.pdf";
        });
        window.adoptDownloadHost(host);
        host.cancelDownload(runtimeId);
        tryVerify(function () {
            return window.downloads.count === 1;
        });
        window.downloads.forget(0);
    }

    function test_settingsSeparatesRendererIsolationFromTheNetworkService() {
        window.settingsOpen = true;
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("privacy");
        const renderer = findChild(settings, "rendererIsolationRow");
        const network = findChild(settings, "networkServiceRow");
        const baseline = findChild(settings, "securityBaselineRow");
        verify(renderer !== null);
        verify(network !== null);
        verify(baseline !== null);
        verify(renderer.note !== network.note);
        verify(network.title.indexOf("browser process") > 0);
        verify(network.note.indexOf("not") > 0);
        verify(network.note.indexOf("is sandboxed") === -1);
        window.settingsOpen = false;
    }

    // Cancel and retry reach the engine profile that started the download.
    // The window keeps one table from download namespace to profile, and the
    // runtime id already begins with the namespace, so nothing else has to be
    // remembered per download.
    function test_aCancelReachesTheProfileThatStartedTheDownloadAndNotTheOther() {
        openPage("https://two.example/library");
        clearDownloads();
        browser.recordOriginInteraction("https://two.example/library");
        // A second download from one origin while the first is still running is
        // automatic and would be held, so each profile downloads for its own.
        browser.recordOriginInteraction("https://three.example/library");

        const component = Qt.createComponent(engineProfileSource);
        const hosts = [];
        for (let index = 0; index < 2; ++index) {
            const host = component.createObject(window, {
                                                    "downloadDirectory": browser.downloadDirectory,
                                                    "acceptDownloads": true,
                                                    "downloads": window.downloads,
                                                    "downloadNamespace": "lab-" + index
                                                });
            verify(host !== null);
            window.adoptSpaceProfile("lab-" + index, host);
            hosts.push(host);
        }

        const first = hosts[0].simulateDownloadRequest("https://two.example/library",
                                                       "https://two.example/omaweb-test-one.pdf",
                                                       "omaweb-test-one.pdf", "application/pdf");
        const second = hosts[1].simulateDownloadRequest("https://three.example/library",
                                                        "https://three.example/omaweb-test-two.pdf",
                                                        "omaweb-test-two.pdf", "application/pdf");
        verify(first.indexOf("lab-0:") === 0);
        verify(second.indexOf("lab-1:") === 0);
        tryCompare(window.downloads, "running", 2);

        window.downloads.cancel(downloadRowFor(first));
        tryCompare(hosts[0].cancelledDownloads, "length", 1);
        compare(hosts[0].cancelledDownloads[0], first);
        compare(hosts[1].cancelledDownloads.length, 0);

        hosts[1].downloadUpdated(second, "interrupted", 5, 100, "network changed");
        tryVerify(function () {
            return downloadRole(downloadRowFor(second), Downloads.StateRole) === "interrupted";
        });
        window.downloads.retry(downloadRowFor(second));
        tryCompare(hosts[1].retriedDownloads, "length", 1);
        compare(hosts[1].retriedDownloads[0], second);
        compare(hosts[0].retriedDownloads.length, 0);

        hosts[1].cancelDownload(second);
        tryCompare(window.downloads, "running", 0);
        clearDownloads();
        const table = window.downloadHostsByNamespace;
        for (let index = 0; index < hosts.length; ++index) {
            delete table["lab-" + index];
            hosts[index].destroy();
        }
        window.downloadHostsByNamespace = table;
        findChild(window.contentItem, "pageNotice").dismiss();
    }

    // The mark's own rules, asked of a stand-in watch. What is behind it — the
    // version comparison, the once-a-day question, the dismissal — is answered
    // in C++ and checked in tst_releasecheck.
    function test_theFooterMarksAReleaseTheReaderHasNotSeen() {
        const outline = findChild(window.contentItem, "sidebar");
        const mark = findChild(outline, "releaseMark");
        verify(mark !== null);
        // No watch at all, as the UI lab runs. The mark is not shown and asks
        // the watch nothing.
        tryCompare(mark, "visible", false);

        const watch = Qt.createQmlObject('import QtQuick\nQtObject {\n'
                                         + '    property bool announcing: true\n'
                                         + '    property string release: "v9.9.9"\n'
                                         + '    property string instruction: "Upgrade with the rest of the system: pacman -Syu"\n'
                                         + '    property url notes: "https://example.invalid/releases/tag/v9.9.9"\n'
                                         + '    property int dismissed: 0\n'
                                         + '    function dismiss() { dismissed = dismissed + 1; }\n'
                                         + '}', mark, "releaseMarkStandIn");
        mark.watch = watch;
        tryCompare(mark, "visible", true);
        compare(mark.Accessible.name, "Omaweb v9.9.9 is out");
        compare(mark.Accessible.description, "Upgrade with the rest of the system: pacman -Syu");
        // The reader looking straight at it gets the same answer the screen
        // reader does. Omaweb's chrome draws no tooltip unless it is asked to,
        // so nothing but this says the instruction was asked for.
        const tip = findChild(mark, "releaseMarkToolTip");
        verify(tip !== null);
        compare(tip.text, "Omaweb v9.9.9 is out\nUpgrade with the rest of the system: pacman -Syu");
        // Not the icon face. The mark draws its glyph in Material Symbols, and
        // the kit's button hands its own `fontFamily` to the tooltip it draws
        // itself, so a sentence there arrives as glyph soup. Compared against
        // the mark's own icon font rather than against a named family, because
        // what matters is that the two are not the same one.
        verify(tip.fontFamily.length > 0);
        verify(tip.fontFamily !== mark.iconFontFamily);

        // Between knowing the release and knowing who owns the binary there is
        // no instruction to give, and the tooltip is the version alone.
        watch.instruction = "";
        tryCompare(tip, "text", "Omaweb v9.9.9 is out");
        watch.instruction = "Upgrade with the rest of the system: pacman -Syu";

        // A Private window says nothing about this installation, and its age is
        // something about this installation.
        mark.privateWindow = true;
        tryCompare(mark, "visible", false);
        mark.privateWindow = false;
        tryCompare(mark, "visible", true);

        // Selecting it opens the notes and has told the reader, so the mark
        // goes. Nothing is installed by the browser either way.
        mark.clicked();
        compare(watch.dismissed, 1);

        mark.watch = null;
        tryCompare(mark, "visible", false);
        watch.destroy();
    }

    // The window these tests share asked the application's watch for the
    // notes an upgrade owes the reader as it started, and only it has: the
    // only other windows here are Private ones. What the watch opens, where,
    // and only once, is checked in tst_releasewatch.
    function test_aStartingWindowAsksForTheUpgradeNotes() {
        compare(releaseWatch.asked, 1);
    }

    // A Private window says nothing about this installation, and leaves the
    // notes to an ordinary window.
    function test_aPrivateWindowNeverAsksForTheUpgradeNotes() {
        const asked = releaseWatch.asked;
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        compare(releaseWatch.asked, asked);
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    function test_theFooterMarksTheDownloadsStillRunning() {
        openPage("https://mirror.example/library");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://mirror.example/library");
        const outline = findChild(window.contentItem, "sidebar");
        const mark = findChild(outline, "downloadMark");
        verify(mark !== null);
        tryCompare(mark, "visible", false);

        const first = host.simulateDownloadRequest("https://mirror.example/library",
                                                   "https://mirror.example/omaweb-test-atlas.pdf",
                                                   "omaweb-test-atlas.pdf", "application/pdf");
        verify(first.length > 0);
        tryCompare(mark, "visible", true);
        compare(mark.running, 1);
        host.simulateDownloadProgress(first, 250, 1000);
        tryCompare(mark, "fraction", 0.25);

        openPage("https://archive.example/set");
        browser.recordOriginInteraction("https://archive.example/set");
        const second = host.simulateDownloadRequest("https://archive.example/set",
                                                    "https://archive.example/omaweb-test-plates.pdf",
                                                    "omaweb-test-plates.pdf", "application/pdf");
        verify(second.length > 0);
        tryCompare(mark, "running", 2);
        host.simulateDownloadProgress(second, 250, 3000);
        tryCompare(mark, "fraction", 0.125);

        host.simulateDownloadFinished(first);
        host.simulateDownloadFinished(second);
        tryCompare(mark, "running", 0);
        const notice = findChild(window.contentItem, "pageNotice");
        notice.dismiss();
        tryCompare(mark, "visible", false);
    }

    function test_theFooterMarkNeverInventsAPercentage() {
        openPage("https://stream.example/feed");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://stream.example/feed");
        const mark = findChild(findChild(window.contentItem, "sidebar"), "downloadMark");
        tryCompare(mark, "visible", false);

        const measured = host.simulateDownloadRequest("https://stream.example/feed",
                                                      "https://stream.example/omaweb-test-reel.pdf",
                                                      "omaweb-test-reel.pdf", "application/pdf");
        verify(measured.length > 0);
        host.simulateDownloadProgress(measured, 400, 800);
        tryCompare(mark, "fraction", 0.5);
        compare(mark.measured, true);
        compare(mark.Accessible.name, "1 download · 50%");
        const fill = findChild(mark, "downloadMarkFill");
        const track = findChild(mark, "downloadMarkTrack");
        verify(fill.width > 0);
        verify(fill.width < track.width);

        openPage("https://tape.example/feed");
        browser.recordOriginInteraction("https://tape.example/feed");
        const unmeasured = host.simulateDownloadRequest("https://tape.example/feed",
                                                        "https://tape.example/omaweb-test-tape.pdf",
                                                        "omaweb-test-tape.pdf", "application/pdf");
        verify(unmeasured.length > 0);
        host.simulateDownloadProgress(unmeasured, 100, 0);
        tryCompare(mark, "measured", false);
        compare(mark.fraction, -1);
        compare(fill.width, 0);
        verify(track.visible);
        compare(mark.Accessible.name, "2 downloads · size unknown");

        host.simulateDownloadFinished(measured);
        host.simulateDownloadFinished(unmeasured);
        tryCompare(mark, "running", 0);
        findChild(window.contentItem, "pageNotice").dismiss();
        tryCompare(mark, "visible", false);
    }

    function test_theFooterMarkHoldsAFinishedDownloadThenLeaves() {
        openPage("https://vault.example/box");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://vault.example/box");
        const outline = findChild(window.contentItem, "sidebar");
        const mark = findChild(outline, "downloadMark");
        tryCompare(mark, "visible", false);

        const saved = host.simulateDownloadRequest("https://vault.example/box",
                                                   "https://vault.example/omaweb-test-ledger.pdf",
                                                   "omaweb-test-ledger.pdf", "application/pdf");
        verify(saved.length > 0);
        tryCompare(mark, "running", 1);
        host.simulateDownloadFinished(saved);
        tryCompare(mark, "running", 0);
        compare(mark.holding, true);
        compare(mark.visible, true);
        compare(mark.Accessible.name, "1 download finished");
        const fill = findChild(mark, "downloadMarkFill");
        const track = findChild(mark, "downloadMarkTrack");
        compare(fill.width, track.width);
        mouseMove(mark, mark.width / 2, mark.height / 2);
        tryCompare(mark, "detailRequested", true);
        compare(findChild(outline, "downloadDetail").visible, false);
        mouseMove(outline, 4, 4);
        tryCompare(mark, "detailRequested", false);
        // The mark stands for exactly as long as the saved-file notice does.
        compare(mark.visible, true);
        findChild(window.contentItem, "pageNotice").dismiss();
        tryCompare(mark, "holding", false);
        tryCompare(mark, "visible", false);

        openPage("https://crate.example/box");
        browser.recordOriginInteraction("https://crate.example/box");
        const dropped = host.simulateDownloadRequest("https://crate.example/box",
                                                     "https://crate.example/omaweb-test-crate.pdf",
                                                     "omaweb-test-crate.pdf", "application/pdf");
        verify(dropped.length > 0);
        tryCompare(mark, "visible", true);
        window.downloads.cancel(0);
        tryCompare(mark, "running", 0);
        compare(mark.holding, false);
        compare(mark.visible, false);
    }

    function test_theFooterMarkOpensTheDownloadsItStandsFor() {
        openPage("https://depot.example/box");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://depot.example/box");
        const mark = findChild(findChild(window.contentItem, "sidebar"), "downloadMark");
        tryCompare(mark, "visible", false);
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = 0;

        const running = host.simulateDownloadRequest("https://depot.example/box",
                                                     "https://depot.example/omaweb-test-depot.pdf",
                                                     "omaweb-test-depot.pdf", "application/pdf");
        verify(running.length > 0);
        tryCompare(mark, "visible", true);
        mark.clicked();
        tryCompare(window, "settingsOpen", true);
        compare(settings.sections[settings.section], "downloads");

        window.settingsOpen = false;
        settings.section = 0;
        verify(window.commands.run("downloads"));
        tryCompare(window, "settingsOpen", true);
        compare(settings.sections[settings.section], "downloads");
        const listed = window.commands.actions().filter(function (action) {
            return action.command === "downloads";
        });
        compare(listed.length, 1);
        compare(listed[0].title, "Downloads");
        verify(listed[0].enabled);
        window.settingsOpen = false;

        host.simulateDownloadFinished(running);
        findChild(window.contentItem, "pageNotice").dismiss();
        tryCompare(mark, "visible", false);
    }

    function test_theFooterMarkNamesEachDownloadOnDemand() {
        openPage("https://atlas.example/box");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://atlas.example/box");
        const outline = findChild(window.contentItem, "sidebar");
        const mark = findChild(outline, "downloadMark");
        tryCompare(mark, "visible", false);
        const detail = findChild(outline, "downloadDetail");
        verify(detail !== null);
        mouseMove(outline, 4, 4);
        compare(detail.visible, false);

        const first = host.simulateDownloadRequest("https://atlas.example/box",
                                                   "https://atlas.example/omaweb-test-north.pdf",
                                                   "omaweb-test-north.pdf", "application/pdf");
        openPage("https://globe.example/box");
        browser.recordOriginInteraction("https://globe.example/box");
        const second = host.simulateDownloadRequest("https://globe.example/box",
                                                    "https://globe.example/omaweb-test-south.pdf",
                                                    "omaweb-test-south.pdf", "application/pdf");
        verify(first.length > 0);
        verify(second.length > 0);
        host.simulateDownloadProgress(first, 600, 1000);
        host.simulateDownloadProgress(second, 100, 0);
        tryCompare(mark, "running", 2);

        compare(detail.visible, false);
        mouseMove(mark, mark.width / 2, mark.height / 2);
        tryCompare(detail, "visible", true);
        // Newest first, as the list itself is.
        const southRow = findChild(detail, "downloadDetail-0");
        const northRow = findChild(detail, "downloadDetail-1");
        verify(northRow !== null);
        verify(southRow !== null);
        compare(northRow.name, "omaweb-test-north.pdf");
        compare(northRow.progressLabel, "60%");
        compare(southRow.name, "omaweb-test-south.pdf");
        compare(southRow.progressLabel, "size unknown");

        mouseMove(outline, 4, 4);
        tryCompare(detail, "visible", false);
        window.requestActivate();
        mark.forceActiveFocus();
        tryCompare(detail, "visible", true);

        findChild(outline, "settingsButton").forceActiveFocus();
        tryCompare(detail, "visible", false);

        host.simulateDownloadFinished(first);
        host.simulateDownloadFinished(second);
        findChild(window.contentItem, "pageNotice").dismiss();
        tryCompare(mark, "visible", false);
    }

    function test_theFooterMarkDropsADownloadThatStoppedShort() {
        openPage("https://cliff.example/box");
        const host = window.spaceProfileHost;
        clearDownloads();
        browser.recordOriginInteraction("https://cliff.example/box");
        const outline = findChild(window.contentItem, "sidebar");
        const mark = findChild(outline, "downloadMark");
        tryCompare(mark, "visible", false);

        const cut = host.simulateDownloadRequest("https://cliff.example/box",
                                                 "https://cliff.example/omaweb-test-cliff.pdf",
                                                 "omaweb-test-cliff.pdf", "application/pdf");
        verify(cut.length > 0);
        host.simulateDownloadProgress(cut, 300, 1000);
        tryCompare(mark, "fraction", 0.3);

        host.downloadUpdated(cut, "interrupted", 300, 1000, "network changed");
        tryCompare(mark, "running", 0);
        compare(mark.holding, false);
        compare(mark.visible, false);
        // Interrupted is neither running nor finished, so it counts towards
        // neither and stays in the list with a retry still open to it.
        compare(downloadRole(downloadRowFor(cut), Downloads.StateRole), "interrupted");

        openPage("https://ledge.example/box");
        browser.recordOriginInteraction("https://ledge.example/box");
        const later = host.simulateDownloadRequest("https://ledge.example/box",
                                                   "https://ledge.example/omaweb-test-ledge.pdf",
                                                   "omaweb-test-ledge.pdf", "application/pdf");
        verify(later.length > 0);
        tryCompare(mark, "running", 1);
        host.simulateDownloadFinished(later);
        tryCompare(mark, "holding", true);
        compare(mark.Accessible.name, "1 download finished");
        findChild(window.contentItem, "pageNotice").dismiss();
        tryCompare(mark, "visible", false);

        host.cancelDownload(cut);
        tryVerify(function () {
            return downloadRole(downloadRowFor(cut), Downloads.StateRole) === "cancelled";
        });
    }

    // Form history (#329). Each test keeps its values under a field name of
    // its own, because every test shares the window and its Space's store.
    function formSuggestions() {
        return findChild(window.contentItem, "formSuggestions");
    }

    function suggestionTexts() {
        const list = formSuggestions();
        const texts = [];
        for (let row = 0; row < list.count; ++row)
            texts.push(findChild(list, "suggestionRow" + row).value);
        return texts;
    }

    function submitForm(engine, name, values) {
        for (const value of values) {
            engine.simulateFormSubmit([
                                          {
                                              "name": name,
                                              "value": value
                                          }
                                      ]);
            wait(3);
        }
    }

    function test_aSubmittedValueIsOfferedOnTheNextFocusOfItsField() {
        const engine = openPage("https://forms.example/");
        submitForm(engine, "remembered-city", ["Oulu", "Turku"]);
        const list = formSuggestions();
        verify(list !== null);
        verify(!list.shown);

        engine.simulateFormFieldFocus("remembered-city", "", 100, 200, 240, 30);
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["Turku", "Oulu"]);
        // At least the field's width, under it.
        const field = engine.mapToItem(list.parent, 100, 200, 240, 30);
        verify(list.width >= field.width);
        compare(Math.round(list.x), Math.round(field.x));
        verify(list.y >= field.y + field.height);

        // Another field's name has nothing to offer.
        engine.simulateFormFieldFocus("remembered-street", "", 100, 200, 240, 30);
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateFormFieldBlur();
    }

    // The list sits above a field it would not fit under.
    function test_theSuggestionsSitAboveAFieldWithNoRoomBelow() {
        const engine = openPage("https://forms-low.example/");
        submitForm(engine, "low-field", ["one", "two", "three"]);
        engine.simulateFormFieldFocus("low-field", "", 100, engine.height - 40, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        const field = engine.mapToItem(list.parent, 100, engine.height - 40, 240, 30);
        verify(list.y + list.height <= field.y);
        engine.simulateFormFieldBlur();
    }

    // Typing narrows the list to the values that start with what was typed,
    // most recent first and six at most. The typed part is drawn regular and
    // the rest bold, as an Engine suggestion is, and a value the field already
    // holds is not offered back to it.
    function test_typingFiltersTheSuggestionsToThoseThatStartWithIt() {
        const engine = openPage("https://forms-typed.example/");
        // "scalp" holds "alp" without starting with it.
        submitForm(engine, "typed-field", ["scalp", "alpha", "beta", "Alder", "almond", "alto", "alps",
                                           "algae", "alley"]);
        engine.simulateFormFieldFocus("typed-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["alley", "algae", "alps", "alto", "almond", "Alder"]);

        engine.simulateFormFieldInput("al");
        tryCompare(list, "count", 6);
        compare(suggestionTexts(), ["alley", "algae", "alps", "alto", "almond", "Alder"]);
        engine.simulateFormFieldInput("alp");
        tryVerify(function () {
            return list.count === 2;
        });
        compare(suggestionTexts(), ["alps", "alpha"]);
        compare(findChild(findChild(list, "suggestionRow0"), "suggestionText").text, "alp<b>s</b>");
        engine.simulateFormFieldInput("alps");
        tryVerify(function () {
            return list.count === 0 && !list.shown;
        });
        engine.simulateFormFieldInput("alpz");
        verify(!list.shown);
        engine.simulateFormFieldInput("b");
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["beta"]);
        engine.simulateFormFieldBlur();
    }

    // The arrow keys walk the rows and Enter fills the field with the one
    // highlighted. The page hands over Enter only while a row is.
    function test_theArrowKeysWalkTheSuggestionsAndEnterFillsTheField() {
        const engine = openPage("https://forms-keys.example/");
        submitForm(engine, "keyed-field", ["first", "second", "third"]);
        engine.simulateFormFieldFocus("keyed-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown && engine.formSuggestionsShown;
        });
        compare(list.highlighted, -1);
        verify(!engine.formSuggestionHighlighted);
        verify(!engine.simulateFormKey("accept"));

        verify(engine.simulateFormKey("down"));
        compare(list.highlighted, 0);
        tryVerify(function () {
            return engine.formSuggestionHighlighted;
        });
        engine.simulateFormKey("down");
        engine.simulateFormKey("down");
        engine.simulateFormKey("down");
        compare(list.highlighted, 2);
        engine.simulateFormKey("up");
        compare(list.highlighted, 1);
        verify(engine.simulateFormKey("accept"));
        compare(engine.formField.value, "second");
        tryVerify(function () {
            return !list.shown && !engine.formSuggestionsShown;
        });
        engine.simulateFormFieldBlur();
    }

    // Escape closes the list until the field is focused again.
    function test_escapeClosesTheSuggestionsUntilTheFieldIsFocusedAgain() {
        const engine = openPage("https://forms-escape.example/");
        submitForm(engine, "escaped-field", ["kept", "known"]);
        engine.simulateFormFieldFocus("escaped-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        verify(engine.simulateFormKey("escape"));
        tryVerify(function () {
            return !list.shown && !engine.formSuggestionsShown;
        });
        engine.simulateFormFieldInput("k");
        wait(50);
        verify(!list.shown);
        verify(!engine.simulateFormKey("escape"));

        engine.simulateFormFieldFocus("escaped-field", "k", 100, 200, 240, 30);
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["known", "kept"]);
        engine.simulateFormFieldBlur();
        tryVerify(function () {
            return !list.shown;
        });
    }

    // Closing the list on one page says nothing about a field on another,
    // whose page counts its focuses from the start again.
    function test_aListClosedOnOnePageStillOpensOnAnother() {
        const first = openPage("https://forms-first.example/");
        submitForm(first, "shared-field", ["shared"]);
        first.simulateFormFieldFocus("shared-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        first.simulateFormKey("escape");
        tryVerify(function () {
            return !list.shown;
        });
        first.simulateFormFieldBlur();

        const engineHost = findChild(window.contentItem, "engineLoader");
        browser.openInput("https://forms-second.example/", true);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item !== first
                    && engineHost.item.currentUrl.toString() === "https://forms-second.example/";
        });
        // Focused until it has counted as far as the page the list was closed
        // on, so the two focuses share a number.
        const second = engineHost.item;
        do {
            second.simulateFormFieldFocus("shared-field", "", 100, 200, 240, 30);
        } while (second.formFieldSerial < first.formFieldSerial)
        compare(second.formField.serial, first.formFieldSerial);
        tryVerify(function () {
            return list.shown;
        });
        second.simulateFormFieldBlur();
    }

    // Shift+Delete forgets the highlighted value, in this Space and for good.
    function test_shiftDeleteForgetsTheHighlightedSuggestion() {
        const engine = openPage("https://forms-forget.example/");
        submitForm(engine, "forgotten-field", ["keep me", "forget me"]);
        engine.simulateFormFieldFocus("forgotten-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateFormKey("down");
        verify(engine.simulateFormKey("forget"));
        compare(suggestionTexts(), ["keep me"]);
        compare(browser.formHistory(engine.spaceId, "forgotten-field"), ["keep me"]);
        compare(list.highlighted, 0);
        engine.simulateFormKey("forget");
        tryVerify(function () {
            return !list.shown;
        });
        compare(browser.formHistory(engine.spaceId, "forgotten-field"), []);
        engine.simulateFormFieldBlur();
    }

    // A press on a row fills the field with it, and the press does not take
    // the keyboard from the page.
    function test_aPressOnASuggestionFillsTheField() {
        const engine = openPage("https://forms-press.example/");
        submitForm(engine, "pressed-field", ["pressed", "passed"]);
        engine.simulateFormFieldFocus("pressed-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        const row = findChild(list, "suggestionRow1");
        const point = row.mapToItem(window.contentItem, row.width / 2, row.height / 2);
        mousePress(row, row.width / 2, row.height / 2);
        compare(engine.formField.value, "pressed");
        // The row is gone with the list by the time the button comes up.
        mouseRelease(window.contentItem, point.x, point.y);
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateFormFieldBlur();
    }

    // What an Agent types into a page is not the reader's, so its forms are
    // not remembered, even in a Space the reader can open.
    function test_anAgentsFormsAreNotRemembered() {
        const drive = driveAnAgentSpace(false);
        const engine = findChild(window.contentItem, "engineLoader").item;
        submitForm(engine, "agent-field", ["typed by an Agent"]);
        compare(browser.formHistory(drive.spaceId, "agent-field"), []);
        endAgentDrive(drive);
    }

    // A Glance remembers and offers in its Space like the tab beneath it,
    // and the Escape that would close the Glance closes its list first.
    function test_aGlanceOffersItsFieldsAndEscapeClosesTheListFirst() {
        openPage("https://forms-glance.example/");
        const glance = openGlance("https://forms-glance.example/glanced");
        const engine = window.glanceEngine;
        submitForm(engine, "glance-field", ["glanced"]);
        compare(browser.formHistory(engine.spaceId, "glance-field"), ["glanced"]);
        engine.simulateFormFieldFocus("glance-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        // The panel holds the keyboard, so both the window's Escape and the
        // Glance's own are in reach of the key.
        glance.forceActiveFocus();
        keyClick(Qt.Key_Escape);
        wait(50);
        verify(window.glanceEngine !== null);
        engine.simulateFormKey("escape");
        tryVerify(function () {
            return !list.shown;
        });
        keyClick(Qt.Key_Escape);
        tryVerify(function () {
            return window.glanceEngine === null;
        });
    }

    // A pointer over a row shows where a press would land and nothing more:
    // Enter goes on submitting the form until the keyboard picks a row.
    function test_aPointerOverARowLeavesEnterToThePage() {
        const engine = openPage("https://forms-hover.example/");
        submitForm(engine, "hovered-field", ["under the pointer"]);
        engine.simulateFormFieldFocus("hovered-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        const row = findChild(list, "suggestionRow0");
        mouseMove(row, row.width / 2, row.height / 2);
        wait(50);
        compare(list.highlighted, -1);
        verify(!engine.formSuggestionHighlighted);
        verify(!engine.simulateFormKey("accept"));
        engine.simulateFormFieldBlur();
    }

    // A field scrolled out of the page has no list, which would otherwise
    // stand over the chrome.
    function test_aFieldOutsideThePageHasNoList() {
        const engine = openPage("https://forms-outside.example/");
        submitForm(engine, "outside-field", ["out of sight"]);
        engine.simulateFormFieldFocus("outside-field", "", 100, -60, 240, 30);
        const list = formSuggestions();
        wait(50);
        verify(!list.shown);
        engine.simulateFormFieldFocus("outside-field", "", 100, 200, 240, 30);
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateFormFieldBlur();
    }

    // A Private window offers nothing and keeps nothing.
    function test_aPrivateWindowNeitherOffersNorKeepsFormHistory() {
        const engine = openPage("https://forms-private.example/");
        submitForm(engine, "private-field", ["from a Space"]);
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://forms-private.example/", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        const page = privateEngine.item;
        page.simulateFormSubmit([
                                    {
                                        "name": "private-field",
                                        "value": "from a Private window"
                                    }
                                ]);
        page.simulateFormFieldFocus("private-field", "", 100, 200, 240, 30);
        const list = findChild(privateBrowser.contentItem, "formSuggestions");
        wait(100);
        verify(!list.shown);
        compare(privateBrowser.windowBrowser.formHistory(page.spaceId, "private-field"), []);
        compare(browser.formHistory(engine.spaceId, "private-field"), ["from a Space"]);
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
    }

    // The list follows its engine away when the field is left, and is told
    // so before its field is: refreshing then must not read the engine that
    // is gone.
    function test_leavingAFieldWithItsListOpenRaisesNoError() {
        const engine = openPage("https://forms-left.example/");
        submitForm(engine, "left-field", ["left behind"]);
        engine.simulateFormFieldFocus("left-field", "", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        failOnWarning(/TypeError/);
        engine.simulateFormFieldBlur();
        tryVerify(function () {
            return !list.shown;
        });
        wait(50);
    }

    // Addresses (#338). They are the reader's, so every test saves its own
    // and removes them again: the window's store is shared.
    function saveAddresses(addresses) {
        const ids = [];
        for (const address of addresses) {
            const id = browser.saveAddress(address);
            verify(id.length > 0);
            ids.push(id);
        }
        return ids;
    }

    function removeAddresses(ids) {
        for (const id of ids)
            browser.removeAddress(id);
    }

    readonly property var homeAddress: ({
                                            "name": "Ville Kivelä",
                                            "street": "Rantakatu 1",
                                            "postalCode": "90100",
                                            "city": "Oulu",
                                            "country": "Finland",
                                            "phone": "+358 40 123 4567",
                                            "email": "ville@home.example"
                                        })
    readonly property var workAddress: ({
                                            "name": "Ville Kivelä",
                                            "street": "Tehtaankatu 5",
                                            "city": "Helsinki"
                                        })

    // A field with an address token offers the saved addresses first, by
    // name with the street and city under it, then its form history below a
    // divider. Accepting an address fills the form from it rather than the
    // field with a value.
    function test_addressesAreOfferedAboveFormHistoryAndFillTheForm() {
        const ids = saveAddresses([homeAddress, workAddress]);
        const engine = openPage("https://addresses.example/");
        submitForm(engine, "address-city", ["Kempele"]);
        engine.simulateFormFieldFocus("address-city", "", 100, 200, 240, 30, "address-level2");
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["Ville Kivelä", "Ville Kivelä", "Kempele"]);
        compare(findChild(list, "suggestionRow0").detail, "Rantakatu 1, Oulu");
        compare(findChild(list, "suggestionRow1").detail, "Tehtaankatu 5, Helsinki");
        compare(findChild(list, "suggestionRow2").detail, "");
        verify(!findChild(list, "suggestionRow0").divided);
        verify(!findChild(list, "suggestionRow1").divided);
        verify(findChild(list, "suggestionRow2").divided);
        compare(findChild(list, "suggestionRow1").Accessible.name,
                "Ville Kivelä, Tehtaankatu 5, Helsinki");

        engine.filledAddress = null;
        engine.simulateFormKey("down");
        engine.simulateFormKey("down");
        verify(engine.simulateFormKey("accept"));
        compare(engine.filledAddress.street, "Tehtaankatu 5");
        compare(engine.filledAddress.city, "Helsinki");
        compare(engine.formField.value, "");
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateFormFieldBlur();
        removeAddresses(ids);
    }

    // Typing narrows the addresses to those whose value for the field starts
    // with it, and a field without an address token is offered none.
    function test_typingNarrowsTheAddressesByTheFieldsOwnValue() {
        const ids = saveAddresses([homeAddress, workAddress]);
        const engine = openPage("https://addresses-typed.example/");
        engine.simulateFormFieldFocus("address-street", "", 100, 200, 240, 30, "street-address");
        const list = formSuggestions();
        tryVerify(function () {
            return list.count === 2;
        });
        engine.simulateFormFieldInput("teh");
        tryVerify(function () {
            return list.count === 1;
        });
        compare(findChild(list, "suggestionRow0").detail, "Tehtaankatu 5, Helsinki");
        engine.simulateFormFieldInput("x");
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateFormFieldFocus("address-street", "", 100, 200, 240, 30, "");
        wait(50);
        verify(!list.shown);
        engine.simulateFormFieldBlur();
        removeAddresses(ids);
    }

    // A press on an address fills the form from it.
    function test_aPressOnAnAddressFillsTheForm() {
        const ids = saveAddresses([homeAddress]);
        const engine = openPage("https://addresses-press.example/");
        engine.simulateFormFieldFocus("", "", 100, 200, 240, 30, "email");
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        engine.filledAddress = null;
        const row = findChild(list, "suggestionRow0");
        mousePress(row, row.width / 2, row.height / 2);
        compare(engine.filledAddress.email, "ville@home.example");
        mouseRelease(row, row.width / 2, row.height / 2);
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateFormFieldBlur();
        removeAddresses(ids);
    }

    // A Private window has no addresses to offer, though the reader saved some.
    function test_aPrivateWindowOffersNoAddress() {
        const ids = saveAddresses([homeAddress]);
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://addresses-private.example/", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        const page = privateEngine.item;
        page.simulateFormFieldFocus("", "", 100, 200, 240, 30, "address-level2");
        const list = findChild(privateBrowser.contentItem, "formSuggestions");
        wait(100);
        verify(!list.shown);
        compare(privateBrowser.windowBrowser.addresses(), []);
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        removeAddresses(ids);
    }

    // Settings lists the saved addresses by name, street and city, and adds,
    // edits and removes them with the fields in place under the list.
    function test_settingsAddsEditsAndRemovesAnAddress() {
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("addresses");
        const field = function (name) {
            return findChild(settings, name);
        };
        const rowTitles = function () {
            const titles = [];
            const list = field("addressList");
            for (let index = 0; index < list.count; ++index)
                titles.push(list.itemAt(index).title);
            return titles;
        };
        let saved = [];
        try {
            verify(settings.sections.indexOf("addresses") >= 0);
            compare(field("addressList").count, 0);
            verify(field("noAddresses").visible);
            verify(!field("addressName").visible);

            field("addAddressButton").clicked();
            verify(field("addressName").visible);
            verify(!field("saveAddressButton").enabled);
            field("addressName").text = "Ville Kivelä";
            field("addressStreet").text = "Rantakatu 1";
            field("addressPostalCode").text = "90100";
            field("addressCity").text = "Oulu";
            field("addressCountry").text = "Finland";
            field("addressPhone").text = "+358 40 123 4567";
            field("addressEmail").text = "ville@home.example";
            verify(field("saveAddressButton").enabled);
            field("saveAddressButton").clicked();
            saved = browser.addresses();
            compare(saved.length, 1);
            compare(saved[0].postalCode, "90100");
            compare(saved[0].email, "ville@home.example");
            compare(rowTitles(), ["Ville Kivelä · Rantakatu 1, Oulu"]);
            verify(!field("addressName").visible);
            verify(!field("noAddresses").visible);

            // Edit opens the fields with the address in them.
            findChild(field("addressList").itemAt(0), "editAddressButton").clicked();
            compare(field("addressName").text, "Ville Kivelä");
            compare(field("addressPhone").text, "+358 40 123 4567");
            field("addressCity").text = "Kempele";
            field("saveAddressButton").clicked();
            saved = browser.addresses();
            compare(saved.length, 1);
            compare(saved[0].city, "Kempele");
            compare(rowTitles(), ["Ville Kivelä · Rantakatu 1, Kempele"]);

            // Cancel leaves the address as it was.
            findChild(field("addressList").itemAt(0), "editAddressButton").clicked();
            field("addressCity").text = "Turku";
            field("cancelAddressButton").clicked();
            verify(!field("addressName").visible);
            compare(browser.addresses()[0].city, "Kempele");

            // A new address starts from empty fields.
            field("addAddressButton").clicked();
            compare(field("addressName").text, "");
            compare(field("addressCity").text, "");
            field("cancelAddressButton").clicked();

            findChild(field("addressList").itemAt(0), "removeAddressButton").clicked();
            compare(browser.addresses(), []);
            compare(field("addressList").count, 0);
            saved = [];
        } finally {
            removeAddresses(saved.map(address => address.id));
            window.settingsOpen = false;
        }
    }

    // Payment cards (#339). They are the reader's, kept in the test's own
    // keyring, so every test saves its own and removes them again.
    readonly property var everydayCard: ({
                                             "number": "4242 4242 4242 4242",
                                             "name": "Meri Laine",
                                             "expiry": "08/29",
                                             "nickname": "Everyday"
                                         })
    readonly property var travelCard: ({
                                           "number": "5555 5555 5555 4444",
                                           "name": "Meri A. Laine",
                                           "expiry": "11/30"
                                       })

    function saveCards(cards) {
        browser.paymentCards();
        tryCompare(browser, "paymentCardsState", "ready");
        const ids = [];
        for (const card of cards) {
            const id = browser.savePaymentCard(card);
            verify(id.length > 0);
            ids.push(id);
        }
        return ids;
    }

    function removeCards(ids) {
        for (const id of ids)
            browser.removePaymentCard(id);
    }

    // A card field the reader pressed offers the saved cards: each by its
    // nickname, or its brand when it has none, and its last four digits, with
    // the name on the card and the expiry muted under them. Accepting one
    // fills the form from it, in the field's frame, for that focus.
    function test_savedCardsAreOfferedUnderACardFieldAndFillTheForm() {
        const ids = saveCards([everydayCard, travelCard]);
        const engine = openPage("https://cards.example/checkout");
        engine.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["Everyday •••• 4242", "Mastercard •••• 4444"]);
        compare(findChild(list, "suggestionRow0").detail, "Meri Laine · 08/29");
        compare(findChild(list, "suggestionRow1").detail, "Meri A. Laine · 11/30");
        compare(findChild(list, "suggestionRow1").Accessible.name,
                "Mastercard •••• 4444, Meri A. Laine · 11/30");

        engine.filledCard = null;
        engine.simulateFormKey("down");
        engine.simulateFormKey("down");
        verify(engine.simulateFormKey("accept"));
        compare(engine.filledCard.number, "5555555555554444");
        compare(engine.filledCard.name, "Meri A. Laine");
        compare(engine.filledCard.expiryMonth, 11);
        compare(engine.filledCard.expiryYear, 2030);
        compare(engine.filledCard.last4, "4444");
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateCardFieldBlur();
        removeCards(ids);
    }

    // The cards stand under an empty card field: once the reader types a card
    // of their own, the list is out of the way.
    function test_typingACardPutsTheSavedCardsAway() {
        const ids = saveCards([everydayCard]);
        const engine = openPage("https://cards-typed.example/");
        engine.simulateCardFieldFocus("cc-name", 100, 200, 240, 30);
        const list = formSuggestions();
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateCardFieldInput();
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateCardFieldBlur();
        removeCards(ids);
    }

    // A page without a certificate the engine accepted is offered no card,
    // and the list says why rather than leaving the reader to wonder. Saying
    // so fills nothing.
    function test_aPageThatIsNotSecureOffersNoCardAndSaysWhy() {
        const ids = saveCards([everydayCard]);
        const list = formSuggestions();
        for (const address of ["http://cards-plain.example/", "https://cards-waived.example/"]) {
            const engine = openPage(address);
            if (address.startsWith("https"))
                engine.simulateCertificateError({
                                                    "url": address,
                                                    "overridable": true
                                                });
            engine.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
            tryVerify(function () {
                return list.shown;
            });
            compare(list.count, 1);
            compare(suggestionTexts(), ["Saved cards are offered only on secure pages"]);
            engine.filledCard = null;
            engine.simulateFormKey("down");
            engine.simulateFormKey("accept");
            compare(engine.filledCard, null);
            engine.simulateCardFieldBlur();
        }

        // A waived check the engine has since forgotten, calling the
        // connection secure again, is still not a secure page.
        const waived = openPage("https://localhost:7443/cards-waived");
        const bar = findChild(window.contentItem, "certificateQuestionBar");
        waived.simulateCertificateError({});
        tryVerify(function () {
            return bar.open;
        });
        const action = findChild(bar, "questionAction0");
        settleActions(action);
        mouseClick(action, action.width / 2, action.height / 2);
        tryVerify(function () {
            return !bar.open;
        });
        waived.certificateErrorOrigin = "";
        compare(waived.connectionState, "secure");
        waived.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["Saved cards are offered only on secure pages"]);
        waived.simulateCardFieldBlur();

        // Nor is a payment frame from the waived origin in a secure page.
        const framed = openPage("https://cards-framed.example/");
        framed.cardFrameOrigin = "https://localhost:7443";
        framed.simulateCardFieldFocus("cc-number", 100, 200, 240, 30, "payment");
        tryVerify(function () {
            return list.shown;
        });
        compare(suggestionTexts(), ["Saved cards are offered only on secure pages"]);
        framed.simulateCardFieldBlur();
        framed.cardFrameOrigin = "";
        removeCards(ids);
    }

    // Each frame counts its focuses from the start, so a field in another
    // frame with the same count is a new focus, with its list open again.
    function test_aCardFieldInAnotherFrameIsANewFocus() {
        const ids = saveCards([everydayCard]);
        const engine = openPage("https://cards-frames.example/");
        const list = formSuggestions();
        engine.simulateCardFieldFocus("cc-number", 100, 200, 240, 30, "main", 7);
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateFormKey("escape");
        tryVerify(function () {
            return !list.shown;
        });
        engine.simulateCardFieldFocus("cc-number", 100, 260, 240, 30, "payment", 7);
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateCardFieldBlur();
        removeCards(ids);
    }

    // Without a saved card there is nothing to offer, and no list.
    function test_aCardFieldWithNoSavedCardHasNoList() {
        const engine = openPage("https://cards-none.example/");
        engine.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
        wait(100);
        verify(!formSuggestions().shown);
        engine.simulateCardFieldBlur();
    }

    // A Private window offers no saved card, though the reader saved some.
    function test_aPrivateWindowOffersNoCard() {
        const ids = saveCards([everydayCard]);
        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://cards-private.example/", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        privateEngine.item.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
        const list = findChild(privateBrowser.contentItem, "formSuggestions");
        wait(100);
        verify(!list.shown);
        compare(privateBrowser.windowBrowser.paymentCards(), []);
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        removeCards(ids);
    }

    readonly property var typedCard: ({
                                          "number": "4242424242424242",
                                          "name": "Meri Laine",
                                          "expiryMonth": 8,
                                          "expiryYear": 2029,
                                          "origin": "https://shop.example"
                                      })

    // A card the reader typed and submitted is offered for saving on the
    // permission bar's surface, by its last four digits, naming the site and
    // the Space. Not now forgets it, Save keeps it in the keyring, and either
    // answer lets go of the number. A card already saved is not offered again.
    function test_aSubmittedCardIsOfferedForSaving() {
        browser.paymentCards();
        tryCompare(browser, "paymentCardsState", "ready");
        const bar = findChild(window.contentItem, "cardSaveBar");
        verify(bar !== null);
        const engine = openPage("https://shop.example/pay");
        engine.simulatePaymentCardSubmit(typedCard);
        tryVerify(function () {
            return bar.open;
        });
        compare(bar.message, "Save card •••• 4242 to the keyring?");
        compare(bar.detail, "shop.example · " + browser.activeSpaceName);
        compare(bar.actions.map(function (action) {
            return action.label;
        }), ["Save", "Not now"]);

        mouseClick(findChild(bar, "questionAction1"));
        tryVerify(function () {
            return !bar.open;
        });
        compare(window.cardOffer, null);
        compare(browser.paymentCards(), []);

        engine.simulatePaymentCardSubmit(typedCard);
        tryVerify(function () {
            return bar.open;
        });
        mouseClick(findChild(bar, "questionAction0"));
        tryVerify(function () {
            return !bar.open;
        });
        compare(window.cardOffer, null);
        const saved = browser.paymentCards();
        compare(saved.length, 1);
        compare(saved[0].last4, "4242");
        compare(saved[0].name, "Meri Laine");
        compare(saved[0].expiryMonth, 8);
        compare(saved[0].expiryYear, 2029);
        compare(saved[0].nickname, "");

        engine.simulatePaymentCardSubmit(typedCard);
        wait(100);
        verify(!bar.open);
        compare(window.cardOffer, null);
        removeCards(saved.map(card => card.id));
    }

    // The offer is made only in a secure context, and never in a Private
    // window, which has no identity to keep a card for. The offer is about
    // the tab it was typed in, and goes when another is on show.
    function test_aCardIsOfferedForSavingOnlyWhereTheDecisionAllows() {
        browser.paymentCards();
        tryCompare(browser, "paymentCardsState", "ready");
        const bar = findChild(window.contentItem, "cardSaveBar");
        const plain = openPage("http://shop-plain.example/pay");
        plain.simulatePaymentCardSubmit(typedCard);
        wait(100);
        verify(!bar.open);

        const waived = openPage("https://shop-waived.example/pay");
        waived.simulateCertificateError({
                                            "url": "https://shop-waived.example/pay",
                                            "overridable": true
                                        });
        waived.simulatePaymentCardSubmit(typedCard);
        wait(100);
        verify(!bar.open);

        const secure = openPage("https://shop-tabs.example/pay");
        secure.simulatePaymentCardSubmit(typedCard);
        tryVerify(function () {
            return bar.open;
        });
        openPageInNewTab("https://elsewhere.example/");
        tryVerify(function () {
            return !bar.open;
        });
        compare(window.cardOffer, null);
        // A tab not on show is not where the reader is typing.
        secure.simulatePaymentCardSubmit(typedCard);
        wait(100);
        verify(!bar.open);
        compare(window.cardOffer, null);

        windowManager.openPrivateWindow();
        tryCompare(windowManager, "privateWindowCount", 1);
        const privateBrowser = window.privateWindows[0];
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader");
        privateBrowser.windowBrowser.openInput("https://shop-private.example/", false);
        tryVerify(function () {
            return privateEngine.item !== null;
        });
        privateEngine.item.simulatePaymentCardSubmit(typedCard);
        wait(100);
        verify(!findChild(privateBrowser.contentItem, "cardSaveBar").open);
        compare(privateBrowser.cardOffer, null);
        privateBrowser.windowBrowser.closeActiveTab();
        tryCompare(windowManager, "privateWindowCount", 0);
        window.requestActivate();
        tryVerify(function () {
            return window.active;
        });
        compare(browser.paymentCards(), []);
    }

    // Site information lists each card filled into the page for the rest of
    // the page's life: which card, by its last four digits, and the origin of
    // the frame it went into when that is not the page's own. A new page
    // starts with none.
    function test_siteInformationNamesTheCardsFilledIntoThePage() {
        const ids = saveCards([everydayCard, travelCard]);
        const card = siteCard();
        const engine = openPage("https://fills.example/checkout");
        const list = formSuggestions();
        openSiteCard("");
        verify(findChild(card, "siteCardFill0") === null || !findChild(card,
                                                                       "siteCardFill0").visible);
        closeSiteCard();

        engine.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateFormKey("down");
        verify(engine.simulateFormKey("accept"));
        engine.simulateCardFieldBlur();

        engine.cardFrameOrigin = "https://pay.processor.example";
        engine.simulateCardFieldFocus("cc-number", 100, 200, 240, 30);
        tryVerify(function () {
            return list.shown;
        });
        engine.simulateFormKey("down");
        engine.simulateFormKey("down");
        verify(engine.simulateFormKey("accept"));
        engine.simulateCardFieldBlur();
        engine.cardFrameOrigin = "";

        openSiteCard("");
        tryVerify(function () {
            return findChild(card, "siteCardFill1") !== null;
        });
        compare(findChild(card, "siteCardFill0").text, "Everyday •••• 4242 was filled in");
        compare(findChild(card, "siteCardFill1").text,
                "Mastercard •••• 4444 was filled in for pay.processor.example");
        closeSiteCard();

        openPage("https://fills-next.example/");
        openSiteCard("");
        verify(findChild(card, "siteCardFill0") === null || !findChild(card,
                                                                       "siteCardFill0").visible);
        closeSiteCard();
        removeCards(ids);
    }

    // Settings lists the saved cards by their nickname or brand and last four
    // digits, with the name and expiry under them, and adds, edits and removes
    // them with the fields in place under the list. Once saved, a card's
    // number is shown only as its last four digits, and an edit that leaves
    // it empty keeps it.
    function test_settingsAddsEditsAndRemovesACard() {
        browser.paymentCards();
        tryCompare(browser, "paymentCardsState", "ready");
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        settings.section = settings.sections.indexOf("payment cards");
        const field = function (name) {
            return findChild(settings, name);
        };
        const rows = function () {
            const shown = [];
            const list = field("cardList");
            for (let index = 0; index < list.count; ++index)
                shown.push(list.itemAt(index).title + " | " + list.itemAt(index).note);
            return shown;
        };
        let saved = [];
        try {
            verify(settings.sections.indexOf("payment cards") >= 0);
            compare(field("cardList").count, 0);
            verify(field("noCards").visible);
            verify(!field("cardNumber").visible);

            field("addCardButton").clicked();
            verify(field("cardNumber").visible);
            verify(!field("saveCardButton").enabled);
            field("cardNumber").text = "4242 4242 4242 4241";
            field("cardHolder").text = "Meri Laine";
            field("cardExpiry").text = "08/29";
            field("cardNickname").text = "Everyday";
            verify(field("saveCardButton").enabled);
            field("saveCardButton").clicked();
            // Not a card number: nothing is kept, and the fields say so.
            compare(browser.paymentCards(), []);
            verify(field("cardNumber").visible);
            verify(field("cardError").visible);

            field("cardNumber").text = "4242 4242 4242 4242";
            field("saveCardButton").clicked();
            saved = browser.paymentCards();
            compare(saved.length, 1);
            compare(saved[0].last4, "4242");
            compare(rows(), ["Everyday •••• 4242 | Meri Laine · 08/29"]);
            verify(!field("cardNumber").visible);
            verify(!field("noCards").visible);

            // Edit opens the fields with the card in them, but its number
            // only as its last four digits.
            findChild(field("cardList").itemAt(0), "editCardButton").clicked();
            compare(field("cardNumber").text, "");
            compare(field("cardNumber").placeholder, "•••• 4242");
            compare(field("cardHolder").text, "Meri Laine");
            compare(field("cardExpiry").text, "08/29");
            compare(field("cardNickname").text, "Everyday");
            field("cardExpiry").text = "09/30";
            field("cardNickname").text = "";
            field("saveCardButton").clicked();
            saved = browser.paymentCards();
            compare(saved.length, 1);
            compare(saved[0].expiryMonth, 9);
            compare(browser.paymentCardForFill(saved[0].id).number, "4242424242424242");
            compare(rows(), ["Visa •••• 4242 | Meri Laine · 09/30"]);

            // Cancel leaves the card as it was.
            findChild(field("cardList").itemAt(0), "editCardButton").clicked();
            field("cardHolder").text = "Someone Else";
            field("cancelCardButton").clicked();
            verify(!field("cardNumber").visible);
            compare(browser.paymentCards()[0].name, "Meri Laine");

            findChild(field("cardList").itemAt(0), "removeCardButton").clicked();
            compare(browser.paymentCards(), []);
            compare(field("cardList").count, 0);
            saved = [];
        } finally {
            removeCards(saved.map(card => card.id));
            window.settingsOpen = false;
        }
    }

    // Clear browsing data offers Payment cards as a category of its own, off
    // each time the dialog opens: a card is not browsing history, and clearing
    // a day's history should not cost the reader their cards. Asked for, it
    // deletes every card from the keyring.
    function test_clearingBrowsingDataTakesCardsOnlyWhenAskedEachTime() {
        const ids = saveCards([everydayCard]);
        readyForBrowsingData();
        const settings = findChild(window.contentItem, "settingsSurface");
        const dialog = findChild(window.contentItem, "clearBrowsingDataDialog");
        const openButton = findChild(window.contentItem, "clearBrowsingDataButton");
        try {
            openButton.clicked();
            tryVerify(function () {
                return dialog.visible;
            });
            const cards = findChild(dialog, "clearCategory-cards");
            verify(cards !== null);
            compare(cards.title, "Payment cards");
            compare(cards.checked, false);
            verify(dialog.categories.indexOf("history") >= 0);
            findChild(dialog, "clearBrowsingDataConfirm").clicked();
            tryVerify(function () {
                return !dialog.visible;
            });
            compare(browser.paymentCards().length, 1);

            openButton.clicked();
            tryVerify(function () {
                return dialog.visible;
            });
            cards.clicked();
            compare(cards.checked, true);
            dialog.dismissed();
            tryVerify(function () {
                return !dialog.visible;
            });
            openButton.clicked();
            tryVerify(function () {
                return dialog.visible;
            });
            compare(cards.checked, false);
            cards.clicked();
            findChild(dialog, "clearBrowsingDataConfirm").clicked();
            tryVerify(function () {
                return !dialog.visible;
            });
            compare(browser.paymentCards(), []);
        } finally {
            removeCards(ids);
            leaveBrowsingData();
        }
    }

    // An Agent's `do` types with real keys, which the page hears as trusted
    // input. What a step typed is never the reader's, so while it runs nothing
    // of it is kept or offered. Each test runs a step in one of the reader's
    // Spaces the Agent was granted, and holds the page's answer so the step is
    // still running while the page reports.
    function startAgentStep(spaceName, address, agent) {
        const step = {
            "startSpaceId": browser.activeSpaceId,
            "spaceName": spaceName
        };
        step.spaceId = browser.createSpace(spaceName);
        verify(step.spaceId.length > 0);
        verify(browser.switchSpace(step.spaceId));
        step.engine = openPage(address);
        agentControl.allowAgents = true;
        step.engine.holdAgentAnswers = true;
        step.replies = agentSocket.replies.length;
        const bar = findChild(window.contentItem, "agentGrantBar");
        agentSocket.send({
                             "verb": "do",
                             "name": agent,
                             "tab": browser.activeTabId,
                             "steps": [
                                 {
                                     "action": "back"
                                 }
                             ]
                         });
        tryCompare(bar, "visible", true);
        mouseClick(findChild(bar, "browserPromptAccept"));
        tryCompare(bar, "visible", false);
        tryVerify(function () {
            return step.engine.heldAgentAnswers.length === 1;
        });
        return step;
    }

    function finishAgentStep(step) {
        step.engine.releaseAgentAnswers();
        tryVerify(function () {
            return agentSocket.replies.length > step.replies;
        });
        agentControl.revokeGrant(step.spaceId);
        agentControl.allowAgents = false;
        verify(browser.switchSpace(step.startSpaceId));
        verify(browser.deleteSpace(step.spaceId, step.spaceName));
    }

    function test_anAgentStepLeavesNoFormHistory() {
        const step = startAgentStep("Agent forms", "https://agent-forms.example/", "step-forms");
        try {
            step.engine.simulateFormSubmit([
                                               {
                                                   "name": "agent-field",
                                                   "value": "typed by an agent"
                                               }
                                           ]);
            wait(50);
            compare(browser.formHistory(step.engine.spaceId, "agent-field"), []);
        } finally {
            finishAgentStep(step);
        }
    }

    function test_anAgentStepOpensNoAddressList() {
        const ids = saveAddresses([homeAddress]);
        const step = startAgentStep("Agent addresses", "https://agent-addresses.example/",
                                    "step-addresses");
        try {
            step.engine.simulateFormFieldFocus("", "", 100, 200, 240, 30, "email");
            wait(100);
            verify(!formSuggestions().shown);
            step.engine.simulateFormFieldBlur();
        } finally {
            finishAgentStep(step);
            removeAddresses(ids);
        }
    }

    // A submit the step made is heard after its answer, once the page has
    // been asked for the card, so the step is held a moment longer; its page
    // is told to forget what it typed.
    function test_anAgentStepRaisesNoCardSaveOffer() {
        browser.paymentCards();
        tryCompare(browser, "paymentCardsState", "ready");
        const step = startAgentStep("Agent cards", "https://agent-cards.example/", "step-cards");
        const bar = findChild(window.contentItem, "cardSaveBar");
        try {
            step.engine.simulatePaymentCardSubmit(typedCard);
            wait(100);
            verify(!bar.open);
            compare(window.cardOffer, null);
            const forgotten = step.engine.typedInputForgotten;
            step.engine.releaseAgentAnswers();
            tryVerify(function () {
                return agentSocket.replies.length > step.replies;
            });
            verify(step.engine.typedInputForgotten > forgotten);
            step.engine.simulatePaymentCardSubmit(typedCard);
            wait(100);
            verify(!bar.open);
            compare(window.cardOffer, null);
        } finally {
            finishAgentStep(step);
        }
        compare(browser.paymentCards(), []);
    }

    // Every control in a Settings row lies wholly inside the pane, in every
    // section, on whole pixels. A control placed on a fractional position is
    // snapped by the renderer when it is drawn, and at the pane's right edge,
    // where a row puts its controls, that can push its border past the pane's
    // clip: a Remove button once lost its right border there.
    function test_everySettingsRowControlLiesWhollyInsideThePane() {
        const cardIds = saveCards([everydayCard]);
        const addressIds = saveAddresses([homeAddress]);
        window.requestSettings();
        const settings = findChild(window.contentItem, "settingsSurface");
        const pane = findChild(settings, "settingsPane");
        const strays = [];
        const isRow = item => item.hasOwnProperty("separated") && item.hasOwnProperty(
                                  "verticalPadding") && item.hasOwnProperty("note");
        const visit = function (item, inRow, section) {
            if (!item || !item.visible)
                return;
            const control = inRow && item.hasOwnProperty("bordered");
            if (control && item.width > 0) {
                const rect = item.mapToItem(pane, 0, 0, item.width, item.height);
                const whole = Number.isInteger(rect.x) && Number.isInteger(rect.width);
                if (rect.x < 0 || rect.x + rect.width > pane.width || !whole)
                    strays.push(settings.sections[section] + ": " + (item.objectName || item.text)
                                + " at " + rect.x + " to " + (rect.x + rect.width) + " of "
                                + pane.width);
            }
            const children = item.children || [];
            for (let index = 0; index < children.length; ++index)
                visit(children[index], inRow || isRow(item), section);
        };
        try {
            for (let section = 0; section < settings.sections.length; ++section) {
                settings.section = section;
                wait(50);
                visit(pane, false, section);
            }
            compare(strays, []);
        } finally {
            window.settingsOpen = false;
            removeCards(cardIds);
            removeAddresses(addressIds);
        }
    }
}
