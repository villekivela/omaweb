import QtQuick
import qs.Commons

// The bar along the top of an Agent's Auxiliary window, which has no row in
// the sidebar to carry its mark: who is driving it, what it did last, and
// Stop for the connection.
Rectangle {
    id: root

    property var colors
    property string iconFontFamily
    // The opener tab's report from the core: the connection's `name`, its
    // last `act` and `busy` while a command is in flight. Null for a window
    // nobody drives.
    property var agent: null

    signal stopRequested

    readonly property string agentName: root.agent && String(root.agent.name || "").length > 0
                                        ? String(root.agent.name) : qsTr("An Agent")
    readonly property string caption: root.agent && String(root.agent.act || "").length > 0 ? qsTr(
                                                                                                  "%1 · %2").arg(
                                                                                                  root.agentName).arg(
                                                                                                  root.agent.act) :
                                                                                              qsTr("%1 is driving").arg(
                                                                                                  root.agentName)

    height: 30
    color: Qt.tint(root.colors.windowOpaque, Qt.rgba(root.colors.agentAccent.r,
                                                     root.colors.agentAccent.g,
                                                     root.colors.agentAccent.b, 0.14))

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: root.colors.agentAccent
    }

    AgentMark {
        id: mark
        anchors.left: parent.left
        anchors.leftMargin: Style.space(10)
        anchors.verticalCenter: parent.verticalCenter
        busy: root.agent !== null && root.agent.busy === true
        color: root.colors.agentAccent
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.iconSmall
    }

    Text {
        objectName: "auxiliaryAgentCaption"
        anchors.left: mark.right
        anchors.leftMargin: Style.space(8)
        anchors.right: stop.left
        anchors.rightMargin: Style.space(12)
        anchors.verticalCenter: parent.verticalCenter
        text: root.caption
        color: root.colors.text
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
    }

    Rectangle {
        id: stop
        objectName: "auxiliaryAgentStop"
        anchors.right: parent.right
        anchors.rightMargin: Style.space(6)
        anchors.verticalCenter: parent.verticalCenter
        width: stopLabel.implicitWidth + Style.space(12)
        height: 22
        radius: Style.cornerRadius
        color: stopArea.containsMouse ? root.colors.agentAccent : "transparent"
        border.width: 1
        border.color: root.colors.agentAccent
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Stop %1").arg(root.agentName)
        Accessible.onPressAction: root.stopRequested()

        Row {
            id: stopLabel
            anchors.centerIn: parent
            spacing: Style.space(3)

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "stop"
                color: stopArea.containsMouse ? root.colors.windowOpaque : root.colors.agentAccent
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.iconSmall
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Stop", "verb: stop the Agent driving this window")
                color: stopArea.containsMouse ? root.colors.windowOpaque : root.colors.agentAccent
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
                font.bold: true
            }
        }

        MouseArea {
            id: stopArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.stopRequested()
        }
    }
}
