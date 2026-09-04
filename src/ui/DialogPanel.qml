import QtQuick
import qs.Commons

// The panel grammar every question about the browser itself arrives in:
// centred over a scrim, one accent-bordered panel, a head that names the
// question and says how to leave it, a foot that says how to answer it.
//
// This file owns only that chrome. The body between the head and the foot
// belongs to whoever fills it, and so do the keys: `CommandDialog` steps rows
// with Up and Down, `ClearBrowsingDataDialog` walks a form with Tab, and a
// container that owned those keys itself could not host both. One grammar, a
// dialog per shape, no container.
Item {
    id: root

    property var colors
    property bool open: false
    property string label: ""
    property bool destructive: false
    property string confirmHint: ""
    property string cancelHint: "esc cancel"

    // The panel rather than the root carries the name, because a test looking
    // for the dialog is looking for the box it draws.
    property alias panelObjectName: panel.objectName

    default property alias content: body.data

    readonly property color edge: destructive ? colors.privateAccent : colors.accent

    signal dismissed()

    visible: open
    focus: open

    Rectangle {
        anchors.fill: parent
        color: "#99000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        id: panel
        width: Math.min(660, parent.width - 96)
        height: head.height + body.height + foot.height + 2 * border.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(80, parent.height * 0.14)
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.edge
        clip: true

        // The rows and rules stop at the border rather than running under it,
        // where a highlighted row would paint over the panel's own edge.
        Item {
            id: head
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: panel.border.width
            height: 38

            SectionLabel {
                objectName: "dialogPanelLabel"
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                text: root.label
                // The kit's header does its own muting, so a destructive
                // dialog hands it the private accent to mute rather than a
                // finished colour to paint.
                foreground: root.destructive ? root.colors.privateAccent : root.colors.text
                // A head is a bar of its own with one line in it, so the
                // label is centred on its glyphs rather than on the lean it
                // carries in a scrolling pane.
                topPadding: overshoot
                bottomPadding: overshoot
            }

            Text {
                objectName: "dialogPanelCancelHint"
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.cancelHint
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.colors.separator
            }
        }

        Column {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: panel.border.width
            anchors.rightMargin: panel.border.width
            anchors.top: head.bottom
            height: implicitHeight
            spacing: 0
        }

        Item {
            id: foot
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: panel.border.width
            anchors.rightMargin: panel.border.width
            anchors.top: body.bottom
            height: root.confirmHint.length > 0 ? 34 : 8

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                visible: root.confirmHint.length > 0
                color: root.colors.separator
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                visible: root.confirmHint.length > 0
                text: root.confirmHint
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }
    }
}
