import QtQuick

// One button shape for the whole shell: filled when it is the answer the
// surface expects, outlined when it is one of the alternatives.
Rectangle {
    id: root

    property var colors
    property var typography
    property string label: ""
    property bool primary: false
    property bool destructive: false
    property string accessibleName: label

    signal clicked()

    readonly property color edge: destructive ? colors.privateAccent : colors.accent

    implicitWidth: text.implicitWidth + 24
    implicitHeight: 30
    radius: 2
    opacity: enabled ? 1.0 : 0.4
    color: primary
        ? edge
        : (mouse.containsMouse && enabled ? colors.surfaceHover : "transparent")
    border.width: primary ? 0 : 1
    border.color: root.activeFocus ? edge : colors.border
    activeFocusOnTab: enabled
    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.onPressAction: root.clicked()

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.clicked()
            event.accepted = true
        }
    }

    Text {
        id: text
        anchors.centerIn: parent
        text: root.label
        color: root.primary ? root.colors.windowOpaque : root.colors.text
        font.family: root.typography.family
        font.pixelSize: root.typography.size
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: root.enabled
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.clicked()
        }
    }
}
