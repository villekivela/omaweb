import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Ui as Omarchy

Rectangle {
    id: root
    objectName: "spaceOutline"

    property var colors
    property string iconFontFamily
    property var browser
    property bool privateWindow: false
    property bool collapsed: false
    property int blockedRequestCount: 0
    property bool statusOpen: false
    property bool useFavicons: true
    property bool tintFavicons: true
    property bool canGoBack: false
    property bool canGoForward: false
    // Something in settings is waiting on the reader. The button wears a mark
    // rather than the outline growing a line for it: settings is a place, and
    // what is wrong is stated there, where it can be acted on.
    property bool settingsAttention: false

    // An empty pinned section takes no room at all.
    property int pinnedCount: browser ? browser.pinnedTabs.rowCount() : 0

    // A Space whose only ordinary tab is blank lists no ordinary tab: the row
    // would stand for a page nobody opened, wearing a site chip for a site
    // that does not exist. The pinned block stays — those are the Space's own
    // furniture and are there whether anything is open or not.
    readonly property bool atRest: browser ? browser.atRest : false

    readonly property url activeUrl: browser ? browser.activeUrl : ""
    // What the connection is, as the engine drawing the page reports it. The
    // outline never works this out from the address: an address is what was
    // asked for, and a lock drawn from one is a claim nothing checked.
    property string connectionState: "internal"
    // What the engine can and cannot answer for. A gap is said out loud rather
    // than drawn as a reassuring blank.
    property bool certificateDecisionsAvailable: false
    property bool thirdPartyCookieControlAvailable: false
    property bool siteDataOnDisk: false
    property bool insecureContentBlocked: true
    // The engine's third-party filter, which is the only thing that knows
    // which embedded origins a page has actually had refused.
    property var cookiePolicy: null
    readonly property bool secure: root.connectionState === "secure"
    readonly property bool certificateError: root.connectionState === "certificate-error"
    readonly property bool blank: String(activeUrl).length === 0 || String(activeUrl) === "about:blank"

    // What Site information is showing. Read when the panel opens rather than
    // kept live: these are answers about one origin at one moment, and a panel
    // nobody has open has nothing to say.
    property var sitePermissionRows: []
    property var cookieAllowanceRows: []
    property var refusedThirdParties: []
    property real siteDataBytes: -1
    // Which reset the panel is waiting on a confirmation for. Clearing site
    // data and resetting permissions are both irreversible, so neither happens
    // on one press.
    property string resetPending: ""

    readonly property string originLabel: {
        const address = String(root.activeUrl)
        const separator = address.indexOf("://")
        if (separator === -1) return address
        const authority = address.substring(separator + 3).split("/")[0]
        return authority.length > 0 ? authority : address
    }

    readonly property string connectionSentence: {
        switch (root.connectionState) {
        case "secure": return "connection is encrypted"
        case "certificate-error":
            return "certificate could not be verified"
        case "insecure": return "connection is not encrypted"
        default: return "no page is loaded"
        }
    }

    function permissionDecisionName(decision) {
        switch (Number(decision)) {
        case 1: return "allowed once"
        case 2: return "always allowed"
        case 3: return "blocked"
        default: return "asked each time"
        }
    }

    function siteDataSentence() {
        if (!root.siteDataOnDisk)
            return "this engine keeps no site data on disk"
        if (root.siteDataBytes < 0)
            return "site data in this Space could not be measured"
        const units = ["B", "kB", "MB", "GB"]
        let size = root.siteDataBytes
        let unit = 0
        while (size >= 1024 && unit < units.length - 1) {
            size = size / 1024
            unit += 1
        }
        const rounded = unit === 0 ? Math.round(size) : Math.round(size * 10) / 10
        return rounded + " " + units[unit] + " of site data in this Space"
    }

    function refreshSiteInformation() {
        if (!root.browser) return
        root.sitePermissionRows = root.browser.sitePermissions(root.activeUrl)
        root.cookieAllowanceRows = root.browser.thirdPartyCookieAllowances()
        root.refusedThirdParties = root.cookiePolicy
            ? root.cookiePolicy.refusedOrigins(root.activeUrl) : []
        root.siteDataBytes = root.siteDataOnDisk && root.browser.activeSpaceId.length > 0
            ? root.browser.siteDataBytes(root.browser.activeSpaceId) : -1
        root.resetPending = ""
    }

    onStatusOpenChanged: if (root.statusOpen) root.refreshSiteInformation()

    signal addressRequested()
    signal tabActivated(string tabId)
    signal tabCloseRequested(string tabId)
    signal tabMuteToggled(string tabId)
    signal tabMenuRequested(string tabId, real anchorX, real anchorY)
    // Where a dragged row was let go, counted inside its own section. The list
    // owns this because placing rows is the list's: a row knows how tall it is
    // and nothing about the rows around it.
    signal tabDropped(string tabId, int destination)
    signal spaceActivated(string spaceId)
    signal spacesMenuRequested(real anchorX, real anchorY)
    signal settingsRequested()
    signal backRequested()
    signal forwardRequested()
    signal reloadRequested()
    signal sidebarToggled()
    signal commandPanelRequested()
    signal windowMoveRequested()
    signal pageFocusRequested()

    // Where "focus the sidebar" lands: the row the reader is already reading,
    // so the keyboard arrives where their attention is.
    property var activeTabItem: null

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
        const rows = []
        for (let index = 0; index < container.children.length; ++index) {
            const child = container.children[index]
            if (child.tabId !== undefined) rows[child.placeInSection] = child
        }
        return rows
    }

    function rowsFor(row) {
        return root.sectionRows(row.pinned ? pinnedSection : ordinarySection)
    }

    // Where the list put a row, whatever the hand has since done with it.
    function homeOf(row) {
        const at = row.mapToItem(root, 0, 0)
        return Qt.point(at.x - row.carry.x, at.y - row.carry.y)
    }

    function beginTabDrag(row) {
        root.draggedRow = row
        root.dropDestination = row.placeInSection
        row.lifted = true
    }

    // The hand's position decides two things: where the held row is drawn, and
    // which place it would take. The second is read off where the list put the
    // other rows, so a wrapped row of pins answers as truthfully as a stack of
    // ordinary rows.
    function updateTabDrag(row, sceneX, sceneY) {
        if (root.draggedRow !== row) return
        const pointer = root.mapFromItem(null, sceneX, sceneY)
        const home = root.homeOf(row)
        const wanted = Qt.point(pointer.x - row.grabbedAt.x, pointer.y - row.grabbedAt.y)
        // An ordinary row is carried along the list it is in; a pin is laid out
        // across the section as well as down it.
        row.carry = row.pinned
            ? Qt.point(wanted.x - home.x, wanted.y - home.y)
            : Qt.point(0, wanted.y - home.y)

        const rows = root.rowsFor(row)
        let destination = row.placeInSection
        for (let place = 0; place < rows.length; ++place) {
            const other = rows[place]
            if (!other) continue
            const at = root.homeOf(other)
            if (pointer.x < at.x || pointer.x >= at.x + other.width) {
                // Outside this row's column: only its band decides, so a
                // stacked list ignores the horizontal miss entirely.
                if (other.pinned) continue
            }
            if (pointer.y < at.y || pointer.y >= at.y + other.height) continue
            destination = place
            break
        }
        // Past the end of the section in either direction, the nearest place is
        // the one the hand meant.
        if (destination === row.placeInSection && rows.length > 0) {
            const first = rows[0] ? root.homeOf(rows[0]) : null
            const last = rows[rows.length - 1]
                ? root.homeOf(rows[rows.length - 1]) : null
            if (first && pointer.y < first.y) destination = 0
            else if (last && pointer.y >= last.y + rows[rows.length - 1].height)
                destination = rows.length - 1
        }
        root.dropDestination = destination
        root.openTheDroppedPlace(row, rows, destination)
    }

    // Every row between where the held row came from and where it would land
    // moves up or down by one place — into the place the arrangement would give
    // it — which is what opens the gap the held row will drop into.
    function openTheDroppedPlace(row, rows, destination) {
        const from = row.placeInSection
        for (let place = 0; place < rows.length; ++place) {
            const other = rows[place]
            if (!other || other === row) continue
            let shifted = place
            if (from < destination && place > from && place <= destination) shifted = place - 1
            else if (destination < from && place >= destination && place < from) shifted = place + 1
            const target = rows[shifted]
            if (!target || shifted === place) {
                other.carry = Qt.point(0, 0)
                continue
            }
            const here = root.homeOf(other)
            const there = root.homeOf(target)
            other.carry = Qt.point(there.x - here.x, there.y - here.y)
        }
    }

    function endTabDrag(row) {
        if (root.draggedRow !== row) return
        const destination = root.dropDestination
        const rows = root.rowsFor(row)
        root.draggedRow = null
        root.dropDestination = -1
        row.lifted = false
        // Every row goes back to the place the list gives it, and the list is
        // told the one thing the drag decided.
        for (let place = 0; place < rows.length; ++place) {
            if (rows[place]) rows[place].carry = Qt.point(0, 0)
        }
        if (destination >= 0 && destination !== row.placeInSection) {
            root.tabDropped(row.tabId, destination)
        }
    }

    function focusOutline() {
        if (activeTabItem !== null && activeTabItem.visible) {
            activeTabItem.forceActiveFocus()
        } else {
            addressButton.forceActiveFocus()
        }
    }

    // Key events climb from the focused row to here, so one handler covers the
    // whole outline: Escape is the way back to the page.
    Keys.onEscapePressed: function(event) {
        if (root.statusOpen) {
            root.statusOpen = false
            event.accepted = true
            return
        }
        root.pageFocusRequested()
        event.accepted = true
    }

    Shortcut {
        sequence: "Esc"
        enabled: root.statusOpen
        context: Qt.WindowShortcut
        onActivated: root.statusOpen = false
    }

    color: colors.sidebar
    clip: true

    Connections {
        target: root.browser ? root.browser.pinnedTabs : null
        function onRowsInserted() { root.pinnedCount = root.browser.pinnedTabs.rowCount() }
        function onRowsRemoved() { root.pinnedCount = root.browser.pinnedTabs.rowCount() }
        function onModelReset() { root.pinnedCount = root.browser.pinnedTabs.rowCount() }
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: root.colors.border
    }

    Column {
        id: outline
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 16
        spacing: 12

        // The navigation controls hold the row the Space heading used to: the
        // commands that act on the page open the outline, and the browsing
        // identity closes it from the footer.
        Item {
            objectName: "sidebarNavigation"
            width: parent.width
            height: 32

            DragHandler {
                target: null
                onActiveChanged: if (active) root.windowMoveRequested()
            }

            Row {
                objectName: "outlineControls"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ChromeButton {
                    objectName: "collapseButton"
                    width: 28
                    height: 26
                    icon: root.collapsed ? "left_panel_open" : "left_panel_close"
                    accessibleName: root.collapsed ? "Show sidebar" : "Hide sidebar"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.sidebarToggled()
                }

                ChromeButton {
                    objectName: "commandPanelButton"
                    width: 28
                    height: 26
                    icon: "search"
                    accessibleName: "Command panel"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.commandPanelRequested()
                }
            }

            Row {
                objectName: "outlineNavigation"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                ChromeButton {
                    objectName: "backButton"
                    width: 28
                    height: 26
                    icon: "arrow_back"
                    accessibleName: "Back"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.text
                    accent: root.colors.accent
                    enabled: root.canGoBack
                    onClicked: root.backRequested()
                }

                ChromeButton {
                    objectName: "forwardButton"
                    width: 28
                    height: 26
                    icon: "arrow_forward"
                    accessibleName: "Forward"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.text
                    accent: root.colors.accent
                    enabled: root.canGoForward
                    onClicked: root.forwardRequested()
                }

                ChromeButton {
                    objectName: "reloadButton"
                    width: 28
                    height: 26
                    icon: "refresh"
                    accessibleName: "Reload"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.text
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
            property string accessibleName: "Search or enter address"
            readonly property bool focused: root.statusOpen || addressButton.activeFocus
            width: parent.width
            height: 34
            radius: 2
            color: Style.controlFill(addressButton.focused, addressMouse.containsMouse,
                root.colors.text, root.colors.accent)
            borderSpec: Border.controlSpec(
                addressButton.focused ? "focus"
                    : (addressMouse.containsMouse ? "hover-cursor" : "normal"),
                root.colors.text, root.colors.accent)
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: addressButton.accessibleName
            Accessible.onPressAction: root.addressRequested()

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                    root.addressRequested()
                    event.accepted = true
                }
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
                text: root.certificateError
                    ? "warning" : (root.secure ? "lock" : "lock_open")
                color: root.certificateError
                    ? root.colors.urgent
                    : (root.secure ? root.colors.text : root.colors.mutedText)
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.iconLarge
                Accessible.role: Accessible.StaticText
                Accessible.name: "Site information: " + root.connectionSentence

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
                text: root.blank
                    ? "search or enter address"
                    : String(root.activeUrl).replace(/^[a-z]+:\/\//, "")
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
                visible: root.blockedRequestCount > 0

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "shield"
                    color: root.colors.mutedText
                    font.family: root.iconFontFamily
                    font.pixelSize: Style.font.icon
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.blockedRequestCount
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
                    addressButton.forceActiveFocus()
                    root.addressRequested()
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
    Flow {
        id: pinnedSection
        objectName: "pinnedList"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: outline.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: visible ? 12 : 0
        height: childrenRect.height
        visible: !root.privateWindow && root.pinnedCount > 0
        readonly property int capacity: Math.max(3,
            Math.min(5, Math.floor(width / 56)))
        readonly property int columns: Math.min(root.pinnedCount, capacity)
        spacing: 4

        Repeater {
            model: root.browser ? root.browser.pinnedTabs : null

            TabRow {
                id: pinnedRow
                required property int index
                placeInSection: index
                readonly property int rowStart: Math.floor(index
                    / pinnedSection.capacity) * pinnedSection.capacity
                readonly property int tabsInRow: Math.min(pinnedSection.capacity,
                    root.pinnedCount - rowStart)
                width: (pinnedSection.width
                    - pinnedSection.spacing * (tabsInRow - 1)) / tabsInRow
                colors: root.colors
                iconFontFamily: root.iconFontFamily
                useFavicons: root.useFavicons
                tintFavicons: root.tintFavicons
                onActivated: function(id) { root.tabActivated(id) }
                onCloseRequested: function(id) { root.tabCloseRequested(id) }
                onMuteToggled: function(id) { root.tabMuteToggled(id) }
                onDragStarted: root.beginTabDrag(pinnedRow)
                onDragMoved: function(id, sceneX, sceneY) {
                    root.updateTabDrag(pinnedRow, sceneX, sceneY)
                }
                onDragEnded: root.endTabDrag(pinnedRow)
                onMenuRequested: function(id, anchorX, anchorY) {
                    root.tabMenuRequested(id, anchorX, anchorY)
                }
                onActiveChanged: if (active) root.activeTabItem = this
                Component.onCompleted: if (active) root.activeTabItem = this
            }
        }
    }

    ScrollView {
        id: tabScroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: pinnedSection.bottom
        anchors.bottom: footerRule.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        clip: true

        Column {
            id: ordinarySection
            objectName: "ordinaryList"
            width: tabScroll.availableWidth
            spacing: 0

            Repeater {
                model: root.atRest || !root.browser ? null : root.browser.unpinnedTabs

                TabRow {
                    id: ordinaryRow
                    required property int index
                    placeInSection: index
                    width: parent.width
                    colors: root.colors
                    iconFontFamily: root.iconFontFamily
                    useFavicons: root.useFavicons
                    tintFavicons: root.tintFavicons
                    onActivated: function(id) { root.tabActivated(id) }
                    onCloseRequested: function(id) { root.tabCloseRequested(id) }
                    onMuteToggled: function(id) { root.tabMuteToggled(id) }
                    onDragStarted: root.beginTabDrag(ordinaryRow)
                    onDragMoved: function(id, sceneX, sceneY) {
                        root.updateTabDrag(ordinaryRow, sceneX, sceneY)
                    }
                    onDragEnded: root.endTabDrag(ordinaryRow)
                    onMenuRequested: function(id, anchorX, anchorY) {
                        root.tabMenuRequested(id, anchorX, anchorY)
                    }
                    onActiveChanged: if (active) root.activeTabItem = this
                    Component.onCompleted: if (active) root.activeTabItem = this
                }
            }
        }
    }

    // What the Space is, and what it carries, are not more tabs: the rule says
    // where the list ends so the footer reads as the outline's own furniture
    // rather than as the row after the last tab.
    Rectangle {
        id: footerRule
        objectName: "outlineFooterRule"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: Style.spacing.lg
        height: Style.spacing.hairline
        color: root.colors.border
    }

    // The Space letters, the Space menu and the settings the Space carries
    // read as one row, and it closes the outline instead of opening it.
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
        Accessible.name: root.privateWindow || !root.browser ? "Private" : root.browser.activeSpaceName

        // Every Space is one letter, the active one lit. The row is the
        // switcher: spelling the active name out again would say what the
        // lit letter already says.
        Row {
            objectName: "spaceSwitcher"
            anchors.left: parent.left
            anchors.right: spacesButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            height: 28
            visible: !root.privateWindow
            spacing: 5

            Repeater {
                model: root.browser ? root.browser.spaces : null

                ChromeButton {
                    required property string spaceId
                    required property string spaceName
                    required property bool active

                    objectName: "space-" + spaceId
                    width: 30
                    height: 28
                    label: spaceName.length > 0 ? spaceName.charAt(0).toUpperCase() : "·"
                    accessibleName: active
                        ? "Current Space: " + spaceName
                        : "Switch to " + spaceName
                    // The letter is the theme's, not the Space's own colour:
                    // the kit derives a control's fill and its border from its
                    // foreground, so a coloured Space painted the whole button
                    // in it — a lit plate louder than anything else in the
                    // outline, for the one thing the reader already knows.
                    // Being the only lit letter in the row is what says which
                    // Space is on show.
                    foreground: active ? root.colors.text : root.colors.mutedText
                    accent: root.colors.accent
                    // The Space on show is the one selected thing in this row,
                    // so it is drawn the way the kit draws a selection and the
                    // way a current tab row is: the kit's own selected fill,
                    // bordered.
                    selected: active
                    bordered: active
                    background: "transparent"
                    onClicked: root.spaceActivated(spaceId)
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
            Accessible.name: "Private window"
        }

        ChromeButton {
            id: spacesButton
            objectName: "manageSpacesButton"
            anchors.right: settingsButton.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 26
            visible: !root.privateWindow
            icon: "more_horiz"
            accessibleName: "Manage Spaces"
            fontFamily: root.iconFontFamily
            foreground: root.colors.mutedText
            accent: root.colors.accent
            onClicked: {
                const corner = spacesButton.mapToItem(null, spacesButton.width,
                    spacesButton.height)
                root.spacesMenuRequested(corner.x, corner.y)
            }
        }

        ChromeButton {
            id: settingsButton
            objectName: "settingsButton"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 30
            icon: "settings"
            accessibleName: root.settingsAttention
                ? "Browsing settings and downloads — needs attention"
                : "Browsing settings and downloads"
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

    MouseArea {
        anchors.fill: parent
        visible: root.statusOpen
        z: 4
        onClicked: root.statusOpen = false
    }

    Rectangle {
        objectName: "siteInformationPanel"
        visible: root.statusOpen
        anchors.left: parent.left
        anchors.leftMargin: 16
        y: outline.y + addressButton.y + addressButton.height + 8
        width: Math.min(320, Math.max(200, root.width - 32))
        height: statusColumn.implicitHeight + 20
        radius: 2
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        z: 5

        MouseArea { anchors.fill: parent }

        Column {
            id: statusColumn
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            SectionLabel {
                colors: root.colors
                text: "site information"
            }

            // The origin first: every line under it is about this site inside
            // this Space, and nothing else.
            Text {
                objectName: "siteInformationOrigin"
                width: parent.width
                text: root.blank ? "no site" : root.originLabel
                color: root.colors.text
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Text {
                objectName: "siteInformationConnection"
                width: parent.width
                text: "· " + root.connectionSentence
                    + (root.certificateError ? " · exception is for this load only" : "")
                color: root.certificateError ? root.colors.urgent : root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                width: parent.width
                visible: !root.insecureContentBlocked
                text: "· this engine is not blocking insecure content"
                color: root.colors.urgent
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                objectName: "siteInformationBlocked"
                width: parent.width
                text: "· " + root.blockedRequestCount + " requests blocked in this window"
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                objectName: "siteInformationSiteData"
                width: parent.width
                text: "· " + root.siteDataSentence()
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                objectName: "siteInformationCookies"
                width: parent.width
                text: root.thirdPartyCookieControlAvailable
                    ? "· third-party cookies and storage are blocked"
                    : "· this engine cannot refuse a third party"
                color: root.thirdPartyCookieControlAvailable
                    ? root.colors.mutedText : root.colors.urgent
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            // A flow that needs a third party can be given one, by name, for a
            // reason the reader can read back. Nothing here survives the
            // session, and every one of them is beside its own way out.
            Repeater {
                model: root.thirdPartyCookieControlAvailable ? root.refusedThirdParties : []

                Row {
                    required property int index
                    required property string modelData

                    objectName: "refusedThirdParty" + index
                    width: statusColumn.width
                    spacing: 6

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: statusColumn.width - 132
                        text: "· " + modelData
                        color: root.colors.mutedText
                        elide: Text.ElideMiddle
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    ActionButton {
                        objectName: "allowSignIn" + index
                        colors: root.colors
                        label: "sign-in"
                        onClicked: {
                            root.browser.allowThirdPartyCookies(modelData, "authentication")
                            root.refreshSiteInformation()
                        }
                    }

                    ActionButton {
                        objectName: "allowPayment" + index
                        colors: root.colors
                        label: "payment"
                        onClicked: {
                            root.browser.allowThirdPartyCookies(modelData, "payment")
                            root.refreshSiteInformation()
                        }
                    }
                }
            }

            Repeater {
                model: root.cookieAllowanceRows

                Row {
                    required property int index
                    required property var modelData

                    objectName: "cookieAllowance" + index
                    width: statusColumn.width
                    spacing: 6

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: statusColumn.width - 80
                        text: "· " + modelData.origin + " — allowed for " + modelData.purpose
                        color: root.colors.text
                        elide: Text.ElideMiddle
                        wrapMode: Text.NoWrap
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    ActionButton {
                        objectName: "revokeAllowance" + index
                        colors: root.colors
                        label: "revoke"
                        onClicked: {
                            root.browser.revokeThirdPartyCookieAllowance(modelData.origin)
                            root.refreshSiteInformation()
                        }
                    }
                }
            }

            SectionLabel {
                colors: root.colors
                text: "permissions in this space"
            }

            Text {
                objectName: "siteInformationNoPermissions"
                width: parent.width
                visible: root.sitePermissionRows.length === 0
                text: root.privateWindow
                    ? "· nothing is remembered in a private window"
                    : "· nothing decided for this site"
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Repeater {
                model: root.sitePermissionRows

                Text {
                    required property int index
                    required property var modelData

                    objectName: "sitePermission" + index
                    width: statusColumn.width
                    text: "· " + modelData.permission + " — "
                        + root.permissionDecisionName(modelData.decision)
                    color: root.colors.mutedText
                    wrapMode: Text.WordWrap
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }
            }

            // Both resets are irreversible, so neither happens on one press.
            // The second press is about one named action rather than a general
            // agreement, and asking again cancels the first.
            Text {
                objectName: "siteInformationResetQuestion"
                width: parent.width
                visible: root.resetPending.length > 0
                text: root.resetPending === "site-data"
                    ? "clear this Space's cookies, storage and cache?"
                    : "reset every decision made for this site?"
                color: root.colors.text
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Row {
                width: parent.width
                spacing: 6

                ActionButton {
                    objectName: "clearSiteData"
                    colors: root.colors
                    label: root.resetPending === "site-data" ? "confirm clear" : "clear site data"
                    primary: root.resetPending === "site-data"
                    enabled: !root.privateWindow && root.siteDataOnDisk
                    onClicked: {
                        if (root.resetPending !== "site-data") {
                            root.resetPending = "site-data"
                            return
                        }
                        root.browser.clearBrowsingData(["cookies", "storage", "cache"], 0)
                        root.refreshSiteInformation()
                    }
                }

                ActionButton {
                    objectName: "resetSitePermissions"
                    colors: root.colors
                    label: root.resetPending === "permissions"
                        ? "confirm reset" : "reset permissions"
                    primary: root.resetPending === "permissions"
                    enabled: !root.blank
                    onClicked: {
                        if (root.resetPending !== "permissions") {
                            root.resetPending = "permissions"
                            return
                        }
                        root.browser.resetSitePermissions(root.activeUrl)
                        root.refreshSiteInformation()
                    }
                }
            }
        }
    }
}
