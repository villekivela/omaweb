// THROWAWAY PROTOTYPE — Variant A, "Stack".
//
// Claim: one column, read top to bottom, nothing hidden. The Space is an
// identity chip at the top that drops down into the switcher; status lives in
// a permanent footer row so the blocked count and permission state are always
// on screen without a click.
import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import "../../ui"

Item {
    id: root

    property var colors
    property var mock
    property string iconFont
    property bool collapsed: false
    property bool spacesOpen: false

    signal omnibarRequested(bool forNewTab)

    readonly property string variantState:
        "stack sidebar " + (collapsed ? "58px collapsed" : "242px")
        + (spacesOpen ? " · spaces open" : "")

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: root.collapsed ? 58 : 242
            color: root.colors.sidebar
            z: 2

            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                // Space identity chip — doubles as the window drag strip.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 10
                    color: spaceMouse.containsMouse || root.spacesOpen
                        ? root.colors.surfaceHover
                        : "transparent"

                    Rectangle {
                        id: spaceDot
                        width: 12
                        height: 12
                        radius: 6
                        anchors.left: parent.left
                        anchors.leftMargin: root.collapsed ? (parent.width - 12) / 2 : 10
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.mock.activeSpace.color
                    }

                    Text {
                        anchors.left: spaceDot.right
                        anchors.leftMargin: 10
                        anchors.right: caret.left
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.collapsed
                        text: root.mock.activeSpace.name
                        color: root.colors.text
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    Text {
                        id: caret
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.collapsed
                        text: root.spacesOpen ? "chevron_left" : "chevron_right"
                        color: root.colors.mutedText
                        font.family: root.iconFont
                        font.pixelSize: 16
                    }

                    MouseArea {
                        id: spaceMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.spacesOpen = !root.spacesOpen
                    }

                    DragHandler {
                        target: null
                        onActiveChanged: if (active) Window.window.startSystemMove()
                    }
                }

                // Address trigger.
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 10
                    color: addressMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.collapsed
                        text: browser.activeUrl.toString() === "about:blank"
                            ? "Search or enter address"
                            : browser.activeUrl.toString()
                        color: browser.activeUrl.toString() === "about:blank"
                            ? root.colors.mutedText
                            : root.colors.text
                        elide: Text.ElideMiddle
                        font.pixelSize: 13
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: root.collapsed
                        text: "search"
                        color: root.colors.text
                        font.family: root.iconFont
                        font.pixelSize: 20
                    }

                    MouseArea {
                        id: addressMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.IBeamCursor
                        onClicked: root.omnibarRequested(false)
                    }
                }

                // Navigation controls.
                GridLayout {
                    Layout.fillWidth: true
                    columns: root.collapsed ? 1 : 4
                    columnSpacing: 4
                    rowSpacing: 4

                    ChromeButton {
                        Layout.fillWidth: true
                        label: "arrow_back"
                        fontFamily: root.iconFont
                        foreground: root.colors.text
                        hoverBackground: root.colors.surfaceHover
                        onClicked: browser.requestBack()
                    }

                    ChromeButton {
                        Layout.fillWidth: true
                        label: "arrow_forward"
                        fontFamily: root.iconFont
                        foreground: root.colors.text
                        hoverBackground: root.colors.surfaceHover
                        onClicked: browser.requestForward()
                    }

                    ChromeButton {
                        Layout.fillWidth: true
                        label: "refresh"
                        fontFamily: root.iconFont
                        foreground: root.colors.text
                        hoverBackground: root.colors.surfaceHover
                        onClicked: browser.requestReload()
                    }

                    ChromeButton {
                        Layout.fillWidth: true
                        label: browser.activeTabPinned ? "bookmark" : "bookmark_border"
                        fontFamily: root.iconFont
                        foreground: browser.activeTabPinned ? root.colors.accent : root.colors.text
                        hoverBackground: root.colors.surfaceHover
                        onClicked: browser.toggleActivePinned()
                    }
                }

                // Pinned tabs as a compact icon grid.
                Grid {
                    Layout.fillWidth: true
                    columns: root.collapsed ? 1 : 4
                    columnSpacing: 6
                    rowSpacing: 6

                    Repeater {
                        model: browser.tabs

                        PinnedTabDelegate {
                            width: root.collapsed ? 38 : (sidebar.width - 20 - 18) / 4
                            visible: pinned
                            height: visible ? (root.collapsed ? 38 : 52) : 0
                            colors: root.colors
                            iconFontFamily: root.iconFont
                            onActivated: function(id) { browser.activateTab(id) }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.colors.border
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    Column {
                        width: parent.width
                        spacing: 3

                        Repeater {
                            model: browser.tabs

                            TabDelegate {
                                width: parent.width
                                visible: !pinned
                                height: visible ? 40 : 0
                                colors: root.colors
                                onActivated: function(id) { browser.activateTab(id) }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    radius: 9
                    color: newTabMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: root.collapsed ? 0 : 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: root.collapsed ? parent.width : 20
                        horizontalAlignment: Text.AlignHCenter
                        text: "add"
                        color: root.colors.text
                        font.family: root.iconFont
                        font.pixelSize: 19
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 34
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.collapsed
                        text: "New tab"
                        color: root.colors.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: newTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.omnibarRequested(true)
                    }
                }

                // Permanent status footer: TLS, permissions, blocked count.
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26

                    Row {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: root.collapsed ? 3 : 4
                        spacing: root.collapsed ? 0 : 12

                        Row {
                            spacing: 5
                            Rectangle {
                                width: 7
                                height: 7
                                radius: 4
                                anchors.verticalCenter: parent.verticalCenter
                                color: root.mock.secure ? "#63d3a5" : "#e2686f"
                            }
                            Text {
                                visible: !root.collapsed
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.mock.secure ? "TLS" : "Not secure"
                                color: root.colors.mutedText
                                font.pixelSize: 11
                            }
                        }

                        Text {
                            visible: !root.collapsed
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mock.permissions + " perms"
                            color: root.colors.mutedText
                            font.pixelSize: 11
                        }

                        Text {
                            visible: !root.collapsed
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.mock.blocked + " blocked"
                            color: root.mock.blockingEnabled ? root.colors.mutedText : "#e2686f"
                            font.pixelSize: 11
                        }
                    }

                    ChromeButton {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !root.collapsed
                        implicitWidth: 26
                        implicitHeight: 24
                        label: "chevron_left"
                        fontFamily: root.iconFont
                        foreground: root.colors.mutedText
                        hoverBackground: root.colors.surfaceHover
                        onClicked: root.collapsed = true
                    }

                    ChromeButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        visible: root.collapsed
                        implicitWidth: 26
                        implicitHeight: 24
                        label: "chevron_right"
                        fontFamily: root.iconFont
                        foreground: root.colors.mutedText
                        hoverBackground: root.colors.surfaceHover
                        onClicked: root.collapsed = false
                    }
                }
            }

            // Space switcher: drops down over the sidebar's own content.
            Rectangle {
                visible: root.spacesOpen
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                anchors.topMargin: 52
                height: spaceColumn.implicitHeight + 12
                radius: 12
                color: root.colors.overlay
                border.width: 1
                border.color: root.colors.border
                z: 5

                Column {
                    id: spaceColumn
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 2

                    Repeater {
                        model: root.mock.spaces

                        Rectangle {
                            required property int index
                            required property var modelData

                            width: spaceColumn.width
                            height: 34
                            radius: 8
                            color: index === root.mock.activeSpaceIndex
                                ? root.colors.surfaceHover
                                : (rowMouse.containsMouse ? root.colors.surface : "transparent")

                            Rectangle {
                                id: dot
                                width: 10
                                height: 10
                                radius: 5
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: modelData.color
                            }

                            Text {
                                anchors.left: dot.right
                                anchors.leftMargin: 9
                                anchors.right: count.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name
                                color: root.colors.text
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }

                            Text {
                                id: count
                                anchors.right: parent.right
                                anchors.rightMargin: 9
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.tabs
                                color: root.colors.mutedText
                                font.pixelSize: 11
                            }

                            MouseArea {
                                id: rowMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.mock.activeSpaceIndex = index
                                    root.spacesOpen = false
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Loader {
                anchors.fill: parent
                source: Qt.resolvedUrl("../MockEngineView.qml")
            }
        }
    }
}
