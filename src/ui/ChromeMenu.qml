import QtQuick
import qs.Commons

// A menu hangs off the control that opened it, so it stays small and close
// rather than taking the middle of the window the way a question does. It has
// the panel's border and the list's row grammar: same materials, less of them.
Item {
    id: root

    property var colors
    property bool open: false
    // Scene coordinates of the opening control's bottom-right corner: the menu
    // hangs from there, so it never covers the button that summoned it.
    property real anchorX: 0
    property real anchorY: 0
    // A menu the pointer opened starts at the pointer instead, the way every
    // context menu does: there is no control under it to keep clear of.
    property bool fromPointer: false
    property var items: []
    property int selected: 0
    property real itemWidth: 232

    signal triggered(int index)
    signal dismissed()

    visible: open
    focus: open

    onOpenChanged: {
        if (!open) return
        selected = root.firstRunnable()
        Qt.callLater(function() { root.forceActiveFocus() })
    }

    // A separator and a row that cannot be run are both passed over: the
    // keyboard never lands somewhere pressing return would do nothing.
    function runnable(index) {
        const item = root.items[index]
        return item !== undefined && item.separator !== true && item.enabled !== false
    }

    function step(delta) {
        if (items.length === 0) return
        for (let count = 0; count < items.length; ++count) {
            selected = (selected + delta + items.length) % items.length
            if (root.runnable(selected)) return
        }
    }

    // Opening on a row that cannot be run would put the keyboard nowhere.
    function firstRunnable() {
        for (let index = 0; index < root.items.length; ++index) {
            if (root.runnable(index)) return index
        }
        return 0
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.dismissed()
        } else if (event.key === Qt.Key_Down) {
            root.step(1)
        } else if (event.key === Qt.Key_Up) {
            root.step(-1)
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (root.runnable(root.selected)) root.triggered(root.selected)
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
        width: root.itemWidth
        height: rows.implicitHeight + 8 + 2 * border.width
        // Kept inside the window: a menu near the bottom rises instead of
        // hanging off the edge where its last row would be unreachable.
        x: Math.max(8, Math.min(root.fromPointer ? root.anchorX : root.anchorX - width,
            root.width - width - 8))
        y: root.anchorY + height + 8 < root.height
            ? root.anchorY + 6
            : Math.max(8, root.anchorY - height - 6)
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent

        // A highlighted row stops at the border instead of painting over it.
        Column {
            id: rows
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: parent.border.width
            anchors.rightMargin: parent.border.width
            anchors.topMargin: 4 + parent.border.width
            spacing: 0

            Repeater {
                model: root.items

                Rectangle {
                    required property int index
                    required property var modelData

                    readonly property bool separator: modelData.separator === true
                    readonly property bool runnable: root.runnable(index)

                    objectName: separator ? "chromeMenuSeparator" + index
                        : "chromeMenuItem" + index
                    width: parent.width
                    height: separator ? 9 : 30
                    color: !separator && index === root.selected
                        ? root.colors.surface : "transparent"
                    Accessible.role: separator ? Accessible.Separator : Accessible.MenuItem
                    Accessible.name: separator ? "" : modelData.label

                    // A rule between groups of rows, at the same inset as their
                    // type so it reads as part of the list rather than across it.
                    Rectangle {
                        visible: parent.separator
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        height: 1
                        color: root.colors.border
                    }

                    Text {
                        visible: !parent.separator
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label !== undefined ? modelData.label : ""
                        color: modelData.destructive === true
                            ? root.colors.privateAccent
                            : (parent.runnable ? root.colors.text : root.colors.mutedText)
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: parent.runnable
                        hoverEnabled: parent.runnable
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = index
                        onClicked: root.triggered(index)
                    }
                }
            }
        }
    }
}
