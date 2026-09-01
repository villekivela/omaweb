import QtQuick
import QtQuick.Controls
import qs.Commons

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
    readonly property bool secure: String(activeUrl).indexOf("https://") === 0
    readonly property bool blank: String(activeUrl).length === 0 || String(activeUrl) === "about:blank"

    signal addressRequested()
    signal tabActivated(string tabId)
    signal tabCloseRequested(string tabId)
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
        root.pageFocusRequested()
        event.accepted = true
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

        Rectangle {
            id: addressButton
            objectName: "addressButton"
            property string accessibleName: "Search or enter address"
            width: parent.width
            height: 34
            radius: 2
            color: addressMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface
            border.width: root.statusOpen || addressButton.activeFocus ? 1 : 0
            border.color: root.colors.accent
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
                text: root.secure ? "lock" : "lock_open"
                color: root.secure ? root.colors.text : root.colors.mutedText
                font.family: root.iconFontFamily
                font.pixelSize: 14
                Accessible.role: Accessible.StaticText
                Accessible.name: root.secure ? "Connection is encrypted" : "Connection is not encrypted"

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
                    font.pixelSize: 13
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

        Flow {
            id: pinnedSection
            objectName: "pinnedList"
            width: parent.width
            height: childrenRect.height
            visible: !root.privateWindow && root.pinnedCount > 0
            readonly property int capacity: Math.max(3,
                Math.min(5, Math.floor(width / 56)))
            readonly property int columns: Math.min(root.pinnedCount, capacity)
            spacing: 4

            Repeater {
                model: root.browser ? root.browser.pinnedTabs : null

                TabRow {
                    required property int index
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
                    onActiveChanged: if (active) root.activeTabItem = this
                    Component.onCompleted: if (active) root.activeTabItem = this
                }
            }
        }

    }

    ScrollView {
        id: tabScroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: outline.bottom
        anchors.bottom: footer.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        clip: true

        Column {
            width: tabScroll.availableWidth
            spacing: 0

            Repeater {
                model: root.atRest || !root.browser ? null : root.browser.unpinnedTabs

                TabRow {
                    width: parent.width
                    colors: root.colors
                    iconFontFamily: root.iconFontFamily
                    useFavicons: root.useFavicons
                    tintFavicons: root.tintFavicons
                    onActivated: function(id) { root.tabActivated(id) }
                    onCloseRequested: function(id) { root.tabCloseRequested(id) }
                    onActiveChanged: if (active) root.activeTabItem = this
                    Component.onCompleted: if (active) root.activeTabItem = this
                }
            }
        }
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
                    required property string spaceColor
                    required property bool active

                    objectName: "space-" + spaceId
                    width: 30
                    height: 28
                    label: spaceName.length > 0 ? spaceName.charAt(0).toUpperCase() : "·"
                    accessibleName: active
                        ? "Current Space: " + spaceName
                        : "Switch to " + spaceName
                    foreground: active
                        ? (spaceColor.length > 0 ? spaceColor : root.colors.text)
                        : root.colors.mutedText
                    accent: root.colors.accent
                    background: active ? root.colors.surface : "transparent"
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

    Rectangle {
        objectName: "siteStatusPanel"
        visible: root.statusOpen
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 172
        width: Math.min(260, root.width - 32)
        height: statusColumn.implicitHeight + 20
        radius: 2
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        z: 5

        Column {
            id: statusColumn
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            SectionLabel {
                colors: root.colors
                text: "site status"
            }

            Text {
                width: parent.width
                text: root.secure
                    ? "· connection is encrypted"
                    : "· connection is not encrypted"
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                width: parent.width
                text: "· " + root.blockedRequestCount + " requests blocked in this window"
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }
    }
}
