import QtQuick
import qs.Commons

Item {
    id: root
    objectName: (pinned ? "pinned-" : "tab-") + tabId

    required property string tabId
    required property string tabTitle
    required property url tabUrl
    required property url tabIconUrl
    required property bool pinned
    required property bool active
    required property bool loading
    property var colors

    signal activated(string tabId)

    height: 28
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: pinned ? "Pinned: " + tabTitle : tabTitle
    Accessible.onPressAction: root.activated(root.tabId)

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.activated(root.tabId)
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 2
        color: root.active
            ? root.colors.surface
            : (mouse.containsMouse ? root.colors.surfaceHover : "transparent")
        border.width: root.activeFocus ? 1 : 0
        border.color: root.colors.accent
    }

    SiteTile {
        id: tile
        implicitWidth: 18
        implicitHeight: 18
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        siteUrl: root.tabUrl
        iconUrl: root.tabIconUrl
        highlighted: root.active
    }

    // The title names the page; its address is already in the address button
    // whenever the tab is the active one, and reading it twice crowds the row.
    Text {
        anchors.left: tile.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.tabTitle.length > 0 ? root.tabTitle : tile.host
        color: root.active ? root.colors.text : root.colors.mutedText
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.activated(root.tabId)
        }
    }
}
