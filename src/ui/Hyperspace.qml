import QtQuick
import Omaweb
import "SceneColour.mjs" as Colour

// Hyperspace, a Start page Scene: the view out of a cockpit window, a deep
// star field drifting slowly toward the reader over a faint nebula in the
// accent. From a commit until the page paints, the stars stretch into streaks
// from the centre behind the Omnibar and rush past, and they hold until the
// page has replaced the Scene: there is no tunnel and no collapse back to
// stars. It is an homage to an effect, not to a film, so it has no logo,
// crawl, ships or film's name.
//
// It keeps NightRoad.qml's contract with SceneHost.qml: it receives `colors`,
// `dark`, `time`, `navigating`, `beat`, `options`, `reducedMotion` and
// `unlit`, and declares `pitch`, `fps`, `glass` with the glass's amounts as
// `crt`, its options, and `horizonY`, where the Omnibar rests. It casts no
// light. What it draws by is share/scenes/hyperspace.json. Space and the
// nebula are drawn once per size and theme into a canvas; the stars are
// scene-graph items over it, placed from the clock, each a star at rest and
// a streak in the jump.
Item {
    id: root
    objectName: "hyperspace"

    // ---- what the host hands it

    // The theme's palette: the field reads `windowOpaque`, `text` and
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
    // A Private window's field has its lights out: the nebula alone, very
    // dim, in the theme's light rather than its accent, and no jump on
    // commit.
    property bool unlit: false
    // The Start page is fading out after a drive, as the page replaces it.
    // The streaks hold meanwhile rather than collapse back to stars, and the
    // stars are back at once when it has gone, unseen. The browser's Start
    // page hands it in.
    property bool leaving: false

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("hyperspace")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options

    // ---- what it drew, for the tests

    readonly property int stars: root.unlit ? root.parameters.unlit.stars :
                                              root.parameters.stars.count
    readonly property real nebulaStrength: root.unlit ? root.parameters.unlit.nebula : 1

    // ---- palette

    // A night in the theme, as the road's. A light theme's night is drawn
    // from its dark text, so space stays dark and the stars stay light.
    readonly property var roles: {
        const night = root.parameters.night;
        const ground = root.dark ? Qt.color(root.colors.windowOpaque) : Colour.mix(root.colors.text,
                                                                                   "black", night.lightThemeGround);
        const light = Qt.color(root.dark ? root.colors.text : root.colors.windowOpaque);
        const glow = root.unlit ? Colour.mix(light, ground, root.parameters.unlit.glow) : Qt.color(
                                      root.colors.accent);
        return {
            black: Qt.color("black"),
            white: Qt.color("white"),
            ground: ground,
            light: light,
            glow: glow,
            deep: Colour.mix(ground, "black", root.dark ? night.deep : night.deepLightTheme)
        };
    }

    // A colour the parameters name: a role, or a mix of two at an amount.
    function colour(name) {
        if (typeof name === "string")
            return root.roles[name];
        return Colour.mix(root.colour(name.mix[0]), root.colour(name.mix[1]), name.mix[2]);
    }

    // A star's light, at its head, and the glow its streak's tail fades to.
    readonly property color starColour: root.colour(root.parameters.stars.colour)
    readonly property color tailColour: Colour.withAlpha(root.colour(root.parameters.stars.tail), 0)

    // ---- geometry, in logical pixels: the Scene's pixels times its pitch

    readonly property real drawWidth: root.width * root.pitch
    readonly property real drawHeight: root.height * root.pitch
    // Where the Omnibar rests, as it rests on the road's horizon.
    readonly property real horizonY: Math.round(root.drawHeight * root.parameters.rest)
    // Where the stars stream from, behind the Omnibar's field.
    readonly property point centre: Qt.point(root.drawWidth / 2, root.horizonY)
    // From the centre to the farthest corner.
    readonly property real reach: Math.hypot(Math.max(root.centre.x, root.drawWidth - root.centre.x),
                                             Math.max(root.centre.y, root.drawHeight
                                                      - root.centre.y))

    // ---- the stars
    //
    // Each star stands on a ray from the centre at a bearing its index
    // scatters, its lateral the share of the reach it stands out from the
    // centre at depth 1, and comes on toward the reader from the far end of
    // the field to the near one, then round again. A star is drawn at its
    // lateral over its depth, so it moves out along its ray, faster the
    // nearer it comes.

    readonly property var starSeeds: {
        const st = root.parameters.stars;
        const out = [];
        for (let index = 1; index <= st.count; ++index) {
            const bearing = 2 * Math.PI * Colour.scatter(index, st.bearing);
            const bright = Colour.scatter(index, st.brightness);
            out.push({
                         cos: Math.cos(bearing),
                         sin: Math.sin(bearing),
                         angle: bearing * 180 / Math.PI,
                         lateral: st.lateral[0] + st.lateral[1] * Math.sqrt(Colour.scatter(index,
                                                                                           st.spread)),
                         phase: Colour.scatter(index, st.phase),
                         alpha: st.alpha.base + st.alpha.range * bright,
                         size: bright > st.brightAbove ? st.sizes.bright : st.sizes.dim
                     });
        }
        return out;
    }

    // The star `index` as it is drawn now: its head, where the star is, and
    // the tail of its streak, toward the centre, both in logical pixels, its
    // alpha and its size. At rest the tail is the head.
    function star(index) {
        const st = root.parameters.stars;
        const s = root.starSeeds[index];
        const span = st.far - st.near;
        const depth = st.near + span * Colour.fraction(s.phase - root.travel / span);
        const distance = s.lateral * root.reach;
        const head = distance / depth;
        const tail = distance / (depth + root.parameters.streak.depth * root.warp);
        const fade = Math.max(0, Math.min(1, (st.far - depth) / st.fadeFar, (depth - st.near)
                                          / st.fadeNear));
        return {
            head: {
                x: root.centre.x + head * s.cos,
                y: root.centre.y + head * s.sin
            },
            tail: {
                x: root.centre.x + tail * s.cos,
                y: root.centre.y + tail * s.sin
            },
            alpha: s.alpha * fade * (root.unlit ? root.parameters.unlit.starAlpha : 1),
            size: s.size
        };
    }

    // ---- motion
    //
    // At rest the field comes on slowly. `warp` eases toward what
    // `navigating` asks: as it rises the stars stretch into streaks and the
    // field rushes past, and it holds while the Start page fades out after a
    // drive. A still or unlit field holds one frame, with no streaks.

    // How far the field has come toward the reader, in depth units.
    property real travel: 0
    property real warp: 0
    property real last: -1

    function move() {
        const w = root.parameters.warp;
        if (root.reducedMotion || root.unlit || root.last < 0) {
            root.travel = root.parameters.still;
            root.warp = 0;
        }
        if (root.reducedMotion || root.unlit)
            return;
        if (root.last < 0)
            root.last = root.time;
        const step = Math.max(0, Math.min(w.longestStep, root.time - root.last));
        root.last = root.time;
        const target = root.leaving ? Math.max(root.navigating, root.warp) : root.navigating;
        root.warp += (target - root.warp) * Math.min(1, step * w.ease);
        root.travel += step * (root.parameters.drift + (w.rush - root.parameters.drift)
                               * root.warp);
    }

    onTimeChanged: root.move()
    onReducedMotionChanged: root.move()
    onUnlitChanged: root.move()
    onLeavingChanged: if (!root.leaving && root.navigating === 0)
                          root.warp = 0
    Component.onCompleted: root.move()

    // ---- drawing

    // What does not move: space, a little lighter toward the centre, and the
    // nebula's clouds over it.
    function drawStill(context) {
        const p = root.parameters;
        const c = root.centre;
        const space = context.createRadialGradient(c.x, c.y, 0, c.x, c.y, root.reach);
        space.addColorStop(0, Colour.css(root.colour(p.space.deep)));
        space.addColorStop(1, Colour.css(root.colour(p.space.edge)));
        context.fillStyle = space;
        context.fillRect(0, 0, root.drawWidth, root.drawHeight);

        for (const cloud of p.nebula) {
            const r = cloud.radius * root.drawWidth;
            const alpha = cloud.alpha * root.nebulaStrength;
            context.save();
            context.translate(cloud.at[0] * root.drawWidth, cloud.at[1] * root.drawHeight);
            context.rotate(cloud.tilt * Math.PI / 180);
            context.scale(1, cloud.squash);
            const haze = context.createRadialGradient(0, 0, 0, 0, 0, r);
            haze.addColorStop(0, Colour.css(root.roles.glow, alpha));
            haze.addColorStop(0.45, Colour.css(root.roles.glow, alpha * 0.5));
            haze.addColorStop(1, Colour.css(root.roles.glow, 0));
            context.fillStyle = haze;
            context.fillRect(-r, -r, 2 * r, 2 * r);
            context.restore();
        }
    }

    readonly property string key: [root.width, root.height, root.roles.deep, root.roles.glow,
        root.unlit].join("/")

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

    // The stars, in logical pixels scaled to the Scene's. A star at rest is a
    // square on the Scene's own pixels, so it is drawn sharp; in the jump it
    // is a streak along its ray, the star's light at its head fading to the
    // glow at its tail.
    Item {
        id: moving

        width: root.drawWidth
        height: root.drawHeight
        transform: Scale {
            xScale: 1 / root.pitch
            yScale: 1 / root.pitch
        }

        Repeater {
            model: root.stars

            Rectangle {
                id: mark

                required property int index

                readonly property var star: root.star(index)
                readonly property real length: Math.hypot(star.head.x - star.tail.x, star.head.y
                                                          - star.tail.y)
                readonly property bool streak: length >= 0.5

                x: streak ? star.tail.x : Math.floor(star.tail.x / root.pitch) * root.pitch
                y: (streak ? star.tail.y : Math.floor(star.tail.y / root.pitch) * root.pitch) - (
                       streak ? height / 2 : 0)
                width: length + star.size
                height: star.size
                transformOrigin: Item.Left
                rotation: streak ? root.starSeeds[index].angle : 0
                antialiasing: streak
                visible: star.alpha > 0.01 && star.tail.x > -star.size && star.tail.x
                         < root.drawWidth + star.size && star.tail.y > -star.size && star.tail.y
                         < root.drawHeight + star.size
                opacity: star.alpha
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: mark.streak ? root.tailColour : root.starColour
                    }
                    GradientStop {
                        position: mark.length / Math.max(1, mark.width)
                        color: root.starColour
                    }
                    GradientStop {
                        position: 1
                        color: root.starColour
                    }
                }
            }
        }
    }
}
