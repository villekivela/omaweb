import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import qs.Commons
import qs.Ui as Omarchy

Rectangle {
    id: root
    objectName: "spaceOutline"

    property var colors
    property string iconFontFamily
    property var browser
    // The window's keymap, for the labels Primary shows, and whether it is
    // being held for them.
    property var keyMap: null
    property bool keyLabelsShown: false
    // What this window's Space is hosting, as `{ key, name, id, popupUrl }`.
    property var hostedExtensions: []
    property bool privateWindow: false
    property bool collapsed: false
    property bool floating: false
    property var blocker: null
    property bool statusOpen: false
    property bool useFavicons: true
    property bool tintFavicons: false
    property bool canGoBack: false
    property bool canGoForward: false
    // Something in settings is waiting on the reader. The button wears a mark
    // rather than the outline growing a line for it: settings is a place, and
    // what is wrong is stated there, where it can be acted on.
    property bool settingsAttention: false
    property var sync: null
    readonly property bool authenticatedSync: !root.privateWindow && root.sync
                                              && root.sync.login.length > 0
    readonly property bool activeSync: root.authenticatedSync && root.sync.enabled
    property var downloads: null
    // What Omaweb knows about its own age. Null in the UI lab and in tests,
    // where nothing asks GitHub anything.
    property var releaseWatch: null
    property bool savedFileNoticeShowing: false
    // What the Agents are doing, as the core reports it: each Agent tab by id,
    // with its `spaceId`, the connection's `name`, and `busy` while one of its
    // commands is in flight. And the Spaces an Agent made that the reader has
    // not taken over.
    property var agentActivity: ({})
    property var agentSpaceIds: []

    // How wide a Space is in the footer, and the count of those left out.
    readonly property int spaceSlot: 18
    readonly property int slotSpacing: 4
    readonly property int overflowSlot: 28

    // How many Spaces the row has room for. The reader's come first and the
    // Agent Spaces after them, so the ones left out are the last in that
    // order, and room is kept for the count of them.
    function spacesShown(available, count) {
        const widthOf = function (shown) {
            return shown * root.spaceSlot + Math.max(0, shown - 1) * root.slotSpacing;
        };
        if (widthOf(count) <= available)
            return count;
        let shown = count;
        while (shown > 0 && widthOf(shown) + root.slotSpacing + root.overflowSlot > available)
            --shown;
        return shown;
    }

    // Where the Space on show is in the list, kept as the list changes: a
    // binding over model data does not see the model change.
    property int activeRow: activeSpaceRow()
    Connections {
        target: root.browser ? root.browser.spaces : null
        function onDataChanged() {
            root.activeRow = root.activeSpaceRow();
        }
        function onRowsInserted() {
            root.activeRow = root.activeSpaceRow();
        }
        function onRowsRemoved() {
            root.activeRow = root.activeSpaceRow();
        }
        function onRowsMoved() {
            root.activeRow = root.activeSpaceRow();
        }
        function onModelReset() {
            root.activeRow = root.activeSpaceRow();
        }
    }

    // Whether the footer draws a Space. The Space on show always has a place:
    // when it would be left out, it takes the last place there is room for,
    // and the Space that stood there is counted instead.
    function spaceInRow(row) {
        const shown = spaceSwitcher.shown;
        if (root.activeRow < shown)
            return row < shown;
        return row < shown - 1 || row === root.activeRow;
    }

    // The Spaces the footer left out, in its order, and where their count
    // stands, for the window to hang their menu from.
    function openHiddenSpaces() {
        if (root.privateWindow || !root.browser || spaceOverflow.hidden <= 0)
            return;
        const spaces = root.browser.spaces;
        const hidden = [];
        for (let row = 0; row < spaces.rowCount(); ++row) {
            if (root.spaceInRow(row))
                continue;
            const index = spaces.index(row, 0);
            const spaceId = spaces.data(index, Qt.UserRole + 1);
            hidden.push({
                            "spaceId": spaceId,
                            "spaceName": spaces.data(index, Qt.UserRole + 2),
                            "spaceColor": spaces.data(index, Qt.UserRole + 3),
                            "active": spaces.data(index, Qt.UserRole + 4),
                            "agentMade": root.agentSpaceIds.indexOf(spaceId) >= 0,
                            "attached": root.agentWorkIn(spaceId).attached
                        });
        }
        const corner = spaceOverflow.mapToItem(null, 0, 0);
        root.hiddenSpacesRequested(hidden, Qt.rect(corner.x, corner.y, spaceOverflow.width,
                                                   spaceOverflow.height));
    }

    // What a Space's palette name is drawn in under the theme on show.
    function spaceColour(name) {
        const spaces = root.colors ? root.colors.spaces : null;
        return spaces && spaces[name] ? spaces[name] : root.colors.accent;
    }

    function agentOf(tabId) {
        return root.agentActivity[tabId] || null;
    }

    // Whether an Agent is attached to one of a Space's tabs, and whether one
    // of its commands is in flight there.
    function agentWorkIn(spaceId) {
        let attached = false;
        let busy = false;
        for (const tabId in root.agentActivity) {
            const agent = root.agentActivity[tabId];
            if (agent.spaceId !== spaceId)
                continue;
            attached = true;
            busy = busy || agent.busy === true;
        }
        return {
            "attached": attached,
            "busy": busy
        };
    }

    // An empty pinned section takes no room at all.
    property int pinnedCount: browser ? browser.pinnedTabs.rowCount() : 0

    // A Space whose only ordinary tab is blank lists no ordinary tab: the row
    // would stand for a page nobody opened, wearing a site chip for a site
    // that does not exist. The pinned block stays — those are the Space's own
    // furniture and are there whether anything is open or not.
    readonly property bool atRest: browser ? browser.atRest : false

    readonly property url activeUrl: browser ? browser.activeUrl : ""

    // Spaces are a row, the row the footer draws them in, and switching one
    // slides the outline along it: the Space to the right arrives from the
    // right as the one on show leaves to the left, the larger square moves
    // along the footer with it, and the page arrives the same way. One
    // movement, in the direction the reader chose. The leaving list is a
    // picture taken while it was at rest, since the models have already
    // become the next Space's by the time the switch is heard. `easeSpaces`
    // is whether what the chrome does next moves, which a key's switch does
    // not; `spacesCanMove` is whether a switch may move at all, which is when the
    // picture is worth keeping ready for one.
    property bool easeSpaces: true
    property bool spacesCanMove: true
    property int settledSpaceRow: activeSpaceRow()
    property bool arriving: false
    // Where the arriving list and the leaving picture stand, as a fraction
    // of the list's width: 1 is one width to the right.
    property real arrivalOffset: 0
    property real departureOffset: 0
    property real arrivalOpacity: 1
    property real departureOpacity: 0
    // The direction of the last switch: 1 to the right, -1 to the left.
    property int switchDirection: 1
    // The row is read off the model on each change rather than bound: a
    // binding over model data does not see the model change.
    function activeSpaceRow() {
        if (!browser)
            return 0;
        const spaces = browser.spaces;
        for (let row = 0; row < spaces.rowCount(); ++row) {
            if (spaces.data(spaces.index(row, 0), Qt.UserRole + 4))
                return row;
        }
        return 0;
    }
    // The picture of the list at rest, retaken after the list settles from
    // any change, so it is the leaving Space's list when a switch comes.
    function retakeDeparture() {
        if (!root.arriving && root.spacesCanMove)
            departureRetake.restart();
    }
    Timer {
        id: departureRetake
        interval: 50
        onTriggered: if (!root.arriving)
                         departure.scheduleUpdate()
    }
    Component.onCompleted: retakeDeparture()
    Connections {
        target: root.browser
        // Heard before the models change: from here on, the picture stands.
        function onSpaceSuspended() {
            if (root.easeSpaces)
                root.arriving = true;
        }
        function onActiveSpaceChanged() {
            const from = root.settledSpaceRow;
            const to = root.activeSpaceRow();
            root.settledSpaceRow = to;
            if (!root.easeSpaces || from === to) {
                root.arriving = false;
                return;
            }
            spaceArrival.stop();
            root.settledTabItem = null;
            root.settledTabId = "";
            root.settledTabX = -1;
            root.settledTabY = -1;
            root.switchDirection = to > from ? 1 : -1;
            root.arrivalOffset = root.switchDirection;
            root.arrivalOpacity = 0;
            root.departureOffset = 0;
            root.departureOpacity = 1;
            spaceArrival.start();
        }
        function onActiveTabChanged() {
            root.retakeDeparture();
        }
    }
    Connections {
        target: root.browser ? root.browser.unpinnedTabs : null
        function onRowsInserted() {
            root.retakeDeparture();
        }
        function onRowsRemoved() {
            root.retakeDeparture();
        }
        function onRowsMoved() {
            root.retakeDeparture();
        }
        function onDataChanged() {
            root.retakeDeparture();
        }
        function onModelReset() {
            root.retakeDeparture();
        }
    }
    Connections {
        target: root.browser ? root.browser.pinnedTabs : null
        function onRowsInserted() {
            root.retakeDeparture();
        }
        function onRowsRemoved() {
            root.retakeDeparture();
        }
        function onRowsMoved() {
            root.retakeDeparture();
        }
        function onDataChanged() {
            root.retakeDeparture();
        }
        function onModelReset() {
            root.retakeDeparture();
        }
    }
    ParallelAnimation {
        id: spaceArrival
        NumberAnimation {
            target: root
            property: "arrivalOffset"
            to: 0
            duration: 240
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "arrivalOpacity"
            to: 1
            duration: 160
        }
        NumberAnimation {
            target: root
            property: "departureOffset"
            to: -root.switchDirection
            duration: 240
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "departureOpacity"
            to: 0
            duration: 160
        }
        onFinished: {
            root.arriving = false;
            root.retakeDeparture();
        }
    }
    // What Content blocking refused for the page on show.
    readonly property int refusalTally: refusals.count

    property RefusalTally refusals: RefusalTally {
        blocker: root.blocker
        browser: root.browser
        pageAddress: root.activeUrl
    }
    // What the connection is, as the engine drawing the page reports it. The
    // outline never works this out from the address: an address is what was
    // asked for, and a lock drawn from one is a claim nothing checked.
    property string connectionState: "internal"
    property string lookupFailedBy: ""
    property var certificateChain: []
    property bool pageCertificatesAvailable: false
    property bool upgradedByHttpsOnly: false
    // What the engine can and cannot answer for. A gap is said out loud rather
    // than drawn as a reassuring blank.
    property bool certificateDecisionsAvailable: false
    property bool thirdPartyCookieControlAvailable: false
    property bool siteDataOnDisk: false
    property bool insecureContentBlocked: true
    // The engine's third-party filter, which is the only thing that knows
    // which embedded origins a page has actually had refused.
    property var cookiePolicy: null
    property var siteDataEntries: []
    property var retainedDataEntries: []
    property int siteDataGeneration: 0
    readonly property bool secure: root.connectionState === "secure"
    readonly property bool certificateError: root.connectionState === "certificate-error"
    readonly property bool blank: String(activeUrl).length === 0 || String(activeUrl)
                                  === "about:blank"
    // The site the panel is headed by, as the window's dialogs name it, and the
    // third parties it had refused. The panel is the one place that asks the
    // engine's filter, so the dialog reads the answer from here rather than
    // asking again for itself.
    readonly property string siteOrigin: sitePanel.originLabel
    readonly property var refusedThirdParties: sitePanel.refusedThirdParties

    signal addressRequested
    signal downloadsRequested
    // Where the popup should come from, in window coordinates. A panel that
    // grows out of the mark is the mark, opened, rather than a second thing
    // that appeared over the page. The keyboard asks for the same rectangle,
    // so the command and the pointer open from the same place.
    function extensionMarkRect() {
        const corner = extensionMark.mapToItem(null, 0, 0);
        return Qt.rect(corner.x, corner.y, extensionMark.width, extensionMark.height);
    }
    signal extensionRequested(rect origin)
    signal releaseNotesRequested(url notes)
    // What the reader asked Site information for, on its way to the window's
    // own dialog. The outline states; the window asks.
    signal siteActionRequested(string action)
    signal tabActivated(string tabId)
    signal tabCloseRequested(string tabId)
    signal tabMuteToggled(string tabId)
    signal tabMenuRequested(string tabId, real anchorX, real anchorY)
    // Where a dragged row was let go, counted inside its own section. The list
    // owns this because placing rows is the list's: a row knows how tall it is
    // and nothing about the rows around it.
    signal tabDropped(string tabId, int destination)
    signal spaceActivated(string spaceId)
    // The count of Spaces the footer had no room for was pressed: those
    // Spaces, each as `{ spaceId, spaceName, spaceColor, active, agentMade,
    // attached }`, and where the count stands.
    signal hiddenSpacesRequested(var spaces, rect origin)
    signal settingsRequested
    signal syncRequested
    signal backRequested
    signal forwardRequested
    signal reloadRequested
    signal sidebarToggled
    signal commandScopeRequested
    signal windowMoveRequested
    signal pageFocusRequested

    // Where "focus the sidebar" lands: the row the reader is already reading,
    // so the keyboard arrives where their attention is.
    property var activeTabItem: null
    // Tabs are the list, and the page follows a switch the way the list
    // reads: a tab further down arrives from below, one further up from
    // above. The distance is the page's; the direction is measured here,
    // where the rows are. A Space switch moves the page its own way and
    // this stays out of it.
    property real settledTabX: -1
    property real settledTabY: -1
    property real tabOffsetX: 0
    property real tabOffsetY: 0
    // The row of the tab beside, while a split is on show. A pane arriving
    // beside the page on show comes from this row, the way a page arrives
    // from its own.
    property var besideTabItem: null
    // The row the page on show was measured from, which is the row that was
    // active when the last arrival was settled, or the row the list built
    // again for the same tab, measured once the list has placed it.
    property var settledTabItem: null
    // The tab that row stands for, which outlives the row when the list
    // builds it again.
    property string settledTabId: ""
    // Set while focus is moving between the halves of a split, for the rest
    // of the turn: the half that was active is announced as beside after the
    // other half is announced as active, and neither arrival is one.
    property bool focusMoving: false
    // Set while a row's arrival is being measured, for the rest of the turn:
    // a split whose row arrived whole comes on show with the active half's
    // move, not with one of its own.
    property bool activeArriving: false
    onBesideTabItemChanged: {
        if (besideTabItem === null || activeTabItem === null || !easeSpaces || arriving)
            return;
        // The page that was on show is now the tab beside: focus moved to the
        // other half, or a blank tab was put beside it, and nothing arrived.
        // A row that arrived whole, both halves with it, is already on its
        // way from the active half's move.
        if (focusMoving || activeArriving || besideTabItem === settledTabItem)
            return;
        // The row is heard of before the model has finished saying what it
        // is: its own half of the pairing lands after the other half's. The
        // measurement waits a turn for both, and for the list to have placed
        // the row where it now belongs.
        const beside = besideTabItem;
        Qt.callLater(function () {
            if (root.besideTabItem !== beside || root.activeTabItem === null)
                return;
            ordinarySection.forceLayout();
            const at = beside.mapToItem(root, 0, 0);
            const from = root.activeTabItem.mapToItem(root, 0, 0);
            if (at.x === from.x)
                return;
            root.tabOffsetY = 0;
            root.tabOffsetX = at.x > from.x ? 10 : -10;
            tabArrival.restart();
        });
    }
    // The row on show, where the list has placed it, as the place the next
    // arrival is measured from.
    function settleOnPlacedRow(row) {
        if (root.activeTabItem !== row || row.tabId === "" || row.parent === null)
            return;
        row.parent.forceLayout();
        const placed = row.mapToItem(root, 0, 0);
        root.settledTabItem = row;
        root.settledTabId = row.tabId;
        root.settledTabX = Math.round(placed.x);
        root.settledTabY = Math.round(placed.y);
    }
    onActiveTabItemChanged: noteActiveRow(activeTabItem)
    // The list builds its rows again when it moves them, as pairing a tab that
    // was in a split before does, and a row it is building can say it is
    // active before the list has placed it, or before it has been told which
    // tab it stands for. Only a different tab on a placed row is an arrival: a
    // row not placed yet, one naming no tab yet, or one for the tab already
    // settled on is measured once the list has placed it and nothing arrives,
    // so a pane coming beside it still does (#507).
    function noteActiveRow(row) {
        if (row === null)
            return;
        if (row.parent === null || row.tabId === "" || row.tabId === settledTabId) {
            if (row.tabId !== "")
                settledTabItem = row;
            Qt.callLater(function () {
                root.settleOnPlacedRow(row);
            });
            return;
        }
        // Whole pixels: a place a layout reached by arithmetic is not a move.
        const placed = row.mapToItem(root, 0, 0);
        const at = Qt.point(Math.round(placed.x), Math.round(placed.y));
        const fromX = settledTabX;
        const fromY = settledTabY;
        // Focus moving to the other half of a split shows nothing new, so
        // nothing arrives: the row now active is the partner of the row the
        // page on show was measured from.
        const focusMoved = settledTabId !== "" && row.splitPartnerId === settledTabId;
        if (focusMoved) {
            focusMoving = true;
            Qt.callLater(function () {
                root.focusMoving = false;
            });
        }
        settledTabItem = row;
        settledTabId = row.tabId;
        settledTabX = at.x;
        settledTabY = at.y;
        if (!easeSpaces || arriving || fromY < 0 || focusMoved)
            return;
        activeArriving = true;
        Qt.callLater(function () {
            root.activeArriving = false;
        });
        // Rows stand under one another, so a different row is a vertical
        // move; pins stand beside one another, so the same row and a
        // different column is a horizontal one.
        if (at.y !== fromY) {
            tabOffsetX = 0;
            tabOffsetY = at.y > fromY ? 10 : -10;
        } else if (at.x !== fromX) {
            tabOffsetY = 0;
            tabOffsetX = at.x > fromX ? 10 : -10;
        } else {
            return;
        }
        tabArrival.restart();
    }
    ParallelAnimation {
        id: tabArrival
        NumberAnimation {
            target: root
            property: "tabOffsetX"
            to: 0
            duration: 160
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "tabOffsetY"
            to: 0
            duration: 160
            easing.type: Easing.OutCubic
        }
    }

    // The row in the hand, and where the arrangement would put it if it were
    // let go now. A drag reorders nothing until it is released: opening the
    // place the row would land in says everything the reader needs, and the
    // session is written once rather than at every row the hand passes over.
    property var draggedRow: null
    property int dropDestination: -1

    // The rows of one section, by the place each holds. The children of a
    // positioner are in no particular order, and a Repeater is among them, so
    // each row is asked which place it is in rather than counted off.
    function sectionRows(container) {
        const rows = [];
        for (let index = 0; index < container.children.length; ++index) {
            const child = container.children[index];
            if (child.tabId !== undefined)
                rows[child.placeInSection] = child;
        }
        return rows;
    }

    function rowsFor(row) {
        return root.sectionRows(row.pinned ? pinnedSection : ordinarySection);
    }

    // Where the list put a row, whatever the hand has since done with it.
    function homeOf(row) {
        const at = row.mapToItem(root, 0, 0);
        return Qt.point(at.x - row.carry.x, at.y - row.carry.y);
    }

    function beginTabDrag(row) {
        root.draggedRow = row;
        root.dropDestination = row.placeInSection;
        row.lifted = true;
        // A row in the hand is in use, so it is not put away under it.
        if (root.browser)
            root.browser.setDraggedTab(row.tabId);
    }

    // The hand's position decides two things: where the held row is drawn, and
    // which place it would take. The second is read off where the list put the
    // other rows, so a wrapped row of pins answers as truthfully as a stack of
    // ordinary rows.
    function updateTabDrag(row, sceneX, sceneY) {
        if (root.draggedRow !== row)
            return;
        const pointer = root.mapFromItem(null, sceneX, sceneY);
        const home = root.homeOf(row);
        const wanted = Qt.point(pointer.x - row.grabbedAt.x, pointer.y - row.grabbedAt.y);
        // An ordinary row is carried along the list it is in; a pin is laid out
        // across the section as well as down it.
        row.carry = row.pinned ? Qt.point(wanted.x - home.x, wanted.y - home.y) : Qt.point(0,
                                                                                           wanted.y
                                                                                           - home.y);

        const rows = root.rowsFor(row);
        let destination = row.placeInSection;
        for (let place = 0; place < rows.length; ++place) {
            const other = rows[place];
            if (!other)
                continue;
            const at = root.homeOf(other);
            if (pointer.x < at.x || pointer.x >= at.x + other.width) {
                // Outside this row's column: only its band decides, so a
                // stacked list ignores the horizontal miss entirely.
                if (other.pinned)
                    continue;
            }
            if (pointer.y < at.y || pointer.y >= at.y + other.height)
                continue;
            destination = place;
            break;
        }
        // Past the end of the section in either direction, the nearest place is
        // the one the hand meant.
        if (destination === row.placeInSection && rows.length > 0) {
            const first = rows[0] ? root.homeOf(rows[0]) : null;
            const last = rows[rows.length - 1] ? root.homeOf(rows[rows.length - 1]) : null;
            if (first && pointer.y < first.y)
                destination = 0;
            else if (last && pointer.y >= last.y + rows[rows.length - 1].height)
                destination = rows.length - 1;
        }
        root.dropDestination = destination;
        root.openTheDroppedPlace(row, rows, destination);
    }

    // Every row between where the held row came from and where it would land
    // moves up or down by one place — into the place the arrangement would give
    // it — which is what opens the gap the held row will drop into.
    function openTheDroppedPlace(row, rows, destination) {
        const from = row.placeInSection;
        for (let place = 0; place < rows.length; ++place) {
            const other = rows[place];
            if (!other || other === row)
                continue;
            let shifted = place;
            if (from < destination && place > from && place <= destination)
                shifted = place - 1;
            else if (destination < from && place >= destination && place < from)
                shifted = place + 1;
            const target = rows[shifted];
            if (!target || shifted === place) {
                other.carry = Qt.point(0, 0);
                continue;
            }
            const here = root.homeOf(other);
            const there = root.homeOf(target);
            other.carry = Qt.point(there.x - here.x, there.y - here.y);
        }
    }

    function endTabDrag(row) {
        if (root.draggedRow !== row)
            return;
        const destination = root.dropDestination;
        const rows = root.rowsFor(row);
        root.draggedRow = null;
        root.dropDestination = -1;
        row.lifted = false;
        if (root.browser)
            root.browser.setDraggedTab("");
        // Every row goes back to the place the list gives it, and the list is
        // told the one thing the drag decided.
        for (let place = 0; place < rows.length; ++place) {
            if (rows[place])
                rows[place].carry = Qt.point(0, 0);
        }
        if (destination >= 0 && destination !== row.placeInSection) {
            root.tabDropped(row.tabId, destination);
        }
    }

    function focusOutline() {
        if (activeTabItem !== null && activeTabItem.visible) {
            activeTabItem.steer();
        } else {
            addressButton.forceActiveFocus();
        }
    }

    function labelFor(command, number) {
        return root.keyMap ? root.keyMap.labelFor(command, number) : "";
    }

    // The key that selects a tab is the number of its place among all of the
    // Space's tabs, not of its place in its section.
    function tabLabel(tabId) {
        if (!root.keyLabelsShown || !root.browser)
            return "";
        const tabs = root.browser.tabs;
        const count = Math.min(9, tabs.rowCount());
        for (let position = 0; position < count; ++position) {
            if (tabs.data(tabs.index(position, 0), Qt.UserRole + 1) === tabId)
                return root.labelFor("select-tab", position + 1);
        }
        return "";
    }

    // The rows as the reader sees them, pins first, which is the order the
    // cursor walks.
    function cursorRows() {
        return root.sectionRows(pinnedSection).concat(root.sectionRows(ordinarySection)).filter(
                    function (row) {
                        return row && row.visible;
                    });
    }

    function stepCursor(delta) {
        const rows = root.cursorRows();
        if (rows.length === 0)
            return;
        let at = rows.findIndex(function (row) {
            return row.activeFocus;
        });
        if (at < 0) {
            rows[Math.max(0, rows.indexOf(root.activeTabItem))].steer();
            return;
        }
        rows[Math.max(0, Math.min(rows.length - 1, at + delta))].steer();
    }

    // While the outline holds the keyboard, h j k l move the sidebar cursor as
    // the keys move through a list anywhere else on the desktop: j and k walk
    // the rows without touching the page, l, towards the page, opens the row
    // under the cursor and hands the keyboard over, as Return does, and h, away
    // from it, brings the cursor back to the tab on show. A field in the outline
    // takes its own letters before they climb this far.
    Keys.onPressed: function (event) {
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier
                               | Qt.ShiftModifier))

            return;
        if (event.key === Qt.Key_J) {
            root.stepCursor(1);
        } else if (event.key === Qt.Key_K) {
            root.stepCursor(-1);
        } else if (event.key === Qt.Key_L || event.key === Qt.Key_Return || event.key
                   === Qt.Key_Enter) {
            const row = root.cursorRows().find(function (candidate) {
                return candidate.activeFocus;
            });
            if (!row)
                return;
            root.tabActivated(row.tabId);
            root.pageFocusRequested();
        } else if (event.key === Qt.Key_H) {
            root.focusOutline();
        } else {
            return;
        }
        event.accepted = true;
    }

    // Key events climb from the focused row to here, so one handler covers the
    // whole outline: Escape is the way back to the page.
    Keys.onEscapePressed: function (event) {
        if (root.statusOpen) {
            root.statusOpen = false;
            event.accepted = true;
            return;
        }
        root.pageFocusRequested();
        event.accepted = true;
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.statusOpen
        context: Qt.WindowShortcut
        onActivated: root.statusOpen = false
    }

    color: floating ? "transparent" : colors.sidebar
    clip: true

    Connections {
        target: root.browser ? root.browser.pinnedTabs : null
        function onRowsInserted() {
            root.pinnedCount = root.browser.pinnedTabs.rowCount();
        }
        function onRowsRemoved() {
            root.pinnedCount = root.browser.pinnedTabs.rowCount();
        }
        function onModelReset() {
            root.pinnedCount = root.browser.pinnedTabs.rowCount();
        }
    }

    // The seam down the sidebar is a divider rather than a frame, so it is
    // drawn as the bar draws one.
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: root.colors.separator
    }

    Column {
        id: outline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 10
        spacing: 8

        // The navigation controls hold the row the Space heading used to: the
        // commands that act on the page open the outline, and the browsing
        // identity closes it from the footer.
        Item {
            objectName: "sidebarNavigation"
            width: parent.width
            height: 32

            DragHandler {
                target: null
                onActiveChanged: if (active)
                                     root.windowMoveRequested()
            }

            Row {
                objectName: "outlineControls"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ChromeButton {
                    objectName: "collapseButton"
                    KeyLabel {
                        objectName: "keyLabel-collapseButton"
                        iconFontFamily: root.iconFontFamily
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: -4
                        keys: root.keyLabelsShown ? root.labelFor("toggle-sidebar") : ""
                        shown: root.keyLabelsShown
                        colors: root.colors
                    }
                    width: 28
                    height: 26
                    icon: root.collapsed ? "left_panel_open" : "left_panel_close"
                    accessibleName: root.collapsed ? qsTr("Show sidebar") : qsTr("Hide sidebar")
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.sidebarToggled()
                }

                ChromeButton {
                    objectName: "commandScopeButton"
                    KeyLabel {
                        objectName: "keyLabel-commandScopeButton"
                        iconFontFamily: root.iconFontFamily
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: -4
                        keys: root.keyLabelsShown ? root.labelFor("command-scope") : ""
                        shown: root.keyLabelsShown
                        colors: root.colors
                    }
                    width: 28
                    height: 26
                    icon: "search"
                    accessibleName: qsTr("Search commands")
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.commandScopeRequested()
                }
            }

            Row {
                objectName: "outlineNavigation"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ChromeButton {
                    objectName: "backButton"
                    KeyLabel {
                        objectName: "keyLabel-backButton"
                        iconFontFamily: root.iconFontFamily
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: -4
                        keys: root.keyLabelsShown ? root.labelFor("back") : ""
                        shown: root.keyLabelsShown
                        colors: root.colors
                    }
                    width: 28
                    height: 26
                    icon: "arrow_back"
                    accessibleName: qsTr("Back")
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    enabled: root.canGoBack
                    onClicked: root.backRequested()
                }

                ChromeButton {
                    objectName: "forwardButton"
                    KeyLabel {
                        objectName: "keyLabel-forwardButton"
                        iconFontFamily: root.iconFontFamily
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: -4
                        keys: root.keyLabelsShown ? root.labelFor("forward") : ""
                        shown: root.keyLabelsShown
                        colors: root.colors
                    }
                    width: 28
                    height: 26
                    icon: "arrow_forward"
                    accessibleName: qsTr("Forward")
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    enabled: root.canGoForward
                    onClicked: root.forwardRequested()
                }

                ChromeButton {
                    objectName: "reloadButton"
                    KeyLabel {
                        objectName: "keyLabel-reloadButton"
                        iconFontFamily: root.iconFontFamily
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.bottom
                        anchors.topMargin: -4
                        keys: root.keyLabelsShown ? root.labelFor("reload") : ""
                        shown: root.keyLabelsShown
                        colors: root.colors
                    }
                    width: 28
                    height: 26
                    icon: "refresh"
                    accessibleName: qsTr("Reload")
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.reloadRequested()
                }
            }
        }

        // The address is a form control, so it is drawn as the kit draws one:
        // a border at rest rather than a borderless plate that only grows an
        // edge once it is focused. A field the reader can type into says so
        // before they touch it.
        Omarchy.BorderSurface {
            id: addressButton
            objectName: "addressButton"
            property string accessibleName: qsTr("Search or enter address")
            readonly property bool focused: root.statusOpen || addressButton.activeFocus
            width: parent.width
            height: 34
            radius: 2
            color: Style.controlFill(addressButton.focused, addressMouse.containsMouse,
                                     root.colors.text, root.colors.accent)
            borderSpec: Border.controlSpec(addressButton.focused ? "focus" : (
                                                                       addressMouse.containsMouse
                                                                       ? "hover-cursor" : "normal"),
                                           root.colors.text, root.colors.accent)
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: addressButton.accessibleName
            Accessible.onPressAction: root.addressRequested()

            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key
                        === Qt.Key_Space) {
                    root.addressRequested();
                    event.accepted = true;
                }
            }

            KeyLabel {
                objectName: "keyLabel-addressButton"
                iconFontFamily: root.iconFontFamily
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                keys: root.keyLabelsShown ? root.labelFor("open-address") : ""
                shown: root.keyLabelsShown
                colors: root.colors
            }

            // The lock takes the same 18px slot a tab row gives its site chip,
            // so the address and every tab title start on one line.
            Text {
                id: securityGlyph
                objectName: "securityIndicator"
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                horizontalAlignment: Text.AlignHCenter
                text: root.certificateError ? "warning" : (root.secure ? "lock" : "lock_open")
                color: root.certificateError ? root.colors.urgent : root.colors.mutedText
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.iconLarge
                Accessible.role: Accessible.StaticText
                Accessible.name: qsTr("Site information: %1").arg(sitePanel.connectionSentence)

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -5
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.statusOpen = !root.statusOpen
                }
            }

            Text {
                anchors.left: securityGlyph.right
                anchors.leftMargin: 9
                anchors.right: blockedCount.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: root.blank ? qsTr("search or enter address") : String(root.activeUrl).replace(
                                       /^[a-z]+:\/\//, "")
                color: root.blank ? root.colors.mutedText : root.colors.text
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Row {
                id: blockedCount
                objectName: "blockedRequestIndicator"
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                visible: root.refusalTally > 0

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "shield"
                    color: root.colors.mutedText
                    font.family: root.iconFontFamily
                    font.pixelSize: Style.font.iconLarge
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.refusalTally
                    color: root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }
            }

            MouseArea {
                id: addressMouse
                anchors.left: securityGlyph.right
                anchors.right: blockedCount.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                hoverEnabled: true
                cursorShape: Qt.IBeamCursor
                onClicked: {
                    addressButton.forceActiveFocus();
                    root.addressRequested();
                }
            }
        }
    }

    // The section stands between the address and the tab list on anchors of
    // its own rather than in the column above. A column leaves out a child
    // that has no height, and never takes it back when one arrives: pinning
    // the first tab in a Space gave the section its pins, and the column went
    // on placing the next thing where the section was not — the pins landing
    // over the controls at the top of the outline.
    // The list, both halves of it, in a layer of its own: the layer is what
    // the leaving picture is a picture of, and it carries nothing but the
    // list, so the picture lays over the sidebar's own ground rather than
    // bringing a second one.
    Item {
        id: listLayer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: outline.bottom
        anchors.bottom: footer.top

        Flow {
            id: pinnedSection
            objectName: "pinnedList"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: visible ? 12 : 0
            height: childrenRect.height
            visible: !root.privateWindow && root.pinnedCount > 0
            opacity: root.arrivalOpacity
            transform: Translate {
                x: root.arrivalOffset * listLayer.width
            }
            readonly property int capacity: Math.max(3, Math.min(5, Math.floor(width / 56)))
            readonly property int columns: Math.min(root.pinnedCount, capacity)
            spacing: 4

            Repeater {
                model: root.browser ? root.browser.pinnedTabs : null

                TabRow {
                    id: pinnedRow
                    required property int index
                    placeInSection: index
                    readonly property int rowStart: Math.floor(index / pinnedSection.capacity)
                                                    * pinnedSection.capacity
                    readonly property int tabsInRow: Math.min(pinnedSection.capacity,
                                                              root.pinnedCount - rowStart)
                    width: (pinnedSection.width - pinnedSection.spacing * (tabsInRow - 1))
                           / tabsInRow
                    colors: root.colors
                    iconFontFamily: root.iconFontFamily
                    useFavicons: root.useFavicons
                    tintFavicons: root.tintFavicons
                    keyLabel: root.tabLabel(tabId)
                    keyLabelShown: root.keyLabelsShown
                    agent: root.agentOf(tabId)
                    onActivated: function (id) {
                        root.tabActivated(id);
                    }
                    onCloseRequested: function (id) {
                        root.tabCloseRequested(id);
                    }
                    onMuteToggled: function (id) {
                        root.tabMuteToggled(id);
                    }
                    onDragStarted: root.beginTabDrag(pinnedRow)
                    onDragMoved: function (id, sceneX, sceneY) {
                        root.updateTabDrag(pinnedRow, sceneX, sceneY);
                    }
                    onDragEnded: root.endTabDrag(pinnedRow)
                    onMenuRequested: function (id, anchorX, anchorY) {
                        root.tabMenuRequested(id, anchorX, anchorY);
                    }
                    onActiveChanged: if (active)
                                         root.activeTabItem = this
                    Component.onCompleted: if (active)
                                               root.activeTabItem = this
                }
            }
        }

        ScrollView {
            id: tabScroll
            objectName: "tabScroll"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: pinnedSection.bottom
            anchors.bottom: parent.bottom
            anchors.leftMargin: 16
            anchors.topMargin: 12
            anchors.bottomMargin: 12
            // Padding rather than a margin on the trailing side: the scrollbar
            // runs in the view's padding, and a margin would leave it no lane
            // but the rows themselves.
            rightPadding: 16
            clip: true
            opacity: root.arrivalOpacity
            transform: Translate {
                x: root.arrivalOffset * listLayer.width
            }

            ScrollBar.vertical: ChromeScrollBar {
                view: tabScroll
                colors: root.colors
            }

            // A flow rather than a column: a split's two tabs are one row of
            // two, so each takes half the width and the two share a line,
            // while every other row takes the whole width and a line of its
            // own. The pair stands together in the model, which is what puts
            // the two halves on one line.
            Flow {
                id: ordinarySection
                objectName: "ordinaryList"
                width: tabScroll.availableWidth

                // The list gaps its rows as the pinned section gaps its pins, so
                // the two halves of the sidebar read as one list, and the two
                // halves of a split row as a row of the kit's distinct options.
                spacing: pinnedSection.spacing

                Repeater {
                    model: root.atRest || !root.browser ? null : root.browser.unpinnedTabs

                    TabRow {
                        id: ordinaryRow
                        required property int index
                        placeInSection: index
                        width: inSplit ? (parent.width - ordinarySection.spacing) / 2 : parent.width
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        useFavicons: root.useFavicons
                        tintFavicons: root.tintFavicons
                        keyLabel: root.tabLabel(tabId)
                        keyLabelShown: root.keyLabelsShown
                        agent: root.agentOf(tabId)
                        onActivated: function (id) {
                            root.tabActivated(id);
                        }
                        onCloseRequested: function (id) {
                            root.tabCloseRequested(id);
                        }
                        onMuteToggled: function (id) {
                            root.tabMuteToggled(id);
                        }
                        onDragStarted: root.beginTabDrag(ordinaryRow)
                        onDragMoved: function (id, sceneX, sceneY) {
                            root.updateTabDrag(ordinaryRow, sceneX, sceneY);
                        }
                        onDragEnded: root.endTabDrag(ordinaryRow)
                        onMenuRequested: function (id, anchorX, anchorY) {
                            root.tabMenuRequested(id, anchorX, anchorY);
                        }
                        onActiveChanged: if (active)
                                             root.activeTabItem = this
                        onTabBesideChanged: {
                            if (tabBeside)
                                root.besideTabItem = this;
                            else if (root.besideTabItem === this)
                                root.besideTabItem = null;
                        }
                        Component.onCompleted: {
                            if (active)
                                root.activeTabItem = this;
                            if (tabBeside)
                                root.besideTabItem = this;
                        }
                        Component.onDestruction: if (root.besideTabItem === this)
                                                     root.besideTabItem = null
                    }
                }
            }
        }
    }

    ShaderEffectSource {
        id: departure
        x: listLayer.x
        y: listLayer.y
        width: listLayer.width
        height: listLayer.height
        sourceItem: listLayer
        live: false
        // Kept visible, parked past the clip when at rest: a picture that
        // is not drawn is not retaken either.
        opacity: root.arriving ? root.departureOpacity : 1
        transform: Translate {
            x: root.arriving ? root.departureOffset * listLayer.width : -2 * root.width
        }
    }

    // Space switching and settings close the outline.
    Item {
        id: footer
        objectName: "spaceHeading"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 16
        height: 30
        Accessible.role: Accessible.Heading
        Accessible.name: root.privateWindow || !root.browser ? qsTr("Private") :
                                                               root.browser.activeSpaceName

        // Every Space of the reader's is a small square in its colour, and
        // every Agent Space the Agent's mark after them. The row is the
        // switcher: the Space on show is the larger square, with no plate,
        // border or fill around it, so the one coloured thing in the footer
        // stays the Spaces' own colours. A name is what hovering says.
        //
        // Nothing scrolls: a Space the row has no room for is left out and
        // counted, and stays reachable by its key and from the count's menu.
        Row {
            id: spaceSwitcher
            objectName: "spaceSwitcher"
            readonly property int shown: root.spacesShown(width, spaceRepeater.count)
            anchors.left: parent.left
            // The row ends where the first mark standing in the footer
            // begins, so no Space and no count sits under one.
            anchors.right: releaseMark.visible ? releaseMark.left : (extensionMark.visible
                                                                     ? extensionMark.left : (
                                                                           downloadMark.visible
                                                                           ? downloadMark.left : (
                                                                                 syncMark.visible
                                                                                 ? syncMark.left :
                                                                                   settingsButton.left)))
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            visible: !root.privateWindow
            spacing: root.slotSpacing

            Repeater {
                id: spaceRepeater
                model: root.browser ? root.browser.spaces : null

                ChromeButton {
                    id: spaceButton
                    required property string spaceId
                    required property string spaceName
                    required property string spaceColor
                    required property bool active
                    required property int index

                    // An Agent Space wears the Agent's mark: in the Agent
                    // accent while an Agent is attached to one of its tabs,
                    // muted while none is. One of the reader's Spaces an Agent
                    // is working in wears it too while the Agent is attached,
                    // in the Space's own colour and in its square's place, so
                    // it can be read while the Space is away; the square comes
                    // back when the Agent leaves.
                    readonly property var agentWork: root.agentWorkIn(spaceId)
                    readonly property bool agentMade: root.agentSpaceIds.indexOf(spaceId) >= 0
                    readonly property bool showsAgent: agentMade || agentWork.attached

                    objectName: "space-" + spaceId
                    visible: root.spaceInRow(index)
                    width: root.spaceSlot
                    height: 28
                    accessibleName: (active ? qsTr("Current Space: %1").arg(spaceName) : qsTr(
                                                  "Switch to %1").arg(spaceName)) + (
                                        agentWork.attached ? " " + qsTr(
                                                                 "(an Agent is working here)") : (
                                                                 agentMade ? " " + qsTr(
                                                                                 "(Agent Space)") :
                                                                             ""))
                    // The kit derives a control's fill and border from its
                    // foreground, which is why the Space's colour is the
                    // square's alone and never the button's: a coloured Space
                    // once painted its whole plate.
                    foreground: active ? root.colors.text : root.colors.mutedText
                    accent: root.colors.accent
                    background: "transparent"
                    onClicked: root.spaceActivated(spaceId)

                    Rectangle {
                        objectName: "spaceMark-" + spaceButton.spaceId
                        readonly property int side: spaceButton.active ? 12 : 8
                        anchors.centerIn: parent
                        visible: !spaceButton.showsAgent
                        width: side
                        height: side
                        radius: 2
                        color: root.spaceColour(spaceButton.spaceColor)
                        Accessible.ignored: true
                        Behavior on width {
                            enabled: root.easeSpaces
                            NumberAnimation {
                                duration: 160
                                easing.type: Easing.OutCubic
                            }
                        }
                        Behavior on height {
                            enabled: root.easeSpaces
                            NumberAnimation {
                                duration: 160
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    AgentMark {
                        objectName: "spaceAgentMark-" + spaceButton.spaceId
                        anchors.centerIn: parent
                        visible: spaceButton.showsAgent
                        busy: spaceButton.agentWork.busy
                        color: !spaceButton.agentMade ? root.spaceColour(spaceButton.spaceColor) : (
                                                            spaceButton.agentWork.attached
                                                            ? root.colors.agentAccent :
                                                              root.colors.mutedText)
                        font.family: root.iconFontFamily
                        font.pixelSize: Style.font.icon
                    }

                    Omarchy.PanelToolTip {
                        objectName: "spaceNote-" + spaceButton.spaceId
                        visible: spaceButton.hot
                        text: spaceButton.spaceName
                        fontFamily: Style.font.family
                    }

                    KeyLabel {
                        objectName: "keyLabel-space-" + spaceButton.spaceId
                        iconFontFamily: root.iconFontFamily
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.top
                        anchors.bottomMargin: 2
                        keys: root.keyLabelsShown && spaceButton.index < 9 ? root.labelFor(
                                                                                 "select-space",
                                                                                 spaceButton.index
                                                                                 + 1) : ""
                        shown: root.keyLabelsShown
                        colors: root.colors
                    }
                }
            }

            // How many Spaces the row left out, as plain muted text. It opens
            // a menu of them.
            Text {
                id: spaceOverflow
                objectName: "spaceOverflow"
                readonly property int hidden: spaceRepeater.count - spaceSwitcher.shown
                visible: hidden > 0
                width: root.overflowSlot
                height: 28
                text: "+" + hidden
                color: root.colors.mutedText
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                activeFocusOnTab: visible
                Accessible.role: Accessible.Button
                Accessible.name: hidden === 1 ? qsTr("1 more Space") : qsTr("%1 more Spaces").arg(
                                                    hidden)
                Accessible.onPressAction: root.openHiddenSpaces()
                Keys.onReturnPressed: root.openHiddenSpaces()
                Keys.onEnterPressed: root.openHiddenSpaces()
                Keys.onSpacePressed: root.openHiddenSpaces()

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.openHiddenSpaces()
                }
            }
        }

        // A mask says private in the space one word took, and says it in the
        // sidebar's own icon language rather than in shouted capitals.
        Text {
            objectName: "privateBadge"
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            visible: root.privateWindow || !root.browser
            text: "domino_mask"
            color: root.colors.privateAccent
            font.family: root.iconFontFamily
            font.pixelSize: Style.font.iconLarge
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Private window")
        }

        ReleaseMark {
            id: releaseMark
            objectName: "releaseMark"
            anchors.right: downloadMark.visible ? downloadMark.left : (syncMark.visible
                                                                       ? syncMark.left :
                                                                         settingsButton.left)
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            watch: root.releaseWatch
            privateWindow: root.privateWindow
            onNotesRequested: function (notes) {
                root.releaseNotesRequested(notes);
            }
        }

        ExtensionMark {
            id: extensionMark
            objectName: "extensionMark"
            anchors.right: downloadMark.visible ? downloadMark.left : (syncMark.visible
                                                                       ? syncMark.left :
                                                                         settingsButton.left)
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            hosted: root.hostedExtensions
            onClicked: root.extensionRequested(root.extensionMarkRect())
        }

        DownloadMark {
            id: downloadMark
            objectName: "downloadMark"
            anchors.right: syncMark.visible ? syncMark.left : settingsButton.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            model: root.downloads
            savedFileNoticeShowing: root.savedFileNoticeShowing
            onClicked: root.downloadsRequested()
        }

        ChromeButton {
            id: syncMark
            objectName: "syncAvatarButton"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 26
            visible: root.authenticatedSync
            accessibleName: !root.authenticatedSync ? qsTr("Settings") : root.settingsAttention
                                                      ? qsTr("Settings for %1 — needs attention").arg(
                                                            root.sync.login) : !root.activeSync
                                                        ? qsTr("Settings for %1 — Sync paused").arg(
                                                              root.sync.login) : qsTr(
                                                              "Settings for %1").arg(
                                                              root.sync.login)
            foreground: root.sync && root.sync.errorMessage.length > 0 ? root.colors.urgent :
                                                                         root.colors.mutedText

            accent: root.colors.accent
            onClicked: root.settingsRequested()

            Rectangle {
                id: avatarClip
                objectName: "syncAvatarClip"
                anchors.centerIn: parent
                width: parent.height - 4
                height: width
                radius: width / 2
                color: root.colors.separator
                clip: true

                Text {
                    objectName: "syncAvatarMonogram"
                    anchors.centerIn: parent
                    visible: avatarImage.status !== Image.Ready
                    text: root.sync && root.sync.login.length > 0 ? root.sync.login.charAt(0).toUpperCase() :
                                                                    ""
                    color: root.activeSync ? root.colors.text : root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                    font.bold: true
                }

                Image {
                    id: avatarImage
                    anchors.fill: parent
                    source: root.sync && root.sync.avatarPath.length > 0 ? "file://"
                                                                           + root.sync.avatarPath :
                                                                           ""
                    sourceSize.width: 48
                    sourceSize.height: 48
                    fillMode: Image.PreserveAspectCrop
                    visible: false
                    smooth: true
                }

                MultiEffect {
                    objectName: "syncAvatarEffect"
                    anchors.fill: parent
                    source: avatarImage
                    visible: avatarImage.status === Image.Ready
                    saturation: root.activeSync ? 0 : -1
                }
            }

            Rectangle {
                objectName: "syncActivityDot"
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 6
                height: 6
                radius: 3
                color: root.sync && root.sync.errorMessage.length > 0 ? root.colors.urgent : root.activeSync
                                                                        ? root.colors.accent :
                                                                          root.colors.mutedText

                Accessible.ignored: true
            }
        }

        ChromeButton {
            id: settingsButton
            objectName: "settingsButton"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 26
            visible: !root.authenticatedSync
            icon: "settings"
            accessibleName: root.settingsAttention ? qsTr(
                                                         "Browsing settings and downloads — needs attention") :
                                                     qsTr("Browsing settings and downloads")
            fontFamily: root.iconFontFamily
            foreground: root.settingsAttention ? root.colors.text : root.colors.mutedText
            accent: root.colors.accent
            onClicked: root.settingsRequested()

            // The same colour the notice inside is drawn in, so the mark out
            // here and the thing it stands for read as one.
            Rectangle {
                objectName: "settingsAttentionDot"
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 3
                anchors.topMargin: 3
                width: 6
                height: 6
                radius: 3
                visible: root.settingsAttention
                color: root.colors.urgent
                Accessible.ignored: true
            }
        }
    }

    Rectangle {
        id: downloadDetail
        objectName: "downloadDetail"
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: footer.top
        anchors.bottomMargin: 8
        width: Math.min(280, Math.max(180, root.width - 32))
        height: detailLines.height + 16
        radius: 2
        z: 5
        visible: detailLift.showing
        // Rises from the mark it was asked from, and sinks back to it.
        transform: SheetLift {
            id: detailLift
            shown: downloadMark.detailRequested && downloadMark.running > 0
            ease: root.easeSpaces
            distance: 8
        }
        opacity: detailLift.progress
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent

        Column {
            id: detailLines
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 8
            spacing: 4

            Repeater {
                // The whole list, settled rows and all, but only while the
                // detail is on screen: a Space's Download records are in here
                // too and none of them is a running download.
                model: downloadDetail.visible ? root.downloads : null

                Item {
                    id: detailLine
                    required property int index
                    required property string fileName
                    required property real fraction
                    required property bool running

                    objectName: "downloadDetail-" + index
                    readonly property string name: detailLine.fileName
                    readonly property string progressLabel: downloadMark.progressLabelFor(
                                                                detailLine.fraction)
                    // A recorded download is in the same list; the mark reports
                    // what is running, so a settled row takes no room.
                    visible: detailLine.running
                    width: detailLines.width
                    height: detailLine.running ? lineName.implicitHeight : 0

                    Text {
                        id: lineProgress
                        anchors.right: parent.right
                        anchors.baseline: lineName.baseline
                        text: detailLine.progressLabel
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.ignored: true
                    }

                    Text {
                        id: lineName
                        anchors.left: parent.left
                        anchors.right: lineProgress.left
                        anchors.rightMargin: 8
                        anchors.top: parent.top
                        text: detailLine.name
                        elide: Text.ElideMiddle
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.role: Accessible.StaticText
                        Accessible.name: detailLine.name + " · " + detailLine.progressLabel
                    }
                }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        visible: root.statusOpen
        z: 4
        onClicked: root.statusOpen = false
    }

    SiteInformationPanel {
        id: sitePanel
        objectName: "siteInformationPanel"
        anchors.left: parent.left
        anchors.leftMargin: 16
        y: outline.y + addressButton.y + addressButton.height + 8
        width: Math.min(320, Math.max(200, root.width - 32))
        z: 5
        // Unfolds from the address it reports on, and folds back into it.
        transform: SheetLift {
            id: siteLift
            shown: sitePanel.open
            ease: root.easeSpaces
            distance: -8
        }
        opacity: siteLift.progress
        visible: siteLift.showing
        colors: root.colors
        browser: root.browser
        cookiePolicy: root.cookiePolicy
        activeUrl: root.activeUrl
        blank: root.blank
        privateWindow: root.privateWindow
        connectionState: root.connectionState
        lookupFailedBy: root.lookupFailedBy
        upgradedByHttpsOnly: root.upgradedByHttpsOnly
        certificateDecisionsAvailable: root.certificateDecisionsAvailable
        certificateChain: root.certificateChain
        pageCertificatesAvailable: root.pageCertificatesAvailable
        thirdPartyCookieControlAvailable: root.thirdPartyCookieControlAvailable
        siteDataOnDisk: root.siteDataOnDisk
        insecureContentBlocked: root.insecureContentBlocked
        blocker: root.blocker
        siteDataEntries: root.siteDataEntries
        retainedDataEntries: root.retainedDataEntries
        siteDataGeneration: root.siteDataGeneration
        open: root.statusOpen

        onActionRequested: function (action) {
            root.siteActionRequested(action);
        }
    }
}
