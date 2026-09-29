import QtQuick
import qs.Commons

// The page of an Agent tab on show, framed in the Agent accent, with a label
// at its top-right corner naming who is driving it and what it did last: the
// reader is watching someone else's hands.
Item {
    id: root

    property var colors
    // The Agent tab's report from the core, or null for a page nobody drives:
    // the connection's `name` and its last `act`.
    property var agent: null
    // Where the label stands, below whatever is docked at the top of the page.
    property real labelTop: 0

    readonly property string caption: root.describe(root.agent)

    function describe(agent) {
        if (!agent)
            return "";
        const act = String(agent.act || "");
        const name = String(agent.name || "");
        const driving = (name.length > 0 ? name : "An Agent") + " is driving";
        return act.length > 0 ? driving + " · " + act : driving;
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 2
        border.color: root.colors.agentAccent
    }

    Rectangle {
        objectName: "agentFrameLabel"
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root.labelTop
        width: Math.min(caption.implicitWidth + 16, parent.width)
        height: caption.implicitHeight + 8
        color: root.colors.agentAccent

        Text {
            id: caption
            objectName: "agentFrameCaption"
            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - 16)
            text: root.caption
            color: root.colors.windowOpaque
            elide: Text.ElideRight
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            font.bold: true
        }
    }
}
