import QtQuick
import QtQuick.Controls
import qs.Commons

// What Agents did, as the activity log keeps it: newest first, one line per
// verb, filtered by Agent and by Space. It stands in the page area for the tab
// holding `omaweb:agent-activity`, which no engine loads.
Rectangle {
    id: root
    objectName: "agentActivitySurface"

    property var colors
    property var activity: null
    property bool open: false
    property var rows: []
    property var agentOptions: []
    property var spaceOptions: []

    visible: open
    color: root.colors.sheet

    function refresh() {
        if (!activity) {
            rows = [];
            return;
        }
        const agents = activity.agents();
        agentOptions = [
                    {
                        value: "",
                        label: "Every Agent"
                    }
                ].concat(agents.map(function (name) {
                    return {
                        value: name,
                        label: name.length > 0 ? name : "unnamed"
                    };
                }));
        spaceOptions = [
                    {
                        value: "",
                        label: "Every Space"
                    }
                ].concat(activity.spaces().map(function (space) {
                    return {
                        value: space.id,
                        label: space.name
                    };
                }));
        rows = activity.rows(agentFilter.value, spaceFilter.value);
    }

    function describe(row) {
        const parts = [row.verb];
        if (row.target.length > 0)
            parts.push(row.target);
        return parts.join(" ");
    }

    onOpenChanged: if (open)
                       refresh()

    Connections {
        target: root.activity
        enabled: root.open
        function onRecorded() {
            root.refresh();
        }
    }

    Column {
        anchors.fill: parent
        anchors.topMargin: Style.space(48)
        anchors.leftMargin: Style.space(48)
        anchors.rightMargin: Style.space(48)
        anchors.bottomMargin: Style.space(48)
        spacing: Style.space(18)

        Item {
            width: parent.width
            height: activityEyebrow.height + Style.spacing.md + activityHeading.height

            Text {
                id: activityEyebrow
                anchors.left: parent.left
                anchors.top: parent.top
                text: "Kept for 7 days · no page content"
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
                font.bold: true
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 1.6
                Accessible.ignored: true
            }

            Text {
                id: activityHeading
                anchors.left: parent.left
                anchors.top: activityEyebrow.bottom
                anchors.topMargin: Style.spacing.md
                text: "Agent activity"
                color: root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.display
                Accessible.role: Accessible.Heading
                Accessible.name: "Agent activity"
            }
        }

        Row {
            spacing: 10

            SettingDropdown {
                id: agentFilter
                objectName: "agentActivityAgentFilter"
                width: 220
                colors: root.colors
                options: root.agentOptions
                value: ""
                accessibleName: "Filter by Agent"
                onChanged: function (chosen) {
                    agentFilter.value = chosen;
                }
                onValueChanged: root.refresh()
            }

            SettingDropdown {
                id: spaceFilter
                objectName: "agentActivitySpaceFilter"
                width: 220
                colors: root.colors
                options: root.spaceOptions
                value: ""
                accessibleName: "Filter by Space"
                onChanged: function (chosen) {
                    spaceFilter.value = chosen;
                }
                onValueChanged: root.refresh()
            }
        }

        ListView {
            id: activityList
            objectName: "agentActivityList"
            width: parent.width
            height: parent.height - y
            clip: true
            spacing: 2
            model: root.rows

            delegate: Rectangle {
                id: line
                required property var modelData
                readonly property bool refused: modelData.outcome !== "ok"
                width: activityList.width
                height: 32
                radius: 6
                color: root.colors.surface
                Accessible.role: Accessible.StaticText
                Accessible.name: when.text + ", " + modelData.agent + ", " + modelData.space + ", "
                                 + root.describe(modelData) + (refused ? ", refused" : "")

                Row {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 14

                    Text {
                        id: when
                        width: 150
                        text: Qt.formatDateTime(new Date(line.modelData.time),
                                                "yyyy-MM-dd hh:mm:ss")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.ignored: true
                    }
                    Text {
                        width: 120
                        text: line.modelData.agent
                        color: root.colors.text
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        font.bold: true
                        Accessible.ignored: true
                    }
                    Text {
                        width: 120
                        text: line.modelData.space
                        color: root.colors.mutedText
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.ignored: true
                    }
                    Text {
                        width: parent.width - x
                        text: root.describe(line.modelData) + (line.modelData.address.length > 0
                                                               && line.modelData.address
                                                               !== line.modelData.target ? "  ·  "
                                                                                           + line.modelData.address :
                                                                                           "") + (line.refused
                                                                                                  ? "  ·  refused ("
                                                                                                    + line.modelData.outcome
                                                                                                    + ")" : "")
                        color: line.refused ? root.colors.mutedText : root.colors.text
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.ignored: true
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: activityList.count === 0
                text: agentFilter.value.length > 0 || spaceFilter.value.length > 0
                      ? "No matching activity" : "No Agent has done anything in the last 7 days"
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
        }
    }
}
