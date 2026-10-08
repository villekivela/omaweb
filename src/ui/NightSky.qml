import QtQuick
import Omaweb
import "SceneColour.mjs" as Colour

// The night sky, the Start page's second Scene, after Netscape Navigator's
// throbber: a still star field over a dark planet whose limb crests just
// below the Omnibar, a comet or a shooting star falling now and then on a slow
// diagonal and going out behind the limb, and streaks falling thick and fast
// while the reader navigates. There is no mark: the Omnibar is the one thing
// in the middle.
//
// It keeps NightRoad.qml's contract with SceneHost.qml: it receives `colors`,
// `dark`, `time`, `navigating`, `beat`, `options`, `reducedMotion` and
// `unlit`, and declares `pitch`, `fps`, `glass` with the glass's amounts as
// `crt`, its options, and `horizonY`, where the Omnibar rests. It casts
// `light`, a passing comet's glint on the Omnibar's rim. What it draws by is
// share/scenes/night-sky.json. What does not move is drawn once per size and
// theme into a canvas; what moves is scene-graph items over it, placed from
// the clock.
Item {
    id: root
    objectName: "nightSky"

    // ---- what the host hands it

    // The theme's palette: the sky reads `windowOpaque`, `text` and `accent`.
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
    // A Private window's sky has its lights out: no stars, no comets and no
    // glow on the planet's limb, even while the reader navigates, and the
    // night is lit by the theme's text rather than its accent.
    property bool unlit: false
    // How far the resting Omnibar reaches below where it rests, which the
    // planet's crest keeps clear. The browser's Start page hands it in; a
    // picture of the sky with no Omnibar over it, such as a thumbnail, keeps
    // the reach at the theme's type size.
    property real omnibarReach: root.parameters.planet.omnibarReach

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("night-sky")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options

    // ---- geometry, in logical pixels: the Scene's pixels times its pitch

    readonly property real drawWidth: root.width * root.pitch
    readonly property real drawHeight: root.height * root.pitch
    // Where the Omnibar rests, as it rests on the road's horizon.
    readonly property real horizonY: Math.round(root.drawHeight * root.parameters.rest)

    // ---- what it drew, for the tests

    readonly property int stars: root.unlit ? 0 : root.parameters.stars.count
    readonly property bool limbGlows: !root.unlit
    readonly property bool cometShown: !root.unlit && (root.reducedMotion || root.cometFalls
                                                       && root.cometRun < 1)
    // 0 for a comet, 1 for a shooting star: the index of its kind.
    readonly property int cometKind: {
        if (root.reducedMotion)
            return root.comets.still.kind;
        return root.hash(root.cometSlot + 3) < root.comets.kinds[0].share ? 0 : 1;
    }
    readonly property int streaks: root.unlit ? 0 : Math.ceil(root.falling.count * root.rushShown)

    // ---- palette

    // A night in the theme, as the road's. A light theme's night is drawn
    // from its dark text, so the sky stays dark and the stars stay light.
    readonly property var roles: {
        const night = root.parameters.night;
        const ground = root.dark ? Qt.color(root.colors.windowOpaque) : Colour.mix(root.colors.text,
                                                                                   "black", night.lightThemeGround);
        const light = Qt.color(root.dark ? root.colors.text : root.colors.windowOpaque);
        const deep = Colour.mix(ground, "black", root.dark ? night.deep : night.deepLightTheme);
        const glow = root.unlit ? Colour.mix(light, ground, 0.3) : Qt.color(root.colors.accent);
        return {
            black: Qt.color("black"),
            white: Qt.color("white"),
            ground: ground,
            light: light,
            glow: glow,
            skyTop: deep,
            skyLow: Colour.mix(ground, glow, night.skyLow),
            face: Colour.mix(ground, "black", root.parameters.planet.face)
        };
    }

    // A colour the parameters name: a role, or a mix of two at an amount.
    function colour(name) {
        if (typeof name === "string")
            return root.roles[name];
        return Colour.mix(root.colour(name.mix[0]), root.colour(name.mix[1]), name.mix[2]);
    }

    // A repeatable scatter: the same index always lands in the same place.
    function hash(index) {
        return Colour.scatter(index, root.parameters.scatter);
    }

    // A place in a [from, span] range that `index` scatters to.
    function spread(range, index) {
        return range[0] + range[1] * root.hash(index);
    }

    // ---- the comets
    //
    // In some of the clock's slots a comet or a shooting star falls on a slow
    // diagonal from a place the slot scatters. A reader who asked for less
    // motion gets one comet held part way across.

    readonly property var comets: root.parameters.comets
    // The diagonal comets and streaks fall down, in degrees.
    readonly property real fallAngle: Math.atan(root.comets.slope) * 180 / Math.PI
    readonly property int cometSlot: Math.floor(root.time / root.comets.every)
    // Whether a comet falls in this slot at all.
    readonly property bool cometFalls: root.hash(root.cometSlot) > root.comets.chance
    readonly property var cometMeasure: root.comets.kinds[root.cometKind]
    // How far through its fall the comet is, from 0 to 1.
    readonly property real cometRun: {
        if (root.reducedMotion)
            return root.comets.still.run;
        return (root.time - root.cometSlot * root.comets.every) / root.cometMeasure.lasts;
    }
    // Its head, in logical pixels.
    readonly property point cometHead: {
        const c = root.comets;
        const still = root.reducedMotion;
        const across = still ? c.still.across : root.spread(c.across, root.cometSlot + 1);
        const high = still ? c.still.height : root.spread(c.height, root.cometSlot + 2);
        const travel = root.cometMeasure.travel * root.drawWidth * root.cometRun;
        const x = across * root.drawWidth + travel;
        return Qt.point(x, high * root.drawHeight + travel * c.slope);
    }

    // ---- the planet
    //
    // A circle centred below the page, its radius the page area's width
    // times `radius`. Its top, the glow over its limb included, stands `gap`
    // below the resting Omnibar's bottom edge, so the planet lies wholly under
    // the Omnibar and its curve shows whole. Unlit, it has no glow and stands
    // in the same place.

    readonly property real planetRadius: root.drawWidth * root.parameters.planet.radius
    readonly property real glowReach: root.parameters.planet.glow.reach
    readonly property real crestY: root.horizonY + root.omnibarReach + root.parameters.planet.gap
                                   + root.glowReach
    // The planet's highest point, the glow over its limb included.
    readonly property real planetTop: root.crestY - (root.limbGlows ? root.glowReach : 0)
    readonly property point planetCentre: Qt.point(root.drawWidth / 2, root.crestY
                                                   + root.planetRadius)

    function behindLimb(point) {
        const dx = point.x - root.planetCentre.x;
        const dy = point.y - root.planetCentre.y;
        return dx * dx + dy * dy < root.planetRadius * root.planetRadius;
    }

    // ---- drawing

    // What does not move: the sky and its stars, a few of them brighter,
    // thinning toward the low glow. A star stands on the Scene's own pixels,
    // so it is drawn sharp rather than smeared across two.
    function drawStill(context) {
        const p = root.parameters;
        const w = root.drawWidth;
        const h = root.drawHeight;
        const gradient = context.createLinearGradient(0, 0, 0, root.crestY);
        for (const stop of p.sky)
            gradient.addColorStop(stop[0], Colour.css(root.colour(stop[1]), stop.length > 2
                                                      ? stop[2] : 1));
        context.fillStyle = gradient;
        context.fillRect(0, 0, w, h);

        const st = p.stars;
        for (let index = 0; index < root.stars; ++index) {
            const across = Colour.scatter(index, st.across);
            const high = Colour.scatter(index, st.height);
            const bright = Colour.scatter(index, st.brightness);
            const brightest = bright > st.brightAbove;
            const size = brightest ? st.sizes.bright : bright > st.mediumAbove ? st.sizes.medium :
                                                                                 st.sizes.dim;
            const alpha = brightest ? 1 : (st.alpha.base + st.alpha.range * bright) * (1 - high
                                                                                       * st.alpha.lowFade);
            context.fillStyle = Colour.css(root.roles.light, alpha);
            const pixel = root.pitch;
            context.fillRect(Math.floor(across * w / pixel) * pixel, Math.floor(high * h / pixel)
                             * pixel, size, size);
        }
    }

    // The planet over what moves, so a comet goes out behind its limb: the
    // glow along the limb, the face, and the limb's thin line. Unlit, the
    // planet and its limb stay without the glow.
    function drawPlanet(context) {
        const p = root.parameters.planet;
        const c = root.planetCentre;
        const r = root.planetRadius;
        if (root.limbGlows) {
            const haze = context.createRadialGradient(c.x, c.y, r, c.x, c.y, r + p.glow.reach);
            for (const stop of p.glow.stops)
                haze.addColorStop(stop[0], Colour.css(root.colour(stop[1]), stop[2]));
            context.fillStyle = haze;
            context.fillRect(0, root.crestY - p.glow.reach, root.drawWidth, root.drawHeight);
        }
        context.beginPath();
        context.arc(c.x, c.y, r, 0, Math.PI * 2, false);
        context.fillStyle = Colour.css(root.roles.face);
        context.fill();
        context.lineWidth = p.limb.width;
        context.strokeStyle = root.limbGlows ? Colour.css(root.colour(p.limb.colour)) : Colour.css(
                                                   root.roles.light, p.limb.unlit);
        context.stroke();
    }

    SceneLayer {
        id: still

        readonly property string key: [root.width, root.height, root.roles.ground, root.roles.light,
            root.roles.glow, root.unlit].join("/")

        pitch: root.pitch
        paintWith: root.drawStill
        onKeyChanged: draw()
    }

    // ---- the light it casts
    //
    // A passing comet catches the Omnibar's rim as it crosses near the field:
    // the light is centred on its head, and its ellipse reaches the rim only
    // from close by. With no comet passing, or once it has gone behind the
    // planet, the light stands far above the picture, where it reaches
    // nothing, so there is no glint at rest, under reduced motion or in a
    // Private window.
    readonly property bool cometGlints: root.cometShown && !root.reducedMotion && !root.behindLimb(
                                            root.cometHead)
    readonly property point glintCentre: root.cometGlints ? root.cometHead : root.nowhere
    readonly property point nowhere: Qt.point(root.drawWidth / 2, -1e5)
    readonly property var light: {
        const g = root.parameters.glint;
        return {
            centre: root.glintCentre,
            across: g.across,
            reach: g.reach,
            stops: g.stops.map(function (stop) {
                return {
                    position: stop[0],
                    colour: Colour.withAlpha(root.colour(stop[1]), stop.length > 2 ? stop[2] : 1)
                };
            }),
            bloom: g.bloom
        };
    }

    // ---- the streaks
    //
    // How thick the streaks fall eases with the clock toward what
    // `navigating` asks, so they come in fast on a commit and thin out after
    // the page has painted.

    readonly property var falling: root.parameters.streaks
    property real rush: 0
    // The rush, once it is enough to draw.
    readonly property real rushShown: root.rush > root.falling.from ? root.rush : 0
    property real last: -1

    function move() {
        const st = root.falling;
        if (root.reducedMotion) {
            root.rush = 0;
            return;
        }
        if (root.last < 0)
            root.last = root.time;
        const step = Math.max(0, Math.min(st.longestStep, root.time - root.last));
        root.last = root.time;
        root.rush += (root.navigating - root.rush) * Math.min(1, step * st.ease);
    }

    onTimeChanged: root.move()
    onReducedMotionChanged: root.move()
    Component.onCompleted: root.move()

    // ---- what moves, in logical pixels scaled to the Scene's

    Item {
        id: moving

        width: root.drawWidth
        height: root.drawHeight
        transform: Scale {
            xScale: 1 / root.pitch
            yScale: 1 / root.pitch
        }

        // The streaks, each falling down the comets' diagonal on a short
        // loop of its own from a place its index scatters, more of them the
        // harder the reader navigates.
        Repeater {
            model: root.streaks

            Rectangle {
                required property int index

                readonly property var st: root.falling
                readonly property real lasts: root.spread(st.lasts, index + 101)
                readonly property real phase: root.hash(index + 202)
                readonly property real run: Colour.fraction(root.time / lasts + phase)
                readonly property real travel: st.travel * root.drawWidth * run
                readonly property real headX: root.spread(st.across, index + 303) * root.drawWidth
                                              + travel
                readonly property real headY: root.spread(st.height, index + 404) * root.drawHeight
                                              + travel * root.comets.slope

                x: headX - width
                y: headY - height / 2
                width: root.drawWidth * root.spread(st.length, index + 505) * root.rush
                height: st.width
                transformOrigin: Item.Right
                rotation: root.fallAngle
                antialiasing: true
                opacity: Math.min(1, root.rush * 2) * Math.sin(Math.PI * run)
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: Colour.withAlpha(root.roles.light, 0)
                    }
                    GradientStop {
                        position: 1
                        color: root.colour(root.falling.colour)
                    }
                }
            }
        }

        // The comet: its tail trailing up the diagonal from its head, the
        // two fading in as it appears and out as it goes.
        Item {
            id: comet

            readonly property real tail: root.drawWidth * root.cometMeasure.length
            readonly property real shown: Math.max(0, Math.min(1, root.cometRun * 6, (1 - root.cometRun)
                                                               * 4))

            visible: root.cometShown
            opacity: root.reducedMotion ? 1 : shown

            Rectangle {
                x: root.cometHead.x - width
                y: root.cometHead.y - height / 2
                width: comet.tail
                height: root.cometMeasure.width
                transformOrigin: Item.Right
                rotation: root.fallAngle
                antialiasing: true
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: Colour.withAlpha(root.colour(root.comets.tail), 0)
                    }
                    GradientStop {
                        position: 1
                        color: root.colour(root.comets.head)
                    }
                }
            }

            Rectangle {
                readonly property real size: root.cometMeasure.head

                visible: size > 0
                x: root.cometHead.x - size / 2
                y: root.cometHead.y - size / 2
                width: size
                height: size
                color: root.colour(root.comets.head)
            }
        }
    }

    SceneLayer {
        id: planet

        readonly property string key: [root.width, root.height, root.roles.face, root.roles.light,
            root.roles.glow, root.unlit].join("/")

        pitch: root.pitch
        paintWith: root.drawPlanet
        onKeyChanged: draw()
    }
}
