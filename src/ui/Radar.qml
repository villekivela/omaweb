import QtQuick
import QtQuick.Shapes
import Omaweb
import "SceneColour.mjs" as Colour

// A radar, a Start page Scene after a plan-position display: the whole page
// area is its face, and a phosphor sweep turns clockwise across it from a
// centre behind the Omnibar, over faint range rings and spokes, its afterglow
// trailing behind the beam, with a triangular blip for each open tab in the
// Space on show, which the sweep lights as it passes. From a commit until the page
// paints the sweep spins fast, and the page being opened has a blip of its
// own that brightens.
//
// It keeps NightRoad.qml's contract with SceneHost.qml: it receives `colors`,
// `dark`, `time`, `navigating`, `beat`, `options`, `reducedMotion` and
// `unlit`, and declares `pitch`, `fps`, `glass` with the glass's amounts as
// `crt`, its options, and `horizonY`, where the Omnibar rests. It casts
// `light`, the sweep's light on the Omnibar's rim where the beam leaves it.
// What it draws by is share/scenes/radar.json. The face, and the rings and
// spokes over the sweep, are drawn once per size and theme into canvases, and
// the afterglow once into a canvas the clock turns; the blips are scene-graph
// items over them.
Item {
    id: root
    objectName: "radar"

    // ---- what the host hands it

    // The theme's palette: the radar reads `windowOpaque`, `text` and
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
    // A Private window's radar has its lights out: the sweep and the rings,
    // in the theme's light dimmed toward the ground, with no glow, no blips
    // and no light on the rim, and it holds still, even on commit.
    property bool unlit: false
    // How far the resting Omnibar reaches below where it rests, which the
    // blips keep clear. The browser's Start page hands it in; a picture with
    // no Omnibar over it, such as a thumbnail, keeps the reach at the theme's
    // type size.
    property real omnibarReach: root.parameters.field.omnibarReach
    // The addresses of the tabs open in the Space on show, a blip for each
    // that has a site. The browser's Start page hands them in; null is a
    // picture with no Space behind it, which shows a few blips all the same.
    property var pages: null
    // The address a commit is opening, or "" while none is.
    property string arriving: ""

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("radar")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options

    // ---- what it drew, for the tests

    readonly property bool glowing: !root.unlit

    function blipLevel(index) {
        return root.level(root.blips[index].bearing);
    }

    // ---- palette

    // A night in the theme, as the road's. A light theme's night is drawn
    // from its dark text, so the face stays dark and the sweep stays lit.
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
            face: Colour.mix(deep, glow, night.face)
        };
    }

    // A colour the parameters name: a role, or a mix of two at an amount.
    function colour(name) {
        if (typeof name === "string")
            return root.roles[name];
        return Colour.mix(root.colour(name.mix[0]), root.colour(name.mix[1]), name.mix[2]);
    }

    function wrap(value, span) {
        return ((value % span) + span) % span;
    }

    // ---- geometry, in logical pixels: the Scene's pixels times its pitch

    readonly property real drawWidth: root.width * root.pitch
    readonly property real drawHeight: root.height * root.pitch
    // Where the Omnibar rests, as it rests on the road's horizon, and the
    // sweep's centre, behind the Omnibar's field.
    readonly property real horizonY: Math.round(root.drawHeight * root.parameters.rest)
    readonly property point centre: Qt.point(root.drawWidth / 2, root.horizonY)
    // How far the sweep reaches: to the page area's farthest corner.
    readonly property real reach: Math.hypot(root.drawWidth / 2, Math.max(root.horizonY,
                                                                          root.drawHeight
                                                                          - root.horizonY))
    // The resting Omnibar, which the blips keep clear.
    readonly property rect field: {
        const f = root.parameters.field;
        const width = Math.min(root.drawWidth - 2 * f.margin, f.widest);
        return Qt.rect(root.centre.x - width / 2, root.horizonY - f.above, width, f.above
                       + root.omnibarReach);
    }

    // The point `distance` from the centre at a bearing, in degrees clockwise
    // from straight up.
    function along(bearing, distance) {
        const b = bearing * Math.PI / 180;
        return Qt.point(root.centre.x + distance * Math.sin(b), root.centre.y - distance * Math.cos(
                            b));
    }

    // How far a ray from the centre at a bearing runs before it leaves
    // `area`, which holds the centre.
    function leaves(bearing, area) {
        const b = bearing * Math.PI / 180;
        const dx = Math.sin(b);
        const dy = -Math.cos(b);
        const across = dx > 1e-9 ? (area.x + area.width - root.centre.x) / dx : dx < -1e-9 ? (
                                                                                                 area.x - root.centre.x)
                                                                                             / dx : Infinity;
        const down = dy > 1e-9 ? (area.y + area.height - root.centre.y) / dy : dy < -1e-9 ? (area.y
                                                                                             - root.centre.y)
                                                                                            / dy : Infinity;
        return Math.max(0, Math.min(across, down));
    }

    // ---- blips

    // The site of an address: its host without a leading "www.", or for a
    // page with no host, such as a file, its scheme. A blank page, the one
    // the Start page stands in, has none.
    function site(address) {
        const text = String(address || "");
        if (text === "" || text === "about:blank")
            return "";
        const host = /^[a-z][a-z0-9+.-]*:\/\/(?:[^@\/?#]*@)?([^\/?#:]+)/i.exec(text);
        if (host)
            return host[1].toLowerCase().replace(/^www\./, "");
        const scheme = /^([a-z][a-z0-9+.-]*):/i.exec(text);
        return scheme ? scheme[1].toLowerCase() : "";
    }

    // A repeatable scatter of text from 0 to 1: the same text and seed always
    // give the same value. FNV-1a, so nearby names land far apart.
    function scatterText(text, seed) {
        let hash = (2166136261 ^ seed) >>> 0;
        for (let index = 0; index < text.length; ++index) {
            hash ^= text.charCodeAt(index);
            hash = Math.imul(hash, 16777619) >>> 0;
        }
        return hash / 4294967296;
    }

    // A blip where the scatter of `key` puts it: at a bearing, and `range`
    // [from, to] of the way out along it from a gap clear of the Omnibar to a
    // margin in from the page area's edge, so no blip stands behind the
    // Omnibar or off the page. Where the page area leaves little room beside
    // the field, the gap and the margin each take a third of it at most.
    function placed(site, key) {
        const b = root.parameters.blips;
        const bearing = 360 * root.scatterText(key, 1);
        const out = b.range[0] + (b.range[1] - b.range[0]) * root.scatterText(key, 2);
        const field = root.leaves(bearing, root.field);
        const edge = root.leaves(bearing, Qt.rect(0, 0, root.drawWidth, root.drawHeight));
        const room = Math.max(0, edge - field);
        const inner = field + Math.min(b.clear, room / 3);
        const outer = edge - Math.min(b.margin, room / 3);
        const at = root.along(bearing, inner + out * (outer - inner));
        return {
            site: site,
            bearing: bearing,
            range: out,
            x: at.x,
            y: at.y
        };
    }

    // A blip's key: the site, and for a second tab of one site its count, so
    // each tab has a blip of its own and a tab's stays put while others open
    // and close.
    function blipKey(site, count) {
        return count > 1 ? site + "#" + count : site;
    }

    // The tabs' blips' keys, and how many tabs each site has.
    readonly property var keys: {
        const sites = [];
        const counts = {};
        if (root.pages === null || root.pages === undefined) {
            for (let index = 0; index < root.parameters.blips.sample; ++index)
                sites.push({
                               site: "",
                               key: "sample " + index
                           });
            return {
                sites: sites,
                counts: counts
            };
        }
        for (const page of root.pages) {
            const site = root.site(page);
            if (site === "")
                continue;
            counts[site] = (counts[site] || 0) + 1;
            sites.push({
                           site: site,
                           key: root.blipKey(site, counts[site])
                       });
        }
        return {
            sites: sites,
            counts: counts
        };
    }

    readonly property var blips: root.unlit ? [] : root.keys.sites.map(function (entry) {
        return root.placed(entry.site, entry.key);
    })

    // The page a commit is opening, where its tab's blip will stand once it
    // is open, or null.
    readonly property var arrivingBlip: {
        const site = root.site(root.arriving);
        if (root.unlit || site === "")
            return null;
        return root.placed(site, root.blipKey(site, (root.keys.counts[site] || 0) + 1));
    }

    // How lit a blip at `bearing` is: as bright as the beam as it is swept,
    // fading with the afterglow behind it to the floor. A still radar's blips
    // are all lit.
    function level(bearing) {
        if (root.reducedMotion)
            return 1;
        const s = root.parameters.sweep;
        const floor = root.parameters.blips.floor;
        const glow = 360 * s.afterglow / s.turn;
        const behind = root.wrap(root.sweep - bearing, 360);
        const lit = behind < glow ? Math.pow(1 - behind / glow, s.fade) : 0;
        return floor + (1 - floor) * lit;
    }

    // ---- the light it casts
    //
    // The sweep's light on the Omnibar's rim, centred where the beam leaves
    // the field, so it rides round the rim as the sweep turns behind it.
    readonly property var lightStops: root.parameters.light.stops.map(function (stop) {
        return {
            position: stop[0],
            colour: Colour.withAlpha(root.colour(stop[1]), stop[2])
        };
    })
    readonly property var light: {
        if (root.unlit)
            return null;
        const l = root.parameters.light;
        return {
            centre: root.along(root.sweep, root.leaves(root.sweep, root.field)),
            across: l.across,
            reach: l.reach,
            stops: root.lightStops,
            bloom: {
                opacity: l.bloom.opacity,
                width: l.bloom.width,
                blur: l.bloom.blur
            }
        };
    }

    // ---- motion
    //
    // The sweep turns at a speed that eases toward what `navigating` asks,
    // and the arriving blip brightens while a page is being opened. A still
    // or unlit radar holds its sweep at one bearing.

    // The beam's bearing, in degrees clockwise from straight up, counted on
    // past a turn.
    property real sweep: root.parameters.sweep.still
    // How many times its pace the sweep turns.
    property real speed: 1
    // How bright the arriving blip is, 0 to 1.
    property real arrival: 0
    property real last: -1

    function move() {
        const s = root.parameters.sweep;
        if (root.reducedMotion || root.unlit) {
            root.sweep = s.still;
            root.speed = 1;
            root.arrival = 0;
            return;
        }
        if (root.last < 0)
            root.last = root.time;
        const step = Math.max(0, Math.min(s.longestStep, root.time - root.last));
        root.last = root.time;
        root.speed += (1 + (s.spin - 1) * root.navigating - root.speed) * Math.min(1, step
                                                                                   * s.ease);
        root.sweep += step * 360 / s.turn * root.speed;
        // Once the page paints, the tab's own blip stands where the arriving
        // one did and takes over from it.
        const rise = root.parameters.blips.arrive.rise;
        root.arrival = root.navigating > 0 && root.arrivingBlip !== null ? Math.min(1, root.arrival
                                                                                    + step / rise) :
                                                                           0;
    }

    onTimeChanged: root.move()
    onReducedMotionChanged: root.move()
    onUnlitChanged: root.move()
    // Each page being opened brightens from dark.
    onArrivingChanged: root.arrival = 0
    Component.onCompleted: root.move()

    // ---- drawing

    // A path drawn as a line of the radar: thin, in `colour` at `alpha`, with
    // a phosphor's glow around it while the lights are on. The width is in
    // logical pixels, scaled to the Scene's with the canvas; the blur is not
    // scaled, so it is in the Scene's pixels already.
    function stroke(context, colour, alpha) {
        const l = root.parameters.lines;
        context.lineWidth = l.width * root.pitch;
        context.strokeStyle = Colour.css(colour, alpha);
        context.shadowOffsetX = 0;
        context.shadowOffsetY = 0;
        context.shadowBlur = root.glowing ? l.glow.blur : 0;
        context.shadowColor = Colour.css(colour, root.glowing ? l.glow.alpha * alpha : 0);
        context.stroke();
    }

    // What does not move under the sweep: the face, the whole page area.
    function drawStill(context) {
        context.fillStyle = Colour.css(root.colour(root.parameters.grid.face));
        context.fillRect(0, 0, root.drawWidth, root.drawHeight);
    }

    // What does not move over the sweep, so the afterglow does not hide it:
    // range rings a set distance apart and spokes from the centre, out past
    // the page area's edges.
    function drawMarks(context) {
        const grid = root.parameters.grid;
        const c = root.centre;
        const colour = root.colour(grid.colour);
        context.beginPath();
        for (let out = grid.ringEvery; out <= root.reach; out += grid.ringEvery) {
            context.moveTo(c.x + out, c.y);
            context.arc(c.x, c.y, out, 0, 2 * Math.PI, false);
        }
        root.stroke(context, colour, grid.alpha);
        context.beginPath();
        for (let bearing = 0; bearing < 360; bearing += grid.spokeEvery) {
            const to = root.along(bearing, root.reach);
            context.moveTo(c.x, c.y);
            context.lineTo(to.x, to.y);
        }
        root.stroke(context, colour, grid.spokeAlpha);
    }

    // The afterglow, with the beam pointing straight up and the swept area
    // trailing anticlockwise behind it: wedges from the beam back, each a
    // little shorter and brighter than the one under it, so the glow is
    // brightest at the beam and fades by the power `fade`. Each is opaque, the
    // face mixed toward the sweep's colour: a canvas keeps eight bits a
    // channel, so a stack of faint wedges would lose the colour to rounding
    // and come out grey. The centre is the canvas's middle.
    function drawSweep(context) {
        const s = root.parameters.sweep;
        const r = root.reach;
        const colour = root.colour(s.colour);
        const face = root.colour(root.parameters.grid.face);
        const glow = 2 * Math.PI * s.afterglow / s.turn;
        const up = -Math.PI / 2;
        for (let slice = 0; slice < s.slices; ++slice) {
            const back = glow * (1 - Math.pow(slice / s.slices, 1 / s.fade));
            context.beginPath();
            context.moveTo(r, r);
            context.arc(r, r, r, up - back, up, false);
            context.closePath();
            context.fillStyle = Colour.css(Colour.mix(face, colour, s.alpha * (slice + 1)
                                                      / s.slices));
            context.fill();
        }
        context.beginPath();
        context.moveTo(r, r);
        context.lineTo(r, 0);
        root.stroke(context, colour, s.beam);
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

    // The afterglow, drawn once and turned with the beam.
    Item {
        objectName: "radarSweep"
        x: (root.centre.x - root.reach) / root.pitch
        y: (root.centre.y - root.reach) / root.pitch
        width: 2 * root.reach / root.pitch
        height: width
        rotation: root.sweep % 360

        SceneLayer {
            id: sweepLayer

            pitch: root.pitch
            paintWith: root.drawSweep
            Component.onCompleted: draw()

            Connections {
                target: root
                function onKeyChanged() {
                    sweepLayer.draw();
                }
            }
        }
    }

    SceneLayer {
        id: marks

        pitch: root.pitch
        paintWith: root.drawMarks
        Component.onCompleted: draw()

        Connections {
            target: root
            function onKeyChanged() {
                marks.draw();
            }
        }
    }

    // A blip's mark: a triangle pointing up, `side` across, centred on its
    // item's origin.
    component Triangle: Shape {
        id: triangle

        property real side: 0
        property color fill

        x: -side / 2
        y: -side * Math.sqrt(3) / 3
        width: side
        height: side * Math.sqrt(3) / 2

        ShapePath {
            fillColor: triangle.fill
            strokeWidth: -1
            startX: triangle.side / 2
            startY: 0

            PathLine {
                x: triangle.side
                y: triangle.height
            }
            PathLine {
                x: 0
                y: triangle.height
            }
            PathLine {
                x: triangle.side / 2
                y: 0
            }
        }
    }

    // A blip: its mark, with a phosphor's glow round it `halo` times as wide
    // at `haloAlpha` of its colour.
    component Blip: Item {
        id: blip

        property real size: 0
        property color colour
        property real halo: 1
        property real haloAlpha: 0

        Triangle {
            visible: blip.haloAlpha > 0
            side: blip.size * blip.halo
            fill: Colour.withAlpha(blip.colour, blip.haloAlpha)
        }

        Triangle {
            side: blip.size
            fill: blip.colour
        }
    }

    // The blips, in logical pixels scaled to the Scene's.
    Item {
        width: root.drawWidth
        height: root.drawHeight
        transform: Scale {
            xScale: 1 / root.pitch
            yScale: 1 / root.pitch
        }

        Repeater {
            model: root.blips

            Blip {
                required property var modelData

                readonly property var blips: root.parameters.blips

                x: modelData.x
                y: modelData.y
                opacity: root.level(modelData.bearing)
                size: blips.size * root.pitch
                colour: root.colour(blips.colour)
                halo: blips.halo
                haloAlpha: root.glowing ? blips.haloAlpha : 0
            }
        }

        // The page being opened, brightening until it paints.
        Blip {
            objectName: "arrivingBlip"

            readonly property var arrive: root.parameters.blips.arrive

            visible: root.arrivingBlip !== null && root.arrival > 0
            x: root.arrivingBlip ? root.arrivingBlip.x : 0
            y: root.arrivingBlip ? root.arrivingBlip.y : 0
            opacity: root.arrival
            size: root.parameters.blips.size * root.pitch
            colour: root.colour(arrive.colour)
            halo: arrive.halo
            haloAlpha: arrive.haloAlpha
        }
    }
}
