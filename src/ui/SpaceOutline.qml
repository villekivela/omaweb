import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import Omaweb
import qs.Commons
import qs.Ui as Omarchy
import "DevicePixels.mjs" as DevicePixels
import "SpaceColour.mjs" as SpaceColour
import "AddressTrigger.mjs" as AddressTrigger
import "SidebarFill.mjs" as SidebarFill
import "SiteChip.mjs" as SiteChip

Rectangle {
    id: root
    objectName: "spaceOutline"
    // The display's pixels per logical pixel, for resting on whole ones.
    readonly property real pixelRatio: Window.window ? Window.window.devicePixelRatio : 1
    // The sidebar's one left edge: the address box and the pins start on it, and
    // each ordinary tab's favicon stands 4 px in from it (#813). On a whole
    // pixel of the display, so a pinned tab's icon centred in its tile rests on
    // one.
    readonly property real listMargin: DevicePixels.snap(16, pixelRatio)
    // The one gap between the sidebar's blocks: the navigation row, the
    // address trigger, each pin, and the tab list. Ordinary rows stand closer,
    // as one list (#779).
    readonly property int blockGap: 8
    readonly property int rowGap: 4

    property var colors
    property string iconFontFamily
    property var browser
    // The Glance standing over a tab of this Space, or null: its `tabId`, and
    // the page's `iconUrl`, `url` and `title`. It is the window's, and the
    // outline only says which row it stands over (ADR 0041).
    property var glance: null
    signal glanceKeepRequested
    // The window's keymap, for the labels Primary shows, and whether it is
    // being held for them.
    property var keyMap: null
    property bool keyLabelsShown: false
    // What this window's Space is hosting, as `{ key, name, id, popupUrl }`.
    property var hostedExtensions: []
    property bool privateWindow: false
    property bool collapsed: false
    // Whether the sidebar stands against the window's right edge, which puts
    // its divider on its left and turns the glyph that hides it round.
    property bool onRight: false
    property bool floating: false
    // Whether Site information is open, which the address is drawn focused
    // for, and the verdict it gives, which the Site information button speaks.
    property bool siteInformationOpen: false
    property string siteInformationVerdict: ""
    // The address, which Site information floats from.
    readonly property Item addressItem: addressButton
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
    // The window's Problem recording, which the footer marks while it runs.
    property var problemRecording: null
    signal problemReportRequested
    property bool savedFileNoticeShowing: false
    // What the Agents are doing, as the core reports it: each Agent tab by id,
    // with its `spaceId`, the connection's `name`, and `busy` while one of its
    // commands is in flight. And the Spaces an Agent made that the reader has
    // not taken over.
    property var agentActivity: ({})
    property var agentSpaceIds: []
    // The Space whose name is held on show as if a pointer rested on its
    // square. Only the introductory film's recording sets it, since the film
    // has no pointer (Main.qml's holdSpaceNote).
    property string heldSpaceId: ""
    // The tab whose row the introductory film's recording holds as a pointer
    // resting on it would (Main.qml's holdTabRow). Empty holds none.
    property string heldTabId: ""

    // PROTOTYPE #838, throwaway: three places for the footer's marks and
    // Settings, set by the UI lab's `--footer-variant a|b|c` and
    // `--footer-items all|one|none`. Empty is the footer as on main. The items
    // are stubs, so the lab needs no downloads or hosted extensions.
    //   a: the issue as written. Settings in the top row, a status pill above
    //      the Space row, the extension mark in the address box.
    //   b: the marks as rows docked at the end of the tab list, labelled like
    //      tabs; Settings stays at the footer's right end.
    //   c: every mark, the extension's too, as a tray at the address box's
    //      right end; Settings in the top row.
    property string labFooterVariant: ""
    property string labFooterItems: "all"
    readonly property bool protoA: root.labFooterVariant === "a"
    readonly property bool protoB: root.labFooterVariant === "b"
    readonly property bool protoC: root.labFooterVariant === "c"
    readonly property bool protoSettingsOnTop: root.protoA || root.protoC
    readonly property var protoStatus: {
        if (root.labFooterItems === "none")
            return [];
        const all = [
                  {
                      "key": "recording",
                      "icon": "radio_button_checked",
                      "label": qsTr("Recording · 3 min left"),
                      "urgent": true
                  },
                  {
                      "key": "downloads",
                      "icon": "download",
                      "label": qsTr("2 downloads · 40%"),
                      "urgent": false
                  },
                  {
                      "key": "release",
                      "icon": "upgrade",
                      "label": qsTr("0.19.1 is out"),
                      "urgent": false
                  }
              ];
        if (root.labFooterItems === "one")
            return [all[1]];
        // A Private window records nothing and is told of no release.
        return root.privateWindow ? [all[1]] : all;
    }
    readonly property var protoExtension: root.labFooterItems === "all" && !root.privateWindow ? [
                                                                                                     {
                                                                                                         "key": "extension",
                                                                                                         "icon": "extension",
                                                                                                         "label": qsTr(
                                                                                                                      "uBlock Origin"),
                                                                                                         "urgent": false
                                                                                                     }
                                                                                                 ] : []

    // How wide a Space is in the footer, and the count of those left out.
    readonly property int spaceSlot: 18
    readonly property int slotSpacing: 4
    readonly property int overflowSlot: 28

    // PROTOTYPE #838: how wide the row's Spaces are, for centring it.
    function protoRowWidth(shown) {
        return shown * root.spaceSlot + Math.max(0, shown - 1) * root.slotSpacing;
    }

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

    // Whether the reader's Spaces are drawn in their colours, which Settings
    // turns off for the muted text colour.
    property bool spaceColours: true

    function spaceColour(name) {
        return SpaceColour.drawn(root.colors, name, root.spaceColours);
    }

    function glanceOf(tabId) {
        return root.glance !== null && root.glance.tabId === tabId ? root.glance : null;
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
    // Whether a tab of any Space is in a Tab window, which a Space at rest may
    // still list.
    readonly property bool hasTabWindows: browser ? browser.tabWindows.length > 0 : false

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
    // A swipe across the sidebar takes the list with the fingers, one pixel
    // for one, and the neighbouring Space's own rows come in beside it: this
    // is how far, in pixels, negative towards the next Space. It follows only
    // where a switch may move; otherwise the swipe still switches once it is
    // far enough, and the Space arrives settled.
    readonly property bool swipeFollows: root.easeSpaces && root.spacesCanMove
    property real swipeOffset: 0
    // The leaving list's picture is held while the list is away from rest,
    // and taken again once it is back.
    onSwipeOffsetChanged: if (swipeOffset === 0)
                              retakeDeparture()
    // What a switch the swipe or the wheel asked for hands the arrival: the
    // direction it went, and how far along the list already stands, as a
    // fraction of its width, so the slide carries on from where the fingers
    // left it rather than starting again.
    property var swipeHandoff: null
    readonly property bool swipeAllowed: !root.privateWindow && root.browser !== null
                                         && spaceRepeater.count > 1 && root.draggedRow === null
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
        if (!root.arriving && root.spacesCanMove && root.swipeOffset === 0)
            departureRetake.restart();
    }
    Timer {
        id: departureRetake
        interval: 50
        onTriggered: if (!root.arriving && root.swipeOffset === 0)
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
            const handoff = root.swipeHandoff;
            root.settledSpaceRow = to;
            root.swipeHandoff = null;
            swipeSpring.stop();
            root.swipeOffset = 0;
            if (!root.easeSpaces || from === to) {
                root.arriving = false;
                return;
            }
            spaceArrival.stop();
            root.settledTabItem = null;
            root.settledTabId = "";
            root.settledTabX = -1;
            root.settledTabY = -1;
            // A swipe or a notch goes the way the hand went, which wraps at
            // the ends of the footer where the rows do not.
            root.switchDirection = handoff ? handoff.direction : (to > from ? 1 : -1);
            const carried = handoff ? handoff.offset : 0;
            root.arrivalOffset = root.switchDirection + carried;
            root.arrivalOpacity = carried !== 0 ? 1 : 0;
            root.departureOffset = carried;
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
    // What the connection is, as the engine drawing the page reports it. The
    // outline never works this out from the address: an address is what was
    // asked for, and a lock drawn from one is a claim nothing checked.
    property string connectionState: "internal"
    readonly property bool secure: root.connectionState === "secure"
    readonly property bool certificateError: root.connectionState === "certificate-error"
    readonly property bool blank: String(activeUrl).length === 0 || String(activeUrl)
                                  === "about:blank"

    // The ordinary row of a tab of the Space on show, or null.
    function tabRow(tabId) {
        for (let index = 0; index < ordinaryRows.count; ++index) {
            const row = ordinaryRows.itemAt(index);
            if (row && row.tabId === tabId)
                return row;
        }
        return null;
    }

    // The lock, which opens Site information.
    readonly property Item siteInformationButton: securityGlyph

    // The footer's button for a Space, or null when the Space has none.
    function spaceButton(spaceId) {
        for (let index = 0; index < spaceRepeater.count; ++index) {
            const button = spaceRepeater.itemAt(index);
            if (button && button.spaceId === spaceId)
                return button;
        }
        return null;
    }

    signal addressRequested
    signal downloadsRequested
    // Where the popup should come from, in window coordinates. A panel that
    // grows out of the mark is the mark, opened, rather than a second thing
    // that appeared over the page. The keyboard asks for the same rectangle,
    // so the command and the pointer open from the same place.
    function extensionMarkRect() {
        return footerMarks.extensionRect();
    }
    signal extensionRequested(rect origin)
    signal releaseNotesRequested(url notes)
    // The Site information button asks the window for the card, and a click on
    // the outline while it is open puts it away.
    signal siteInformationRequested
    signal siteInformationDismissed
    signal tabActivated(string tabId)
    signal tabCloseRequested(string tabId)
    signal agentStopRequested(string tabId)
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
    // Set while a page that is already in place becomes the tab on show, as a
    // kept Glance's does: its row is new, but the page did not come from
    // anywhere, so nothing arrives.
    property bool arrivalHeld: false
    // The row a kept Glance landed in, and how strongly it is lit.
    property string washTabId: ""
    property real washLevel: 0

    function washArrival(tabId) {
        root.washTabId = tabId;
        root.washLevel = 0.35;
        washFade.restart();
    }

    NumberAnimation {
        id: washFade
        target: root
        property: "washLevel"
        to: 0
        duration: 300
        easing.type: Easing.OutCubic
        onFinished: root.washTabId = ""
    }
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
        if (!easeSpaces || arriving || fromY < 0 || focusMoved || arrivalHeld)
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
        root.pageFocusRequested();
        event.accepted = true;
    }

    color: sidebarDither.fill
    // The sidebar meets the page with no edge of its own. Said, because an
    // unset border still reads as a pixel wide, and the dither would start its
    // ground inside it and leave the desktop showing down the seam.
    border.width: 0
    clip: true

    GroundDither {
        id: sidebarDither
        objectName: "sidebarDither"
        ground: root.floating ? "transparent" : root.colors.sidebar
    }

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

    Column {
        id: outline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.listMargin
        anchors.rightMargin: 16
        anchors.topMargin: 10
        spacing: root.blockGap

        // The navigation controls hold the row the Space heading used to: the
        // commands that act on the page open the outline, and the browsing
        // identity closes it from the footer.
        Item {
            objectName: "sidebarNavigation"
            width: parent.width
            height: 32
            // Its key labels hang below it into the address row, and the
            // column paints a later sibling over an earlier one.
            z: 1

            DragHandler {
                target: null
                onActiveChanged: if (active)
                                     root.windowMoveRequested()
            }

            Row {
                id: outlineControls
                objectName: "outlineControls"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ChromeButton {
                    id: collapseButton
                    objectName: "collapseButton"
                    width: 28
                    height: 26
                    icon: root.onRight ? (root.collapsed ? "right_panel_open" :
                                                           "right_panel_close") : (root.collapsed
                                                                                   ? "left_panel_open" :
                                                                                     "left_panel_close")
                    accessibleName: root.collapsed ? qsTr("Show sidebar") : qsTr("Hide sidebar")
                    fontFamily: root.iconFontFamily
                    accent: root.colors.accent
                    onClicked: root.sidebarToggled()
                }

                ChromeButton {
                    id: commandScopeButton
                    objectName: "commandScopeButton"
                    width: 28
                    height: 26
                    icon: "search"
                    accessibleName: qsTr("Search commands")
                    fontFamily: root.iconFontFamily
                    accent: root.colors.accent
                    onClicked: root.commandScopeRequested()
                }

                // PROTOTYPE #838: Settings in the top row, variants a and c.
                SettingsButton {
                    objectName: "prototypeTopSettings"
                    visible: root.protoSettingsOnTop
                    colors: root.colors
                    iconFontFamily: root.iconFontFamily
                    sync: root.sync
                    authenticatedSync: root.authenticatedSync
                    activeSync: root.activeSync
                    settingsAttention: root.settingsAttention
                    pixelRatio: root.pixelRatio
                    onClicked: root.settingsRequested()
                }
            }

            Row {
                id: outlineNavigation
                objectName: "outlineNavigation"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ChromeButton {
                    id: backButton
                    objectName: "backButton"
                    width: 28
                    height: 26
                    icon: "arrow_back"
                    accessibleName: qsTr("Back")
                    fontFamily: root.iconFontFamily
                    accent: root.colors.accent
                    enabled: root.canGoBack
                    onClicked: root.backRequested()
                }

                ChromeButton {
                    id: forwardButton
                    objectName: "forwardButton"
                    width: 28
                    height: 26
                    icon: "arrow_forward"
                    accessibleName: qsTr("Forward")
                    fontFamily: root.iconFontFamily
                    accent: root.colors.accent
                    enabled: root.canGoForward
                    onClicked: root.forwardRequested()
                }

                ChromeButton {
                    id: reloadButton
                    objectName: "reloadButton"
                    width: 28
                    height: 26
                    icon: "refresh"
                    accessibleName: qsTr("Reload")
                    fontFamily: root.iconFontFamily
                    accent: root.colors.accent
                    onClicked: root.reloadRequested()
                }
            }

            NavigationKeyLabel {
                objectName: "keyLabel-collapseButton"
                button: collapseButton
                row: outlineControls
                command: "toggle-sidebar"
            }

            NavigationKeyLabel {
                objectName: "keyLabel-commandScopeButton"
                button: commandScopeButton
                row: outlineControls
                command: "command-scope"
            }

            NavigationKeyLabel {
                objectName: "keyLabel-backButton"
                button: backButton
                row: outlineNavigation
                command: "back"
            }

            NavigationKeyLabel {
                objectName: "keyLabel-forwardButton"
                button: forwardButton
                row: outlineNavigation
                command: "forward"
            }

            NavigationKeyLabel {
                objectName: "keyLabel-reloadButton"
                button: reloadButton
                row: outlineNavigation
                command: "reload"
            }
        }

        // The address trigger is a quiet line, not a field: where the page is, at a
        // glance, and the way into the Omnibar. It has no border at rest or
        // under the pointer and rests on the fill a pin rests on, so the two
        // read as one surface (#779). Holding the keyboard, it takes the
        // kit's focus border, which nothing else on it would show.
        Omarchy.BorderSurface {
            id: addressButton
            objectName: "addressButton"
            property string accessibleName: qsTr("Search or enter address")
            readonly property bool focused: root.siteInformationOpen || addressButton.activeFocus
            width: parent.width
            height: Style.space(36)
            radius: Style.cornerRadius
            color: SidebarFill.fill(root.colors.text, addressHover.hovered)
            readonly property var focusBorder: Border.controlSpec("focus", root.colors.text,
                                                                  root.colors.accent)
            borderSpec: addressButton.focused ? addressButton.focusBorder : Border.none()
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: addressButton.accessibleName

            // Anywhere on the line, the Site information button included.
            HoverHandler {
                id: addressHover
            }
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
                anchors.rightMargin: Style.space(4)
                anchors.verticalCenter: parent.verticalCenter
                keys: root.keyLabelsShown ? root.labelFor("open-address") : ""
                shown: root.keyLabelsShown
                colors: root.colors
            }

            // The Site information button takes an 18px slot, and a tab row sets its title in
            // past the same inset and a chip (TabRow's titleInset), so the address and every tab
            // title start on one line.
            Text {
                id: securityGlyph
                objectName: "securityIndicator"
                anchors.left: parent.left
                anchors.leftMargin: Style.space(8)
                anchors.verticalCenter: parent.verticalCenter
                width: Style.space(18)
                horizontalAlignment: Text.AlignHCenter
                text: root.certificateError ? "warning" : (root.secure ? "tune" : "lock_open")
                color: root.certificateError ? root.colors.urgent : root.colors.mutedText
                font.family: root.iconFontFamily
                font.pixelSize: SiteChip.side(Style.font.caption)
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Site information: %1").arg(root.siteInformationVerdict)
                Accessible.onPressAction: root.siteInformationRequested()

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -5
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.siteInformationOpen ? root.siteInformationDismissed() :
                                                          root.siteInformationRequested()
                }
            }

            Text {
                objectName: "addressText"
                anchors.left: securityGlyph.right
                anchors.leftMargin: Style.space(9)
                anchors.right: parent.right
                anchors.rightMargin: Style.space(9) + (addressTray.width > 0 ? addressTray.width
                                                                               + 4 : 0)
                anchors.verticalCenter: parent.verticalCenter
                text: root.blank ? qsTr("search or enter address") : AddressTrigger.shown(
                                       root.activeUrl)
                color: root.blank || !addressHover.hovered ? root.colors.mutedText :
                                                             root.colors.text
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            MouseArea {
                id: addressMouse
                anchors.left: securityGlyph.right
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    addressButton.forceActiveFocus();
                    root.addressRequested();
                }
            }

            // PROTOTYPE #838: the address box's right end. Variant a puts the
            // extension mark here alone; variant c puts every mark here.
            Row {
                id: addressTray
                objectName: "prototypeAddressTray"
                anchors.right: parent.right
                anchors.rightMargin: Style.space(4)
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Repeater {
                    model: root.protoA ? root.protoExtension : root.protoC ? root.protoStatus.concat(
                                                                                 root.protoExtension) :
                                                                             []

                    ChromeButton {
                        required property var modelData
                        objectName: "prototypeTray-" + modelData.key
                        width: 24
                        height: 26
                        icon: modelData.icon
                        iconSize: Style.font.icon
                        foreground: modelData.urgent ? root.colors.urgent : restingForeground
                        accessibleName: modelData.label
                        fontFamily: root.iconFontFamily
                        accent: root.colors.accent
                    }
                }
            }
        }
    }

    // The labels belong to the navigation row rather than to their
    // buttons: a disabled button is drawn at 35%, and a label that
    // inherited that would let the address show through it. Each hangs
    // 4px into the row below, centred on its button.
    component NavigationKeyLabel: KeyLabel {
        required property Item button
        required property Item row
        required property string command

        iconFontFamily: root.iconFontFamily
        x: DevicePixels.snap(row.x + button.x + (button.width - width) / 2, root.pixelRatio)
        y: DevicePixels.snap(row.y + button.y + button.height - 4, root.pixelRatio)
        keys: root.keyLabelsShown ? root.labelFor(command) : ""
        shown: root.keyLabelsShown
        colors: root.colors
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
        // PROTOTYPE #838: the list ends at the pill's top, or at the docked rows'.
        anchors.bottom: protoPill.visible ? protoPill.top : protoRows.visible ? protoRows.top :
                                                                                footer.top

        // The space the rows leave empty moves the window, as the navigation
        // strip does (#179). A row holds its press for a reorder, which this
        // cannot take from it.
        DragHandler {
            target: null
            onActiveChanged: if (active)
                                 root.windowMoveRequested()
        }

        Flow {
            id: pinnedSection
            objectName: "pinnedList"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: root.listMargin
            anchors.rightMargin: 16
            anchors.topMargin: visible ? root.blockGap : 0
            height: childrenRect.height
            visible: !root.privateWindow && root.pinnedCount > 0
            opacity: root.arrivalOpacity
            transform: Translate {
                x: root.arrivalOffset * listLayer.width + root.swipeOffset
            }
            readonly property int capacity: Math.max(3, Math.min(5, Math.floor(width / 56)))
            spacing: root.blockGap

            Repeater {
                model: root.browser ? root.browser.pinnedTabs : null

                TabRow {
                    id: pinnedRow
                    required property int index
                    placeInSection: index
                    width: (pinnedSection.width - pinnedSection.spacing * (pinnedSection.capacity
                                                                           - 1)) / pinnedSection.capacity
                    colors: root.colors
                    iconFontFamily: root.iconFontFamily
                    useFavicons: root.useFavicons
                    tintFavicons: root.tintFavicons
                    keyLabel: root.tabLabel(tabId)
                    keyLabelShown: root.keyLabelsShown
                    agent: root.agentOf(tabId)
                    held: root.heldTabId === tabId
                    onAgentStopRequested: function (id) {
                        root.agentStopRequested(id);
                    }
                    glance: root.glanceOf(tabId)
                    onGlanceKeepRequested: root.glanceKeepRequested()
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
            anchors.topMargin: root.blockGap
            anchors.bottomMargin: 12
            // The view spans the sidebar, and the list's margins are inside
            // it: a row's wash, and the pointer it answers, reach past them to
            // the sidebar's edges, as the Omnibar's rows reach its panel's. A
            // view clips its content and delivers no press beyond its own
            // edges, so margins outside it would cut both off. The scrollbar
            // runs over the trailing margin, where a row has only its wash.
            clip: true
            opacity: root.arrivalOpacity
            transform: Translate {
                x: root.arrivalOffset * listLayer.width + root.swipeOffset
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
                leftPadding: root.listMargin
                rightPadding: 16
                readonly property real rowWidth: width - leftPadding - rightPadding
                // Where a split's two halves meet: half way across the gap between
                // them, on a whole pixel of the display.
                readonly property real splitMiddle: DevicePixels.snap(leftPadding + rowWidth / 2,
                                                                      root.pixelRatio)

                // The halves of a split keep the gap between them, and their
                // washes meet across it.
                spacing: root.rowGap

                // At rest the tabs a Tab window shows are still listed, and the
                // blank one standing in for a page is not.
                Repeater {
                    id: ordinaryRows
                    model: !root.browser || (root.atRest && !root.hasTabWindows) ? null :
                                                                                   root.browser.unpinnedTabs

                    TabRow {
                        id: ordinaryRow
                        required property int index
                        visible: !root.atRest || poppedOut
                        placeInSection: index
                        width: inSplit ? (ordinarySection.rowWidth - ordinarySection.spacing) / 2 :
                                         ordinarySection.rowWidth
                        trailingHalf: inSplit && x > ordinarySection.leftPadding
                        // A split's halves meet in the middle of the gap
                        // between them, and reach their own edges.
                        reachLeft: trailingHalf ? x - ordinarySection.splitMiddle :
                                                  ordinarySection.leftPadding
                        reachRight: inSplit && !trailingHalf ? ordinarySection.splitMiddle - x
                                                               - width : ordinarySection.rightPadding
                        colors: root.colors
                        iconFontFamily: root.iconFontFamily
                        useFavicons: root.useFavicons
                        tintFavicons: root.tintFavicons
                        keyLabel: root.tabLabel(tabId)
                        keyLabelShown: root.keyLabelsShown
                        agent: root.agentOf(tabId)
                        held: root.heldTabId === tabId
                        onAgentStopRequested: function (id) {
                            root.agentStopRequested(id);
                        }
                        glance: root.glanceOf(tabId)
                        onGlanceKeepRequested: root.glanceKeepRequested()
                        arrivalWash: tabId === root.washTabId ? root.washLevel : 0
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

    // The rows of a Space either side of the one on show, as a swipe brings
    // them in from `side`: 1 the right, the next Space, -1 the left, the
    // previous one. They are the Space's own, read from the store before the
    // swipe starts, laid out as the list lays out its own, and take no input.
    // The sidebar's edge cuts them off as it cuts off a Space arriving.
    component NeighbourList: Item {
        id: neighbourList

        property var neighbour: null
        property int side: 1

        x: listLayer.x + side * listLayer.width + root.swipeOffset
        y: listLayer.y
        width: listLayer.width
        height: listLayer.height
        visible: root.swipeOffset * side < 0
        enabled: false
        Accessible.ignored: true

        // Built between frames, as Qt incubates what an asynchronous loader
        // makes, and taken down before the rows change so the list never
        // builds them inside the change.
        Connections {
            target: neighbourList.neighbour
            function onRowsAboutToChange() {
                neighbourContent.active = false;
            }
            function onRowsChanged() {
                neighbourContent.active = true;
            }
        }

        Loader {
            id: neighbourContent
            anchors.fill: parent
            asynchronous: true
            sourceComponent: Item {
                Flow {
                    id: neighbourPins
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: root.listMargin
                    anchors.rightMargin: 16
                    anchors.topMargin: neighbourPinRows.count > 0 ? 12 : 0
                    height: childrenRect.height
                    readonly property int capacity: Math.max(3, Math.min(5, Math.floor(width / 56)))
                    spacing: 4

                    Repeater {
                        id: neighbourPinRows
                        model: neighbourList.neighbour ? neighbourList.neighbour.pinnedTabs : null

                        TabRow {
                            objectName: "neighbour-" + tabId
                            width: (neighbourPins.width - neighbourPins.spacing * (
                                        neighbourPins.capacity - 1)) / neighbourPins.capacity
                            colors: root.colors
                            iconFontFamily: root.iconFontFamily
                            useFavicons: root.useFavicons
                            tintFavicons: root.tintFavicons
                        }
                    }
                }

                Item {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: neighbourPins.bottom
                    anchors.bottom: parent.bottom
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    clip: true

                    Flow {
                        id: neighbourRows
                        width: parent.width
                        leftPadding: root.listMargin
                        rightPadding: 16
                        readonly property real rowWidth: width - leftPadding - rightPadding
                        readonly property real splitMiddle: DevicePixels.snap(leftPadding
                                                                              + rowWidth / 2,
                                                                              root.pixelRatio)
                        spacing: neighbourPins.spacing

                        Repeater {
                            model: neighbourList.neighbour ? neighbourList.neighbour.unpinnedTabs :
                                                             null

                            TabRow {
                                objectName: "neighbour-" + tabId
                                width: inSplit ? (neighbourRows.rowWidth - neighbourRows.spacing)
                                                 / 2 : neighbourRows.rowWidth
                                trailingHalf: inSplit && x > neighbourRows.leftPadding
                                reachLeft: trailingHalf ? x - neighbourRows.splitMiddle :
                                                          neighbourRows.leftPadding
                                reachRight: inSplit && !trailingHalf ? neighbourRows.splitMiddle
                                                                       - x - width :
                                                                       neighbourRows.rightPadding
                                colors: root.colors
                                iconFontFamily: root.iconFontFamily
                                useFavicons: root.useFavicons
                                tintFavicons: root.tintFavicons
                            }
                        }
                    }
                }
            }
        }
    }

    NeighbourList {
        objectName: "previousSpaceList"
        neighbour: root.browser ? root.browser.previousSpace : null
        side: -1
    }

    NeighbourList {
        objectName: "nextSpaceList"
        neighbour: root.browser ? root.browser.nextSpace : null
        side: 1
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
            anchors.right: root.protoSettingsOnTop ? parent.right : root.labFooterVariant.length
                                                     > 0 || !footerMarks.anyShown
                                                     ? settingsButton.left : footerMarks.left
            anchors.rightMargin: root.protoSettingsOnTop ? 0 : 8
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            visible: !root.privateWindow
            spacing: root.slotSpacing
            // PROTOTYPE #838: with the footer to itself, the row is centred
            // while every Space fits and fills the width when they do not.
            leftPadding: root.protoSettingsOnTop && shown === spaceRepeater.count ? Math.max(0, (
                                                                                                 width - root.protoRowWidth(
                                                                                                     shown)) / 2) :
                                                                                    0

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
                    hasCursor: root.heldSpaceId === spaceId
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
            anchors.left: root.protoSettingsOnTop ? undefined : parent.left
            anchors.horizontalCenter: root.protoSettingsOnTop ? parent.horizontalCenter : undefined
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

        FooterMarks {
            id: footerMarks
            objectName: "footerMarks"
            visible: root.labFooterVariant.length === 0
            anchors.right: settingsButton.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            problemRecording: root.problemRecording
            releaseWatch: root.releaseWatch
            privateWindow: root.privateWindow
            hostedExtensions: root.hostedExtensions
            downloads: root.downloads
            savedFileNoticeShowing: root.savedFileNoticeShowing
            onProblemReportRequested: root.problemReportRequested()
            onReleaseNotesRequested: function (notes) {
                root.releaseNotesRequested(notes);
            }
            onExtensionsClicked: root.extensionRequested(root.extensionMarkRect())
            onDownloadsRequested: root.downloadsRequested()
        }

        SettingsButton {
            id: settingsButton
            visible: !root.protoSettingsOnTop
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            sync: root.sync
            authenticatedSync: root.authenticatedSync
            activeSync: root.activeSync
            settingsAttention: root.settingsAttention
            pixelRatio: root.pixelRatio
            onClicked: root.settingsRequested()
        }
    }

    // PROTOTYPE #838, variant a: the status pill. It spans the sidebar inside
    // the margins just above the Space row and rises from the footer as a
    // sheet lifts. One item is its icon and a label; several are icons.
    Rectangle {
        id: protoPill
        objectName: "prototypeStatusPill"
        readonly property bool single: root.protoStatus.length === 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.leftMargin: root.listMargin
        anchors.rightMargin: 16
        anchors.bottomMargin: 6
        height: 30
        radius: Style.cornerRadius
        color: SidebarFill.fill(root.colors.text, false)
        visible: pillLift.showing
        opacity: pillLift.progress
        transform: SheetLift {
            id: pillLift
            shown: root.protoA && root.protoStatus.length > 0
            ease: root.easeSpaces
            distance: 8
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: root.protoA ? root.protoStatus : []

                ProtoStatusItem {
                    labelled: protoPill.single
                    width: labelled ? protoPill.width - 4 : 28
                    height: 26
                }
            }
        }
    }

    // PROTOTYPE #838, variant b: the marks as rows docked at the end of the
    // tab list, under a rule, each labelled as a tab is.
    Column {
        id: protoRows
        objectName: "prototypeStatusRows"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.leftMargin: root.listMargin
        anchors.rightMargin: 16
        anchors.bottomMargin: 6
        visible: root.protoB && root.protoStatus.length + root.protoExtension.length > 0
        spacing: root.rowGap

        Rectangle {
            width: parent.width
            height: 1
            color: root.colors.separator
        }

        Repeater {
            model: root.protoB ? root.protoStatus.concat(root.protoExtension) : []

            ProtoStatusItem {
                labelled: true
                width: protoRows.width
                height: 28
            }
        }
    }

    // PROTOTYPE #838: one status item, its icon alone or with its label.
    component ProtoStatusItem: Item {
        id: statusItem
        required property var modelData
        property bool labelled: false
        objectName: "prototypeStatus-" + modelData.key

        ChromeButton {
            anchors.fill: parent
            leftAlign: true
            horizontalPadding: 5
            icon: statusItem.modelData.icon
            iconSize: Style.font.icon
            foreground: statusItem.modelData.urgent ? root.colors.urgent : restingForeground
            accessibleName: statusItem.modelData.label
            fontFamily: root.iconFontFamily
            accent: root.colors.accent
        }

        Text {
            visible: statusItem.labelled
            anchors.left: parent.left
            anchors.leftMargin: 30
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: statusItem.modelData.label
            elide: Text.ElideRight
            color: statusItem.modelData.urgent ? root.colors.urgent : root.colors.text
            font.family: Style.font.family
            font.pixelSize: Style.font.body
            Accessible.ignored: true
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
            shown: footerMarks.downloadItem.detailRequested && footerMarks.downloadItem.running > 0
            ease: root.easeSpaces
            distance: 8
        }
        opacity: detailLift.progress
        color: detailDither.fill
        border.width: 1
        border.color: root.colors.accent

        GroundDither {
            id: detailDither
            ground: root.colors.overlay
        }

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
                    readonly property string progressLabel:
                        footerMarks.downloadItem.progressLabelFor(detailLine.fraction)
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

    NumberAnimation {
        id: swipeSpring
        target: root
        property: "swipeOffset"
        to: 0
        duration: 240
        easing.type: Easing.OutCubic
    }

    // The neighbour a swipe or a notch in `direction` goes to, or null when
    // there is none to go to.
    function neighbourTowards(direction) {
        if (!root.browser)
            return null;
        const neighbour = direction > 0 ? root.browser.nextSpace : root.browser.previousSpace;
        return neighbour && neighbour.spaceId.length > 0 ? neighbour : null;
    }

    function swipeTo(direction, carried) {
        const neighbour = root.neighbourTowards(direction);
        if (neighbour) {
            const before = root.browser.activeSpaceId;
            root.swipeHandoff = {
                "direction": direction,
                "offset": carried
            };
            root.spaceActivated(neighbour.spaceId);
            if (root.browser.activeSpaceId !== before)
                return;
        }
        root.swipeHandoff = null;
        swipeSpring.start();
    }

    // Over the whole sidebar, the navigation row to the footer, and above
    // everything in it: the wheel reaches it first, and what is not a swipe
    // or a notch goes on to the list's scrolling under it.
    SpaceSwipe {
        id: spaceSwipe
        objectName: "spaceSwipe"
        anchors.fill: parent
        z: 3
        enabled: root.swipeAllowed
        settling: root.arriving
        onOffsetChanged: {
            if (!swiping || !root.swipeFollows)
                return;
            swipeSpring.stop();
            root.swipeOffset = offset;
        }
        onCommitted: function (direction) {
            root.swipeTo(direction, root.swipeOffset / listLayer.width);
        }
        onSprangBack: swipeSpring.start()
        onStepped: function (direction) {
            root.swipeTo(direction, 0);
        }
    }

    // Site information is the window's: a card wider than the outline, over
    // the page edge. A click on the outline while it is open puts it away.
    MouseArea {
        anchors.fill: parent
        visible: root.siteInformationOpen
        z: 4
        onClicked: root.siteInformationDismissed()
    }
}
