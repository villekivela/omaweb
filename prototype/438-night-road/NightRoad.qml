// PROTOTYPE for #438 — throwaway, not production code. Run ./run.sh.
//
// Question: how does the Start page's night road look and move?
// Three compositions of the same road, picked with F1 F2 F3 or the buttons at the top:
//   A Horizon   the Omnibar sits on the horizon, backlit by the sun
//   B Road      a high horizon; the Omnibar floats over the road, lane marks run under it
//   C Gantry    the Omnibar is the overhead sign on a gantry over the road
//
// Keys (the HUD lists them, Ctrl+H hides it):
//   F1 F2 F3    composition          F5 F6         theme
//   Ctrl+P      Private window       Ctrl+R        road off (sidebar fill only)
//   Ctrl+T      Start page over page Escape        back to the page
//   Return      commit               Ctrl+D        first-paint delay 0.6 / 1.6 / 4 s
//   Ctrl+E      next commit fails    Ctrl+U        simulate an unfocused window
//   Ctrl+B      back to a Space at rest
import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.Effects
import QtQuick.Window
import "themes.js" as Themes

Window {
    id: win
    width: 1440
    height: 900
    visible: true
    title: "PROTOTYPE #438 — night road"
    color: "black"

    // ---- state -----------------------------------------------------------
    property int variant: 0
    readonly property var variantNames: ["A Horizon", "B Road ahead", "C Gantry"]
    property int themeIndex: 0
    property bool privateWindow: false
    property bool roadOn: true
    property bool simulatedUnfocus: false
    property bool hud: true
    property bool nextFails: false
    property var delays: [600, 1600, 4000]
    property int delayIndex: 1
    // rest | page | over-page | driving | loading | error
    property string mode: "rest"
    property string pageAddress: ""
    property int frames: 0
    property int roadFrames: 0

    readonly property bool startShown: mode === "rest" || mode === "over-page" || mode === "driving"
    readonly property bool moving: startShown && roadOn && win.active && !simulatedUnfocus
                                   && win.visibility !== Window.Minimized
                                   && win.visibility !== Window.Hidden

    // ---- palette ---------------------------------------------------------
    readonly property var t: Themes.all[themeIndex % Themes.all.length]
    function c(name, fallback) {
        return t[name] !== undefined ? t[name] : fallback;
    }
    function mix(a, b, k) {
        a = Qt.color(a);
        b = Qt.color(b);
        return Qt.rgba(a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k, 1);
    }
    function alpha(a, k) {
        a = Qt.color(a);
        return Qt.rgba(a.r, a.g, a.b, k);
    }
    readonly property bool light: c("mode", "dark") === "light"
                                  || Qt.color(c("background", "#000")).hslLightness > 0.6
    // The chrome keeps the theme as it is. The road is always night: a light
    // theme's night is drawn from its dark foreground, unless Ctrl+N asks for
    // the theme's own light ground.
    property bool lightNight: true
    // 0 clean, 1 screenprint, 2 vector display
    property int finish: 1
    readonly property var finishNames: ["clean", "screenprint", "vector"]
    readonly property bool screenprint: finish === 1
    readonly property bool vector: finish === 2
    readonly property color uiBg: c("background", "#1a1b26")
    readonly property color uiFg: c("foreground", "#c0caf5")
    readonly property color uiDeep: c("darker_background", mix(uiBg, "black", light ? 0.08 : 0.45))
    readonly property bool inverted: light && lightNight
    readonly property color bg: inverted ? mix(uiFg, "black", 0.45) : uiBg
    readonly property color fg: inverted ? uiBg : uiFg
    readonly property color accent: c("accent", c("color4", "#7aa2f7"))
    readonly property color warm: c("yellow", c("color3", "#e0af68"))
    readonly property color hot: c("red", c("color1", "#f7768e"))
    readonly property color deep: inverted ? mix(uiFg, "black", 0.72) : uiDeep
    readonly property color sidebarFill: mix(uiBg, uiDeep, 0.5)
    // The sky is the theme's darkest ground, lifted toward the accent at the horizon.
    readonly property color skyTop: privateWindow ? mix(deep, "black", 0.5) : deep
    readonly property color skyLow: privateWindow ? mix(bg, "black", 0.35) : mix(bg, accent, light && !inverted ? 0.18 : 0.28)
    readonly property color groundNear: mix(deep, "black", light && !inverted ? 0.05 : 0.35)
    readonly property color glow: privateWindow ? mix(fg, bg, 0.7) : accent
    readonly property color sunTop: mix(warm, "white", 0.15)
    readonly property color sunLow: mix(accent, hot, 0.35)

    // ---- the road's motion -----------------------------------------------
    // One clock drives every moving mark; it stops with the window.
    property real travel: 0
    property real speed: 1
    property real targetSpeed: 1
    FrameAnimation {
        running: win.moving
        onTriggered: {
            win.roadFrames++;
            win.speed += (win.targetSpeed - win.speed) * Math.min(1, frameTime * 2.2);
            win.travel += frameTime * 1.6 * win.speed;
        }
    }
    Connections {
        target: win
        function onFrameSwapped() {
            win.frames++;
        }
    }

    // `./run.sh -- shot=out.png variant=1 theme=2 private=1 mode=driving` grabs one frame and quits.
    property var args: {
        const out = {};
        for (const a of Qt.application.arguments) {
            const i = a.indexOf("=");
            if (i > 0)
                out[a.slice(0, i)] = a.slice(i + 1);
        }
        return out;
    }
    Component.onCompleted: {
        if (args.variant !== undefined) variant = +args.variant;
        if (args.theme !== undefined) themeIndex = +args.theme;
        if (args.private === "1") privateWindow = true;
        if (args.hud === "0") hud = false;
        if (args.finish !== undefined) finish = +args.finish;
        if (args.mode === "driving") { mode = "over-page"; delayIndex = 2; commit(); }
        if (args.mode === "over-page") mode = "over-page";
    }
    Timer {
        running: win.args.shot !== undefined
        interval: win.args.after !== undefined ? +win.args.after : 2500
        onTriggered: win.contentItem.grabToImage(r => { r.saveToFile(win.args.shot); Qt.quit(); })
    }

    Timer {
        id: firstPaint
        onTriggered: win.mode = "page"
    }
    Timer {
        id: handOver
        interval: 2000
        onTriggered: if (win.mode === "driving")
            win.mode = "loading"
    }

    function commit() {
        if (mode !== "rest" && mode !== "over-page")
            return;
        pageAddress = field.text.length ? field.text : "news.ycombinator.com";
        field.text = pageAddress;
        if (nextFails) {
            nextFails = false;
            mode = "error";
            speed = 0;
            return;
        }
        mode = "driving";
        targetSpeed = 9;
        firstPaint.interval = delays[delayIndex];
        firstPaint.restart();
        handOver.restart();
    }
    onModeChanged: {
        if (mode !== "driving") {
            targetSpeed = 1;
            if (mode !== "rest" && mode !== "over-page")
                speed = 1;
        }
        if (startShown && mode !== "driving") {
            field.text = "";
            field.forceActiveFocus();
        }
    }

    // ---- keys ------------------------------------------------------------
    // The focused field takes the arrow keys, so layouts and themes sit on F keys.
    Shortcut { sequence: "F1"; onActivated: win.variant = 0 }
    Shortcut { sequence: "F2"; onActivated: win.variant = 1 }
    Shortcut { sequence: "F3"; onActivated: win.variant = 2 }
    Shortcut { sequence: "F10"; onActivated: win.finish = (win.finish + 1) % 3 }
    Shortcut { sequence: "F5"; onActivated: win.themeIndex = (win.themeIndex + Themes.all.length - 1) % Themes.all.length }
    Shortcut { sequence: "F6"; onActivated: win.themeIndex = (win.themeIndex + 1) % Themes.all.length }
    Shortcut { sequence: "Ctrl+P"; onActivated: win.privateWindow = !win.privateWindow }
    Shortcut { sequence: "Ctrl+R"; onActivated: win.roadOn = !win.roadOn }
    Shortcut { sequence: "Ctrl+U"; onActivated: win.simulatedUnfocus = !win.simulatedUnfocus }
    Shortcut { sequence: "Ctrl+H"; onActivated: win.hud = !win.hud }
    Shortcut { sequence: "Ctrl+E"; onActivated: win.nextFails = !win.nextFails }
    Shortcut { sequence: "Ctrl+D"; onActivated: win.delayIndex = (win.delayIndex + 1) % 3 }
    Shortcut { sequence: "Ctrl+N"; onActivated: win.lightNight = !win.lightNight }
    Shortcut { sequence: "Ctrl+B"; onActivated: win.mode = "rest" }
    Shortcut {
        sequence: "Ctrl+T"
        onActivated: if (win.mode === "page" || win.mode === "error" || win.mode === "loading")
            win.mode = "over-page"
    }
    Shortcut {
        sequence: "Ctrl+L"
        onActivated: if (win.startShown)
            field.forceActiveFocus()
        else
            win.mode = "over-page"
    }
    Shortcut {
        sequence: "Escape"
        onActivated: if (win.mode === "over-page")
            win.mode = "page"
    }
    Shortcut {
        sequence: "Ctrl+/"
        onActivated: sheet.visible = !sheet.visible
    }

    // ---- chrome ----------------------------------------------------------
    Rectangle {
        id: sidebar
        width: 248
        height: parent.height
        color: win.sidebarFill
        Column {
            x: 16
            y: 18
            spacing: 6
            Text {
                text: win.privateWindow ? "Private" : "Work"
                color: win.accent
                font.family: "JetBrainsMono Nerd Font"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                bottomPadding: 10
            }
            Repeater {
                model: win.privateWindow ? [] : ["github.com/villekivela", "Hacker News", "Arch Wiki — Hyprland", "omarchy.org"]
                Rectangle {
                    required property string modelData
                    required property int index
                    width: 216
                    height: 30
                    radius: 3
                    color: index === 1 && win.mode === "page" ? win.alpha(win.uiFg, 0.08) : "transparent"
                    Text {
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.modelData
                        color: win.alpha(win.uiFg, 0.72)
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }

    Item {
        id: pageArea
        x: sidebar.width
        width: parent.width - sidebar.width
        height: parent.height
        clip: true

        // A stand-in web page.
        Rectangle {
            id: page
            anchors.fill: parent
            color: "#f6f6ef"
            visible: win.mode !== "rest"
            Rectangle {
                width: parent.width
                height: 32
                color: "#ff6600"
                Text {
                    x: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Y  Hacker News   new | past | comments | ask | show | jobs"
                    font.pixelSize: 13
                    font.bold: true
                }
            }
            Column {
                x: 24
                y: 52
                spacing: 16
                Repeater {
                    model: 18
                    Column {
                        required property int index
                        spacing: 4
                        Rectangle { width: 380 + (index * 97) % 360; height: 11; radius: 2; color: "#222" }
                        Rectangle { width: 180 + (index * 53) % 140; height: 8; radius: 2; color: "#999" }
                    }
                }
            }
            Rectangle {
                anchors.fill: parent
                visible: win.mode === "loading"
                color: "white"
            }
            // The page loading indicator the road hands over to after two seconds.
            Rectangle {
                id: loadingBar
                visible: win.mode === "loading"
                height: 2
                color: win.accent
                width: parent.width * 0.35
                SequentialAnimation on x {
                    running: loadingBar.visible
                    loops: Animation.Infinite
                    NumberAnimation { from: -pageArea.width * 0.35; to: pageArea.width; duration: 1100 }
                }
            }
            Rectangle {
                anchors.fill: parent
                visible: win.mode === "error"
                color: win.bg
                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        text: "This site can't be reached"
                        color: win.fg
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 20
                    }
                    Text {
                        text: win.pageAddress + " refused to connect."
                        color: win.alpha(win.fg, 0.6)
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 13
                    }
                }
            }
        }

        // ---- the Start page ------------------------------------------------
        Item {
            id: start
            anchors.fill: parent
            opacity: win.startShown ? 1 : 0
            visible: opacity > 0
            Behavior on opacity {
                NumberAnimation { duration: win.startShown ? 140 : 260; easing.type: Easing.OutCubic }
            }

            Rectangle {
                anchors.fill: parent
                color: win.sidebarFill
                visible: !win.roadOn
            }

            Road {
                id: road
                anchors.fill: parent
                visible: win.roadOn
                horizon: [0.5, 0.36, 0.6][win.variant]
            }

            // Gantry for composition C: a sign bridge standing on the road's
            // shoulders at the depth where its span fits the Omnibar.
            Item {
                id: gantry
                visible: win.roadOn && win.variant === 2
                anchors.fill: parent
                readonly property real span: omni.width / 2 + 54
                readonly property real gz: road.halfW * 1.5 / span
                readonly property real groundY: road.horizonY - 1 + (road.railY - road.horizonY + 1) / gz + road.depth * 0.11 / gz
                readonly property real beamY: omni.y - 30
                readonly property color steel: win.mix(win.deep, win.fg, 0.16)
                Repeater {
                    model: 2
                    Rectangle {
                        required property int index
                        x: parent.width / 2 + (index === 0 ? -gantry.span - width : gantry.span)
                        y: gantry.beamY
                        width: Math.max(4, 16 / gantry.gz)
                        height: gantry.groundY - y
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: gantry.steel }
                            GradientStop { position: 1; color: win.mix(gantry.steel, "black", 0.5) }
                        }
                    }
                }
                // Two beams with a lattice between them.
                Repeater {
                    model: 2
                    Rectangle {
                        required property int index
                        x: parent.width / 2 - gantry.span
                        y: gantry.beamY + index * 12
                        width: gantry.span * 2
                        height: 3
                        color: gantry.steel
                    }
                }
                Repeater {
                    model: 26
                    Rectangle {
                        required property int index
                        x: parent.width / 2 - gantry.span + (index + 0.5) * (gantry.span * 2 / 26)
                        y: gantry.beamY + 1
                        width: 1.5
                        height: 15
                        rotation: index % 2 === 0 ? 38 : -38
                        antialiasing: true
                        color: win.alpha(gantry.steel, 0.9)
                    }
                }
                // Lamps under the beam light the sign's top edge.
                Repeater {
                    model: 3
                    Rectangle {
                        required property int index
                        x: parent.width / 2 + (index - 1) * omni.width * 0.34 - width / 2
                        y: gantry.beamY + 15
                        width: 18
                        height: 3
                        radius: 1.5
                        color: win.mix(win.fg, win.warm, 0.3)
                        visible: !win.privateWindow
                    }
                }
                Shape {
                    anchors.fill: parent
                    visible: !win.privateWindow
                    ShapePath {
                        strokeWidth: -1
                        fillGradient: LinearGradient {
                            x1: 0
                            y1: gantry.beamY + 16
                            x2: 0
                            y2: omni.y + 16
                            GradientStop { position: 0; color: win.alpha(win.mix(win.fg, win.warm, 0.3), 0.22) }
                            GradientStop { position: 1; color: "transparent" }
                        }
                        startX: gantry.width / 2 - omni.width * 0.55
                        startY: gantry.beamY + 16
                        PathLine { x: gantry.width / 2 + omni.width * 0.55; y: gantry.beamY + 16 }
                        PathLine { x: gantry.width / 2 + omni.width * 0.6; y: omni.y + 16 }
                        PathLine { x: gantry.width / 2 - omni.width * 0.6; y: omni.y + 16 }
                    }
                }
            }

            // ---- the Omnibar -----------------------------------------------
            Item {
                id: omni
                visible: win.args.omni !== "0"
                width: Math.min(640, parent.width * 0.62)
                height: 62
                x: (parent.width - width) / 2
                y: win.variant === 0 ? road.horizonY - height / 2 : parent.height * 0.5 - height / 2
                opacity: win.mode === "driving" ? 0.85 : 1
                Behavior on opacity { NumberAnimation { duration: 300 } }

                // Soft accent bloom under the field so it reads against any part of the road.
                RectangularShadow {
                    anchors.fill: panel
                    radius: 3
                    blur: 38
                    spread: 2
                    color: win.alpha(win.privateWindow ? "black" : win.accent, win.privateWindow ? 0.6 : 0.28)
                }
                Rectangle {
                    id: panel
                    anchors.fill: parent
                    radius: 3
                    color: win.alpha(win.mix(win.uiBg, win.uiDeep, 0.5), 0.9)
                    border.width: 1
                    border.color: win.privateWindow ? win.mix(win.uiFg, win.uiBg, 0.55) : win.accent

                    Shape {
                        id: mark
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: 40.4
                        height: 18.4
                        scale: 22 / 40.4
                        transformOrigin: Item.Left
                        preferredRendererType: Shape.CurveRenderer
                        layer.enabled: true
                        layer.effect: MultiEffect {
                            shadowEnabled: true
                            shadowColor: win.glow
                            shadowBlur: 0.5
                            shadowHorizontalOffset: 0
                            shadowVerticalOffset: 0
                        }
                        ShapePath {
                            fillColor: win.glow
                            strokeWidth: -1
                            PathSvg {
                                path: "m 9.187009,0 -9.187009,9.187005 9.187009,9.18763 h 3.69342 l 13.915739,-13.91637 4.72873,4.72874 -9.187009,9.18763 h 6.305601 l 9.18701,-9.18763 -9.187622,-9.187005 H 24.949459 l -13.91574,13.915735 -4.728742,-4.72873 9.18701,-9.187005 z"
                            }
                        }
                    }
                    TextField {
                        id: field
                        x: 52
                        width: parent.width - 52 - 44
                        anchors.verticalCenter: parent.verticalCenter
                        height: 40
                        background: null
                        padding: 0
                        focus: true
                        verticalAlignment: TextInput.AlignVCenter
                        color: win.uiFg
                        placeholderText: win.mode === "over-page" ? "address or search — opens in a new tab" : "address or search"
                        placeholderTextColor: win.alpha(win.uiFg, 0.45)
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 17
                        readOnly: win.mode === "driving"
                        onAccepted: win.commit()
                        cursorDelegate: Rectangle {
                            width: 9
                            height: 19
                            color: win.glow
                            visible: field.cursorVisible && win.mode !== "driving"
                            SequentialAnimation on opacity {
                                running: field.activeFocus && win.moving
                                loops: Animation.Infinite
                                PropertyAction { value: 0.9 }
                                PauseAnimation { duration: 550 }
                                PropertyAction { value: 0 }
                                PauseAnimation { duration: 450 }
                            }
                        }
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 18
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→"
                        color: win.glow
                        font.family: "JetBrainsMono Nerd Font"
                        font.pixelSize: 18
                    }
                }

                // The one line naming `?`.
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.bottom
                    anchors.topMargin: win.variant === 0 ? 22 : 16
                    visible: win.mode !== "driving"
                    text: "<b>?</b>  shortcuts"
                    textFormat: Text.StyledText
                    color: win.alpha(win.fg, 0.55)
                    style: Text.Raised
                    styleColor: win.alpha("black", 0.4)
                    font.family: "JetBrainsMono Nerd Font"
                    font.pixelSize: 12
                    font.letterSpacing: 1
                }
            }
        }

        // Stand-in for the shortcut sheet.
        Rectangle {
            id: sheet
            visible: false
            anchors.centerIn: parent
            width: 520
            height: 320
            radius: 3
            color: win.bg
            border.color: win.accent
            Text {
                anchors.centerIn: parent
                text: "shortcut sheet"
                color: win.fg
                font.family: "JetBrainsMono Nerd Font"
            }
        }
    }

    // ---- clickable switcher, top centre of the page area ------------------
    Flow {
        z: 10
        x: sidebar.width + 20
        width: pageArea.width - 40
        y: 12
        spacing: 6
        Repeater {
            model: win.variantNames.length
            PrototypeButton {
                required property int index
                label: "F" + (index + 1) + "  " + win.variantNames[index]
                active: win.variant === index
                onClicked: win.variant = index
            }
        }
        Item { width: 14; height: 1 }
        PrototypeButton { label: "◀ theme"; onClicked: win.themeIndex = (win.themeIndex + Themes.all.length - 1) % Themes.all.length }
        PrototypeButton { label: win.t.name; active: true }
        PrototypeButton { label: "theme ▶"; onClicked: win.themeIndex = (win.themeIndex + 1) % Themes.all.length }
        Item { width: 14; height: 1 }
        PrototypeButton { label: "Private"; active: win.privateWindow; onClicked: win.privateWindow = !win.privateWindow }
        Item { width: 14; height: 1 }
        Repeater {
            model: win.finishNames.length
            PrototypeButton {
                required property int index
                label: "F10 " + win.finishNames[index]
                active: win.finish === index
                onClicked: win.finish = index
            }
        }
    }

    component PrototypeButton: Rectangle {
        id: button
        property string label
        property bool active: false
        signal clicked
        width: buttonText.implicitWidth + 20
        height: 26
        radius: 13
        color: active ? "#ffcc00" : "#e0101010"
        border.color: "#ffcc00"
        Text {
            id: buttonText
            anchors.centerIn: parent
            text: button.label
            color: button.active ? "#101010" : "#ffcc00"
            font.family: "JetBrainsMono Nerd Font"
            font.pixelSize: 11
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                button.clicked();
                field.forceActiveFocus();
            }
        }
    }

    // ---- HUD -------------------------------------------------------------
    Rectangle {
        visible: win.hud
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        width: hudText.implicitWidth + 24
        height: hudText.implicitHeight + 18
        radius: 6
        color: "#e0101010"
        border.color: "#ffcc00"
        Text {
            id: hudText
            anchors.centerIn: parent
            color: "#ffcc00"
            font.family: "JetBrainsMono Nerd Font"
            font.pixelSize: 11
            text: "PROTOTYPE #438   layout " + win.variantNames[win.variant] + "   (F1 F2 F3)\n"
                  + "theme " + win.t.name + " (F5 F6)   private " + win.privateWindow + " (^P)   road " + win.roadOn + " (^R)\n"
                  + "mode " + win.mode + "   moving " + win.moving + "   speed " + win.speed.toFixed(1) + "\n"
                  + "road frames " + win.roadFrames + "   window frames " + win.frames + "\n"
                  + "first paint " + win.delays[win.delayIndex] + " ms (^D)   next fails " + win.nextFails + " (^E)   unfocus " + win.simulatedUnfocus + " (^U)\n"
                  + "^N light theme night " + win.lightNight + "   ^T start over page   Esc back   Return commit   ^B Space at rest   ^/ sheet   ^H hide"
        }
    }

    // =====================================================================
    // The road. Everything static is drawn once into cached layers; only the
    // lane marks, the grid and the guardrail posts move, and they are plain
    // rectangles.
    // =====================================================================
    component Road: Item {
        id: roadRoot
        property real horizon: 0.5
        readonly property real horizonY: Math.round(height * horizon)
        readonly property real vpX: width / 2
        // Half the road's width where it meets the bottom edge.
        readonly property real halfW: width * 0.42
        readonly property real depth: height - horizonY
        readonly property real railY: height - depth * 0.3
        // World depth z >= 1 maps to the screen; z = 1 is the bottom edge.
        function py(z) {
            return horizonY + depth / z;
        }
        function px(u, z) {
            return vpX + u * halfW / z;
        }

        // The vector display's phosphor: every lit line blooms. Applied to the
        // cached static layers, so it costs one blur when the scene changes.
        Component {
            id: bloom
            MultiEffect {
                shadowEnabled: true
                shadowColor: win.glow
                shadowBlur: 0.9
                shadowOpacity: 0.9
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 0
                blurMax: 48
                autoPaddingEnabled: false
            }
        }

        // ---- sky, static, cached
        Item {
            id: sky
            anchors.fill: parent
            layer.enabled: true
            layer.effect: win.vector ? bloom : null
            Rectangle {
                visible: win.finish === 0
                width: parent.width
                height: roadRoot.horizonY + 1
                gradient: Gradient {
                    GradientStop { position: 0; color: win.skyTop }
                    GradientStop { position: 0.55; color: win.mix(win.skyTop, win.skyLow, 0.35) }
                    GradientStop { position: 1; color: win.skyLow }
                }
            }
            // Screenprint: the sky in five flat inks.
            Column {
                visible: win.screenprint
                Repeater {
                    model: 5
                    Rectangle {
                        required property int index
                        readonly property var heights: [0.4, 0.22, 0.16, 0.12, 0.1]
                        width: roadRoot.width
                        height: Math.ceil((roadRoot.horizonY + 1) * heights[index])
                        color: win.mix(win.skyTop, win.skyLow, Math.pow(index / 4, 1.3))
                    }
                }
            }
            Rectangle {
                visible: win.vector
                width: parent.width
                height: roadRoot.horizonY + 1
                color: win.mix(win.skyTop, "black", 0.35)
            }
            // Stars thin out toward the horizon, where the glow washes them out.
            Repeater {
                model: win.privateWindow ? 0 : 260
                Rectangle {
                    required property int index
                    function fract(v) {
                        return v - Math.floor(v);
                    }
                    readonly property real r1: fract(Math.sin(index * 12.9898) * 43758.5453)
                    readonly property real r2: fract(Math.sin(index * 78.233) * 12543.123)
                    readonly property real r3: fract(Math.sin(index * 39.425) * 9321.77)
                    readonly property real h: Math.pow(r2, 1.6)
                    x: r1 * roadRoot.width
                    y: h * roadRoot.horizonY * 0.9
                    width: r3 > 0.96 ? 2.5 : (r3 > 0.8 ? 1.6 : 1)
                    height: width
                    radius: width / 2
                    color: r3 > 0.85 ? win.mix(win.fg, win.accent, 0.45) : win.mix(win.fg, "white", 0.3)
                    opacity: (0.2 + 0.8 * r3) * (1 - h * 0.85)
                }
            }
            // Halftone dots rising off the horizon, the website art's print texture.
            Canvas {
                id: halftone
                anchors.fill: parent
                visible: win.screenprint && !win.privateWindow
                renderStrategy: Canvas.Immediate
                readonly property string key: width + "x" + height + win.accent + win.sunLow + roadRoot.horizonY
                onKeyChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    const band = roadRoot.horizonY * 0.75;
                    const top = roadRoot.horizonY - band;
                    const step = 8;
                    const tint = win.mix(win.sunLow, win.accent, 0.5);
                    ctx.fillStyle = Qt.rgba(tint.r, tint.g, tint.b, 0.75);
                    let row = 0;
                    for (let y = top; y < roadRoot.horizonY; y += step, ++row) {
                        const t = (y - top) / band;
                        for (let x = (row % 2) * step / 2; x < width; x += step) {
                            const dx = (x - roadRoot.vpX) / (width * 0.42);
                            const r = (0.1 + 2.9 * Math.pow(t, 2)) * (0.3 + 0.7 * Math.exp(-dx * dx));
                            if (r < 0.3)
                                continue;
                            ctx.beginPath();
                            ctx.arc(x, y, r, 0, Math.PI * 2);
                            ctx.fill();
                        }
                    }
                }
            }
            // The sun's glow: a wide soft bloom and a tight hot core.
            Shape {
                anchors.fill: parent
                visible: !win.privateWindow
                ShapePath {
                    strokeWidth: -1
                    fillGradient: RadialGradient {
                        centerX: roadRoot.vpX
                        centerY: roadRoot.horizonY
                        centerRadius: roadRoot.width * 0.55
                        focalX: centerX
                        focalY: centerY
                        GradientStop { position: 0; color: win.alpha(win.sunLow, 0.6) }
                        GradientStop { position: 0.18; color: win.alpha(win.sunLow, 0.28) }
                        GradientStop { position: 0.45; color: win.alpha(win.accent, 0.1) }
                        GradientStop { position: 1; color: win.alpha(win.accent, 0) }
                    }
                    startX: 0
                    startY: 0
                    PathLine { x: roadRoot.width; y: 0 }
                    PathLine { x: roadRoot.width; y: roadRoot.horizonY }
                    PathLine { x: 0; y: roadRoot.horizonY }
                }
            }
            // The sun: a disc cut by bands that widen toward the horizon.
            Item {
                id: sun
                visible: !win.privateWindow && !win.vector
                readonly property real r: roadRoot.height * [0.15, 0.11, 0.12][win.variant]
                x: roadRoot.vpX - r
                y: roadRoot.horizonY - r
                width: r * 2
                height: r
                Rectangle {
                    id: disc
                    width: sun.r * 2
                    height: sun.r * 2
                    radius: sun.r
                    visible: false
                    layer.enabled: true
                    gradient: Gradient {
                        GradientStop { position: 0; color: win.sunTop }
                        GradientStop { position: 0.5; color: win.sunLow }
                    }
                }
                Item {
                    id: bands
                    width: sun.r * 2
                    height: sun.r * 2
                    visible: false
                    layer.enabled: true
                    Rectangle {
                        width: parent.width
                        height: sun.r * 0.42
                        color: "white"
                    }
                    Repeater {
                        model: 7
                        Rectangle {
                            required property int index
                            readonly property real band: sun.r * 0.58 / 7
                            readonly property real cut: band * (0.12 + index * 0.1)
                            y: sun.r * 0.42 + index * band
                            width: parent.width
                            height: band - cut
                            color: "white"
                        }
                    }
                }
                MultiEffect {
                    width: sun.r * 2
                    height: sun.r * 2
                    source: disc
                    maskEnabled: true
                    maskSource: bands
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 0.2
                }
            }
            // Screenprint: the sun's red plate printed a little off register.
            Item {
                visible: win.screenprint && !win.privateWindow
                x: roadRoot.vpX - sun.r + 5
                y: roadRoot.horizonY - sun.r - 3
                width: sun.r * 2
                height: sun.r
                clip: true
                Rectangle {
                    width: sun.r * 2
                    height: sun.r * 2
                    radius: sun.r
                    color: "transparent"
                    border.width: 2
                    border.color: win.alpha(win.hot, 0.6)
                }
            }
            // Vector: the sun as an outline and its bands as scan lines.
            Shape {
                anchors.fill: parent
                visible: win.vector && !win.privateWindow
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: win.sunTop
                    strokeWidth: 2
                    PathAngleArc {
                        centerX: roadRoot.vpX
                        centerY: roadRoot.horizonY
                        radiusX: sun.r
                        radiusY: sun.r
                        startAngle: 180
                        sweepAngle: 180
                    }
                }
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: win.sunLow
                    strokeWidth: 1.5
                    PathMultiline {
                        paths: {
                            const out = [];
                            for (let k = 1; k <= 9; ++k) {
                                const dy = sun.r * Math.pow(k / 10, 0.8);
                                const half = Math.sqrt(Math.max(0, sun.r * sun.r - dy * dy));
                                out.push([Qt.point(roadRoot.vpX - half, roadRoot.horizonY - dy), Qt.point(roadRoot.vpX + half, roadRoot.horizonY - dy)]);
                            }
                            return out;
                        }
                    }
                }
            }
            Ridge {
                anchors.fill: parent
                seed: 1.3
                base: roadRoot.horizonY
                amplitude: 0.3
                notch: 0.16
                fillColor: win.mix(win.skyLow, win.skyTop, 0.3)
                rim: win.alpha(win.glow, win.privateWindow ? 0.18 : 0.35)
            }
            Ridge {
                anchors.fill: parent
                seed: 4.2
                base: roadRoot.horizonY
                amplitude: 0.2
                notch: 0.22
                fillColor: win.mix(win.skyLow, win.skyTop, 0.65)
                rim: win.alpha(win.glow, win.privateWindow ? 0.25 : 0.55)
            }
            Ridge {
                anchors.fill: parent
                seed: 8.9
                base: roadRoot.horizonY
                amplitude: 0.11
                notch: 0.3
                fillColor: win.mix(win.skyTop, win.groundNear, 0.7)
                rim: win.alpha(win.glow, win.privateWindow ? 0.35 : 0.85)
            }
        }

        // While driving, the horizon brightens with speed. Not cached: it is
        // one gradient whose opacity changes.
        Shape {
            anchors.fill: parent
            visible: !win.privateWindow && opacity > 0
            opacity: Math.min(1, (win.speed - 1) / 8)
            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: roadRoot.vpX
                    centerY: roadRoot.horizonY
                    centerRadius: roadRoot.width * 0.35
                    focalX: centerX
                    focalY: centerY
                    GradientStop { position: 0; color: win.alpha(win.sunTop, 0.45) }
                    GradientStop { position: 0.3; color: win.alpha(win.sunLow, 0.15) }
                    GradientStop { position: 1; color: win.alpha(win.sunLow, 0) }
                }
                startX: 0
                startY: 0
                PathLine { x: roadRoot.width; y: 0 }
                PathLine { x: roadRoot.width; y: roadRoot.height }
                PathLine { x: 0; y: roadRoot.height }
            }
        }

        // ---- ground and road, static, cached
        Item {
            anchors.fill: parent
            layer.enabled: true
            layer.effect: win.vector ? bloom : null
            Rectangle {
                y: roadRoot.horizonY
                width: parent.width
                height: roadRoot.depth
                visible: win.vector
                color: win.mix(win.groundNear, "black", 0.5)
            }
            Rectangle {
                y: roadRoot.horizonY
                width: parent.width
                height: roadRoot.depth
                visible: !win.vector
                gradient: Gradient {
                    GradientStop { position: 0; color: win.mix(win.skyLow, win.groundNear, 0.4) }
                    GradientStop { position: 0.1; color: win.groundNear }
                    GradientStop { position: 1; color: win.mix(win.groundNear, "black", 0.35) }
                }
            }
            // Screenprint: the ground engraved, lines thickening toward the viewer.
            Canvas {
                anchors.fill: parent
                visible: win.screenprint
                renderStrategy: Canvas.Immediate
                readonly property string key: width + "x" + height + roadRoot.horizonY
                onKeyChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.reset();
                    ctx.fillStyle = "rgba(0,0,0,0.55)";
                    for (let y = roadRoot.horizonY + 2; y < height; y += 4) {
                        const t = (y - roadRoot.horizonY) / roadRoot.depth;
                        ctx.fillRect(0, y, width, 0.4 + 2.2 * t);
                    }
                }
            }
            // Vector: a ground grid converging on the vanishing point.
            Shape {
                anchors.fill: parent
                visible: win.vector
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: win.alpha(win.glow, win.privateWindow ? 0.12 : 0.35)
                    strokeWidth: 1
                    PathMultiline {
                        paths: {
                            const out = [];
                            for (let u = -9; u <= 9; ++u)
                                out.push([Qt.point(roadRoot.vpX, roadRoot.horizonY), Qt.point(roadRoot.px(u * 0.5, 1), roadRoot.height)]);
                            return out;
                        }
                    }
                }
            }
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                // Asphalt, catching the horizon's light where it runs out.
                ShapePath {
                    strokeWidth: -1
                    fillGradient: LinearGradient {
                        x1: 0
                        y1: roadRoot.horizonY
                        x2: 0
                        y2: roadRoot.height
                        GradientStop { position: 0; color: win.vector ? win.mix(win.groundNear, "black", 0.6) : win.mix(win.sunLow, win.bg, win.privateWindow ? 0.9 : 0.35) }
                        GradientStop { position: 0.12; color: win.vector ? win.mix(win.groundNear, "black", 0.6) : win.mix(win.bg, win.groundNear, 0.2) }
                        GradientStop { position: 1; color: win.vector ? win.mix(win.groundNear, "black", 0.6) : win.mix(win.groundNear, "black", 0.1) }
                    }
                    startX: roadRoot.vpX - 1
                    startY: roadRoot.horizonY
                    PathLine { x: roadRoot.vpX + 1; y: roadRoot.horizonY }
                    PathLine { x: roadRoot.px(1.08, 1); y: roadRoot.height }
                    PathLine { x: roadRoot.px(-1.08, 1); y: roadRoot.height }
                }
            }
            // The sun on wet asphalt: a narrow streak from the vanishing point.
            Shape {
                anchors.fill: parent
                visible: !win.privateWindow && !win.vector
                ShapePath {
                    strokeWidth: -1
                    fillGradient: LinearGradient {
                        x1: 0
                        y1: roadRoot.horizonY
                        x2: 0
                        y2: roadRoot.height
                        GradientStop { position: 0; color: win.alpha(win.sunTop, 0.4) }
                        GradientStop { position: 0.35; color: win.alpha(win.sunLow, 0.12) }
                        GradientStop { position: 1; color: win.alpha(win.sunLow, 0) }
                    }
                    startX: roadRoot.vpX - 2
                    startY: roadRoot.horizonY
                    PathLine { x: roadRoot.vpX + 2; y: roadRoot.horizonY }
                    PathLine { x: roadRoot.px(0.3, 1); y: roadRoot.height }
                    PathLine { x: roadRoot.px(-0.3, 1); y: roadRoot.height }
                }
            }
            // Edge lines and guard rails from the vanishing point, glowing.
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: win.glow
                    shadowBlur: 0.8
                    shadowOpacity: win.privateWindow ? 0.25 : 1
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                }
                ShapePath {
                    strokeColor: win.alpha(win.glow, win.privateWindow ? 0.3 : 1)
                    strokeWidth: 2.5
                    fillColor: "transparent"
                    startX: roadRoot.vpX
                    startY: roadRoot.horizonY
                    PathLine { x: roadRoot.px(-1, 1); y: roadRoot.height }
                    PathMove { x: roadRoot.vpX; y: roadRoot.horizonY }
                    PathLine { x: roadRoot.px(1, 1); y: roadRoot.height }
                }
                ShapePath {
                    strokeColor: win.screenprint ? win.alpha(win.mix(win.glow, win.hot, 0.5), win.privateWindow ? 0.1 : 0.5) : "transparent"
                    strokeWidth: 1
                    fillColor: "transparent"
                    startX: roadRoot.vpX + 1
                    startY: roadRoot.horizonY
                    PathLine { x: roadRoot.px(-1, 1) + 5; y: roadRoot.height }
                    PathMove { x: roadRoot.vpX + 1; y: roadRoot.horizonY }
                    PathLine { x: roadRoot.px(1, 1) + 5; y: roadRoot.height }
                }
                ShapePath {
                    strokeColor: win.alpha(win.mix(win.fg, win.glow, 0.4), win.privateWindow ? 0.15 : 0.55)
                    strokeWidth: 2
                    fillColor: "transparent"
                    startX: roadRoot.vpX
                    startY: roadRoot.horizonY - 1
                    PathLine { x: roadRoot.px(-1.5, 1); y: roadRoot.railY }
                    PathMove { x: roadRoot.vpX; y: roadRoot.horizonY - 1 }
                    PathLine { x: roadRoot.px(1.5, 1); y: roadRoot.railY }
                }
            }
            // Horizon seam: hot in the middle, gone at the edges.
            Rectangle {
                y: roadRoot.horizonY - 1
                width: parent.width
                height: 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: win.alpha(win.glow, 0) }
                    GradientStop { position: 0.5; color: win.alpha(win.privateWindow ? win.glow : win.sunTop, win.privateWindow ? 0.5 : 1) }
                    GradientStop { position: 1; color: win.alpha(win.glow, 0) }
                }
            }
        }

        // ---- moving: grid, lane marks, posts
        Item {
            id: marks
            anchors.fill: parent
            readonly property real period: 30
            function wrap(v, span) {
                return ((v % span) + span) % span;
            }
            // A faint grid line across the ground every few metres; the one
            // cue of speed that survives the Private window's lights being off.
            Repeater {
                model: 12
                Rectangle {
                    required property int index
                    readonly property real dz: 1 + marks.wrap(index * 2.5 - win.travel, 30)
                    y: roadRoot.py(dz)
                    width: roadRoot.width
                    height: 1
                    color: win.glow
                    opacity: (win.vector ? 0.4 : win.privateWindow ? 0.05 : 0.08) * Math.min(1, (dz - 1)) * (1 - dz / 32)
                }
            }
            Repeater {
                model: win.privateWindow ? 0 : 26
                Rectangle {
                    required property int index
                    readonly property real dz: 1 + marks.wrap(index * 1.15 - win.travel, 26 * 1.15)
                    readonly property real len: 0.42 + Math.min(0.7, (win.speed - 1) * 0.1)
                    readonly property real y0: roadRoot.py(dz)
                    readonly property real y1: roadRoot.py(dz + len)
                    x: roadRoot.vpX - width / 2
                    y: y1
                    width: Math.max(1, 12 / dz)
                    height: Math.max(1, y0 - y1)
                    radius: width / 3
                    antialiasing: true
                    color: win.mix(win.warm, "white", 0.35)
                    opacity: Math.min(1, 1.3 - dz / 30) * Math.min(1, (dz - 1) * 2.5)
                }
            }
            Repeater {
                model: win.privateWindow ? 0 : 24
                Rectangle {
                    required property int index
                    readonly property real side: index % 2 === 0 ? -1.5 : 1.5
                    readonly property int k: Math.floor(index / 2)
                    readonly property real span: 12 * 1.6
                    readonly property real dz: 1.2 + marks.wrap(k * 1.6 - win.travel, span)
                    readonly property real railAt: roadRoot.horizonY - 1 + (roadRoot.railY - roadRoot.horizonY + 1) / dz
                    x: roadRoot.px(side, dz) - width / 2
                    y: railAt
                    width: Math.max(1, 6 / dz)
                    height: Math.max(1, roadRoot.depth * 0.11 / dz)
                    color: win.mix(win.groundNear, win.fg, 0.45)
                    opacity: Math.min(1, 1.2 - dz / span) * Math.min(1, (dz - 1.2) * 2)
                }
            }
        }

        // Vector: scan lines, drawn once per size.
        Canvas {
            anchors.fill: parent
            visible: win.vector
            renderStrategy: Canvas.Immediate
            readonly property string key: width + "x" + height
            onKeyChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.fillStyle = "rgba(0,0,0,0.28)";
                for (let y = 0; y < height; y += 3)
                    ctx.fillRect(0, y, width, 1);
            }
        }

        // Vignette, darker at the corners like a windscreen's edge.
        Shape {
            anchors.fill: parent
            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: roadRoot.vpX
                    centerY: roadRoot.horizonY
                    centerRadius: Math.max(roadRoot.width, roadRoot.height) * 0.85
                    focalX: centerX
                    focalY: centerY
                    GradientStop { position: 0.4; color: "transparent" }
                    GradientStop { position: 1; color: win.alpha("black", win.light && !win.inverted ? 0.2 : 0.6) }
                }
                startX: 0
                startY: 0
                PathLine { x: roadRoot.width; y: 0 }
                PathLine { x: roadRoot.width; y: roadRoot.height }
                PathLine { x: 0; y: roadRoot.height }
            }
        }
    }

    // A mountain ridge: sharp peaks from folded sines, falling to the horizon
    // in a notch where the road runs out.
    component Ridge: Shape {
        id: ridge
        property real seed: 1
        property real amplitude: 0.2
        property real notch: 0.2
        property real base: 0
        property color fillColor
        property color rim
        preferredRendererType: Shape.CurveRenderer
        readonly property var points: {
            const pts = [];
            const n = 220;
            for (let i = 0; i <= n; ++i) {
                const u = i / n;
                const d = Math.min(1, Math.abs(u - 0.5) / notch);
                const valley = d * d * (3 - 2 * d);
                let h = 0;
                h += 0.55 * Math.pow(1 - Math.abs(Math.sin(u * 6.3 + seed)), 1.6);
                h += 0.3 * Math.pow(1 - Math.abs(Math.sin(u * 15.7 + seed * 2.3)), 2);
                h += 0.15 * Math.pow(1 - Math.abs(Math.sin(u * 37.1 + seed * 5.1)), 2);
                h = 0.25 + h * 0.75;
                pts.push(Qt.point(u * width, base - h * amplitude * base * valley));
            }
            return pts;
        }
        // Ink hatching down the faces: short strokes from the crest, longer
        // where the mountain is taller.
        readonly property var hatches: {
            const out = [];
            const pts = points;
            for (let i = 1; i < pts.length - 1; ++i) {
                const p = pts[i];
                const h = base - p.y;
                const r = Math.abs(Math.sin(i * 91.7 + seed * 13.1));
                const slope = (pts[i + 1].y - pts[i - 1].y) / (pts[i + 1].x - pts[i - 1].x);
                // Only the faces turned toward the sun in the notch.
                const facing = p.x < width / 2 ? slope > 0.08 : slope < -0.08;
                if (h < 8 || r < 0.45 || !facing)
                    continue;
                const len = h * (0.12 + 0.35 * r);
                out.push([p, Qt.point(p.x + len / slope * 0.35, p.y + len)]);
            }
            return out;
        }
        // Vector: two contour lines inside each ridge.
        readonly property var contours: {
            const out = [];
            for (const k of [0.62, 0.3]) {
                const line = [];
                for (const p of points)
                    line.push(Qt.point(p.x, base - (base - p.y) * k));
                out.push(line);
            }
            return out;
        }
        ShapePath {
            fillGradient: LinearGradient {
                x1: 0
                y1: ridge.base - ridge.amplitude * ridge.base
                x2: 0
                y2: ridge.base
                GradientStop { position: 0; color: win.vector ? win.mix(win.skyTop, "black", 0.5) : win.mix(ridge.fillColor, ridge.rim, 0.22) }
                GradientStop { position: 0.6; color: win.vector ? win.mix(win.skyTop, "black", 0.5) : ridge.fillColor }
                GradientStop { position: 1; color: win.vector ? win.mix(win.skyTop, "black", 0.5) : win.mix(ridge.fillColor, "black", 0.25) }
            }
            strokeColor: ridge.rim
            strokeWidth: 1.2
            joinStyle: ShapePath.MiterJoin
            PathPolyline { path: ridge.points }
            PathLine { x: ridge.width; y: ridge.base + 1 }
            PathLine { x: 0; y: ridge.base + 1 }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: win.screenprint ? win.alpha(ridge.rim, 0.5) : "transparent"
            strokeWidth: 1
            PathMultiline { paths: ridge.hatches }
        }
        ShapePath {
            fillColor: "transparent"
            strokeColor: win.vector ? win.alpha(ridge.rim, 0.45) : "transparent"
            strokeWidth: 1
            PathMultiline { paths: ridge.contours }
        }
    }
}
