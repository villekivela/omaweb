import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

// The road the Start page stands on: a night drive toward a banded sun, drawn
// from the palette and shown as a monochrome pixel display in the accent.
//
// The scene is drawn in the theme's colours, then colourized to the accent and
// sampled at one texel per display pixel, so every theme gets the same display
// in its own colour and a live theme change redraws it. Everything that does
// not move is cached in a layer; the lane marks, the posts and the ground grid
// are the only things that move, on one frame clock that runs only while
// `running` is set.
Item {
    id: root
    objectName: "nightRoad"

    property var colors
    // A Private window's road has its lights off: no sun, no stars, no lane
    // marks and no posts, and the display turned up so what remains reads.
    property bool privateWindow: false
    // Whether the road may move. Off while the Start page is hidden or the
    // window is not the reader's, so a road nobody watches costs no frame.
    property bool running: false
    // A destination was committed and its page has not painted yet: the road
    // speeds up toward it.
    property bool driving: false
    // Logical pixels per display pixel.
    property int pitch: 4
    // How many frames the road has moved in, for the tests that check it stays
    // still.
    property int frames: 0

    readonly property real horizonY: Math.round(root.height / 2)
    // What a Private window's road leaves out is counted here.
    readonly property int stars: root.privateWindow ? 0 : 240
    readonly property int laneMarks: root.privateWindow ? 0 : 26
    readonly property int posts: root.privateWindow ? 0 : 24
    readonly property int gridLines: 12
    readonly property int movingMarks: root.laneMarks + root.posts + root.gridLines

    // ---- palette

    function mix(from, to, amount) {
        const a = Qt.color(from);
        const b = Qt.color(to);
        return Qt.rgba(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount, a.b + (b.b - a.b)
                       * amount, 1);
    }

    function alpha(color, amount) {
        const c = Qt.color(color);
        return Qt.rgba(c.r, c.g, c.b, amount);
    }

    // The road is always night. A light theme's night is drawn from its dark
    // text, so the ground stays dark and the lit lines stay light.
    readonly property color windowColor: root.colors.windowOpaque
    readonly property color textColor: root.colors.text
    readonly property bool lightTheme: Qt.color(root.windowColor).hslLightness > 0.6
    readonly property color ground: root.lightTheme ? root.mix(root.textColor, "black", 0.45) :
                                                      root.windowColor
    readonly property color light: root.lightTheme ? root.windowColor : root.textColor
    readonly property color deep: root.mix(root.ground, "black", root.lightTheme ? 0.72 : 0.45)
    // What the display is lit in: the accent, or the text with the lights off.
    readonly property color glow: root.privateWindow ? root.mix(root.light, root.ground, 0.3) :
                                                       root.colors.accent
    readonly property color skyTop: root.privateWindow ? root.mix(root.deep, "black", 0.2) :
                                                         root.deep
    readonly property color skyLow: root.privateWindow ? root.mix(root.ground, root.light, 0.14) :
                                                         root.mix(root.ground, root.glow, 0.28)
    readonly property color groundNear: root.mix(root.deep, "black", 0.35)
    readonly property color sunTop: root.mix(root.light, "white", 0.4)
    readonly property color sunLow: root.mix(root.glow, root.light, 0.25)

    // ---- geometry
    //
    // A point on the road at depth z, where z = 1 is the bottom edge and the
    // horizon is at infinity, and lateral position u, where the road's edges
    // are at -1 and 1.

    readonly property real vanishingX: root.width / 2
    readonly property real halfWidth: root.width * 0.42
    readonly property real depth: root.height - root.horizonY
    readonly property real railY: root.height - root.depth * 0.3

    function screenY(z) {
        return root.horizonY + root.depth / z;
    }

    function screenX(u, z) {
        return root.vanishingX + u * root.halfWidth / z;
    }

    function wrap(value, span) {
        return ((value % span) + span) % span;
    }

    // ---- motion

    property real travel: 0
    property real speed: 1
    readonly property real targetSpeed: root.driving ? 9 : 1
    // How far the sun has lit up for a commit, from 0 to 1. It rises on its
    // own curve rather than with the speed: a page that paints quickly leaves
    // the road no time to speed up, and the light is what says the reader was
    // heard. It stays lit until the road stops.
    property real lightUp: 0

    NumberAnimation {
        id: lightingUp
        target: root
        property: "lightUp"
        to: 1
        duration: 700
        easing.type: Easing.OutCubic
    }

    onDrivingChanged: if (driving)
                          lightingUp.restart()

    FrameAnimation {
        running: root.running
        onTriggered: {
            root.frames += 1;
            root.speed += (root.targetSpeed - root.speed) * Math.min(1, frameTime * 2.2);
            root.travel += frameTime * 1.6 * root.speed;
        }
    }

    // Speed is a state of the drive, not of the clock, so a road that stopped
    // mid-drive starts again at rest.
    onRunningChanged: {
        if (running)
            return;
        lightingUp.stop();
        root.speed = 1;
        root.lightUp = 0;
    }

    // ---- the scene, drawn in the palette

    Item {
        id: scene
        anchors.fill: parent

        // The sky, the sun and the mountains, drawn once.
        Item {
            anchors.fill: parent
            layer.enabled: true

            Rectangle {
                width: parent.width
                height: root.horizonY + 1
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: root.skyTop
                    }
                    GradientStop {
                        position: 0.55
                        color: root.mix(root.skyTop, root.skyLow, 0.35)
                    }
                    GradientStop {
                        position: 1
                        color: root.skyLow
                    }
                }
            }

            // Stars thin out toward the horizon, where the glow washes them out.
            Repeater {
                model: root.stars

                Rectangle {
                    required property int index

                    function fraction(value) {
                        return value - Math.floor(value);
                    }

                    readonly property real across: fraction(Math.sin(index * 12.9898) * 43758.5453)
                    readonly property real height01: Math.pow(fraction(Math.sin(index * 78.233)
                                                                       * 12543.123), 1.6)
                    readonly property real bright: fraction(Math.sin(index * 39.425) * 9321.77)

                    x: across * root.width
                    y: height01 * root.horizonY * 0.9
                    width: bright > 0.96 ? 2.5 : (bright > 0.8 ? 1.6 : 1)
                    height: width
                    radius: width / 2
                    color: root.light
                    opacity: (0.2 + 0.8 * bright) * (1 - height01 * 0.85)
                }
            }

            // The sun's glow.
            Shape {
                anchors.fill: parent
                visible: !root.privateWindow

                ShapePath {
                    strokeWidth: -1
                    fillGradient: RadialGradient {
                        centerX: root.vanishingX
                        centerY: root.horizonY
                        centerRadius: root.width * 0.55
                        focalX: centerX
                        focalY: centerY
                        GradientStop {
                            position: 0
                            color: root.alpha(root.sunLow, 0.6)
                        }
                        GradientStop {
                            position: 0.18
                            color: root.alpha(root.sunLow, 0.28)
                        }
                        GradientStop {
                            position: 0.45
                            color: root.alpha(root.glow, 0.1)
                        }
                        GradientStop {
                            position: 1
                            color: root.alpha(root.glow, 0)
                        }
                    }
                    startX: 0
                    startY: 0
                    PathLine {
                        x: root.width
                        y: 0
                    }
                    PathLine {
                        x: root.width
                        y: root.horizonY
                    }
                    PathLine {
                        x: 0
                        y: root.horizonY
                    }
                }
            }

            // The sun: a disc cut by bands that widen toward the horizon.
            Item {
                id: sun

                readonly property real radius: root.height * 0.15

                visible: !root.privateWindow
                x: root.vanishingX - radius
                y: root.horizonY - radius
                width: radius * 2
                height: radius

                Rectangle {
                    id: disc
                    width: sun.radius * 2
                    height: sun.radius * 2
                    radius: sun.radius
                    visible: false
                    layer.enabled: true
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: root.sunTop
                        }
                        GradientStop {
                            position: 0.5
                            color: root.sunLow
                        }
                    }
                }

                Item {
                    id: bands
                    width: sun.radius * 2
                    height: sun.radius * 2
                    visible: false
                    layer.enabled: true

                    Rectangle {
                        width: parent.width
                        height: sun.radius * 0.42
                        color: "white"
                    }

                    Repeater {
                        model: 7

                        Rectangle {
                            required property int index

                            readonly property real band: sun.radius * 0.58 / 7

                            y: sun.radius * 0.42 + index * band
                            width: parent.width
                            height: band * (0.88 - index * 0.1)
                            color: "white"
                        }
                    }
                }

                MultiEffect {
                    width: sun.radius * 2
                    height: sun.radius * 2
                    source: disc
                    maskEnabled: true
                    maskSource: bands
                    maskThresholdMin: 0.5
                    maskSpreadAtMin: 0.2
                }
            }

            Ridge {
                anchors.fill: parent
                seed: 1.3
                amplitude: 0.3
                notch: 0.16
                fillColor: root.mix(root.skyLow, root.skyTop, 0.3)
                rim: root.alpha(root.glow, root.privateWindow ? 0.4 : 0.35)
            }

            Ridge {
                anchors.fill: parent
                seed: 4.2
                amplitude: 0.2
                notch: 0.22
                fillColor: root.mix(root.skyLow, root.skyTop, 0.65)
                rim: root.alpha(root.glow, 0.55)
            }

            Ridge {
                anchors.fill: parent
                seed: 8.9
                amplitude: 0.11
                notch: 0.3
                fillColor: root.mix(root.skyTop, root.groundNear, 0.7)
                rim: root.alpha(root.glow, root.privateWindow ? 0.75 : 0.85)
            }
        }

        // The sun lighting up for a commit. One gradient whose opacity
        // changes, so it is not cached.
        Shape {
            anchors.fill: parent
            visible: !root.privateWindow && opacity > 0
            opacity: root.lightUp

            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: root.vanishingX
                    centerY: root.horizonY
                    centerRadius: root.width * 0.35
                    focalX: centerX
                    focalY: centerY
                    GradientStop {
                        position: 0
                        color: root.alpha(root.sunTop, 0.45)
                    }
                    GradientStop {
                        position: 0.3
                        color: root.alpha(root.sunLow, 0.15)
                    }
                    GradientStop {
                        position: 1
                        color: root.alpha(root.sunLow, 0)
                    }
                }
                startX: 0
                startY: 0
                PathLine {
                    x: root.width
                    y: 0
                }
                PathLine {
                    x: root.width
                    y: root.height
                }
                PathLine {
                    x: 0
                    y: root.height
                }
            }
        }

        // The ground and the road, drawn once.
        Item {
            anchors.fill: parent
            layer.enabled: true

            Rectangle {
                y: root.horizonY
                width: parent.width
                height: root.depth
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: root.mix(root.skyLow, root.groundNear, 0.4)
                    }
                    GradientStop {
                        position: 0.1
                        color: root.groundNear
                    }
                    GradientStop {
                        position: 1
                        color: root.mix(root.groundNear, "black", 0.35)
                    }
                }
            }

            // Asphalt, catching the horizon's light where it runs out.
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeWidth: -1
                    fillGradient: LinearGradient {
                        x1: 0
                        y1: root.horizonY
                        x2: 0
                        y2: root.height
                        GradientStop {
                            position: 0
                            color: root.mix(root.sunLow, root.ground, root.privateWindow ? 0.9 :
                                                                                           0.35)

                        }
                        GradientStop {
                            position: 0.12
                            color: root.mix(root.ground, root.groundNear, 0.2)
                        }
                        GradientStop {
                            position: 1
                            color: root.mix(root.groundNear, "black", 0.1)
                        }
                    }
                    startX: root.vanishingX - 1
                    startY: root.horizonY
                    PathLine {
                        x: root.vanishingX + 1
                        y: root.horizonY
                    }
                    PathLine {
                        x: root.screenX(1.08, 1)
                        y: root.height
                    }
                    PathLine {
                        x: root.screenX(-1.08, 1)
                        y: root.height
                    }
                }
            }

            // The sun on the asphalt: a narrow streak from the vanishing point.
            Shape {
                anchors.fill: parent
                visible: !root.privateWindow

                ShapePath {
                    strokeWidth: -1
                    fillGradient: LinearGradient {
                        x1: 0
                        y1: root.horizonY
                        x2: 0
                        y2: root.height
                        GradientStop {
                            position: 0
                            color: root.alpha(root.sunTop, 0.4)
                        }
                        GradientStop {
                            position: 0.35
                            color: root.alpha(root.sunLow, 0.12)
                        }
                        GradientStop {
                            position: 1
                            color: root.alpha(root.sunLow, 0)
                        }
                    }
                    startX: root.vanishingX - 2
                    startY: root.horizonY
                    PathLine {
                        x: root.vanishingX + 2
                        y: root.horizonY
                    }
                    PathLine {
                        x: root.screenX(0.3, 1)
                        y: root.height
                    }
                    PathLine {
                        x: root.screenX(-0.3, 1)
                        y: root.height
                    }
                }
            }

            // Edge lines and guard rails from the vanishing point, glowing.
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: root.glow
                    shadowBlur: 0.8
                    shadowOpacity: root.privateWindow ? 0.6 : 1
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                }

                ShapePath {
                    strokeColor: root.alpha(root.glow, root.privateWindow ? 0.8 : 1)
                    strokeWidth: 2.5
                    fillColor: "transparent"
                    startX: root.vanishingX
                    startY: root.horizonY
                    PathLine {
                        x: root.screenX(-1, 1)
                        y: root.height
                    }
                    PathMove {
                        x: root.vanishingX
                        y: root.horizonY
                    }
                    PathLine {
                        x: root.screenX(1, 1)
                        y: root.height
                    }
                }

                ShapePath {
                    strokeColor: root.alpha(root.mix(root.light, root.glow, 0.4),
                                            root.privateWindow ? 0.4 : 0.55)
                    strokeWidth: 2
                    fillColor: "transparent"
                    startX: root.vanishingX
                    startY: root.horizonY - 1
                    PathLine {
                        x: root.screenX(-1.5, 1)
                        y: root.railY
                    }
                    PathMove {
                        x: root.vanishingX
                        y: root.horizonY - 1
                    }
                    PathLine {
                        x: root.screenX(1.5, 1)
                        y: root.railY
                    }
                }
            }

            // The horizon: hot in the middle, gone at the edges.
            Rectangle {
                y: root.horizonY - 1
                width: parent.width
                height: 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: root.alpha(root.glow, 0)
                    }
                    GradientStop {
                        position: 0.5
                        color: root.alpha(root.privateWindow ? root.glow : root.sunTop, root.privateWindow
                                          ? 0.9 : 1)
                    }
                    GradientStop {
                        position: 1
                        color: root.alpha(root.glow, 0)
                    }
                }
            }
        }

        // What moves: a faint grid across the ground, the centre line and the
        // guard-rail posts, all plain rectangles placed from the travel.
        Item {
            anchors.fill: parent

            Repeater {
                model: root.gridLines

                Rectangle {
                    required property int index

                    readonly property real distance: 1 + root.wrap(index * 2.5 - root.travel, 30)

                    y: root.screenY(distance)
                    width: root.width
                    height: 1
                    color: root.glow
                    opacity: (root.privateWindow ? 0.05 : 0.08) * Math.min(1, distance - 1) * (1 - distance
                                                                                               / 32)
                }
            }

            Repeater {
                model: root.laneMarks

                Rectangle {
                    required property int index

                    readonly property real distance: 1 + root.wrap(index * 1.15 - root.travel, 26
                                                                   * 1.15)
                    // Lane marks stretch as the road speeds up.
                    readonly property real length: 0.42 + Math.min(0.7, (root.speed - 1) * 0.1)
                    readonly property real near: root.screenY(distance)
                    readonly property real far: root.screenY(distance + length)

                    x: root.vanishingX - width / 2
                    y: far
                    width: Math.max(1, 12 / distance)
                    height: Math.max(1, near - far)
                    radius: width / 3
                    antialiasing: true
                    color: root.mix(root.sunTop, "white", 0.35)
                    opacity: Math.min(1, 1.3 - distance / 30) * Math.min(1, (distance - 1) * 2.5)
                }
            }

            Repeater {
                model: root.posts

                Rectangle {
                    required property int index

                    readonly property real side: index % 2 === 0 ? -1.5 : 1.5
                    readonly property real span: 12 * 1.6
                    readonly property real distance: 1.2 + root.wrap(Math.floor(index / 2) * 1.6
                                                                     - root.travel, span)

                    x: root.screenX(side, distance) - width / 2
                    y: root.horizonY - 1 + (root.railY - root.horizonY + 1) / distance
                    width: Math.max(1, 6 / distance)
                    height: Math.max(1, root.depth * 0.11 / distance)
                    color: root.mix(root.groundNear, root.light, 0.45)
                    opacity: Math.min(1, 1.2 - distance / span) * Math.min(1, (distance - 1.2) * 2)
                }
            }
        }

        // Darker at the corners, like a windscreen's edge.
        Shape {
            anchors.fill: parent

            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: root.vanishingX
                    centerY: root.horizonY
                    centerRadius: Math.max(root.width, root.height) * 0.85
                    focalX: centerX
                    focalY: centerY
                    GradientStop {
                        position: 0.4
                        color: "transparent"
                    }
                    GradientStop {
                        position: 1
                        color: root.alpha("black", 0.6)
                    }
                }
                startX: 0
                startY: 0
                PathLine {
                    x: root.width
                    y: 0
                }
                PathLine {
                    x: root.width
                    y: root.height
                }
                PathLine {
                    x: 0
                    y: root.height
                }
            }
        }

        // A 4 by 4 ordered dither at the display's pitch, which is what shading
        // becomes on a display with one colour.
        PatternCanvas {
            anchors.fill: parent
            tileSize: root.pitch * 4
            drawTile: function (tile) {
                const order = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5];
                const side = root.pitch * 4;
                for (let y = 0; y < side; ++y) {
                    for (let x = 0; x < side; ++x) {
                        const cell = order[Math.floor(y / root.pitch) * 4 + Math.floor(x
                                                                                       / root.pitch)];
                        tile.data[(y * root.pitch * 4 + x) * 4 + 3] = Math.round(cell / 16 * 0.5
                                                                                 * 255);
                    }
                }
            }
        }
    }

    // ---- the display

    // Captures the scene and hides it. A hidden item would leave its cached
    // layers undrawn, so the scene stays visible and this takes it away.
    ShaderEffectSource {
        id: capture
        anchors.fill: parent
        visible: false
        sourceItem: scene
        hideSource: true
        live: true
    }

    MultiEffect {
        id: monochrome
        anchors.fill: parent
        visible: false
        source: capture
        colorization: 1
        colorizationColor: root.glow
        contrast: root.privateWindow ? 0.75 : 0.6
        // The whole display brightens with the sun, which is what makes it read
        // as light rather than as a brighter patch.
        brightness: (root.privateWindow ? 0.5 : 0.28) + root.lightUp * 0.18
    }

    ShaderEffectSource {
        objectName: "nightRoadDisplay"
        anchors.fill: parent
        sourceItem: monochrome
        live: true
        smooth: false
        textureSize: Qt.size(Math.max(1, Math.ceil(width / root.pitch)), Math.max(1, Math.ceil(
                                                                                      height / root.pitch)))
    }

    // The dark seam between display pixels.
    PatternCanvas {
        anchors.fill: parent
        tileSize: root.pitch
        drawTile: function (tile) {
            const seam = Math.round(0.45 * 255);
            for (let index = 0; index < root.pitch; ++index) {
                tile.data[index * 4 + 3] = seam;
                tile.data[index * root.pitch * 4 + 3] = seam;
            }
        }
    }

    // A canvas filled with one small tile, repeated. Drawn only while it can be
    // seen, and again only for a new size or pitch: the fill is one call, so
    // a page area that is resized does not wait on it.
    component PatternCanvas: Canvas {
        id: pattern

        property int tileSize: 4
        // Writes one tile's pixels into the image data it is handed.
        property var drawTile: function (tile) {}
        property bool stale: true
        readonly property string layoutKey: width + "x" + height + "@" + tileSize

        renderStrategy: Canvas.Immediate
        onLayoutKeyChanged: {
            stale = true;
            if (visible)
                requestPaint();
        }
        onVisibleChanged: if (visible && stale)
                              requestPaint()
        onPaint: {
            stale = false;
            const context = getContext("2d");
            context.reset();
            const tile = context.createImageData(tileSize, tileSize);
            pattern.drawTile(tile);
            context.fillStyle = context.createPattern(tile, "repeat");
            context.fillRect(0, 0, width, height);
        }
    }

    // A mountain ridge: sharp peaks from folded sines, falling to the horizon
    // in a notch where the road runs out.
    component Ridge: Shape {
        id: ridge

        property real seed: 1
        property real amplitude: 0.2
        property real notch: 0.2
        property color fillColor
        property color rim

        readonly property real base: root.horizonY
        readonly property var points: {
            const out = [];
            const count = 220;
            for (let index = 0; index <= count; ++index) {
                const u = index / count;
                const d = Math.min(1, Math.abs(u - 0.5) / ridge.notch);
                const valley = d * d * (3 - 2 * d);
                let height = 0.55 * Math.pow(1 - Math.abs(Math.sin(u * 6.3 + ridge.seed)), 1.6);
                height += 0.3 * Math.pow(1 - Math.abs(Math.sin(u * 15.7 + ridge.seed * 2.3)), 2);
                height += 0.15 * Math.pow(1 - Math.abs(Math.sin(u * 37.1 + ridge.seed * 5.1)), 2);
                height = 0.25 + height * 0.75;
                out.push(Qt.point(u * ridge.width, ridge.base - height * ridge.amplitude
                                  * ridge.base * valley));
            }
            return out;
        }

        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillGradient: LinearGradient {
                x1: 0
                y1: ridge.base - ridge.amplitude * ridge.base
                x2: 0
                y2: ridge.base
                GradientStop {
                    position: 0
                    color: root.mix(ridge.fillColor, ridge.rim, 0.22)
                }
                GradientStop {
                    position: 0.6
                    color: ridge.fillColor
                }
                GradientStop {
                    position: 1
                    color: root.mix(ridge.fillColor, "black", 0.25)
                }
            }
            strokeColor: ridge.rim
            strokeWidth: 1.2
            joinStyle: ShapePath.MiterJoin
            PathPolyline {
                path: ridge.points
            }
            PathLine {
                x: ridge.width
                y: ridge.base + 1
            }
            PathLine {
                x: 0
                y: ridge.base + 1
            }
        }
    }
}
