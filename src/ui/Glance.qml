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
// the find bar, back and forward are that tab's, and what the Glance's page
// wants to say it says in its own head.
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
    // PROTOTYPE #704: the keep-as-tab motion, driven by Main.qml's clock.
    // `keep` is 0 at rest and 1 at the page area; `dissolve` fades the frame
    // away from a page that is already the tab's; `keepPulse` scales the panel.
    property bool keeping: false
    property real keep: 0
    property real dissolve: 0
    property real keepPulse: 1
    function toPage(value, page) {
        return value + (page - value) * keep;
    }
    readonly property bool fromOrigin: origin.width > 0

    // PROTOTYPE #704: the title stays while the frame dissolves off the tab.
    property string keptTitle: ""
    readonly property string pageTitle: root.engine && root.engine.pageTitle.length > 0
                                        ? root.engine.pageTitle : root.keeping && keptTitle !== ""
                                          ? keptTitle : qsTr("Glance")
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
    // Whether keeping the page is on offer. A Glance from a link is a page the
    // reader may want to stay on. An extension's popup is not a page: it is a
    // control that closes when it is done with, and a tab holding one would go
    // on showing a menu for a window it no longer belongs to.
    property bool openAsTabAllowed: true
    // The head sits above the page, so a panel asked for a page's room is that
    // much taller.
    readonly property real chrome: 38 + 2
    // PROTOTYPE #704, variant D: Zen's Glance. No head; the panel stands the
    // page area's full height between wide side margins, the page it came from
    // scaled back and dimmed behind it, and its buttons float beside it.
    property bool zenRest: false
    readonly property real sideInset: zenRest ? DevicePixels.snap(width * 0.12, pixelRatio) : inset
    readonly property real topInset: zenRest ? 0 : inset
    readonly property real restWidth: root.preferredSize.width > 0 ? Math.min(
                                                                         root.preferredSize.width,
                                                                         Math.max(0, width - 2
                                                                                  * inset)) :
                                                                     Math.max(0, width - 2
                                                                              * sideInset)
    readonly property real restHeight: root.preferredSize.height > 0 ? Math.min(
                                                                           root.preferredSize.height
                                                                           + root.chrome, Math.max(0,
                                                                                                   height - 2
                                                                                                   * inset)) :
                                                                       Math.max(0, height - 2
                                                                                * topInset)
    // A panel the size of the page area sits where the page would. One the
    // size of a popup stands in the middle of it, because a small panel pinned
    // to a corner reads as something that slipped rather than something that
    // opened. Rounded, because the middle of an odd room is half a pixel: the
    // page's texture is drawn as two triangles, and at half a pixel each one
    // rounds the other way, which splits the page along its diagonal.
    readonly property real restX: root.preferredSize.width > 0 ? root.centred(width,
                                                                              root.restWidth) :
                                                                 root.sideInset
    readonly property real restY: root.preferredSize.height > 0 ? root.centred(height,
                                                                               root.restHeight) :
                                                                  root.topInset

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
    visible: open || leaving || keeping
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
        if (!ease || keeping) {
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

    // The scrim stands still while the panel grows: a ground that moved would
    // show the page's edge above it for the length of the arrival.
    PageBackdrop {
        objectName: "glanceBackdrop"
        anchors.fill: parent
        source: root.pageSource
        tint: root.colors.sheet
        opacity: root.zenRest ? 0 : root.arrival * (1 - root.keep) * (1 - root.dissolve)

        MouseArea {
            anchors.fill: parent
            onClicked: root.closed()
        }
    }

    // PROTOTYPE #704, variant D: the page beneath is dimmed, not blurred.
    Rectangle {
        anchors.fill: parent
        visible: root.zenRest
        color: "black"
        opacity: 0.45 * root.arrival * (1 - root.keep)

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
        x: root.toPage(root.lerp(root.fromX, root.restX), 0)
        y: root.toPage(root.lerp(root.fromY, root.restY), 0)
        width: root.toPage(root.lerp(root.fromWidth, root.restWidth), root.width)
        height: root.toPage(root.lerp(root.fromHeight, root.restHeight), root.height)
        opacity: root.fromOrigin ? 1 : root.arrival
        // PROTOTYPE #704: the pulse widens the panel, never heightens it.
        transform: Scale {
            origin.x: panel.width / 2
            xScale: root.keepPulse
        }
        radius: 3 * (1 - root.keep)
        color: root.keeping && !root.engine ? "transparent" : panelDither.fill
        // One logical pixel, rounded to whole pixels of the display, so the
        // page inside it starts on one.
        border.width: DevicePixels.snap(1, root.pixelRatio)
        border.color: Qt.rgba(root.colors.accent.r, root.colors.accent.g, root.colors.accent.b, (1
                                                                                                 - root.keep)
                              * (1 - root.dissolve))
        clip: true

        GroundDither {
            id: panelDither
            ground: root.colors.overlay
        }

        // A click on the panel's own edge is not a click outside it.
        MouseArea {
            anchors.fill: parent
        }

        Item {
            id: head
            visible: !root.zenRest
            x: panel.border.width
            y: panel.border.width - 39 * root.keep - 10 * root.dissolve
            opacity: 1 - root.dissolve
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
                anchors.right: openAsTab.visible ? openAsTab.left : closeButton.left
                anchors.rightMargin: Style.spacing.md
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("esc closes")
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            ChromeButton {
                id: openAsTab
                objectName: "openGlanceAsTabButton"
                visible: root.openAsTabAllowed
                anchors.right: closeButton.left
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
                icon: "open_in_new"
                accessibleName: qsTr("Open as tab")
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
                accessibleName: qsTr("Close Glance")
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
            x: panel.border.width * (1 - root.keep)
            y: root.zenRest ? panel.border.width * (1 - root.keep) : root.keeping ? (panel.border.width
                                                                                     + 38) * (1
                                                                                              - root.keep) :
                                                                                    DevicePixels.snap(
                                                                                        head.y + head.height,
                                                                                        root.pixelRatio)
            // PROTOTYPE #704: the page is laid out once at the page area's size
            // as the motion starts, and the growing panel reveals it.
            width: root.keeping ? root.width : head.width
            height: root.keeping ? root.height : root.restHeight - y - panel.border.width
            color: root.keeping && !root.engine ? "transparent" : root.colors.windowOpaque
            clip: true
        }
    }

    // PROTOTYPE #704, variant D: the Glance's buttons, beside the panel rather
    // than in a head, gone as it grows into the tab.
    Rectangle {
        visible: root.zenRest && root.keep < 1
        x: panel.x + panel.width + 8
        y: panel.y + 8
        width: zenButtons.implicitWidth + 8
        height: zenButtons.implicitHeight + 8
        radius: width / 2
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.separator
        opacity: root.arrival * (1 - Math.min(1, root.keep * 3))

        Column {
            id: zenButtons
            anchors.centerIn: parent
            spacing: 4

            ChromeButton {
                objectName: "zenCloseGlanceButton"
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
                objectName: "zenOpenGlanceAsTabButton"
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
