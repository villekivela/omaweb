import QtQuick

Item {
    id: root
    objectName: "tab-" + tabId

    required property string tabId
    required property string tabTitle
    required property url tabUrl
    required property bool pinned
    required property bool active
    required property bool loading
    property var colors
    property var typography

    signal activated(string tabId)

    height: 42
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: tabTitle
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

    Rectangle {
        width: 2
        height: parent.height
        anchors.left: parent.left
        color: root.active ? root.colors.accent : "transparent"
    }

    SiteTile {
        id: tile
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        typography: root.typography
        siteUrl: root.tabUrl
        highlighted: root.active
    }

    Text {
        id: title
        anchors.left: tile.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.topMargin: 5
        text: root.tabTitle.length > 0 ? root.tabTitle : tile.host
        color: root.active ? root.colors.text : root.colors.mutedText
        elide: Text.ElideRight
        font.family: root.typography.family
        font.pixelSize: root.typography.size
    }

    Text {
        anchors.left: tile.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: title.bottom
        anchors.topMargin: 2
        text: root.loading ? "loading…" : tile.host
        color: root.colors.mutedText
        opacity: 0.85
        elide: Text.ElideRight
        font.family: root.typography.family
        font.pixelSize: root.typography.smallSize
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
