import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    objectName: "spaceOutline"

    property var colors
    property var typography
    property string iconFontFamily
    property var browser
    property bool privateWindow: false
    property bool collapsed: false
    property int blockedRequestCount: 0
    property bool statusOpen: false

    // Re-read when the active Space changes: creating or deleting one moves it.
    readonly property int spaceCount: {
        if (!browser) return 0
        browser.activeSpaceId
        return browser.spaces.rowCount()
    }

    // An empty pinned section takes no room at all.
    property int pinnedCount: browser ? browser.pinnedTabs.rowCount() : 0

    readonly property url activeUrl: browser ? browser.activeUrl : ""
    readonly property bool secure: String(activeUrl).indexOf("https://") === 0
    readonly property bool blank: String(activeUrl).length === 0 || String(activeUrl) === "about:blank"

    signal addressRequested()
    signal newTabRequested()
    signal tabActivated(string tabId)
    signal spaceActivated(string spaceId)
    signal spacesMenuRequested()
    signal settingsRequested()
    signal windowMoveRequested()

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

        Item {
            objectName: "spaceHeading"
            width: parent.width
            height: 44
            Accessible.role: Accessible.Heading
            Accessible.name: root.privateWindow || !root.browser ? "Private" : root.browser.activeSpaceName

            DragHandler {
                target: null
                onActiveChanged: if (active) root.windowMoveRequested()
            }

            // The name alone identifies the Space. Isolation is what every
            // Space is rather than something one of them has, and the tab count
            // is already the length of the list underneath it.
            Text {
                id: spaceName
                anchors.left: parent.left
                anchors.right: spacesButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: root.privateWindow || !root.browser ? "Private" : root.browser.activeSpaceName
                color: root.privateWindow ? root.colors.privateAccent : root.colors.text
                font.family: root.typography.family
                font.pixelSize: root.typography.headingSize
                font.letterSpacing: 1.4
                font.capitalization: Font.AllUppercase
                elide: Text.ElideRight
            }

            ChromeButton {
                id: spacesButton
                objectName: "manageSpacesButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 26
                visible: !root.privateWindow
                label: "more_horiz"
                accessibleName: "Manage Spaces"
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                hoverBackground: root.colors.surfaceHover
                onClicked: root.spacesMenuRequested()
            }
        }

        Row {
            objectName: "spaceSwitcher"
            width: parent.width
            visible: !root.privateWindow && root.spaceCount > 1
            spacing: 5

            Repeater {
                model: root.browser ? root.browser.spaces : null

                ChromeButton {
                    required property string spaceId
                    required property string spaceName
                    required property string spaceColor
                    required property bool active

                    objectName: "space-" + spaceId
                    visible: !active
                    width: visible ? 30 : 0
                    height: 28
                    label: spaceName.length > 0 ? spaceName.charAt(0).toUpperCase() : "·"
                    accessibleName: "Switch to " + spaceName
                    foreground: spaceColor.length > 0 ? spaceColor : root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: root.spaceActivated(spaceId)
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

            Text {
                id: securityGlyph
                objectName: "securityIndicator"
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.verticalCenter: parent.verticalCenter
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
                anchors.leftMargin: 8
                anchors.right: blockedCount.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: root.blank
                    ? "search or enter address"
                    : String(root.activeUrl).replace(/^[a-z]+:\/\//, "")
                color: root.blank ? root.colors.mutedText : root.colors.text
                elide: Text.ElideMiddle
                font.family: root.typography.family
                font.pixelSize: root.typography.size
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
                    font.family: root.typography.family
                    font.pixelSize: root.typography.smallSize
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

        SectionLabel {
            objectName: "pinnedLabel"
            visible: !root.privateWindow && root.pinnedCount > 0
            colors: root.colors
            typography: root.typography
            text: "pinned"
        }

        Column {
            id: pinnedSection
            objectName: "pinnedList"
            width: parent.width
            height: childrenRect.height
            visible: !root.privateWindow
            spacing: 0

            Repeater {
                model: root.browser ? root.browser.pinnedTabs : null

                PinnedRow {
                    width: parent.width
                    colors: root.colors
                    typography: root.typography
                    onActivated: function(id) { root.tabActivated(id) }
                }
            }
        }

        SectionLabel {
            colors: root.colors
            typography: root.typography
            text: "tabs"
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
        anchors.topMargin: 6
        anchors.bottomMargin: 8
        clip: true

        Column {
            width: tabScroll.availableWidth
            spacing: 1

            Repeater {
                model: root.browser ? root.browser.unpinnedTabs : null

                TabRow {
                    width: parent.width
                    colors: root.colors
                    typography: root.typography
                    onActivated: function(id) { root.tabActivated(id) }
                }
            }
        }
    }

    Row {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 14
        height: 30
        spacing: 6

        Rectangle {
            id: newTabButton
            objectName: "newTabButton"
            property string accessibleName: "New tab"
            width: footer.width - settingsButton.width - footer.spacing
            height: 30
            radius: 2
            color: newTabMouse.containsMouse ? root.colors.surfaceHover : "transparent"
            border.width: newTabButton.activeFocus ? 1 : 0
            border.color: root.colors.accent
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: newTabButton.accessibleName
            Accessible.onPressAction: root.newTabRequested()

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                    root.newTabRequested()
                    event.accepted = true
                }
            }

            Text {
                id: newTabGlyph
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: "add"
                color: root.colors.mutedText
                font.family: root.iconFontFamily
                font.pixelSize: root.typography.iconSize
            }

            Text {
                anchors.left: newTabGlyph.right
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "new tab"
                color: root.colors.mutedText
                font.family: root.typography.family
                font.pixelSize: root.typography.size
            }

            MouseArea {
                id: newTabMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    newTabButton.forceActiveFocus()
                    root.newTabRequested()
                }
            }
        }

        ChromeButton {
            id: settingsButton
            objectName: "settingsButton"
            width: 30
            height: 30
            label: "settings"
            accessibleName: "Browsing settings and downloads"
            fontFamily: root.iconFontFamily
            foreground: root.colors.mutedText
            hoverBackground: root.colors.surfaceHover
            onClicked: root.settingsRequested()
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
                typography: root.typography
                text: "site status"
            }

            Text {
                width: parent.width
                text: root.secure
                    ? "· connection is encrypted"
                    : "· connection is not encrypted"
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: root.typography.family
                font.pixelSize: root.typography.smallSize
            }

            Text {
                width: parent.width
                text: "· " + root.blockedRequestCount + " requests blocked in this window"
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: root.typography.family
                font.pixelSize: root.typography.smallSize
            }
        }
    }
}
