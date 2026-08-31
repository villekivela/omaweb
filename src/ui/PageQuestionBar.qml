import QtQuick

// A question about the page docks above the page, not over the middle of it.
// The page keeps rendering and stays readable while the bar waits, because what
// the page is showing is usually what the answer depends on.
Item {
    id: root

    property var colors
    property var typography
    property string iconFontFamily
    property bool open: false
    property string glyph: "help"
    property string message: ""
    property string detail: ""
    // Index 0 is the primary action, drawn filled; Escape picks the last one,
    // which every caller sets to the refusing answer.
    property var actions: []

    signal actionTriggered(int index)

    visible: open
    height: open ? bar.implicitHeight + 26 : 0
    focus: open

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && root.actions.length > 0) {
            root.actionTriggered(root.actions.length - 1)
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: root.colors.overlay

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            color: root.colors.accent
        }
    }

    Column {
        id: bar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 28
        anchors.rightMargin: 20
        spacing: 8

        Row {
            width: parent.width
            spacing: 10

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.glyph
                color: root.colors.accent
                font.family: root.iconFontFamily
                font.pixelSize: root.typography.iconSize
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 40
                spacing: 3

                Text {
                    width: parent.width
                    text: root.message
                    color: root.colors.text
                    elide: Text.ElideRight
                    font.family: root.typography.family
                    font.pixelSize: root.typography.size
                }

                Text {
                    width: parent.width
                    visible: root.detail.length > 0
                    text: root.detail
                    color: root.colors.mutedText
                    elide: Text.ElideRight
                    font.family: root.typography.family
                    font.pixelSize: root.typography.smallSize
                }
            }
        }

        Row {
            spacing: 8

            Repeater {
                model: root.actions

                ActionButton {
                    required property int index
                    required property var modelData

                    objectName: "questionAction" + index
                    colors: root.colors
                    typography: root.typography
                    label: modelData.label
                    primary: index === 0
                    enabled: modelData.enabled === undefined || modelData.enabled
                    onClicked: root.actionTriggered(index)
                }
            }
        }
    }
}
