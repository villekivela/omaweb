import QtQuick
import qs.Commons

// Who is driving and what it did last, under an Agent tab's title and on the
// bar over its Auxiliary window. Each case is a whole sentence for the
// translators, since a connection may have no name.
Text {
    id: root

    // The core's report: the connection's `name` and its last `act`.
    property var agent: null

    readonly property string agentName: root.agent ? String(root.agent.name || "") : ""
    readonly property string act: root.agent ? String(root.agent.act || "") : ""

    function caption(name, act) {
        if (name.length > 0)
            return act.length > 0 ? qsTr("%1 · %2").arg(name).arg(act) : qsTr("%1 is driving").arg(
                                        name);
        return act.length > 0 ? qsTr("An Agent · %1").arg(act) : qsTr("An Agent is driving");
    }

    text: root.caption(root.agentName, root.act)
    elide: Text.ElideRight
    font.family: Style.font.family
    font.pixelSize: Style.font.caption
}
