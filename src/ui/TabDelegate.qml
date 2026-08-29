import QtQuick

Rectangle {
    id: root

    required property string tabId
    required property string tabTitle
    required property url tabUrl
    required property bool pinned
    required property bool active
    required property bool loading
    property var colors

    signal activated(string tabId)

    width: ListView.view ? ListView.view.width : 200
    height: 40
    radius: 9
    color: active ? colors.surface : (mouse.containsMouse ? colors.surfaceHover : "transparent")
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
        width: 3
        height: 18
        radius: 2
        anchors.left: parent.left
        anchors.leftMargin: 3
        anchors.verticalCenter: parent.verticalCenter
        color: root.active ? root.colors.accent : "transparent"
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.right: status.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: root.tabTitle.length > 0 ? root.tabTitle : root.tabUrl.toString()
        color: root.colors.text
        elide: Text.ElideRight
        font.pixelSize: 13
    }

    Text {
        id: status
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.loading ? "·" : ""
        color: root.colors.mutedText
        font.pixelSize: 11
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
