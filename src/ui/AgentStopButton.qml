import QtQuick
import qs.Commons

// Stop for an Agent's connection, on an Agent tab's row and on the bar over
// its Auxiliary window. Whoever holds it says when the pointer is on it.
Rectangle {
    id: root

    property var colors
    property string iconFontFamily
    property string agentName
    property bool hot: false

    signal activated

    readonly property color foreground: root.hot ? root.colors.windowOpaque :
                                                   root.colors.agentAccent

    width: label.implicitWidth + Style.space(12)
    height: 24
    radius: Style.cornerRadius
    color: root.hot ? root.colors.agentAccent : "transparent"
    border.width: 1
    border.color: root.colors.agentAccent
    Accessible.role: Accessible.Button
    Accessible.name: qsTr("Stop %1").arg(root.agentName)
    Accessible.onPressAction: root.activated()

    Row {
        id: label
        anchors.centerIn: parent
        spacing: Style.space(3)

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "stop"
            color: root.foreground
            font.family: root.iconFontFamily
            font.pixelSize: Style.font.iconSmall
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Stop", "verb: stop the Agent driving this tab or window")
            color: root.foreground
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            font.bold: true
        }
    }
}
