// THROWAWAY PROTOTYPE — the Omnibar is shared by every variant and is NOT
// under evaluation here. It exists so the address trigger does something.
import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var colors
    property bool open: false
    property bool newTabIntent: false
    property string presetText: ""

    signal dismissed()
    signal committed(string text)

    visible: open

    onOpenChanged: {
        if (!open) return
        input.text = presetText
        Qt.callLater(function() {
            input.forceActiveFocus()
            input.selectAll()
        })
    }

    Rectangle {
        anchors.fill: parent
        color: "#66000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        width: Math.min(720, parent.width - 80)
        height: 70
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(90, parent.height * 0.18)
        radius: 16
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.border

        TextField {
            id: input
            anchors.fill: parent
            anchors.margins: 12
            background: null
            color: root.colors.text
            placeholderText: root.newTabIntent ? "Open in new tab" : "Search or enter address"
            placeholderTextColor: root.colors.mutedText
            font.pixelSize: 19
            selectByMouse: true

            onAccepted: {
                if (text.trim().length === 0) return
                root.committed(text)
            }

            Keys.onEscapePressed: function(event) {
                root.dismissed()
                event.accepted = true
            }
        }
    }
}
