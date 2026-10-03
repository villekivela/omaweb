import QtQuick
import qs.Commons

// The line under the Omnibar's results that names the keys working its list, as
// the website's dash does: each key as a key cap and what it does in 11 px dim
// text, over a rule. Its words follow the scope the field is in. Another item
// can join its right end by being declared inside it, as the website's Radio
// toggle sits beside its keys.
Item {
    id: root
    objectName: "omnibarHints"

    property var colors
    property var keymap
    property bool commandScope: false
    property color plate: root.colors ? root.colors.overlayOpaque : "black"
    // Items for the row's right end.
    default property alias trailing: trailingSlot.data

    // The keys for each thing the field does in the scope it is in. Command
    // scope also has a key that leaves it.
    readonly property var keys: ({
                                     "select": root.keymap.omnibarKeys.select,
                                     "go": root.commandScope ? root.keymap.omnibarKeys.run :
                                                               root.keymap.omnibarKeys.go,
                                     "leave": root.keymap.omnibarKeys.leave
                                 })
    readonly property var groups: root.groupsFor(root.commandScope)

    function groupsFor(commandScope) {
        const groups = [
                  {
                      "keys": root.keys.select,
                      "word": qsTr("select")
                  },
                  {
                      "keys": root.keys.go,
                      "word": commandScope ? qsTr("run") : qsTr("go")
                  }
              ];
        if (commandScope)
            groups.push({
                            "keys": root.keys.leave,
                            "word": qsTr("back")
                        });
        return groups;
    }

    implicitHeight: Math.max(cap.height, line.implicitHeight) + 16
    height: visible ? implicitHeight : 0
    Accessible.ignored: true

    Rectangle {
        objectName: "omnibarHintRule"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: root.colors.separator
    }

    KeyCap {
        id: cap
        visible: false
    }

    Row {
        id: line
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: 0.5
        spacing: 18

        Repeater {
            model: root.groups

            Row {
                id: group
                required property var modelData
                spacing: 6
                anchors.verticalCenter: parent.verticalCenter

                Row {
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter

                    Repeater {
                        model: group.modelData.keys

                        KeyCap {
                            required property string modelData
                            colors: root.colors
                            text: modelData
                            plate: root.plate
                        }
                    }
                }

                Text {
                    objectName: "omnibarHintWord"
                    anchors.verticalCenter: parent.verticalCenter
                    text: group.modelData.word
                    color: root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.bodySmall
                    Accessible.ignored: true
                }
            }
        }
    }

    Row {
        id: trailingSlot
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 18
    }
}
