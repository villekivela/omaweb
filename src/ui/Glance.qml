import QtQuick
import qs.Commons
import "DevicePixels.mjs" as DevicePixels

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
// the find bar, back and forward are that tab's. The Glance is the page alone,
// as Zen's is, with its two buttons beside it rather than a head over it
// (#704).
//
// A `FocusScope`, for the reason `DialogPanel` is one: the page beneath settles
// its own `focus` binding after the Glance opens, and a flat item would lose
// the keyboard to it.
FocusScope {
    id: root
    objectName: "glance"
    // The display's pixels per logical pixel, for resting on whole ones.
    readonly property real pixelRatio: Window.window ? Window.window.devicePixelRatio : 1

    property var colors
    property string iconFontFamily
    property bool open: false
    property bool ease: true
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

    readonly property string pageAddress: root.engine ? String(root.engine.currentUrl) : ""
    // Where the engine goes, for the window to parent it into.
    readonly property alias pageHost: pageHost
    readonly property alias panelObjectName: panel.objectName
    // How far the page beneath stands back while the Glance is over it, for
    // whoever draws that page to scale it by: it stays in sight, behind, as
    // the page the Glance came from, and comes forward again as it closes.
    readonly property real pageScale: visible ? 1 - 0.04 * arrival : 1

    signal closed
    signal openAsTabRequested

    // A page's Glance stands the page area's full height between margins wide
    // enough that the page beneath is seen either side, so the panel reads as
    // over the page rather than as the page, and its buttons stand in one.
    readonly property real margin: DevicePixels.snap(width * 0.12, pixelRatio)
    // How close an extension's popup may come to the page area's edges.
    readonly property real inset: 40
    // What the page inside asks to be, where it knows: an extension's popup is
    // drawn for a panel of its own size and spreads badly over a whole page
    // area. Zero is a page, which takes the room it is given.
    property size preferredSize: Qt.size(0, 0)
    // Whether keeping the page is on offer. A Glance from a link is a page the
    // reader may want to stay on. An extension's popup is not a page: it is a
    // control that closes when it is done with, and a tab holding one would go
    // on showing a menu for a window it no longer belongs to.
    property bool openAsTabAllowed: true
    readonly property real restWidth: root.preferredSize.width > 0 ? Math.min(
                                                                         root.preferredSize.width,
                                                                         Math.max(0, width - 2
                                                                                  * inset)) :
                                                                     Math.max(0, width - 2 * margin)
    readonly property real restHeight: root.preferredSize.height > 0 ? Math.min(
                                                                           root.preferredSize.height,
                                                                           Math.max(0, height - 2
                                                                                    * inset)) :
                                                                       height
    // A panel the size of the page area sits where the page would. One the
    // size of a popup stands in the middle of it, because a small panel pinned
    // to a corner reads as something that slipped rather than something that
    // opened. Rounded, because the middle of an odd room is half a pixel: the
    // page's texture is drawn as two triangles, and at half a pixel each one
    // rounds the other way, which splits the page along its diagonal.
    readonly property real restX: root.preferredSize.width > 0 ? root.centred(width,
                                                                              root.restWidth) :
                                                                 root.margin
    readonly property real restY: root.preferredSize.height > 0 ? root.centred(height,
                                                                               root.restHeight) : 0

    // Where the panel starts and ends: the link, or its own place a sheet's
    // lift below.
    readonly property real fromX: fromOrigin ? origin.x : restX
    readonly property real fromY: fromOrigin ? origin.y : restY + 24
    readonly property real fromWidth: fromOrigin ? origin.width : restWidth
    readonly property real fromHeight: fromOrigin ? origin.height : restHeight

    function centred(room, size) {
        return Math.max(root.inset, DevicePixels.snap((room - size) / 2, root.pixelRatio));
    }

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

    // A list Omaweb drew under a field of the Glance's page takes the next
    // Escape for itself.
    property bool escapeTaken: false

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape && !root.escapeTaken) {
            root.closed();
            event.accepted = true;
        }
    }

    // The Glance's page takes the wheel over the panel; the scrim takes it
    // for the page beneath.
    SheetFloor {}

    // The page beneath is dimmed rather than blurred: it is still the page the
    // Glance came from, and stays readable as one. The dim stands still while
    // the panel grows, and a click on it is a click outside the Glance.
    Rectangle {
        objectName: "glanceDim"
        anchors.fill: parent
        color: "black"
        opacity: 0.45 * root.arrival

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
        color: panelDither.fill
        // One logical pixel, rounded to whole pixels of the display, so the
        // page inside it starts on one.
        border.width: DevicePixels.snap(1, root.pixelRatio)
        border.color: root.colors.accent
        clip: true

        GroundDither {
            id: panelDither
            ground: root.colors.overlay
        }

        // A click on the panel's own edge is not a click outside it.
        MouseArea {
            anchors.fill: parent
        }

        // The page, on the opaque ground a webpage needs, inside the border
        // rather than under it. Clipped at its own bounds: the engine paints a
        // pixel past its item, which is the border's pixel on the two sides the
        // page meets.
        Rectangle {
            id: pageHost
            objectName: "glancePageHost"
            x: panel.border.width
            y: panel.border.width
            width: root.restWidth - 2 * panel.border.width
            height: root.restHeight - 2 * panel.border.width
            color: root.colors.windowOpaque
            clip: true
        }
    }

    // Close and keep, floating beside the panel's top corner where a head
    // would have held them.
    Rectangle {
        objectName: "glanceButtons"
        x: panel.x + panel.width + Style.spacing.lg
        y: panel.y + Style.spacing.lg
        width: buttons.implicitWidth + 2 * Style.spacing.sm
        height: buttons.implicitHeight + 2 * Style.spacing.sm
        radius: width / 2
        color: buttonsDither.fill
        border.width: DevicePixels.snap(1, root.pixelRatio)
        border.color: root.colors.separator
        opacity: root.arrival

        GroundDither {
            id: buttonsDither
            ground: root.colors.overlay
        }

        Column {
            id: buttons
            anchors.centerIn: parent
            spacing: Style.spacing.sm

            ChromeButton {
                objectName: "closeGlanceButton"
                width: 30
                height: 30
                icon: "close"
                accessibleName: qsTr("Close Glance")
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.closed()
            }

            ChromeButton {
                objectName: "openGlanceAsTabButton"
                visible: root.openAsTabAllowed
                width: 30
                height: 30
                icon: "open_in_new"
                accessibleName: qsTr("Open as tab")
                fontFamily: root.iconFontFamily
                foreground: root.colors.mutedText
                accent: root.colors.accent
                onClicked: root.openAsTabRequested()
            }
        }
    }
}
