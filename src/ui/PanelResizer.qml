import QtQuick

// The seam between a panel and the page beside it — the sidebar on the left,
// docked Developer tools on the right. It straddles the border rather than
// taking a column of its own, so the panel keeps every pixel of the width the
// reader asked for. Dragging and the arrow keys move the same seam: a pointer
// is a convenience here, never the only way through.
Item {
    id: root

    property var colors
    property real currentWidth: 0
    property real minimumWidth: 0
    property real maximumWidth: 0
    property real defaultWidth: 0
    property real step: 16
    property real coarseStep: 48
    property bool dragging: false
    // Which side of the window the panel is anchored to. A panel on the right
    // grows as the pointer moves left, and its arrow keys read the same way
    // round: the key that points away from the window's edge widens it.
    property bool measureFromRight: false
    // What the seam is the width of, for the reader who reaches it by keyboard.
    property string panelName: "Sidebar"

    signal widthRequested(real width)
    // The handle reads as part of the panel, so it leaves like the rest of it:
    // Escape hands the keyboard back to the page.
    signal pageFocusRequested()

    width: 10
    activeFocusOnTab: enabled
    Accessible.role: Accessible.Splitter
    Accessible.name: root.panelName + " width"
    Accessible.description: "Arrow keys resize the " + root.panelName.toLowerCase()

    function request(width) {
        root.widthRequested(Math.max(root.minimumWidth, Math.min(root.maximumWidth, width)))
    }

    Keys.onPressed: function(event) {
        const distance = (event.modifiers & Qt.ShiftModifier) ? root.coarseStep : root.step
        const widen = root.measureFromRight ? -distance : distance
        switch (event.key) {
        case Qt.Key_Left:
            root.request(root.currentWidth - widen)
            break
        case Qt.Key_Right:
            root.request(root.currentWidth + widen)
            break
        case Qt.Key_Home:
            root.request(root.minimumWidth)
            break
        case Qt.Key_End:
            root.request(root.maximumWidth)
            break
        case Qt.Key_Return:
        case Qt.Key_Enter:
        case Qt.Key_Space:
            root.request(root.defaultWidth)
            break
        case Qt.Key_Escape:
            root.pageFocusRequested()
            break
        default:
            return
        }
        event.accepted = true
    }

    // Only lit while it is being used: an idle window shows the border the
    // sidebar already draws, not a second one.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: 2
        height: parent.height
        opacity: root.dragging || root.activeFocus || pointer.containsMouse ? 1.0 : 0.0
        color: root.dragging || root.activeFocus ? root.colors.accent : root.colors.border

        Behavior on opacity {
            NumberAnimation { duration: 90 }
        }
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.SplitHCursor

        // The handle moves with the seam it is dragging, so the grab is held
        // against the window rather than against this item.
        property real grabOffset: 0

        // The distance from the panel's own edge of the window, which is the
        // width the seam is being dragged to.
        function pointerWidth(mouse) {
            const x = root.mapToItem(root.parent, mouse.x, 0).x
            return root.measureFromRight ? root.parent.width - x : x
        }

        onPressed: function(mouse) {
            root.forceActiveFocus()
            grabOffset = pointerWidth(mouse) - root.currentWidth
            root.dragging = true
        }

        onPositionChanged: function(mouse) {
            if (root.dragging) {
                root.request(pointerWidth(mouse) - grabOffset)
            }
        }

        onReleased: root.dragging = false
        onCanceled: root.dragging = false
        onDoubleClicked: root.request(root.defaultWidth)
    }
}
