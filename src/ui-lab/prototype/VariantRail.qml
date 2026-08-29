// THROWAWAY PROTOTYPE — Variant B, "Rail + flyout".
//
// Claim: the page never reflows. A 54px rail is the only permanent chrome and
// it belongs to Spaces, not tabs — every Space is one keystroke-sized target,
// always visible. Tabs live in a flyout that floats OVER the page on hover or
// when pinned open. Status is a stack of rail pills that expand into a popover.
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "../../ui"

Item {
    id: root

    property var colors
    property var mock
    property string iconFont
    property bool flyoutPinned: false
    property bool statusOpen: false

    signal omnibarRequested(bool forNewTab)

    readonly property string activeHost: {
        const value = String(browser.activeUrl)
        if (value === "about:blank") return value
        return value.replace(/^[a-z]+:\/\//, "").split("/")[0]
    }

    readonly property bool revealed: flyoutPinned || railHover.hovered || flyoutHover.hovered
    readonly property string variantState:
        "rail 54px · flyout " + (flyoutPinned ? "pinned open" : (revealed ? "revealed" : "hidden"))
        + " · page never reflows"

    // Page first: it owns everything right of the rail, at all times.
    Item {
        id: pageArea
        anchors.left: rail.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Loader {
            anchors.fill: parent
            source: Qt.resolvedUrl("../MockEngineView.qml")
        }
    }

    Rectangle {
        id: rail
        width: 54
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: root.colors.sidebar
        z: 3

        HoverHandler { id: railHover }

        DragHandler {
            target: null
            onActiveChanged: if (active) Window.window.startSystemMove()
        }

        Column {
            anchors.top: parent.top
            anchors.topMargin: 12
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6

            // Space avatars — the rail's primary job.
            Repeater {
                model: root.mock.spaces

                Rectangle {
                    required property int index
                    required property var modelData

                    width: 40
                    height: 40
                    radius: 13
                    color: index === root.mock.activeSpaceIndex
                        ? Qt.rgba(0, 0, 0, 0.25)
                        : (avatarMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface)
                    border.width: index === root.mock.activeSpaceIndex ? 2 : 0
                    border.color: modelData.color

                    Text {
                        anchors.centerIn: parent
                        text: modelData.letter
                        color: index === root.mock.activeSpaceIndex
                            ? modelData.color
                            : root.colors.mutedText
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        visible: index !== root.mock.activeSpaceIndex && modelData.tabs > 0
                        anchors.right: parent.right
                        anchors.rightMargin: -2
                        anchors.top: parent.top
                        anchors.topMargin: -2
                        width: 15
                        height: 15
                        radius: 8
                        color: root.colors.window

                        Text {
                            anchors.centerIn: parent
                            text: modelData.tabs
                            color: root.colors.mutedText
                            font.pixelSize: 9
                        }
                    }

                    MouseArea {
                        id: avatarMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.mock.activeSpaceIndex = index
                    }
                }
            }

            Item { width: 1; height: 4 }

            Rectangle {
                width: 22
                height: 1
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.colors.border
            }
        }

        Column {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 76
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 5

            // Status pills. Two characters each; the popover carries the words.
            Repeater {
                model: [
                    { label: root.mock.secure ? "TLS" : "!", tint: root.mock.secure ? "#63d3a5" : "#e2686f" },
                    { label: String(root.mock.permissions), tint: root.colors.accent },
                    { label: String(root.mock.blocked), tint: root.mock.blockingEnabled ? root.colors.mutedText : "#e2686f" }
                ]

                Rectangle {
                    required property var modelData

                    width: 34
                    height: 26
                    radius: 8
                    color: statusMouse.containsMouse || root.statusOpen
                        ? root.colors.surfaceHover
                        : root.colors.surface

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: modelData.tint
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: statusMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.statusOpen = !root.statusOpen
                    }
                }
            }
        }

        ChromeButton {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 40
            anchors.horizontalCenter: parent.horizontalCenter
            label: "add"
            fontFamily: root.iconFont
            foreground: root.colors.text
            hoverBackground: root.colors.surfaceHover
            onClicked: root.omnibarRequested(true)
        }

        ChromeButton {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            label: root.flyoutPinned ? "chevron_left" : "chevron_right"
            fontFamily: root.iconFont
            foreground: root.flyoutPinned ? root.colors.accent : root.colors.mutedText
            hoverBackground: root.colors.surfaceHover
            onClicked: root.flyoutPinned = !root.flyoutPinned
        }
    }

    // Status popover, anchored to the rail.
    Rectangle {
        visible: root.statusOpen
        anchors.left: rail.right
        anchors.leftMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 76
        width: 232
        height: statusColumn.implicitHeight + 20
        radius: 12
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.border
        z: 6

        Column {
            id: statusColumn
            anchors.fill: parent
            anchors.margins: 10
            spacing: 7

            Text {
                text: root.activeHost
                color: root.colors.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }

            Text {
                text: root.mock.secure ? "Connection is encrypted" : "Connection is not encrypted"
                color: root.colors.mutedText
                font.pixelSize: 11
            }

            Text {
                text: root.mock.permissions + " permissions granted in this Space"
                color: root.colors.mutedText
                font.pixelSize: 11
            }

            Text {
                text: root.mock.blocked + " requests blocked"
                    + (root.mock.blockingEnabled ? "" : " · blocking off for this site")
                color: root.colors.mutedText
                font.pixelSize: 11
            }
        }
    }

    // Flyout: floats over the page, so the viewport width never changes.
    Rectangle {
        id: flyout
        width: 272
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 8
        anchors.bottomMargin: 8
        x: root.revealed ? rail.width + 8 : rail.width - width
        opacity: root.revealed ? 1 : 0
        visible: opacity > 0.01
        radius: 14
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.border
        z: 4

        HoverHandler { id: flyoutHover }

        Behavior on x {
            NumberAnimation { duration: 130; easing.type: Easing.OutCubic }
        }

        Behavior on opacity {
            NumberAnimation { duration: 110 }
        }

        Column {
            id: flyoutColumn
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Rectangle {
                width: parent.width
                height: 36
                radius: 10
                color: flyoutAddressMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 11
                    anchors.right: parent.right
                    anchors.rightMargin: 11
                    anchors.verticalCenter: parent.verticalCenter
                    text: browser.activeUrl.toString() === "about:blank"
                        ? "Search or enter address"
                        : browser.activeUrl.toString()
                    color: browser.activeUrl.toString() === "about:blank"
                        ? root.colors.mutedText
                        : root.colors.text
                    elide: Text.ElideMiddle
                    font.pixelSize: 12
                }

                MouseArea {
                    id: flyoutAddressMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    onClicked: root.omnibarRequested(false)
                }
            }

            Row {
                width: parent.width
                spacing: 4

                ChromeButton {
                    width: (flyoutColumn.width - 12) / 4
                    label: "arrow_back"
                    fontFamily: root.iconFont
                    foreground: root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.requestBack()
                }

                ChromeButton {
                    width: (flyoutColumn.width - 12) / 4
                    label: "arrow_forward"
                    fontFamily: root.iconFont
                    foreground: root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.requestForward()
                }

                ChromeButton {
                    width: (flyoutColumn.width - 12) / 4
                    label: "refresh"
                    fontFamily: root.iconFont
                    foreground: root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.requestReload()
                }

                ChromeButton {
                    width: (flyoutColumn.width - 12) / 4
                    label: browser.activeTabPinned ? "bookmark" : "bookmark_border"
                    fontFamily: root.iconFont
                    foreground: browser.activeTabPinned ? root.colors.accent : root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.toggleActivePinned()
                }
            }

            Grid {
                width: parent.width
                columns: 5
                columnSpacing: 5
                rowSpacing: 5

                Repeater {
                    model: browser.tabs

                    PinnedTabDelegate {
                        width: (flyoutColumn.width - 20) / 5
                        visible: pinned
                        height: visible ? 40 : 0
                        colors: root.colors
                        iconFontFamily: root.iconFont
                        onActivated: function(id) { browser.activateTab(id) }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: root.colors.border
            }

            ScrollView {
                width: parent.width
                height: flyoutColumn.height - y
                clip: true

                Column {
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: browser.tabs

                        TabDelegate {
                            width: parent.width
                            visible: !pinned
                            height: visible ? 38 : 0
                            colors: root.colors
                            onActivated: function(id) { browser.activateTab(id) }
                        }
                    }
                }
            }
        }
    }
}
