// THROWAWAY PROTOTYPE — Variant C, "Outline".
//
// Claim: the sidebar is a document about this Space, not a toolbar. It opens
// with a Space header and a row of sibling Spaces, then labelled sections you
// read: PINNED as named rows, TABS as two-line rows with the host. Status is
// carried inline by the address pill. Navigation controls leave the sidebar
// entirely and float over the bottom-left of the page.
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "../../ui"

Item {
    id: root

    property var colors
    property var mock
    property string iconFont
    property bool statusOpen: false

    signal omnibarRequested(bool forNewTab)

    readonly property string variantState:
        "outline sidebar 288px · nav floats over page · status inline in address"
        + (statusOpen ? " · popover open" : "")

    Item {
        id: pageArea
        anchors.left: sidebar.right
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Loader {
            anchors.fill: parent
            source: Qt.resolvedUrl("../MockEngineView.qml")
        }

        // Navigation cluster: quiet until you reach for it.
        Rectangle {
            id: navCluster
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 84
            width: 122
            height: 40
            radius: 20
            color: root.colors.overlay
            border.width: 1
            border.color: root.colors.border
            opacity: navHover.hovered ? 1 : 0.45

            HoverHandler { id: navHover }

            Behavior on opacity {
                NumberAnimation { duration: 140 }
            }

            Row {
                anchors.centerIn: parent
                spacing: 2

                ChromeButton {
                    label: "arrow_back"
                    fontFamily: root.iconFont
                    foreground: root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.requestBack()
                }

                ChromeButton {
                    label: "arrow_forward"
                    fontFamily: root.iconFont
                    foreground: root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.requestForward()
                }

                ChromeButton {
                    label: "refresh"
                    fontFamily: root.iconFont
                    foreground: root.colors.text
                    hoverBackground: root.colors.surfaceHover
                    onClicked: browser.requestReload()
                }
            }
        }
    }

    Rectangle {
        id: sidebar
        width: 288
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        color: root.colors.sidebar
        z: 2

        Column {
            id: outline
            anchors.fill: parent
            anchors.margins: 14
            anchors.topMargin: 16
            spacing: 12

            // Space header — the document's title block, and the drag strip.
            Item {
                width: parent.width
                height: 46

                DragHandler {
                    target: null
                    onActiveChanged: if (active) Window.window.startSystemMove()
                }

                Rectangle {
                    id: spaceBar
                    width: 3
                    height: 38
                    radius: 2
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.mock.activeSpace.color
                }

                Text {
                    id: spaceName
                    anchors.left: spaceBar.right
                    anchors.leftMargin: 11
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.topMargin: 2
                    text: root.mock.activeSpace.name
                    color: root.colors.text
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Text {
                    anchors.left: spaceBar.right
                    anchors.leftMargin: 11
                    anchors.top: spaceName.bottom
                    anchors.topMargin: 3
                    text: root.mock.activeSpace.tabs + " tabs in this Space"
                    color: root.colors.mutedText
                    font.pixelSize: 11
                }
            }

            // Sibling Spaces as a horizontal chip row.
            Row {
                width: parent.width
                spacing: 6

                Repeater {
                    model: root.mock.spaces

                    Rectangle {
                        required property int index
                        required property var modelData

                        width: 32
                        height: 32
                        radius: 10
                        visible: index !== root.mock.activeSpaceIndex
                        color: chipMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface

                        Text {
                            anchors.centerIn: parent
                            text: modelData.letter
                            color: modelData.color
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: chipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.mock.activeSpaceIndex = index
                        }
                    }
                }

                Rectangle {
                    width: 32
                    height: 32
                    radius: 10
                    color: "transparent"
                    border.width: 1
                    border.color: root.colors.border

                    Text {
                        anchors.centerIn: parent
                        text: "add"
                        color: root.colors.mutedText
                        font.family: root.iconFont
                        font.pixelSize: 16
                    }
                }
            }

            // Address pill carrying security and blocking status inline.
            Rectangle {
                width: parent.width
                height: 40
                radius: 12
                color: addressMouse.containsMouse ? root.colors.surfaceHover : root.colors.surface

                Rectangle {
                    id: securityDot
                    width: 20
                    height: 20
                    radius: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 9
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.statusOpen || securityMouse.containsMouse
                        ? root.colors.surfaceHover
                        : "transparent"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 8
                        height: 8
                        radius: 4
                        color: root.mock.secure ? "#63d3a5" : "#e2686f"
                    }

                    MouseArea {
                        id: securityMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.statusOpen = !root.statusOpen
                    }
                }

                Text {
                    anchors.left: securityDot.right
                    anchors.leftMargin: 8
                    anchors.right: blockedBadge.left
                    anchors.rightMargin: 8
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

                Rectangle {
                    id: blockedBadge
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: badgeText.implicitWidth + 14
                    height: 20
                    radius: 10
                    color: root.mock.blockingEnabled ? Qt.rgba(0, 0, 0, 0.25) : "#3a2530"

                    Text {
                        id: badgeText
                        anchors.centerIn: parent
                        text: root.mock.blocked
                        color: root.mock.blockingEnabled ? root.colors.mutedText : "#e2686f"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }

                MouseArea {
                    id: addressMouse
                    anchors.left: securityDot.right
                    anchors.right: blockedBadge.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    onClicked: root.omnibarRequested(false)
                }
            }

            Text {
                text: "PINNED"
                color: root.colors.mutedText
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }

            // Pinned tabs as named rows, not an icon grid.
            Column {
                width: parent.width
                spacing: 1

                Repeater {
                    model: browser.tabs

                    Rectangle {
                        required property string tabId
                        required property string tabTitle
                        required property bool pinned
                        required property bool active

                        width: outline.width
                        visible: pinned
                        height: visible ? 30 : 0
                        radius: 8
                        color: active
                            ? root.colors.surface
                            : (pinnedMouse.containsMouse ? root.colors.surfaceHover : "transparent")

                        Rectangle {
                            id: pinnedMark
                            width: 16
                            height: 16
                            radius: 5
                            anchors.left: parent.left
                            anchors.leftMargin: 7
                            anchors.verticalCenter: parent.verticalCenter
                            color: active ? root.colors.accent : root.colors.border
                        }

                        Text {
                            anchors.left: pinnedMark.right
                            anchors.leftMargin: 9
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: tabTitle
                            color: active ? root.colors.text : root.colors.mutedText
                            elide: Text.ElideRight
                            font.pixelSize: 12
                        }

                        MouseArea {
                            id: pinnedMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: browser.activateTab(tabId)
                        }
                    }
                }
            }

            Text {
                text: "TABS"
                color: root.colors.mutedText
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2
            }
        }

        // Two-line tab rows fill whatever is left under the outline sections.
        ScrollView {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.top: outline.bottom
            anchors.topMargin: 6
            anchors.bottom: newTabRow.top
            anchors.bottomMargin: 6
            clip: true

            Column {
                width: parent.width
                spacing: 2

                Repeater {
                    model: browser.tabs

                    Rectangle {
                        required property string tabId
                        required property string tabTitle
                        required property url tabUrl
                        required property bool pinned
                        required property bool active
                        required property bool loading

                        width: sidebar.width - 28
                        visible: !pinned
                        height: visible ? 48 : 0
                        radius: 10
                        color: active
                            ? root.colors.surface
                            : (tabMouse.containsMouse ? root.colors.surfaceHover : "transparent")

                        Rectangle {
                            id: favicon
                            width: 22
                            height: 22
                            radius: 7
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            color: active ? root.colors.accent : root.colors.border

                            Text {
                                anchors.centerIn: parent
                                text: tabTitle.length > 0 ? tabTitle.charAt(0).toUpperCase() : "·"
                                color: active ? root.colors.window : root.colors.mutedText
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }

                        Text {
                            id: titleText
                            anchors.left: favicon.right
                            anchors.leftMargin: 10
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.top: parent.top
                            anchors.topMargin: 8
                            text: tabTitle
                            color: root.colors.text
                            elide: Text.ElideRight
                            font.pixelSize: 12
                            font.weight: active ? Font.DemiBold : Font.Normal
                        }

                        Text {
                            anchors.left: favicon.right
                            anchors.leftMargin: 10
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.top: titleText.bottom
                            anchors.topMargin: 2
                            text: loading
                                ? "loading…"
                                : String(tabUrl).replace(/^[a-z]+:\/\//, "")
                            color: root.colors.mutedText
                            elide: Text.ElideRight
                            font.pixelSize: 10
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: browser.activateTab(tabId)
                        }
                    }
                }
            }
        }

        Item {
            id: newTabRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 14
            height: 32

            Rectangle {
                anchors.fill: parent
                radius: 9
                color: newTabMouse.containsMouse ? root.colors.surfaceHover : "transparent"
            }

            Text {
                id: newTabPlus
                anchors.left: parent.left
                anchors.leftMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                text: "add"
                color: root.colors.mutedText
                font.family: root.iconFont
                font.pixelSize: 17
            }

            Text {
                anchors.left: newTabPlus.right
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "New tab"
                color: root.colors.mutedText
                font.pixelSize: 12
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 9
                anchors.verticalCenter: parent.verticalCenter
                text: Qt.platform.os === "osx" ? "⌘T" : "Ctrl+T"
                color: root.colors.border
                font.pixelSize: 10
            }

            MouseArea {
                id: newTabMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.omnibarRequested(true)
            }
        }

        // Status popover, opened from the address pill's security dot.
        Rectangle {
            visible: root.statusOpen
            anchors.left: parent.left
            anchors.leftMargin: 14
            anchors.top: parent.top
            anchors.topMargin: 168
            width: 246
            height: statusColumn.implicitHeight + 20
            radius: 12
            color: root.colors.overlay
            border.width: 1
            border.color: root.colors.border
            z: 5

            Column {
                id: statusColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 7

                Text {
                    text: root.mock.secure ? "Connection is encrypted" : "Connection is not encrypted"
                    color: root.colors.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                Text {
                    text: root.mock.permissions + " permissions granted for this origin"
                    color: root.colors.mutedText
                    font.pixelSize: 11
                }

                Text {
                    text: root.mock.blocked + " requests blocked on this page"
                    color: root.colors.mutedText
                    font.pixelSize: 11
                }

                Text {
                    text: root.mock.blockingEnabled
                        ? "Content blocking on for this site"
                        : "Content blocking off for this site"
                    color: root.mock.blockingEnabled ? root.colors.mutedText : "#e2686f"
                    font.pixelSize: 11
                }
            }
        }
    }
}
