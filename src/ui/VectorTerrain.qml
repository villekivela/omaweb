import QtQuick
import Omaweb
import "SceneColour.mjs" as Colour

// A vector terrain, a Start page Scene after Battlezone and the vector arcade
// games: wireframe mountains on a horizon below the Omnibar, a perspective
// grid on the ground before them and a wireframe crescent moon low in the
// sky, in thin lines with a vector monitor's glow. At rest the ridges drift
// by in slow parallax; from a commit until the page paints the grid rushes
// toward the reader while the mountains hold the horizon.
//
// It keeps NightRoad.qml's contract with SceneHost.qml: it receives `colors`,
// `dark`, `time`, `navigating`, `beat`, `options`, `reducedMotion` and
// `unlit`, and declares `pitch`, `fps`, `glass` with the glass's amounts as
// `crt`, its options, and `horizonY`, where the Omnibar rests. It casts no
// light. What it draws by is share/scenes/vector-terrain.json. The sky, the
// grid's radials and the moon are drawn once per size and theme into a
// canvas, and each ridge once into a canvas two loops wide that slides under
// the clock; the grid's lines across are scene-graph items placed from it.
Item {
    id: root
    objectName: "vectorTerrain"

    // ---- what the host hands it

    // The theme's palette: the terrain reads `windowOpaque`, `text` and
    // `accent`.
    property var colors: ({
                              windowOpaque: "black",
                              text: "white",
                              accent: "white"
                          })
    property bool dark: true
    property real time: 0
    property real navigating: 0
    property real beat: 0
    property var options: ({})
    property bool reducedMotion: false
    // A Private window's terrain has its lights out: the wireframe alone, its
    // lines the theme's light dimmed toward the ground, with no glow and no
    // moon, and it holds still, even on commit.
    property bool unlit: false
    // How far the resting Omnibar reaches below where it rests, which the
    // mountains keep clear. The browser's Start page hands it in; a picture
    // with no Omnibar over it, such as a thumbnail, keeps the reach at the
    // theme's type size.
    property real omnibarReach: root.parameters.peaks.omnibarReach

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("vector-terrain")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options

    // ---- what it drew, for the tests

    readonly property bool moonShown: !root.unlit
    readonly property bool glowing: !root.unlit
    readonly property int depthLines: root.parameters.grid.count

    // ---- palette

    // A night in the theme, as the road's. A light theme's night is drawn
    // from its dark text, so the sky stays dark and the lines stay lit.
    readonly property var roles: {
        const night = root.parameters.night;
        const ground = root.dark ? Qt.color(root.colors.windowOpaque) : Colour.mix(root.colors.text,
                                                                                   "black", night.lightThemeGround);
        const light = Qt.color(root.dark ? root.colors.text : root.colors.windowOpaque);
        const deep = Colour.mix(ground, "black", root.dark ? night.deep : night.deepLightTheme);
        const glow = root.unlit ? Colour.mix(light, ground, root.parameters.lines.unlit) : Qt.color(
                                      root.colors.accent);
        return {
            black: Qt.color("black"),
            white: Qt.color("white"),
            ground: ground,
            light: light,
            glow: glow,
            skyTop: deep,
            skyLow: Colour.mix(ground, glow, night.skyLow),
            groundNear: Colour.mix(deep, "black", night.groundNear)
        };
    }

    // A colour the parameters name: a role, or a mix of two at an amount.
    function colour(name) {
        if (typeof name === "string")
            return root.roles[name];
        return Colour.mix(root.colour(name.mix[0]), root.colour(name.mix[1]), name.mix[2]);
    }

    function hash(index) {
        return Colour.scatter(index, root.parameters.scatter);
    }

    // A place in a [from, span] range that `index` scatters to.
    function spread(range, index) {
        return range[0] + range[1] * root.hash(index);
    }

    function wrap(value, span) {
        return ((value % span) + span) % span;
    }

    // ---- geometry, in logical pixels: the Scene's pixels times its pitch
    //
    // A point on the ground at depth z, where z = 1 is the bottom edge, and
    // lateral position u, where u = 1 is half the width out from the middle
    // at the bottom edge.

    readonly property real drawWidth: root.width * root.pitch
    readonly property real drawHeight: root.height * root.pitch
    // Where the Omnibar rests, as it rests on the road's horizon.
    readonly property real horizonY: Math.round(root.drawHeight * root.parameters.rest)
    // The highest a peak may stand, a gap below the resting Omnibar's hint
    // row, and the ground's horizon the mountains stand on.
    readonly property real peaksTop: root.horizonY + root.omnibarReach + root.parameters.peaks.gap
    readonly property real groundY: Math.round(root.peaksTop + root.parameters.peaks.share * (
                                                   root.drawHeight - root.peaksTop))
    readonly property real mountains: root.groundY - root.peaksTop
    readonly property real depth: root.drawHeight - root.groundY
    readonly property real vanishingX: root.drawWidth / 2

    function screenY(z) {
        return root.groundY + root.depth / z;
    }

    function screenX(u, z) {
        return root.vanishingX + u * root.vanishingX / z;
    }

    // A ridge's outline over one loop, the width long, as [x, y] points: a
    // valley, a peak, a valley and so on, the last valley the first one a
    // loop on, so the loop joins up.
    function ridge(index) {
        const layer = root.parameters.ridges.layers[index];
        const count = layer.peaks;
        const points = [];
        for (let peak = 0; peak <= count; ++peak) {
            // Three scatters a peak: its valley, where it stands and its height.
            const seed = layer.seed + (peak % count) * 3;
            points.push([peak / count * root.drawWidth, root.groundY - layer.valley * root.hash(seed)
                         * root.mountains]);
            if (peak === count)
                break;
            const across = (peak + root.spread(layer.place, seed + 1)) / count;
            const high = root.spread(layer.height, seed + 2);
            points.push([across * root.drawWidth, root.groundY - high * root.mountains]);
        }
        return points;
    }

    // The resting Omnibar's field, which the moon keeps clear.
    readonly property rect omnibar: {
        const o = root.parameters.omnibar;
        const width = Math.min(root.drawWidth - 2 * o.margin, o.widest);
        return Qt.rect(root.vanishingX - width / 2, root.horizonY - o.above, width, o.above
                       + root.omnibarReach);
    }

    // The moon hangs low beside the Omnibar where the page area has room for
    // it there, and otherwise just above the Omnibar, rather than behind it.
    readonly property real moonRadius: Math.min(root.drawWidth, root.drawHeight)
                                       * root.parameters.moon.radius
    readonly property point moonCentre: {
        const m = root.parameters.moon;
        const gap = root.parameters.peaks.gap;
        const r = root.moonRadius;
        const beside = Math.max(root.drawWidth * m.across, root.omnibar.x + root.omnibar.width + gap
                                + r);
        if (beside + r + gap <= root.drawWidth)
            return Qt.point(beside, root.peaksTop - m.lift * r);
        return Qt.point(Math.min(root.drawWidth * m.across, root.drawWidth - gap - r),
                        root.omnibar.y - gap - r);
    }

    // ---- motion
    //
    // At rest the ridges drift by, nearer ones faster, and the grid holds
    // still. The grid's speed eases toward what `navigating` asks, so its
    // lines rush toward the reader on a commit and slow to a stop after the
    // page has painted. A still or unlit terrain holds one frame.

    // Seconds the ridges have drifted.
    property real drift: 0
    // How far the grid has come toward the reader, in depth units.
    property real travel: 0
    property real speed: 0
    property real last: -1

    function move() {
        const g = root.parameters.grid;
        if (root.reducedMotion || root.unlit || root.last < 0) {
            root.drift = root.parameters.ridges.still;
            root.travel = g.still;
            root.speed = 0;
        }
        if (root.reducedMotion || root.unlit)
            return;
        if (root.last < 0)
            root.last = root.time;
        const step = Math.max(0, Math.min(g.longestStep, root.time - root.last));
        root.last = root.time;
        root.drift += step;
        root.speed += (g.rush * root.navigating - root.speed) * Math.min(1, step * g.ease);
        root.travel += step * root.speed;
    }

    onTimeChanged: root.move()
    onReducedMotionChanged: root.move()
    onUnlitChanged: root.move()
    Component.onCompleted: root.move()

    // How far the ridge `index` has slid left, from 0 to a loop.
    function ridgeOffset(index) {
        return Colour.fraction(root.drift * root.parameters.ridges.layers[index].drift)
                * root.drawWidth;
    }

    // Where the grid's line across `index` stands: lines a spacing apart in
    // depth, coming round to the far end once they pass the bottom edge.
    function depthLineY(index) {
        return root.screenY(root.lineDepth(index));
    }

    function lineDepth(index) {
        const g = root.parameters.grid;
        return 1 + root.wrap(index * g.spacing - root.travel, g.count * g.spacing);
    }

    // ---- drawing

    // A path drawn as a line of the terrain: thin, in `colour`, with a vector
    // monitor's glow around it while the lights are on. The width is in
    // logical pixels, scaled to the Scene's with the canvas; the blur is not
    // scaled, so it is in the Scene's pixels already. Joins are round: a
    // mitred join at a sharp peak would reach up past the peak toward the
    // Omnibar.
    function stroke(context, colour) {
        const l = root.parameters.lines;
        context.lineWidth = l.width * root.pitch;
        context.lineJoin = "round";
        context.strokeStyle = Colour.css(colour);
        context.shadowOffsetX = 0;
        context.shadowOffsetY = 0;
        context.shadowBlur = root.glowing ? l.glow.blur : 0;
        context.shadowColor = Colour.css(colour, root.glowing ? l.glow.alpha : 0);
        context.stroke();
    }

    // What does not move: the sky, the ground, the horizon, the grid's lines
    // toward the vanishing point and the moon.
    function drawStill(context) {
        const p = root.parameters;
        const w = root.drawWidth;
        const sky = context.createLinearGradient(0, 0, 0, root.groundY);
        for (const stop of p.sky)
            sky.addColorStop(stop[0], Colour.css(root.colour(stop[1])));
        context.fillStyle = sky;
        context.fillRect(0, 0, w, root.groundY);
        context.fillStyle = Colour.css(root.roles.groundNear);
        context.fillRect(0, root.groundY, w, root.depth);

        const g = p.grid;
        context.beginPath();
        for (let radial = -g.radials; radial <= g.radials; ++radial) {
            const u = radial * g.spread;
            context.moveTo(root.screenX(u, g.far), root.screenY(g.far));
            context.lineTo(root.screenX(u, 1), root.drawHeight);
        }
        root.stroke(context, root.colour(g.colour));

        context.beginPath();
        context.moveTo(0, root.groundY);
        context.lineTo(w, root.groundY);
        root.stroke(context, root.colour(p.horizon.colour));

        if (root.moonShown)
            root.drawMoon(context);
    }

    // The moon: a circle with a second one, as large, cut from it toward its
    // dark side, leaving the crescent's outer and inner arcs.
    function drawMoon(context) {
        const m = root.parameters.moon;
        const c = root.moonCentre;
        const r = root.moonRadius;
        const dark = m.tilt * Math.PI / 180;
        const cut = m.phase * r;
        // Where the two circles cross, either side of the dark side.
        const half = Math.acos(cut / (2 * r));
        const cutter = Qt.point(c.x + cut * Math.cos(dark), c.y + cut * Math.sin(dark));
        const crossing = function (angle) {
            const x = c.x + r * Math.cos(angle) - cutter.x;
            const y = c.y + r * Math.sin(angle) - cutter.y;
            return Math.atan2(y, x);
        };
        context.beginPath();
        context.arc(c.x, c.y, r, dark + half, dark + 2 * Math.PI - half, false);
        context.arc(cutter.x, cutter.y, r, crossing(dark - half), crossing(dark + half), true);
        context.closePath();
        root.stroke(context, root.colour(m.colour));
    }

    // A ridge over two loops, so it can slide a loop and join up: filled with
    // the ground to hide what stands behind it, outlined, and a facet line
    // falling from each peak to the ground.
    function drawRidge(context, index) {
        const layer = root.parameters.ridges.layers[index];
        const points = root.ridge(index);
        const colour = root.colour(layer.colour);
        const outline = function (shift) {
            context.beginPath();
            context.moveTo(points[0][0] + shift, points[0][1]);
            for (const point of points.slice(1))
                context.lineTo(point[0] + shift, point[1]);
        };
        for (let loop = 0; loop < 2; ++loop) {
            const shift = loop * root.drawWidth;
            outline(shift);
            context.lineTo(points[points.length - 1][0] + shift, root.groundY);
            context.lineTo(points[0][0] + shift, root.groundY);
            context.closePath();
            context.fillStyle = Colour.css(root.roles.ground);
            context.fill();

            outline(shift);
            for (let peak = 1; peak < points.length; peak += 2) {
                const top = points[peak];
                const next = points[peak + 1];
                context.moveTo(top[0] + shift, top[1]);
                context.lineTo(top[0] + layer.facet * (next[0] - top[0]) + shift, root.groundY);
            }
            root.stroke(context, colour);
        }
    }

    readonly property string key: [root.width, root.height, root.roles.ground, root.roles.light,
        root.roles.glow, root.unlit, root.omnibarReach].join("/")

    SceneLayer {
        id: still

        pitch: root.pitch
        paintWith: root.drawStill
        Component.onCompleted: draw()

        Connections {
            target: root
            function onKeyChanged() {
                still.draw();
            }
        }
    }

    // The grid's lines across, each fading in from the far end and out at
    // the bottom edge, in logical pixels scaled to the Scene's.
    Item {
        id: moving

        width: root.drawWidth
        height: root.drawHeight
        transform: Scale {
            xScale: 1 / root.pitch
            yScale: 1 / root.pitch
        }

        Repeater {
            model: root.depthLines

            Item {
                id: line

                required property int index

                readonly property var grid: root.parameters.grid
                readonly property var lines: root.parameters.lines
                readonly property real depth: root.lineDepth(index)
                readonly property color colour: root.colour(grid.colour)
                readonly property real reach: lines.glow.blur * root.pitch

                y: root.depthLineY(index)
                width: root.drawWidth
                opacity: Math.max(0, Math.min(1, (grid.count * grid.spacing + 1 - depth) / grid.fadeFar,
                                              (depth - 1) / grid.fadeNear))

                Rectangle {
                    visible: root.glowing
                    y: -line.reach
                    width: line.width
                    height: 2 * line.reach
                    gradient: Gradient {
                        GradientStop {
                            position: 0
                            color: Colour.withAlpha(root.roles.glow, 0)
                        }
                        GradientStop {
                            position: 0.5
                            color: Colour.withAlpha(root.roles.glow, line.lines.glow.across)
                        }
                        GradientStop {
                            position: 1
                            color: Colour.withAlpha(root.roles.glow, 0)
                        }
                    }
                }

                Rectangle {
                    y: -height / 2
                    width: line.width
                    height: line.lines.width * root.pitch
                    color: line.colour
                }
            }
        }
    }

    // The ridges, far to near, each sliding left under the clock.
    Repeater {
        model: root.parameters.ridges.layers.length

        Item {
            id: range

            required property int index

            x: -root.ridgeOffset(index) / root.pitch
            width: root.width * 2
            height: root.height

            SceneLayer {
                id: ridgeLayer

                pitch: root.pitch
                paintWith: function (context) {
                    root.drawRidge(context, range.index);
                }
                Component.onCompleted: draw()

                Connections {
                    target: root
                    function onKeyChanged() {
                        ridgeLayer.draw();
                    }
                }
            }
        }
    }
}
