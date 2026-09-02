import QtQuick

// Where the engine's inspector is shown: beside the tab it inspects, at a width
// the reader owns. The view itself belongs to the engine adapter — Omaweb builds
// no inspector and knows nothing about the one it is handed — so this takes it
// as a child and gives it the dock's shape and nothing else.
Item {
    id: root

    property var colors
    property var developerToolsView: null

    // A view handed over once stays this dock's until the adapter takes it
    // away. Adoption is idempotent, because the dock is also rebuilt whenever
    // the tab on show changes.
    function adopt() {
        if (!root.developerToolsView) return
        root.developerToolsView.parent = root
        root.developerToolsView.anchors.fill = root
        root.developerToolsView.visible = true
    }

    onDeveloperToolsViewChanged: root.adopt()
    Component.onCompleted: root.adopt()

    // The inspector is a page of its own and never translucent, so the dock
    // seals the desktop out from behind it as the page viewport does.
    Rectangle {
        anchors.fill: parent
        color: root.colors.windowOpaque
    }

    // The seam itself is the resizer's; this is the hairline that says where
    // the page stops and the inspector starts.
    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: root.colors.border
    }
}
