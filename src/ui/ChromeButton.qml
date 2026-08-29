import QtQuick

Item {
    id: root

    property string label: ""
    property color foreground: "white"
    property color background: "transparent"
    property color hoverBackground: "#30ffffff"
    property string fontFamily: ""
    property string accessibleName: ""
    signal clicked()

    implicitWidth: 34
    implicitHeight: 32
    opacity: enabled ? 1.0 : 0.35
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

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: mouse.containsMouse && root.enabled ? root.hoverBackground : root.background
        border.width: root.activeFocus ? 1 : 0
        border.color: root.foreground
    }

    Text {
        anchors.centerIn: parent
        text: root.label
        color: root.foreground
        font.family: root.fontFamily
        font.pixelSize: 15
        font.weight: Font.DemiBold
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
