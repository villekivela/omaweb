import QtQuick
import QtQuick.Shapes
import Omaweb

// The CRT road, the Scene the Start page stands on: a night drive toward a
// banded sun over a desert, its ground lit by the sun's glow, layered ridges,
// a wide road with only its dashed centre line, now and then a saguaro, a rock
// or a lone sign at the roadside, and a rare shooting star, as the website
// draws it.
//
// A Scene is a drawing and nothing else. SceneHost.qml sizes it in its own
// pixels, keeps its clock, and hands it what it may know: `colors`, `dark`,
// `time`, `navigating`, `beat`, `options` and `reducedMotion`, and in this
// browser `unlit`. It declares `pitch`, `fps`, `glass` with the glass's
// amounts as `crt`, and its options, and the host shows it through the glass
// it declares. It also casts `light`, the sun's light on the Omnibar's rim,
// which the host lays on the page. What it draws by, every amount, colour
// recipe and timing, is share/scenes/crt-road.json, the file the website's
// road (website/crt-road.js) reads too; this is only how it draws. What does
// not move is drawn once per size, theme and option into a canvas. What moves
// is scene-graph items over it, placed from the clock rather than painted
// again on every tick.
Item {
    id: root
    objectName: "nightRoad"

    // ---- what the host hands it

    // The theme's palette: the road reads `windowOpaque`, `text` and `accent`.
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
    // A Private window's road has its lights off: the sun has set, leaving
    // its glow on the horizon, and there are no stars, no centre line and no
    // shooting star, and the night is lit by the theme's text rather than its
    // accent. The browser's input alone;
    // the website has no Private window.
    property bool unlit: false

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("crt-road")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options

    // ---- what it drew, for the tests

    readonly property bool sunShown: !root.unlit
    readonly property int stars: root.unlit ? 0 : root.parameters.stars.count
    readonly property int centreMarks: root.unlit ? 0 : root.parameters.centreLine.marks
    readonly property int roadsideMarks: root.parameters.roadside.order.length

    // ---- palette

    function mixColour(from, to, amount) {
        const a = Qt.color(from);
        const b = Qt.color(to);
        return Qt.rgba(a.r + (b.r - a.r) * amount, a.g + (b.g - a.g) * amount, a.b + (b.b - a.b)
                       * amount, 1);
    }

    // A night in the theme. A light theme's night is drawn from its dark
    // text, so the ground stays dark and the lit lines stay light.
    readonly property var roles: {
        const night = root.parameters.night;
        const ground = root.dark ? Qt.color(root.colors.windowOpaque) : root.mixColour(root.colors.text,
                                                                                       "black", night.lightThemeGround);
        const light = Qt.color(root.dark ? root.colors.text : root.colors.windowOpaque);
        const deep = root.mixColour(ground, "black", root.dark ? night.deep : night.deepLightTheme);
        const glow = root.unlit ? root.mixColour(light, ground, 0.3) : Qt.color(root.colors.accent);
        return {
            black: Qt.color("black"),
            white: Qt.color("white"),
            ground: ground,
            light: light,
            glow: glow,
            skyTop: deep,
            skyLow: root.mixColour(ground, glow, night.skyLow),
            groundNear: root.mixColour(deep, "black", night.groundNear),
            sunTop: root.mixColour(light, "white", night.sunTop),
            sunLow: root.mixColour(glow, light, night.sunLow)
        };
    }

    // A colour the parameters name: a role, or a mix of two at an amount.
    function colour(name) {
        if (typeof name === "string")
            return root.roles[name];
        return root.mixColour(root.colour(name.mix[0]), root.colour(name.mix[1]), name.mix[2]);
    }

    // A colour at an alpha, for an item to draw.
    function withAlpha(colour, alpha) {
        const c = Qt.color(colour);
        return Qt.rgba(c.r, c.g, c.b, alpha);
    }

    // A colour as the canvas takes it, at an alpha.
    function css(colour, alpha) {
        const c = Qt.color(colour);
        return "rgba(" + Math.round(c.r * 255) + ", " + Math.round(c.g * 255) + ", " + Math.round(
                    c.b * 255) + ", " + (alpha === undefined ? 1 : alpha) + ")";
    }

    // A gradient's stops, from [position, colour] or [position, colour,
    // alpha], the alphas scaled by `scale`.
    function stops(gradient, list, scale) {
        for (const stop of list)
            gradient.addColorStop(stop[0], root.css(root.colour(stop[1]), (stop.length > 2 ? stop[2] :
                                                                                             1) * (scale
                                                                                                   === undefined
                                                                                                   ? 1 : scale)));
        return gradient;
    }

    function fraction(value) {
        return value - Math.floor(value);
    }

    function wrap(value, span) {
        return ((value % span) + span) % span;
    }

    // A repeatable scatter: the same index always lands in the same place.
    function hash(index) {
        const s = root.parameters.scatter;
        return root.fraction(Math.sin(index * s[0] + s[1]) * s[2]);
    }

    // ---- geometry
    //
    // Drawn in logical pixels, the Scene's pixels times its pitch, and scaled
    // down to them as it is drawn. A point on the road at depth z, where z = 1
    // is the bottom edge, and lateral position u, where the road's edges are at
    // -1 and 1.

    readonly property real drawWidth: root.width * root.pitch
    readonly property real drawHeight: root.height * root.pitch
    readonly property real horizonY: Math.round(root.drawHeight * root.parameters.horizon.at)
    readonly property real depth: root.drawHeight - root.horizonY
    readonly property real vanishingX: root.drawWidth / 2
    readonly property real halfWidth: root.drawWidth * (
                                          root.parameters.roadWidth[root.options.road] || 1)

    function screenY(z) {
        return root.horizonY + root.depth / z;
    }

    function screenX(u, z) {
        return root.vanishingX + u * root.halfWidth / z;
    }

    // ---- the light it casts
    //
    // The sun's light on the Omnibar's rim, as the website's road casts it
    // from the same `light` block: the rim's gradient on an ellipse centred on
    // the sun, `across` of the lit plate's width and `reach` down, the file's
    // sun radii turned into logical pixels, each stop a position and a colour with its alpha, and the bloom
    // over it, its opacity lifted by the beat's glow as the sky draws it.
    // Reduced motion holds the bloom at rest.
    readonly property var light: {
        const p = root.parameters;
        const glow = !root.reducedMotion && root.beat > p.beatGlow.from ? root.beat : 0;
        return {
            centre: Qt.point(root.vanishingX, root.horizonY),
            across: p.light.rim.across,
            reach: p.light.rim.reach * p.sun.radius * root.drawHeight,
            stops: p.light.rim.stops.map(function (stop) {
                return {
                    position: stop[0],
                    colour: root.withAlpha(root.colour(stop[1]), stop.length > 2 ? stop[2] : 1)
                };
            }),
            bloom: {
                opacity: p.light.bloom.rest + p.light.bloom.beat * glow,
                width: p.light.bloom.width,
                blur: p.light.bloom.blur
            }
        };
    }

    // ---- motion
    //
    // Travel advances with the clock at a speed that eases toward what
    // `navigating` asks, and the sun brightens with it.

    property real travel: 0
    property real speed: 1
    property real sunUp: 0
    property real last: -1

    function move() {
        const m = root.parameters.motion;
        if (root.reducedMotion) {
            root.travel = m.stillTravel;
            root.speed = 1;
            root.sunUp = 0;
            return;
        }
        if (root.last < 0)
            root.last = root.time;
        const step = Math.max(0, Math.min(m.longestStep, root.time - root.last));
        root.last = root.time;
        root.speed += (1 + m.navigatingSpeed * root.navigating - root.speed) * Math.min(1, step
                                                                                        * m.ease);
        root.sunUp += (root.navigating - root.sunUp) * Math.min(1, step * m.lightEase);
        root.travel += step * m.speed * m.pace * root.speed;
    }

    onTimeChanged: root.move()
    onReducedMotionChanged: root.move()
    Component.onCompleted: root.move()

    // ---- drawing

    // A ridge's outline: peaks from folded sines, falling to the horizon in a
    // notch where the road runs out.
    function ridge(layer) {
        const r = root.parameters.ridges;
        const points = [];
        for (let index = 0; index <= r.points; ++index) {
            const u = index / r.points;
            const d = Math.min(1, Math.abs(u - 0.5) / layer.notch);
            const valley = d * d * (3 - 2 * d);
            let height = 0;
            for (const h of r.harmonics)
                height += h.weight * Math.pow(1 - Math.abs(Math.sin(u * h.frequency + layer.seed
                                                                    * h.phase)), h.sharpness);
            height = r.floor + height * (1 - r.floor);
            points.push([u * root.drawWidth, root.horizonY - height * layer.height * root.horizonY
                         * valley]);
        }
        return points;
    }

    // The sun: a solid cap, then `bands` cuts that thicken toward the horizon.
    function sun(context, bands) {
        const p = root.parameters.sun;
        const radius = root.drawHeight * p.radius;
        const solid = radius * p.cap;
        context.save();
        context.beginPath();
        context.rect(root.vanishingX - radius, root.horizonY - radius, radius * 2, solid);
        const unit = (radius - solid) / (bands * 2);
        let y = root.horizonY - (radius - solid);
        for (let band = 0; band < bands; ++band) {
            const gap = unit * (p.firstGap + band / bands);
            context.rect(root.vanishingX - radius, y + gap, radius * 2, unit * 2 - gap);
            y += unit * 2;
        }
        context.clip();
        context.fillStyle = root.stops(context.createLinearGradient(0, root.horizonY - radius, 0,
                                                                    root.horizonY), p.stops);
        context.beginPath();
        context.arc(root.vanishingX, root.horizonY, radius, 0, Math.PI * 2, false);
        context.fill();
        context.restore();
    }

    function radial(context, x, y, radius, list, scale) {
        return root.stops(context.createRadialGradient(x, y, 0, x, y, radius), list, scale);
    }

    // What does not move: the sky and its stars, the sunrise glow, the sun,
    // the ridges, the desert and the road on it.
    function drawStill(context) {
        const p = root.parameters;
        const w = root.drawWidth;
        const h = root.drawHeight;
        context.fillStyle = root.stops(context.createLinearGradient(0, 0, 0, root.horizonY), p.sky);
        context.fillRect(0, 0, w, root.horizonY + 1);

        // Stars, a few of them brighter, thinning toward the horizon.
        const st = p.stars;
        for (let index = 0; index < root.stars; ++index) {
            const across = root.fraction(Math.sin(index * st.across[0]) * st.across[1]);
            const high = Math.pow(root.fraction(Math.sin(index * st.height[0]) * st.height[1]),
                                  st.heightPower);


            const bright = root.fraction(Math.sin(index * st.brightness[0]) * st.brightness[1]);
            const brightest = bright > st.brightAbove;
            const size = brightest ? st.sizes.bright : bright > st.mediumAbove ? st.sizes.medium :
                                                                                 st.sizes.dim;
            const alpha = brightest ? 1 : (st.alpha.base + st.alpha.range * bright) * (1 - high
                                                                                       * st.alpha.horizonFade);
            context.fillStyle = root.css(root.roles.light, alpha);
            context.fillRect(across * w, high * root.horizonY * st.reach, size, size);
        }

        context.fillStyle = root.radial(context, root.vanishingX, root.horizonY, w * p.halo.radius,
                                        p.halo.stops);
        context.fillRect(0, 0, w, root.horizonY);
        if (root.sunShown)
            root.sun(context, Number(root.options.bands) || 4);

        // The ridges as filled silhouettes, each a tone darker than the one
        // behind it.
        for (const layer of p.ridges.layers) {
            const points = root.ridge(layer);
            context.beginPath();
            context.moveTo(points[0][0], points[0][1]);
            for (const point of points)
                context.lineTo(point[0], point[1]);
            context.lineTo(w, root.horizonY + 1);
            context.lineTo(0, root.horizonY + 1);
            context.closePath();
            context.fillStyle = root.css(root.colour(layer.tone));
            context.fill();
        }

        // The desert at night, lit near the horizon and where the sun's glow
        // lies on it.
        context.fillStyle = root.stops(context.createLinearGradient(0, root.horizonY, 0, h),
                                       p.desert.stops);
        context.fillRect(0, root.horizonY, w, root.depth);
        const lying = p.desert.glow;
        context.save();
        context.translate(root.vanishingX, root.horizonY);
        context.scale(1, lying.squash);
        context.fillStyle = root.radial(context, 0, 0, w * lying.radius, lying.stops);
        context.fillRect(-w / 2, 0, w, root.depth / lying.squash);
        context.restore();

        // The road: the sand a step darker at every depth, so it reads as a
        // road through the desert, with wider passes for a soft edge rather
        // than a line, and the sun on the asphalt.
        const wedge = function (spread, fill) {
            context.fillStyle = fill;
            context.beginPath();
            context.moveTo(root.vanishingX - p.road.tip, root.horizonY);
            context.lineTo(root.vanishingX + p.road.tip, root.horizonY);
            context.lineTo(root.screenX(spread, 1), h);
            context.lineTo(root.screenX(-spread, 1), h);
            context.closePath();
            context.fill();
        };
        for (const edge of p.road.edges)
            wedge(edge.spread, root.css("black", edge.shade));
        wedge(p.road.reflection.spread, root.stops(context.createLinearGradient(0, root.horizonY, 0,
                                                                                h), p.road.reflection.stops));

        const line = p.horizon.lineHeight;
        context.fillStyle = root.stops(context.createLinearGradient(0, 0, w, 0), p.horizon.line);
        context.fillRect(0, root.horizonY - line / 2, w, line);
    }

    // A canvas the Scene draws into in logical pixels, scaled to its own. It
    // paints only while it can be seen; a paint asked for while it cannot is
    // kept for when it can.
    component Layer: Canvas {
        id: layer

        property var paintWith: function (context) {}
        property bool stale: true

        anchors.fill: parent
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Immediate

        function draw() {
            layer.stale = true;
            if (layer.visible)
                layer.requestPaint();
        }

        onVisibleChanged: if (visible && stale)
                              requestPaint()
        onPaint: {
            layer.stale = false;
            const context = getContext("2d");
            context.reset();
            context.scale(1 / root.pitch, 1 / root.pitch);
            layer.paintWith(context);
        }
    }

    Layer {
        id: still

        readonly property string key: [root.width, root.height, root.roles.ground, root.roles.light,
            root.roles.glow, root.unlit, root.options.bands, root.options.road].join("/")

        paintWith: root.drawStill
        onKeyChanged: draw()
    }

    // The sun brightening as the reader navigates.
    Glow {
        gradient: root.parameters.navigatingGlow
        radius: root.drawWidth * root.parameters.navigatingGlow.radius
        reach: root.drawHeight
        opacity: root.sunShown && root.sunUp > root.parameters.navigatingGlow.from ? root.sunUp : 0
    }

    // The beat swells a glow in the sky around the sun, wider and brighter on
    // each hit. The browser plays no music, so its host hands every Scene a
    // beat of 0 and this is never drawn there.
    Glow {
        readonly property var pulse: root.parameters.beatGlow

        gradient: pulse
        radius: root.drawWidth * pulse.radius * (1 + pulse.grow * root.beat)
        reach: root.horizonY
        opacity: root.sunShown && root.beat > pulse.from ? root.beat : 0
    }

    // ---- what moves, in logical pixels scaled to the Scene's

    Item {
        id: moving

        width: root.drawWidth
        height: root.drawHeight
        transform: Scale {
            xScale: 1 / root.pitch
            yScale: 1 / root.pitch
        }

        // A shooting star in some of its slots, for under a second.
        Rectangle {
            id: shootingStar

            readonly property var star: root.parameters.shootingStar
            readonly property int slot: Math.floor(root.time / star.every)
            readonly property real into: root.time - slot * star.every
            readonly property real run: into / star.lasts
            readonly property real length: root.drawWidth * star.length
            readonly property real headX: (star.across[0] + star.across[1] * root.hash(slot + 1))
                                          * root.drawWidth + length * run
            readonly property real headY: (star.height[0] + star.height[1] * root.hash(slot + 2))
                                          * root.horizonY + length * star.slope * run
            readonly property real tail: length * star.tail * Math.sqrt(1 + star.slope * star.slope)

            visible: root.sunShown && !root.reducedMotion && into < star.lasts && root.hash(slot)
                     > star.chance
            x: headX - tail
            y: headY - height / 2
            width: tail
            height: star.width
            transformOrigin: Item.Right
            rotation: Math.atan(star.slope) * 180 / Math.PI
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0
                    color: root.withAlpha(root.roles.light, 0)
                }
                GradientStop {
                    position: 1
                    color: root.withAlpha(root.colour(shootingStar.star.colour), 1
                                          - shootingStar.run * shootingStar.star.fade)
                }
            }
        }

        // The dashed centre line, its marks stretching as the road speeds up
        // and widening with it.
        Repeater {
            model: root.centreMarks

            Rectangle {
                required property int index

                readonly property var line: root.parameters.centreLine
                readonly property real d: 1 + root.wrap(index * line.spacing - root.travel, line.marks
                                                        * line.spacing)
                readonly property real markLength: line.length + Math.min(line.stretchMost, (
                                                                              root.speed - 1)
                                                                          * line.stretch)
                readonly property real near: root.screenY(d)
                readonly property real far: root.screenY(d + markLength)

                x: root.vanishingX - width / 2
                y: far
                width: Math.max(1, line.width / d) * root.halfWidth / (root.drawWidth
                                                                       * line.widthAtRoad)
                height: Math.max(1, near - far)
                color: root.colour(line.colour)
                opacity: Math.max(0, Math.min(1, line.fadeFrom - d / line.fadeFar) * Math.min(1, (d
                                                                                                  - 1) * line.fadeNear))
            }
        }

        // The roadside: a few silhouettes on a long loop, far apart, each
        // standing on the ground where its depth puts it.
        Repeater {
            model: root.roadsideMarks

            Item {
                id: mark

                required property int index

                readonly property var side: root.parameters.roadside
                readonly property var shape: side.shapes[side.order[index]]
                readonly property real place: index * (side.span / side.order.length) + root.hash(
                                                  index) * side.scatter - root.travel
                readonly property real d: side.nearest + root.wrap(place, side.span)
                readonly property real out: side.out[0] + root.hash(index + side.seeds.out)
                                            * side.out[1]
                readonly property real size: root.depth * shape.height / d

                x: root.screenX((root.hash(index + side.seeds.side) > 0.5 ? 1 : -1) * out, d)
                y: root.screenY(d)
                opacity: Math.max(0, Math.min(1, (side.span - d) / side.fadeFar) * Math.min(1, (d
                                                                                                - side.nearest)
                                                                                            * side.fadeNear))

                Repeater {
                    model: mark.shape.rects || []

                    Rectangle {
                        required property var modelData

                        x: modelData[0] * mark.size
                        y: modelData[1] * mark.size
                        width: modelData[2] * mark.size
                        height: modelData[3] * mark.size
                        color: root.colour(mark.side.colour)
                    }
                }

                // Outlined once at a hundred pixels and scaled: a path that
                // changed with every tick would be worked out again each
                // time, and cost the window a second frame for it.
                Shape {
                    visible: !!mark.shape.polygon
                    transformOrigin: Item.TopLeft
                    scale: mark.size / 100

                    ShapePath {
                        strokeWidth: -1
                        fillColor: root.colour(mark.side.colour)
                        PathPolyline {
                            path: (mark.shape.polygon || []).map(function (point) {
                                return Qt.point(point[0] * 100, point[1] * 100);
                            })
                        }
                    }
                }
            }
        }
    }

    // A radial glow from where the road meets the horizon, down to `reach`,
    // drawn once per size and theme at full strength and shown at the
    // strength asked for.
    component Glow: Layer {
        property var gradient
        property real radius: 0
        property real reach: 0

        visible: opacity > 0
        paintWith: function (context) {
            context.fillStyle = root.radial(context, root.vanishingX, root.horizonY, radius,
                                            gradient.stops);
            context.fillRect(0, 0, root.drawWidth, reach);
        }
        onRadiusChanged: draw()
        onReachChanged: draw()
        onWidthChanged: draw()
        Component.onCompleted: draw()

        Connections {
            target: root
            function onRolesChanged() {
                draw();
            }
        }
    }
}
