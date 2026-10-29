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
// trigger and the find bar are that tab's. The Glance's own controls stand in
// a column beside the panel: back, forward and reload there walk the history
// the Glance's engine keeps, and the window sends those commands here from
// keys and the command scope too while a Glance is open (#839).
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

    // The keymap the controls' Key labels are read from, and whether Primary
    // is held for them.
    property var keyMap: null
    property bool keyLabelsShown: false
    // Whether the keyboard is in the controls rather than in the page.
    readonly property bool controlsFocused: controls.activeFocus

    // A page's Glance stands the page area's full height between margins wide
    // enough that the page beneath is seen either side, so the panel reads as
    // over the page rather than as the page, and its controls stand in one.
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

    // The keyboard comes into the controls on the first button it can press.
    function focusControls() {
        controls.cursor = controls.step(-1, 1, false);
        controls.forceActiveFocus();
    }

    // The controls stand by the panel's top, so up from the page reaches them
    // and down from them returns. The keyboard is the Glance's while it is in
    // either, and a move another way leaves it where it is. Answers whether
    // the move was the Glance's to take.
    function moveFocus(stepY) {
        if (!root.activeFocus)
            return false;
        if (root.controlsFocused && stepY > 0)
            root.focusPage();
        else if (!root.controlsFocused && stepY < 0)
            root.focusControls();
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
        // rather than under it. Clipped at its own bounds: the engine paints a
        // pixel past its item, which is the border's pixel on the two sides the
        // page meets.
        Rectangle {
            id: pageHost
            objectName: "glancePageHost"
            x: panel.border.width * (1 - root.keep)
            y: panel.border.width * (1 - root.keep)
            // Laid out once at the page area's size as keeping starts, and
            // revealed by the panel as it grows, as the arrival reveals it.
            width: root.keeping ? root.width : root.restWidth - 2 * panel.border.width
            height: root.keeping ? root.height : root.restHeight - 2 * panel.border.width
            color: root.keeping && !root.engine ? "transparent" : root.colors.windowOpaque
            clip: true
        }
    }

    // The Glance's controls, a column of buttons on a plate beside the
    // panel's top corner: close and open as tab, then back, forward and reload
    // for the page inside. Gone early in the keeping, as the controls of a
    // page that is a tab now.
    //
    // The buttons are no keyboard stops of their own. The column holds the
    // keyboard and walks a cursor over them, as the outline walks the Sidebar
    // cursor over its rows: Tab inside a Glance belongs to its page, so
    // `Alt+K` is the way in, and `Alt+J` or `Escape` the way out.
    FocusScope {
        id: controls
        objectName: "glanceButtons"
        // On whole pixels of the display, as the panel is, so the glyphs are
        // not drawn soft at a fractional scale.
        x: DevicePixels.snap(panel.x + panel.width + Style.spacing.lg, root.pixelRatio)
        y: DevicePixels.snap(panel.y + Style.spacing.lg, root.pixelRatio)
        width: buttons.width
        height: buttons.height
        opacity: root.arrival * (1 - Math.min(1, 3 * root.keep))

        // The button under the cursor, by its place in `order`.
        property int cursor: 0
        readonly property var order: [closeButton, openAsTabButton, backButton, forwardButton,
            reloadButton]

        function pressable(index) {
            const button = controls.order[index];
            return button !== undefined && button.visible && button.enabled;
        }

        // Back at the first page, back is disabled under the cursor: the
        // cursor moves on to a button the keyboard can still press.
        readonly property bool cursorPressable: controls.pressable(controls.cursor)
        onCursorPressableChanged: if (!controls.cursorPressable)
                                      controls.cursor = controls.step(controls.cursor, 1, true)

        // The next button the keyboard can press from `from` towards `delta`,
        // round the ends when `wrap` is set, or `from` itself when there is
        // none that way.
        function step(from, delta, wrap) {
            const count = controls.order.length;
            let at = from;
            for (let tried = 0; tried < count; ++tried) {
                at += delta;
                if (wrap)
                    at = (at + count) % count;
                else if (at < 0 || at >= count)
                    return from;
                if (controls.pressable(at))
                    return at;
            }
            return from;
        }

        Keys.onPressed: function (event) {
            switch (event.key) {
            case Qt.Key_Up:
                controls.cursor = controls.step(controls.cursor, -1, false);
                break;
            case Qt.Key_Down:
                controls.cursor = controls.step(controls.cursor, 1, false);
                break;
            case Qt.Key_Tab:
                controls.cursor = controls.step(controls.cursor, 1, true);
                break;
            case Qt.Key_Backtab:
                controls.cursor = controls.step(controls.cursor, -1, true);
                break;
            case Qt.Key_Return:
            case Qt.Key_Enter:
            case Qt.Key_Space:
                if (controls.pressable(controls.cursor))
                    controls.order[controls.cursor].clicked();
                break;
            case Qt.Key_Escape:
                root.focusPage();
                break;
            default:
                return;
            }
            event.accepted = true;
        }

        // The panel's own ground under the buttons (#863). They stand over the
        // dimmed page the Glance came from, which over a white page is a mid
        // grey and over a black one black, and a glyph in the theme's text
        // colour fades into one or the other. On the plate it keeps the
        // contrast the theme gives text on its own surfaces, whatever the page.
        Rectangle {
            objectName: "glanceButtonsPlate"
            anchors.fill: parent
            radius: 3
            color: plateDither.fill
            border.width: DevicePixels.snap(1, root.pixelRatio)
            border.color: root.colors.separator

            GroundDither {
                id: plateDither
                ground: root.colors.overlay
            }
        }

        // A click on a disabled button, or between two, is not a click
        // outside the Glance: the column stands over the dim, which would
        // take it and close the Glance.
        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: buttons
            padding: Style.spacing.sm
            spacing: Style.spacing.sm

            GlanceButton {
                id: closeButton
                objectName: "closeGlanceButton"
                icon: "close"
                accessibleName: qsTr("Close Glance")
                onClicked: root.closed()
            }

            GlanceButton {
                id: openAsTabButton
                objectName: "openGlanceAsTabButton"
                visible: root.openAsTabAllowed
                icon: "open_in_new"
                accessibleName: qsTr("Open as tab")
                onClicked: root.openAsTabRequested()
            }

            GlanceButton {
                id: backButton
                objectName: "glanceBackButton"
                icon: "arrow_back"
                accessibleName: qsTr("Back")
                enabled: root.engine ? root.engine.canGoBack : false
                onClicked: root.backRequested()
            }

            GlanceButton {
                id: forwardButton
                objectName: "glanceForwardButton"
                icon: "arrow_forward"
                accessibleName: qsTr("Forward")
                enabled: root.engine ? root.engine.canGoForward : false
                onClicked: root.forwardRequested()
            }

            GlanceButton {
                id: reloadButton
                objectName: "glanceReloadButton"
                icon: "refresh"
                accessibleName: qsTr("Reload")
                onClicked: root.reloadRequested()
            }
        }

        // The Sidebar cursor, on the button holding the keyboard: the accent
        // at 7% with an accent border, as on a tab row.
        Omarchy.BorderSurface {
            objectName: "glanceButtonsCursor"
            readonly property Item button: controls.order[controls.cursor] || null
            visible: controls.activeFocus && controls.pressable(controls.cursor)
            x: button ? button.x : 0
            y: button ? button.y : 0
            width: button ? button.width : 0
            height: button ? button.height : 0
            radius: 2
            color: Qt.alpha(root.colors.accent, 0.07)
            borderSpec: Border.flat(root.colors.accent, 1)
        }

        // The labels belong to the column rather than to their buttons: a
        // disabled button is drawn at 35%, and a label that inherited that
        // would let the page show through it. Each stands beside its button,
        // away from the panel, since the next button is below it.
        GlanceKeyLabel {
            objectName: "keyLabel-closeGlanceButton"
            button: closeButton
            command: "close-tab"
        }

        GlanceKeyLabel {
            objectName: "keyLabel-openGlanceAsTabButton"
            button: openAsTabButton
            command: "glance-to-tab"
        }

        GlanceKeyLabel {
            objectName: "keyLabel-glanceBackButton"
            button: backButton
            command: "back"
        }

        GlanceKeyLabel {
            objectName: "keyLabel-glanceForwardButton"
            button: forwardButton
            command: "forward"
        }

        GlanceKeyLabel {
            objectName: "keyLabel-glanceReloadButton"
            button: reloadButton
            command: "reload"
        }
    }

    component GlanceButton: ChromeButton {
        width: 30
        height: 30
        // The column walks its cursor over the buttons, so none is a stop the
        // kit marks with its own focus ring.
        focusable: false
        // The window's own text, as the panel's ground is the window's own
        // overlay: a Private window draws both in its own palette.
        foreground: root.colors.text
        fontFamily: root.iconFontFamily
        accent: root.colors.accent
    }

    component GlanceKeyLabel: KeyLabel {
        required property Item button
        required property string command

        // Beside its button, away from the panel, or on the panel's side over
        // the edge of the page where that would run past the window, as the
        // whole chord on Open as tab does in a narrow one (#863).
        readonly property real beside: button.x + button.width + Style.spacing.sm
        readonly property bool fits: controls.x + beside + width <= root.width

        iconFontFamily: root.iconFontFamily
        x: DevicePixels.snap(fits ? beside : -Style.spacing.sm - width, root.pixelRatio)
        y: DevicePixels.snap(button.y + (button.height - height) / 2, root.pixelRatio)
        keys: root.keyLabelsShown && root.keyMap ? root.keyMap.labelFor(command) : ""
        shown: root.keyLabelsShown && button.visible
        colors: root.colors
    }
}
