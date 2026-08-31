import QtQuick

// A menu hangs off the control that opened it, so it stays small and close
// rather than taking the middle of the window the way a question does. It has
// the panel's border and the list's row grammar: same materials, less of them.
Item {
    id: root

    property var colors
    property var typography
    property bool open: false
    // Scene coordinates of the opening control's bottom-right corner: the menu
    // hangs from there, so it never covers the button that summoned it.
    property real anchorX: 0
    property real anchorY: 0
    property var items: []
    property int selected: 0

    signal triggered(int index)
    signal dismissed()

    visible: open
    focus: open

    onOpenChanged: {
        if (!open) return
        selected = 0
        Qt.callLater(function() { root.forceActiveFocus() })
    }

    function step(delta) {
        if (items.length === 0) return
        selected = (selected + delta + items.length) % items.length
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.dismissed()
        } else if (event.key === Qt.Key_Down) {
            root.step(1)
        } else if (event.key === Qt.Key_Up) {
            root.step(-1)
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (root.items.length > 0) root.triggered(root.selected)
        } else {
            return
        }
        event.accepted = true
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.dismissed()
    }

    Rectangle {
        id: panel
        objectName: "chromeMenuPanel"
        width: 232
        height: rows.implicitHeight + 8
        // Kept inside the window: a menu near the bottom rises instead of
        // hanging off the edge where its last row would be unreachable.
        x: Math.max(8, Math.min(root.anchorX - width, root.width - width - 8))
        y: root.anchorY + height + 8 < root.height
            ? root.anchorY + 6
            : Math.max(8, root.anchorY - height - 6)
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent

        Column {
            id: rows
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 4
            spacing: 0

            Repeater {
                model: root.items

                Rectangle {
                    required property int index
                    required property var modelData

                    objectName: "chromeMenuItem" + index
                    width: parent.width
                    height: 30
                    color: index === root.selected ? root.colors.surface : "transparent"
                    Accessible.role: Accessible.MenuItem
                    Accessible.name: modelData.label

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: modelData.destructive === true
                            ? root.colors.privateAccent
                            : root.colors.text
                        elide: Text.ElideRight
                        font.family: root.typography.family
                        font.pixelSize: root.typography.size
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = index
                        onClicked: root.triggered(index)
                    }
                }
            }
        }
    }
}
