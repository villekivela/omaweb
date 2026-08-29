import QtQuick

Rectangle {
    id: root
    objectName: "pinned-tab-" + tabId

    required property string tabId
    required property string tabTitle
    required property url tabUrl
    required property bool pinned
    required property bool active
    required property bool loading
    property var colors
    property string iconFontFamily

    signal activated(string tabId)

    radius: 10
    color: active ? colors.surfaceHover : (mouse.containsMouse ? colors.surfaceHover : colors.surface)
    border.width: active ? 1 : 0
    border.color: colors.accent
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

    Text {
        anchors.centerIn: parent
        text: "language"
        color: root.active ? root.colors.accent : root.colors.text
        font.family: root.iconFontFamily
        font.pixelSize: 22
    }

    Rectangle {
        width: 5
        height: 5
        radius: 3
        anchors.right: parent.right
        anchors.rightMargin: 7
        anchors.top: parent.top
        anchors.topMargin: 7
        visible: root.loading
        color: root.colors.accent
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
