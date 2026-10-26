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

    AgentCaption {
        id: caption
        objectName: "auxiliaryAgentCaption"
        anchors.left: mark.right
        anchors.leftMargin: Style.space(8)
        anchors.right: stop.left
        anchors.rightMargin: Style.space(12)
        anchors.verticalCenter: parent.verticalCenter
        agent: root.agent
        color: root.colors.text
    }

    AgentStopButton {
        id: stop
        objectName: "auxiliaryAgentStop"
        anchors.right: parent.right
        anchors.rightMargin: Style.space(6)
        anchors.verticalCenter: parent.verticalCenter
        height: 22
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        agentName: caption.agentName
        hot: stopArea.containsMouse
        onActivated: root.stopRequested()

        MouseArea {
            id: stopArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.stopRequested()
        }
    }
}
