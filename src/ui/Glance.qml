import QtQuick
import qs.Commons
import qs.Ui as Omarchy
import "DevicePixels.mjs" as DevicePixels

// A page opened by a link that asked for a new tab, shown over the tab it came
// from instead of as a tab of its own. The reader came for a look, and most
// looks end in closing: a tab for each would leave the outline holding pages
// that were never meant to stay. So the page is drawn here, over the page it
// came from, in the panel grammar every question about the browser arrives in,
// and the outline gets no row for it, only a keep button on the row of the tab
// it stands over (ADR 0041, #747). Keeping it is one command, which makes it
// an ordinary tab, page and all.
//
// The window owns the engine and its life; this is where it is drawn while it
// is a Glance. The chrome goes on answering for the tab beneath: the address
// trigger and the find bar are that tab's. The page's own controls are the
// Glance's head, along the panel's top edge: back, forward and reload walk the
// history the Glance's engine keeps, and the window sends those commands here
// from keys and the command scope too while a Glance is open (#839).
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
    // Keeping the Glance as a tab, as Zen's does: the panel grows into the
    // page area, and the page beneath comes forward and loses its dim under
    // it. `keep` is 0 at rest and 1 at the page area, where the panel is the
    // page as the tab will draw it, so the window can hand the engine over
    // without anything on screen changing (#704).
    property real keep: 0
    property bool keeping: false
    readonly property bool fromOrigin: origin.width > 0

    readonly property string pageAddress: root.engine ? String(root.engine.currentUrl) : ""
    // Where the engine goes, for the window to parent it into.
    readonly property alias pageHost: pageHost
    readonly property alias panelObjectName: panel.objectName
    // How far the page beneath stands back while the Glance is over it, for
    // whoever draws that page to scale it by: it stays in sight, behind, as
    // the page the Glance came from, and comes forward again as it closes.
    readonly property real beneathScale: visible ? 1 - 0.04 * arrival * (1 - keep) : 1

    signal closed
    signal openAsTabRequested
    signal backRequested
    signal forwardRequested
    signal reloadRequested
    // The panel is the page area: the engine is the tab's to take.
    signal kept

    // The keymap the head's Key labels are read from, and whether Primary is
    // held for them.
    property var keyMap: null
    property bool keyLabelsShown: false
    // Whether the keyboard is in the head rather than in the page.
    readonly property bool headFocused: head.activeFocus

    // On whole pixels of the display, so the page under it starts on one.
    readonly property real headHeight: DevicePixels.snap(Style.space(36), pixelRatio)

    // A page's Glance stands the page area's full height between margins wide
    // enough that the page beneath is seen either side, so the panel reads as
    // over the page rather than as the page.
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
    // A popup's height is its page's: the head stands on top of it rather than
    // taking from it.
    readonly property real restHeight: root.preferredSize.height > 0 ? Math.min(
                                                                           root.preferredSize.height
                                                                           + root.headHeight,
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

    function toPage(value, page) {
        return value + (page - value) * keep;
    }

    // Grows into the page area and says so, or says so at once with motion
    // off, which makes keeping a cut.
    function keepAsTab() {
        if (root.keeping)
            return;
        if (!root.ease) {
            root.kept();
            return;
        }
        root.keeping = true;
        keepEase.restart();
    }

    // The page is the tab's and drawn by it, so the Glance can go.
    function settle() {
        keepEase.stop();
        root.keeping = false;
        root.keep = 0;
    }

    // Keeping refuses every close, so it cannot be left to stick: a page its
    // tab never came to draw would leave the Glance over the page for good.
    // Well past the growth and the handoff, it lets go on its own.
    Timer {
        interval: 1000
        running: root.keeping
        onTriggered: root.settle()
    }

    // As quick as the arrival, so keeping reads as the arrival carried on.
    NumberAnimation {
        id: keepEase
        target: root
        property: "keep"
        from: 0
        to: 1
        duration: 180
        easing.type: Easing.OutCubic
        onFinished: root.kept()
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

    // Drawn for the length of the retreat, and while the tab takes the page
    // over.
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
        // A kept Glance does not go back into its link: it is the tab now.
        if (!ease || keeping) {
            arrival = 1;
            return;
        }
        leaving = true;
        departure.restart();
    }

    // The window opens the Glance as it hands it the engine, and `open` can
    // be told first: the page is focused once it is there to focus.
    onEngineChanged: if (root.open && root.engine)
                         root.focusPage()

    function focusPage() {
        root.forceActiveFocus();
        if (root.engine)
            root.engine.focusPage();
    }

    // The keyboard comes into the head on the first button it can press.
    function focusHead() {
        head.cursor = head.step(-1, 1, false);
        head.forceActiveFocus();
    }

    // The head stands above the page, so up from the page reaches it and down
    // from it returns. The keyboard is the Glance's while it is in either, and
    // a move another way leaves it where it is. Answers whether the move was
    // the Glance's to take.
    function moveFocus(stepY) {
        if (!root.activeFocus)
            return false;
        if (root.headFocused && stepY > 0)
            root.focusPage();
        else if (!root.headFocused && stepY < 0)
            root.focusHead();
        return true;
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
        radius: 3 * (1 - root.keep)
        // Clear once the tab has the engine, so its page shows through until
        // the Glance goes.
        color: root.keeping && !root.engine ? "transparent" : panelDither.fill
        // One logical pixel, rounded to whole pixels of the display, so the
        // page inside it starts on one.
        border.width: DevicePixels.snap(1, root.pixelRatio)
        border.color: Qt.alpha(root.colors.accent, 1 - root.keep)
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
        // rather than under it and below the head. Clipped at its own bounds:
        // the engine paints a pixel past its item, which is the border's pixel
        // on the two sides the page meets.
        Rectangle {
            id: pageHost
            objectName: "glancePageHost"
            x: panel.border.width * (1 - root.keep)
            y: (panel.border.width + root.headHeight) * (1 - root.keep)
            readonly property real rest: root.restHeight - 2 * panel.border.width - root.headHeight
            // Laid out once at the page area's size as keeping starts, and
            // revealed by the panel as it grows, as the arrival reveals it.
            width: root.keeping ? root.width : root.restWidth - 2 * panel.border.width
            height: root.keeping ? root.height : rest
            color: root.keeping && !root.engine ? "transparent" : root.colors.windowOpaque
            clip: true
        }

        // The head of the page the Glance holds, at its resting width so the
        // panel reveals it as it reveals the page. Gone early in the keeping,
        // as the head of a page that is a tab now.
        //
        // The buttons are no keyboard stops of their own. The head holds the
        // keyboard and walks a cursor over them, as the outline walks the
        // Sidebar cursor over its rows: Tab inside a Glance belongs to its
        // page, so `Alt+K` is the way in, and `Alt+J` or `Escape` the way out.
        FocusScope {
            id: head
            objectName: "glanceHead"
            x: panel.border.width
            y: panel.border.width
            width: root.restWidth - 2 * panel.border.width
            height: root.headHeight
            opacity: 1 - Math.min(1, 3 * root.keep)

            // The button under the cursor, by its place in `buttons`.
            property int cursor: 0
            readonly property var buttons: [backButton, forwardButton, reloadButton, openAsTabButton,
                closeButton]

            function pressable(index) {
                const button = head.buttons[index];
                return button !== undefined && button.visible && button.enabled;
            }

            // Back at the first page, back is disabled under the cursor: the
            // cursor moves on to a button the keyboard can still press.
            readonly property bool cursorPressable: head.pressable(head.cursor)
            onCursorPressableChanged: if (!head.cursorPressable)
                                          head.cursor = head.step(head.cursor, 1, true)

            // The next button the keyboard can press from `from` towards
            // `delta`, round the ends when `wrap` is set, or `from` itself
            // when there is none that way.
            function step(from, delta, wrap) {
                const count = head.buttons.length;
                let at = from;
                for (let tried = 0; tried < count; ++tried) {
                    at += delta;
                    if (wrap)
                        at = (at + count) % count;
                    else if (at < 0 || at >= count)
                        return from;
                    if (head.pressable(at))
                        return at;
                }
                return from;
            }

            Keys.onPressed: function (event) {
                switch (event.key) {
                case Qt.Key_Left:
                    head.cursor = head.step(head.cursor, -1, false);
                    break;
                case Qt.Key_Right:
                    head.cursor = head.step(head.cursor, 1, false);
                    break;
                case Qt.Key_Tab:
                    head.cursor = head.step(head.cursor, 1, true);
                    break;
                case Qt.Key_Backtab:
                    head.cursor = head.step(head.cursor, -1, true);
                    break;
                case Qt.Key_Return:
                case Qt.Key_Enter:
                case Qt.Key_Space:
                    if (head.pressable(head.cursor))
                        head.buttons[head.cursor].clicked();
                    break;
                case Qt.Key_Escape:
                    root.focusPage();
                    break;
                default:
                    return;
                }
                event.accepted = true;
            }

            Row {
                id: startButtons
                x: DevicePixels.snap(Style.spacing.sm, root.pixelRatio)
                y: DevicePixels.snap((head.height - height) / 2, root.pixelRatio)
                spacing: Style.spacing.sm

                HeadButton {
                    id: backButton
                    objectName: "glanceBackButton"
                    icon: "arrow_back"
                    accessibleName: qsTr("Back")
                    enabled: root.engine ? root.engine.canGoBack : false
                    onClicked: root.backRequested()
                }

                HeadButton {
                    id: forwardButton
                    objectName: "glanceForwardButton"
                    icon: "arrow_forward"
                    accessibleName: qsTr("Forward")
                    enabled: root.engine ? root.engine.canGoForward : false
                    onClicked: root.forwardRequested()
                }

                HeadButton {
                    id: reloadButton
                    objectName: "glanceReloadButton"
                    icon: "refresh"
                    accessibleName: qsTr("Reload")
                    onClicked: root.reloadRequested()
                }
            }

            Text {
                objectName: "glanceAddress"
                anchors.left: startButtons.right
                anchors.leftMargin: Style.spacing.lg
                anchors.right: endButtons.left
                anchors.rightMargin: Style.spacing.lg
                height: head.height
                verticalAlignment: Text.AlignVCenter
                text: root.pageAddress
                color: root.colors.mutedText
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Row {
                id: endButtons
                x: DevicePixels.snap(head.width - width - Style.spacing.sm, root.pixelRatio)
                y: startButtons.y
                spacing: Style.spacing.sm

                HeadButton {
                    id: openAsTabButton
                    objectName: "openGlanceAsTabButton"
                    visible: root.openAsTabAllowed
                    icon: "open_in_new"
                    accessibleName: qsTr("Open as tab")
                    onClicked: root.openAsTabRequested()
                }

                HeadButton {
                    id: closeButton
                    objectName: "closeGlanceButton"
                    icon: "close"
                    accessibleName: qsTr("Close Glance")
                    onClicked: root.closed()
                }
            }

            // The Sidebar cursor, on the button holding the keyboard: the
            // accent at 7% with an accent border, as on a tab row.
            Omarchy.BorderSurface {
                objectName: "glanceHeadCursor"
                readonly property Item button: head.buttons[head.cursor] || null
                visible: head.activeFocus && head.pressable(head.cursor)
                x: button ? button.parent.x + button.x : 0
                y: button ? button.parent.y + button.y : 0
                width: button ? button.width : 0
                height: button ? button.height : 0
                radius: 2
                color: Qt.alpha(root.colors.accent, 0.07)
                borderSpec: Border.flat(root.colors.accent, 1)
            }

            // The labels belong to the head rather than to their buttons: a
            // disabled button is drawn at 35%, and a label that inherited that
            // would let the page show through it. Each hangs from the bottom
            // of its button, as in the outline's navigation row.
            HeadKeyLabel {
                objectName: "keyLabel-glanceBackButton"
                button: backButton
                command: "back"
            }

            HeadKeyLabel {
                objectName: "keyLabel-glanceForwardButton"
                button: forwardButton
                command: "forward"
            }

            HeadKeyLabel {
                objectName: "keyLabel-glanceReloadButton"
                button: reloadButton
                command: "reload"
            }

            HeadKeyLabel {
                objectName: "keyLabel-openGlanceAsTabButton"
                button: openAsTabButton
                command: "glance-to-tab"
            }

            HeadKeyLabel {
                objectName: "keyLabel-closeGlanceButton"
                button: closeButton
                command: "close-tab"
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: DevicePixels.snap(1, root.pixelRatio)
                color: root.colors.separator
            }
        }
    }

    // The outline's navigation buttons, in its sizes.
    component HeadButton: ChromeButton {
        width: 28
        height: 26
        // The head walks its cursor over the buttons, so none is a stop the
        // kit marks with its own focus ring.
        focusable: false
        fontFamily: root.iconFontFamily
        accent: root.colors.accent
    }

    component HeadKeyLabel: KeyLabel {
        required property Item button
        required property string command

        iconFontFamily: root.iconFontFamily
        // Centred under its button, or, wider than it, as a chord of caps is,
        // ending at its right edge, so it keeps clear of the next button's.
        x: DevicePixels.snap(button.parent.x + button.x + Math.min(0, button.width - width)
                             + Math.max(0, button.width - width) / 2, root.pixelRatio)
        y: DevicePixels.snap(button.parent.y + button.y + button.height - 4, root.pixelRatio)
        keys: root.keyLabelsShown && root.keyMap ? root.keyMap.labelFor(command) : ""
        shown: root.keyLabelsShown && button.visible
        colors: root.colors
    }
}
