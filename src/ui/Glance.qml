import QtQuick
import qs.Commons

// A page opened by a link that asked for a new tab, shown over the tab it came
// from instead of as a tab of its own. The reader came for a look, and most
// looks end in closing: a tab for each would leave the outline holding pages
// that were never meant to stay. So the page is drawn here, over the page it
// came from, in the panel grammar every question about the browser arrives in,
// and the outline is not told. Keeping it is one command, which makes it an
// ordinary tab, page and all.
//
// The window owns the engine and its life; this is where it is drawn while it
// is a Glance. The chrome goes on answering for the tab beneath: the address,
// the find bar, back and forward are that tab's, and what the Glance's page
// wants to say it says in its own head.
//
// A `FocusScope`, for the reason `DialogPanel` is one: the page beneath settles
// its own `focus` binding after the Glance opens, and a flat item would lose
// the keyboard to it.
FocusScope {
    id: root
    objectName: "glance"

    property var colors
    property string iconFontFamily
    property bool open: false
    property bool ease: true
    // The page to blur behind the panel, when there is one. Must not be an
    // ancestor of this item.
    property Item pageSource: null
    // The engine drawing the Glance's page, parented to `pageHost` by whoever
    // built it. Null while the Glance is closed.
    property var engine: null
    // Where the Glance comes from: the link the reader pressed, in this item's
    // coordinates. A panel that grows out of the link is the link, opened,
    // rather than a second thing that appeared over the page, and it goes back
    // into the link when it closes. An empty origin means the panel lifts from
    // a little below its place instead, as a sheet does.
    property rect origin: Qt.rect(0, 0, 0, 0)
    // 0 at the origin, 1 at rest.
    property real arrival: 1
    property bool leaving: false
    readonly property bool fromOrigin: origin.width > 0

    readonly property string pageTitle: root.engine && root.engine.pageTitle.length > 0
                                        ? root.engine.pageTitle : "Glance"
    readonly property string pageAddress: root.engine ? String(root.engine.currentUrl) : ""
    // Where the engine goes, for the window to parent it into.
    readonly property alias pageHost: pageHost
    readonly property alias panelObjectName: panel.objectName

    signal closed
    signal openAsTabRequested

    // Inset enough that the page beneath is seen around it and the panel
    // reads as over the page rather than as the page.
    readonly property real inset: 40
    // What the page inside asks to be, where it knows: an extension's popup is
    // drawn for a panel of its own size and spreads badly over a whole page
    // area. Zero is a page, which takes the room it is given.
    property size preferredSize: Qt.size(0, 0)
    // The head sits above the page, so a panel asked for a page's room is that
    // much taller.
    readonly property real chrome: 38 + 2
    readonly property real restWidth: root.preferredSize.width > 0 ? Math.min(
                                                                         root.preferredSize.width,
                                                                         Math.max(0, width - 2
                                                                                  * inset)) :
                                                                     Math.max(0, width - 2 * inset)
    readonly property real restHeight: root.preferredSize.height > 0 ? Math.min(
                                                                           root.preferredSize.height
                                                                           + root.chrome, Math.max(0,
                                                                                                   height - 2
                                                                                                   * inset)) :
                                                                       Math.max(0, height - 2
                                                                                * inset)
    // A panel the size of the page area sits where the page would. One the
    // size of a popup stands in the middle of it, because a small panel pinned
    // to a corner reads as something that slipped rather than something that
    // opened.
    readonly property real restX: root.preferredSize.width > 0 ? Math.max(root.inset, (width
                                                                                       - root.restWidth)
                                                                          / 2) : root.inset
    readonly property real restY: root.preferredSize.height > 0 ? Math.max(root.inset, (height
                                                                                        - root.restHeight)
                                                                           / 2) : root.inset

    // Where the panel starts and ends: the link, or its own place a sheet's
    // lift below.
    readonly property real fromX: fromOrigin ? origin.x : inset
    readonly property real fromY: fromOrigin ? origin.y : inset + 24
    readonly property real fromWidth: fromOrigin ? origin.width : restWidth
    readonly property real fromHeight: fromOrigin ? origin.height : restHeight

    function lerp(a, b) {
        return a + (b - a) * arrival;
    }

    NumberAnimation {
        id: arrivalEase
        target: root
        property: "arrival"
        to: 1
        duration: 180
        easing.type: Easing.OutCubic
    }
    // Back into the link it grew from, quicker than it came.
    NumberAnimation {
        id: departure
        target: root
        property: "arrival"
        to: 0
        duration: 120
        easing.type: Easing.InCubic
        onFinished: {
            root.leaving = false;
            root.arrival = 1;
        }
    }

    // Drawn for the length of the retreat.
    visible: open || leaving
    focus: root.open

    onOpenChanged: {
        if (open) {
            departure.stop();
            leaving = false;
            if (ease) {
                arrival = 0;
                arrivalEase.restart();
            } else {
                arrival = 1;
            }
            root.focusPage();
            return;
        }
        arrivalEase.stop();
        if (!ease) {
            arrival = 1;
            return;
        }
        leaving = true;
        departure.restart();
    }

    function focusPage() {
        root.forceActiveFocus();
        if (root.engine)
            root.engine.focusPage();
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.closed();
            event.accepted = true;
        }
    }

    // The Glance's page takes the wheel over the panel; the scrim takes it
    // for the page beneath.
    SheetFloor {}

    // The scrim stands still while the panel grows: a ground that moved would
    // show the page's edge above it for the length of the arrival.
    PageBackdrop {
        objectName: "glanceBackdrop"
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
        opacity: root.arrival

        MouseArea {
            anchors.fill: parent
            onClicked: root.closed()
        }
    }

    // Between the origin and rest by `arrival`. What is inside keeps its
    // resting size the whole way and the panel clips it, so the page is
    // revealed rather than laid out again at every size the panel passes
    // through: a webpage viewport is never resized for a movement.
    Rectangle {
        id: panel
        objectName: "glancePanel"
        x: root.lerp(root.fromX, root.restX)
        y: root.lerp(root.fromY, root.restY)
        width: root.lerp(root.fromWidth, root.restWidth)
        height: root.lerp(root.fromHeight, root.restHeight)
        opacity: root.fromOrigin ? 1 : root.arrival
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        clip: true

        // A click on the panel's own edge is not a click outside it.
        MouseArea {
            anchors.fill: parent
        }

        Item {
            id: head
            x: panel.border.width
            y: panel.border.width
            width: root.restWidth - 2 * panel.border.width
            height: 38

            SectionLabel {
                id: title
                objectName: "glanceTitle"
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: address.left
                anchors.rightMargin: Style.spacing.md
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                text: root.pageTitle
                elide: Text.ElideRight
                topPadding: overshoot
                bottomPadding: overshoot
            }

            // The address beside the name, quiet, because the address trigger
            // in the sidebar goes on reporting the tab beneath and the reader
            // has to be able to see where the Glance has taken them.
            Text {
                id: address
                objectName: "glanceAddress"
                anchors.right: hint.left
                anchors.rightMargin: Style.spacing.lg
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, head.width * 0.4)
                text: root.pageAddress
                elide: Text.ElideMiddle
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                id: hint
                objectName: "glanceHint"
                anchors.right: openAsTab.left
                anchors.rightMargin: Style.spacing.md
                anchors.verticalCenter: parent.verticalCenter
                text: "esc closes"
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            ChromeButton {
                id: openAsTab
                objectName: "openGlanceAsTabButton"
                anchors.right: closeButton.left
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                icon: "open_in_new"
                accessibleName: "Open as tab"
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.openAsTabRequested()
            }

            ChromeButton {
                id: closeButton
                objectName: "closeGlanceButton"
                anchors.right: parent.right
                anchors.rightMargin: Style.spacing.sm
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                icon: "close"
                accessibleName: "Close Glance"
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.closed()
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.colors.separator
            }
        }

        // The page, on the opaque ground a webpage needs, inside the border
        // rather than under it. Clipped at its own bounds: the engine paints a
        // pixel past its item, which is the border's pixel on the two sides the
        // page meets.
        Rectangle {
            id: pageHost
            objectName: "glancePageHost"
            x: panel.border.width
            y: head.y + head.height
            width: head.width
            height: root.restHeight - y - panel.border.width
            color: root.colors.windowOpaque
            clip: true
        }
    }
}
