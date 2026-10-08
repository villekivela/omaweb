import QtQuick
import QtQuick.Shapes
import Omaweb
import "SceneColour.mjs" as Colour

// The night sky, the Start page's second Scene, after Netscape Navigator's
// throbber: a still star field over a dark planet standing low below the
// Omnibar, its limb's glow pulsing, comets and shooting stars falling from the
// upper right and going out behind the limb, a larger comet now and then
// falling in front of the planet and lighting it up, and streaks falling thick
// and fast while the reader navigates. There is no mark: the Omnibar is the
// one thing in the middle.
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
    // How wide the resting Omnibar stands, centred. Without one handed in,
    // the width the Omnibar takes in a page area this wide.
    property real omnibarWidth: Math.min(root.parameters.omnibar.widest, root.drawWidth
                                         - root.parameters.omnibar.inset)

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
    readonly property bool cometShown: root.fallen.some(function (comet) {
        return comet !== null;
    })
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
    // In some of the clock's slots a group of comets and shooting stars
    // falls, one first and up to `most` in the sky at once, each from the
    // upper right toward the lower left on the same diagonal. Each is placed
    // by where it goes out behind the planet's limb: it appears `run` up the
    // diagonal from there and falls until its tail has gone out too, so none
    // stops short in the sky. A slot's group has gone before the next slot
    // begins. A reader who asked for less motion gets one comet held part
    // way down its run.

    readonly property var comets: root.parameters.comets
    // The diagonal comets and streaks fall down, in degrees.
    readonly property real fallAngle: Math.atan(root.comets.slope) * 180 / Math.PI
    // The way down the diagonal, as a unit step.
    readonly property point fallStep: {
        const length = Math.hypot(1, root.comets.slope);
        return Qt.point(-1 / length, root.comets.slope / length);
    }
    readonly property int cometSlot: Math.floor(root.time / root.comets.every)

    // Where on the limb a comet `across` of the width along goes out.
    function limbAt(across) {
        const x = across * root.drawWidth;
        const dx = x - root.planetCentre.x;
        return Qt.point(x, root.planetCentre.y - Math.sqrt(Math.max(0, root.planetRadius
                                                                    * root.planetRadius - dx
                                                                    * dx)));
    }

    // A comet's head `fallen` of the width down its diagonal, which starts
    // `run` of the width up from where it goes out.
    function headOf(out, run, fallen) {
        const back = (run - fallen) * root.drawWidth;
        return Qt.point(out.x - root.fallStep.x * back, out.y - root.fallStep.y * back);
    }

    // A comet in the sky, as `fallen` lists it.
    function cometOf(slot, index, kind, head, shown, steered) {
        return {
            slot: slot,
            index: index,
            kind: kind,
            head: head,
            shown: shown,
            steered: steered
        };
    }

    // The Omnibar's resting field: `omnibarWidth` wide, centred, from
    // `above` over where it rests to `omnibarReach` below.
    readonly property rect field: Qt.rect((root.drawWidth - root.omnibarWidth) / 2, root.horizonY
                                          - root.parameters.omnibar.above, root.omnibarWidth,
                                          root.parameters.omnibar.above + root.omnibarReach)

    // How far out in a comet's light at `head` the Omnibar's rim stands: 0
    // at the light's centre, 1 at the edge of its ellipse.
    function rimDistance(head) {
        const g = root.parameters.glint;
        const f = root.field;
        const dx = Math.max(f.x - head.x, 0, head.x - f.x - f.width);
        const dy = Math.max(f.y - head.y, 0, head.y - f.y - f.height);
        return Math.hypot(dx / (f.width * g.across), dy / g.reach);
    }

    // Where a comet of `measure` steered past the field's lower right corner
    // goes out, and how far up the diagonal it appears: its line passes the
    // corner with its glow `clear` off the field and a Scene pixel more, and
    // it appears `beyond` of the width before it gets there. Null where that
    // line misses the planet.
    function steerPast(measure, beyond) {
        const d = root.fallStep;
        const reachOff = root.passingDistance(measure.halo);
        const f = root.field;
        // Off the corner, square to the line, below and to the right.
        const foot = Qt.point(f.x + f.width + d.y * reachOff, f.y + f.height - d.x * reachOff);
        const c = root.planetCentre;
        const b = d.x * (foot.x - c.x) + d.y * (foot.y - c.y);
        const rest = (foot.x - c.x) * (foot.x - c.x) + (foot.y - c.y) * (foot.y - c.y)
              - root.planetRadius * root.planetRadius;
        if (b * b - rest < 0)
            return null;
        const along = -b - Math.sqrt(b * b - rest);
        if (along <= 0)
            return null;
        return {
            out: Qt.point(foot.x + d.x * along, foot.y + d.y * along),
            run: along / root.drawWidth + beyond
        };
    }

    // The comet `index` of the group falling in `slot`, or null where it is
    // not in the sky at `time`. Each slot scatters from 32 indices of its
    // own: 0 whether it falls, 1 how many fall together, then four each, one
    // per comet, for when it starts (from 2), its kind (8), its run (12),
    // where it goes out (16), whether it is steered past the Omnibar (24)
    // and how far before it it appears (28), so `most` can be four at most.
    // The comet in front of the planet takes 20 for its angle.
    function cometAt(slot, index, time) {
        const c = root.comets;
        const seed = slot * 32;
        if (root.frontSlot(slot) || root.hash(seed) <= c.chance)
            return null;
        const pick = root.hash(seed + 1);
        const together = c.together.findIndex(function (share) {
            return pick < share;
        }) + 1;
        if (index >= together)
            return null;
        let start = slot * c.every;
        for (let before = 1; before <= index; ++before)
            start += root.spread(c.apart, seed + 2 + before);
        // One that started before the sky was last shown stays out of
        // sight rather than popping in part way across.
        if (start < root.revealedAt)
            return null;
        const kind = root.hash(seed + 8 + index) < c.kinds[0].share ? 0 : 1;
        const measure = c.kinds[kind];
        let run = root.spread(measure.run, seed + 12 + index);
        let out = root.limbAt(root.spread(c.out, seed + 16 + index));
        // Some are steered past the Omnibar, where their light catches its
        // rim, while their fall still ends before the slot does.
        let steered = root.hash(seed + 24 + index) < c.steer.share;
        if (steered) {
            const path = root.steerPast(measure, root.spread(c.steer.beyond, seed + 28 + index));
            const lasts = path !== null ? (path.run + measure.length) / measure.speed : Infinity;
            steered = start - slot * c.every + lasts < c.every;
            if (steered) {
                run = path.run;
                out = path.out;
            }
        }
        const fallen = (time - start) * measure.speed;
        if (fallen < 0 || fallen > run + measure.length)
            return null;
        const shown = Math.min(1, (time - start) / c.fadeIn);
        return root.cometOf(slot, index, kind, root.headOf(out, run, fallen), shown, steered);
    }

    // The comets in the sky now, one place for each of the most there can be,
    // null where none falls.
    readonly property var fallen: {
        const c = root.comets;
        const still = root.headOf(root.limbAt(c.still.out), c.still.run, c.still.run * c.still.at);
        const places = [];
        for (let index = 0; index < c.most; ++index) {
            if (root.unlit)
                places.push(null);
            else if (root.reducedMotion)
                places.push(index > 0 ? null : root.cometOf(-1, 0, c.still.kind, still, 1, false));
            else
                places.push(root.cometAt(root.cometSlot, index, root.time));
        }
        return places;
    }

    // ---- the comet in front of the planet
    //
    // In one of every `every` slots, with no other comet in the sky, a large
    // comet comes in from the right edge on a line of its own that passes
    // the resting Omnibar's lower right corner, catching its rim, then
    // crosses the planet's face in front of it, and leaves through the
    // bottom edge `lasts` seconds later, lighting the planet up as it
    // passes. Its trail fades over `fades` seconds once it has gone.
    // It never falls under reduced motion, in a Private window, or while a
    // commit's streaks fall.

    readonly property var front: root.parameters.front

    function frontSlot(slot) {
        return slot % root.front.every === root.front.at;
    }

    // The last slot the streaks fell in. Its front comet goes out as they
    // come and does not come back part way across once they have gone.
    property int streakSlot: -1

    // Seconds since the front comet came in, or -1 with none in the sky.
    readonly property real frontRun: {
        const f = root.front;
        if (root.reducedMotion || root.unlit || root.streakSlot === root.cometSlot || !root.frontSlot(
                    root.cometSlot))
            return -1;
        const start = root.cometSlot * root.comets.every + f.after;
        const run = root.time - start;
        return start >= root.revealedAt && run >= 0 && run < f.lasts + f.fades ? run : -1;
    }
    readonly property bool frontShown: root.frontRun >= 0
    // How far square to its line a comet with a glow `halo` across passes
    // the Omnibar's lower right corner: its glow `steer.clear` off the
    // field, and a Scene pixel more for the glow drawn in whole ones. That
    // corner is where a line falling from the upper right comes closest.
    function passingDistance(halo) {
        return root.comets.steer.clear + halo / 2 + root.pitch;
    }

    // This crossing's line: its angle below the horizontal, in degrees, and
    // where it comes in at the right edge. The line passes the Omnibar's
    // lower right corner at `passingDistance`, close enough for its light to
    // catch the rim. Its angle is scattered over `angles` [from, span], from
    // no shallower than will still leave through the bottom edge at least
    // `exit` of the width from the left, which in a window taller than it
    // is wide is steeper, up to `steepest`. Where even that would leave
    // through the left edge, the comet comes in lower, missing the rim.
    readonly property var frontPath: {
        const f = root.front;
        const x = root.drawWidth + f.halo / 2;
        const bottom = root.drawHeight + f.halo / 2;
        const corner = Qt.point(root.field.x + root.field.width, root.field.y + root.field.height);
        const distance = root.passingDistance(f.halo);
        const leaves = function (degrees) {
            const a = degrees * Math.PI / 180;
            const atCorner = corner.y + distance / Math.cos(a);
            return corner.x - (bottom - atCorner) / Math.tan(a) >= f.exit * root.drawWidth;
        };
        let least = f.angles[0];
        while (least < f.steepest && !leaves(least))
            least += 1;
        const span = Math.max(0, Math.min(f.angles[1], f.steepest - least));
        const degrees = least + span * root.hash(root.cometSlot * 32 + 20);
        const a = degrees * Math.PI / 180;
        const passing = corner.y + distance / Math.cos(a) - (x - corner.x) * Math.tan(a);
        const lowest = bottom - (x - f.exit * root.drawWidth) * Math.tan(a);
        return {
            degrees: degrees,
            from: Qt.point(x, Math.max(passing, lowest))
        };
    }
    readonly property real frontAngle: root.frontPath.degrees
    readonly property point frontHead: {
        const f = root.front;
        const a = root.frontAngle * Math.PI / 180;
        const from = root.frontPath.from;
        const across = (root.drawHeight + f.halo / 2 - from.y) / Math.sin(a);
        const fallen = across * Math.max(0, root.frontRun) / f.lasts;
        return Qt.point(from.x - Math.cos(a) * fallen, from.y + Math.sin(a) * fallen);
    }
    // How much of its trail is left: whole while it crosses, fading once it
    // has gone.
    readonly property real frontTrail: {
        const f = root.front;
        if (!root.frontShown)
            return 0;
        return Math.min(1, 1 - (root.frontRun - f.lasts) / f.fades);
    }
    // How far the planet has lit up: most with the comet half way down from
    // the crest, none once it has left through the bottom.
    readonly property real flare: {
        if (!root.frontShown)
            return 0;
        const down = (root.frontHead.y - root.crestY) / (root.drawHeight - root.crestY);
        return down > 0 && down < 1 ? root.front.flare.peak * Math.sin(Math.PI * down) : 0;
    }

    // ---- the planet
    //
    // A circle centred below the page, its radius the page area's width
    // times `radius`. Its top, the glow over its limb included, stands
    // `share` of the way from the resting Omnibar's bottom edge to the
    // Scene's, so it falls with the window's height. Where that would take
    // the limb's ends lower than `edgeClear` above the bottom, off the page or
    // onto the lower right corner the Shortcut sheet's cue holds, it stands
    // higher, but never closer to the Omnibar than `gap`. Unlit, it has no
    // glow and stands in the same place.

    readonly property real planetRadius: root.drawWidth * root.parameters.planet.radius
    readonly property real glowReach: root.parameters.planet.glow.reach
    readonly property real crestY: {
        const p = root.parameters.planet;
        const omnibarBottom = root.horizonY + root.omnibarReach;
        const r = root.planetRadius;
        const half = root.drawWidth / 2;
        const sag = r - Math.sqrt(r * r - half * half);
        const low = omnibarBottom + p.share * (root.drawHeight - omnibarBottom) + root.glowReach;
        const whole = root.drawHeight - p.edgeClear - sag;
        return Math.max(omnibarBottom + p.gap + root.glowReach, Math.min(low, whole));
    }
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

    // A haze along the planet's limb, `reach` out, in gradient `stops`.
    function drawHaze(context, stops) {
        const reach = root.parameters.planet.glow.reach;
        const c = root.planetCentre;
        const r = root.planetRadius;
        const haze = context.createRadialGradient(c.x, c.y, r, c.x, c.y, r + reach);
        for (const stop of stops)
            haze.addColorStop(stop[0], Colour.css(root.colour(stop[1]), stop[2]));
        context.fillStyle = haze;
        context.fillRect(0, root.crestY - reach, root.drawWidth, root.drawHeight);
    }

    // The planet's disc, for a fill and the limb's line.
    function tracePlanet(context) {
        const c = root.planetCentre;
        context.beginPath();
        context.arc(c.x, c.y, root.planetRadius, 0, Math.PI * 2, false);
    }

    // The limb's thin line, its glow's colour lit and the light's unlit.
    function strokeLimb(context) {
        const limb = root.parameters.planet.limb;
        context.lineWidth = limb.width;
        context.strokeStyle = root.limbGlows ? Colour.css(root.colour(limb.colour)) : Colour.css(
                                                   root.roles.light, limb.unlit);
        context.stroke();
    }

    // The glow along the planet's limb, over what moves, so a comet goes out
    // behind it.
    function drawGlow(context) {
        root.drawHaze(context, root.parameters.planet.glow.stops);
    }

    // The planet lit up by the comet passing in front of it: its face
    // brighter, and the glow along its limb.
    function drawFlare(context) {
        const f = root.front.flare;
        root.drawHaze(context, f.glow);
        root.tracePlanet(context);
        context.fillStyle = Colour.css(root.colour(f.face));
        context.fill();
        root.strokeLimb(context);
    }

    // The planet over the glow: the face, and the limb's thin line. Unlit,
    // the planet and its limb stay without the glow.
    function drawPlanet(context) {
        root.tracePlanet(context);
        context.fillStyle = Colour.css(root.roles.face);
        context.fill();
        root.strokeLimb(context);
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
    // the light is centred on the head of the comet above the limb nearest
    // where the Omnibar rests, and its ellipse reaches the rim only
    // from close by. With no comet passing, or once it has gone behind the
    // planet, the light stands far above the picture, where it reaches
    // nothing, so there is no glint at rest, under reduced motion or in a
    // Private window.
    // Of the comets above the limb, the one nearest where the Omnibar rests.
    readonly property var glinting: {
        if (root.reducedMotion)
            return null;
        let nearest = null;
        let distance = Infinity;
        for (const comet of root.fallen) {
            if (comet === null || root.behindLimb(comet.head))
                continue;
            const d = root.rimDistance(comet.head);
            if (d < distance) {
                nearest = comet;
                distance = d;
            }
        }
        return nearest;
    }
    // The comet in front of the planet glints while it crosses, with no
    // other comet in the sky.
    readonly property bool frontGlints: root.frontShown && root.frontRun < root.front.lasts
    readonly property point glintCentre: root.frontGlints ? root.frontHead : root.glinting !== null
                                                            ? root.glinting.head : root.nowhere
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

    // The clock's time the sky was last shown afresh: on its first frame,
    // after the clock jumped, after reduced motion was turned on or off, and
    // after it changed size or Omnibar. No comet that started before then is
    // drawn.
    property real revealedAt: 0

    function reveal() {
        root.revealedAt = root.time;
    }

    function move() {
        const st = root.falling;
        // The host's clock steps at most `longestStep` a tick, and a stalled
        // tick's sum can come out a hair over it, so only a step clearly past
        // it is a jump.
        if (root.last < 0 || root.time < root.last || root.time - root.last > st.longestStep
                + 0.001)

            root.reveal();
        const step = root.last < 0 ? 0 : Math.max(0, Math.min(st.longestStep, root.time
                                                              - root.last));
        root.last = root.time;
        if (root.reducedMotion) {
            root.rush = 0;
            return;
        }
        root.rush += (root.navigating - root.rush) * Math.min(1, step * st.ease);
        if (root.rushShown > 0)
            root.streakSlot = root.cometSlot;
    }

    onTimeChanged: root.move()
    onDrawWidthChanged: root.reveal()
    onDrawHeightChanged: root.reveal()
    onOmnibarReachChanged: root.reveal()
    onOmnibarWidthChanged: root.reveal()
    onReducedMotionChanged: {
        root.reveal();
        root.move();
    }
    Component.onCompleted: root.move()

    // ---- what moves, in logical pixels scaled to the Scene's

    // A comet as Navigator's throbber drew one: a white head in a glow of
    // the palette, and a tail that tapers and fades into the sky up the
    // diagonal from it.
    component Comet: Item {
        id: comet

        property point head
        property real length
        property real tailWidth
        property real headSize
        property real haloSize
        // How far below the horizontal it falls, in degrees.
        property real angle: root.fallAngle

        Shape {
            id: tail
            objectName: "cometTail"

            readonly property real headWidth: comet.tailWidth
            readonly property real endWidth: 0

            x: comet.head.x
            y: comet.head.y
            transformOrigin: Item.TopLeft
            rotation: -comet.angle
            antialiasing: true

            ShapePath {
                strokeWidth: -1
                strokeColor: "transparent"
                fillGradient: LinearGradient {
                    x1: 0
                    y1: 0
                    x2: comet.length
                    y2: 0
                    GradientStop {
                        position: 0
                        color: root.colour(root.comets.head)
                    }
                    GradientStop {
                        position: 0.15
                        color: root.colour(root.comets.halo)
                    }
                    GradientStop {
                        position: 1
                        color: Colour.withAlpha(root.colour(root.comets.halo), 0)
                    }
                }
                startX: 0
                startY: -tail.headWidth / 2
                PathLine {
                    x: comet.length
                    y: -tail.endWidth / 2
                }
                PathLine {
                    x: comet.length
                    y: tail.endWidth / 2
                }
                PathLine {
                    x: 0
                    y: tail.headWidth / 2
                }
                PathLine {
                    x: 0
                    y: -tail.headWidth / 2
                }
            }
        }

        Rectangle {
            objectName: "cometHalo"
            x: comet.head.x - width / 2
            y: comet.head.y - height / 2
            width: comet.haloSize
            height: comet.haloSize
            radius: width / 2
            opacity: 0.5
            antialiasing: true
            color: root.colour(root.comets.halo)
        }

        Rectangle {
            objectName: "cometHead"
            x: comet.head.x - width / 2
            y: comet.head.y - height / 2
            width: comet.headSize
            height: comet.headSize
            radius: width / 2
            antialiasing: true
            color: root.colour(root.comets.head)
        }
    }

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
                                              - travel
                readonly property real headY: root.spread(st.height, index + 404) * root.drawHeight
                                              + travel * root.comets.slope

                x: headX
                y: headY - height / 2
                width: root.drawWidth * root.spread(st.length, index + 505) * root.rush
                height: st.width
                transformOrigin: Item.Left
                rotation: -root.fallAngle
                antialiasing: true
                opacity: Math.min(1, root.rush * 2) * Math.sin(Math.PI * run)
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop {
                        position: 0
                        color: root.colour(root.falling.colour)
                    }
                    GradientStop {
                        position: 1
                        color: Colour.withAlpha(root.roles.light, 0)
                    }
                }
            }
        }

        // The comets, their tails trailing up the diagonal from their heads,
        // each fading in as it appears.
        Repeater {
            model: root.comets.most

            Comet {
                required property int index

                readonly property var comet: root.fallen[index]
                readonly property var measure: root.comets.kinds[comet !== null ? comet.kind : 0]

                visible: comet !== null
                opacity: comet !== null ? comet.shown : 0
                head: comet !== null ? comet.head : Qt.point(0, 0)
                length: root.drawWidth * measure.length
                tailWidth: measure.width
                headSize: measure.head
                haloSize: measure.halo
            }
        }
    }

    // The limb's glow pulses on a slow sine, down to `low` of its peak and
    // back every `every` seconds, its reach the same throughout. Reduced
    // motion holds it at its peak; unlit, there is none.
    readonly property real glowOpacity: {
        if (!root.limbGlows)
            return 0;
        if (root.reducedMotion)
            return 1;
        const pulse = root.parameters.planet.glow.pulse;
        return pulse.low + (1 - pulse.low) * (1 + Math.cos(2 * Math.PI * root.time / pulse.every))
                / 2;
    }

    SceneLayer {
        id: glow
        objectName: "planetGlow"

        readonly property string key: [root.width, root.height, root.roles.glow, root.unlit,
            root.omnibarReach].join("/")

        visible: root.limbGlows
        opacity: root.glowOpacity
        pitch: root.pitch
        paintWith: root.drawGlow
        onKeyChanged: draw()
    }

    SceneLayer {
        id: planet

        readonly property string key: [root.width, root.height, root.roles.face, root.roles.light,
            root.roles.glow, root.unlit, root.omnibarReach].join("/")

        pitch: root.pitch
        paintWith: root.drawPlanet
        onKeyChanged: draw()
    }

    SceneLayer {
        id: flare
        objectName: "planetFlare"

        readonly property string key: [root.width, root.height, root.roles.face, root.roles.glow,
            root.unlit, root.omnibarReach].join("/")

        visible: root.flare > 0
        opacity: root.flare
        pitch: root.pitch
        paintWith: root.drawFlare
        onKeyChanged: draw()
    }

    Item {
        width: root.drawWidth
        height: root.drawHeight
        transform: Scale {
            xScale: 1 / root.pitch
            yScale: 1 / root.pitch
        }

        Comet {
            objectName: "frontComet"
            visible: root.frontShown
            opacity: root.frontTrail
            head: root.frontHead
            length: root.drawWidth * root.front.length
            tailWidth: root.front.width
            headSize: root.front.head
            haloSize: root.front.halo
            angle: root.frontAngle
        }
    }
}
