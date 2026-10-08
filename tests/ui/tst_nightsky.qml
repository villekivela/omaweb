import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// The night sky under the Start page, as a Scene: where it rests the Omnibar,
// what a Private window's sky leaves out, and how its comets fall for the clock
// and the reader the host hands it.
TestCase {
    id: testCase
    name: "NightSky"
    when: windowShown
    width: 800
    height: 500

    Component {
        id: skyComponent

        Omaweb.NightSky {
            width: 200
            height: 125
            colors: crtRoadKeyColours.dark.theme
        }
    }

    // The host in a window of its own on screen, so what it shows is drawn.
    Component {
        id: hostComponent

        Window {
            property alias host: host

            width: testCase.width
            height: testCase.height
            visible: true

            Omaweb.SceneHost {
                id: host
                anchors.fill: parent
                colors: crtRoadKeyColours.dark.theme
                scene: Component {
                    Omaweb.NightSky {}
                }
            }
        }
    }

    function makeHost(properties) {
        const window = createTemporaryObject(hostComponent, testCase);
        verify(window !== null);
        const host = window.host;
        for (const name in properties || {})
        host[name] = properties[name];
        tryVerify(function () {
            return host.sceneItem !== null;
        });
        return host;
    }

    function makeSky(properties) {
        const sky = createTemporaryObject(skyComponent, testCase, properties || {});
        verify(sky !== null);
        return sky;
    }

    // The sky is a Scene the host shows as it shows the road: in its own
    // pixels, through the CRT glass it declares, with the Omnibar resting on
    // the page area's middle as it rests on the road's horizon.
    function test_theHostShowsTheSkyAsAScene() {
        const host = makeHost();
        const sky = host.sceneItem;
        compare(sky.objectName, "nightSky");
        compare(sky.parameters.id, "night-sky");
        tryCompare(sky, "width", 800 / sky.pitch);
        compare(sky.horizonY, 250);
        compare(sky.glass, "crt");
        compare(sky.crt, sky.parameters.crt);
        verify(findChild(host, "crtGlass").visible);
    }

    function lightness(colour) {
        const c = Qt.color(colour);
        return (Math.max(c.r, c.g, c.b) + Math.min(c.r, c.g, c.b)) / 2;
    }

    // The sky is drawn from the theme's palette and is always night: a light
    // theme's night is drawn from its dark text, so the sky stays dark and its
    // stars light, and a theme change redraws it.
    function test_theSkyIsAlwaysNight_data() {
        return [
                    {
                        tag: "dark",
                        theme: "dark",
                        dark: true
                    },
                    {
                        tag: "light",
                        theme: "light",
                        dark: false
                    }
                ];
    }

    function test_theSkyIsAlwaysNight(data) {
        const sky = makeSky({
                                colors: crtRoadKeyColours[data.theme].theme,
                                dark: data.dark
                            });
        verify(lightness(sky.roles.skyTop) < 0.25, sky.roles.skyTop);
        verify(lightness(sky.roles.skyLow) < 0.5, sky.roles.skyLow);
        verify(lightness(sky.roles.light) > 0.75, sky.roles.light);
        compare(sky.roles.glow, Qt.color(crtRoadKeyColours[data.theme].theme.accent));

        const other = data.theme === "dark" ? "light" : "dark";
        sky.colors = crtRoadKeyColours[other].theme;
        sky.dark = !data.dark;
        compare(sky.roles.glow, Qt.color(crtRoadKeyColours[other].theme.accent));
    }

    // A dark planet rises from the bottom, as Navigator's globe did: its limb
    // crests below the page area's middle, where the Omnibar rests, and
    // curves away toward the edges. The window's test holds the crest just
    // below the Omnibar.
    function test_thePlanetsLimbCurvesBelowWhereTheOmnibarRests() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        const centre = sky.planetCentre;
        compare(centre.x, sky.drawWidth / 2);
        compare(sky.horizonY, 400);
        verify(centre.y - sky.planetRadius > sky.horizonY);
        // At the edges the limb has fallen below the crest, and it is still
        // on the page.
        const r = sky.planetRadius;
        const half = sky.drawWidth / 2;
        const fall = centre.y - Math.sqrt(r * r - half * half);
        verify(fall > sky.horizonY + 20, fall);
        verify(fall < sky.drawHeight, fall);
    }

    // Where the limb stands at the Scene's side edges, below its crest.
    function edgeFall(sky) {
        const r = sky.planetRadius;
        const half = sky.drawWidth / 2;
        return sky.planetCentre.y - Math.sqrt(r * r - half * half);
    }

    // The planet stands low: its top, the glow included, a third of the way
    // from the resting Omnibar's bottom edge to the Scene's, so it falls with
    // the window's height. Where that would take the limb's ends off the
    // bottom, or onto the lower right corner the Shortcut sheet's cue holds,
    // the planet stands higher, and never closer to the Omnibar than its gap.
    function test_thePlanetStandsLow_data() {
        return [
                    {
                        // The Omnibar's bottom at 400 + 50, a third of the
                        // 350 below it.
                        tag: "1200 by 800",
                        width: 1200,
                        height: 800,
                        top: 450 + 350 / 3
                    },
                    {
                        // 600 + 50, a third of the 550 below.
                        tag: "1000 by 1200",
                        width: 1000,
                        height: 1200,
                        top: 650 + 550 / 3
                    },
                    {
                        // A third of the way would leave the limb's ends 881
                        // down: they stand 72 above the bottom instead, the
                        // limb's sag 3840 - sqrt(3840² - 1280²) above them,
                        // and its glow's 28 above the crest.
                        tag: "2560 by 900",
                        width: 2560,
                        height: 900,
                        top: 900 - 72 - (3840 - Math.sqrt(3840 * 3840 - 1280 * 1280)) - 28
                    }
                ];
    }

    function test_thePlanetStandsLow(data) {
        const sky = makeSky({
                                width: data.width / 4,
                                height: data.height / 4,
                                omnibarReach: 50
                            });
        fuzzyCompare(sky.planetTop, data.top, 0.5);
        verify(edgeFall(sky) <= data.height - 72, edgeFall(sky));
        verify(sky.planetTop >= sky.horizonY + 50 + 12, sky.planetTop);
    }

    // Whether a point lies on the planet's face.
    function behindTheLimb(sky, point) {
        return Math.hypot(point.x - sky.planetCentre.x, point.y - sky.planetCentre.y)
                < sky.planetRadius;
    }

    // How far out in the sky's light the Omnibar's rim stands with the light
    // on `head`: 0 at its centre, 1 at the edge of its ellipse, which is
    // `across` of the field's width wide and `reach` tall. The rim catches
    // the light's brighter half below 0.5.
    function lightOnRim(sky, head, field) {
        const dx = Math.max(field.x - head.x, 0, head.x - field.x - field.width);
        const dy = Math.max(field.y - head.y, 0, head.y - field.y - field.height);
        return Math.hypot(dx / (field.width * sky.light.across), dy / sky.light.reach);
    }

    // The resting Omnibar of a 1200 by 800 sky: 720 wide, centred, from 50
    // above where it rests to its bottom edge 50 below.
    function fieldOf(sky) {
        return Qt.rect(240, sky.horizonY - 50, 720, 100);
    }

    // The comets in the sky now, each with its head, its kind and the slot
    // and place in it that name it.
    function shown(sky) {
        return sky.fallen.filter(function (comet) {
            return comet !== null;
        });
    }

    // A comet that has fallen behind the limb has gone out: it casts no light.
    function test_aCometBehindThePlanetCastsNoLight() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        let behind = 0;
        drive(sky, 600, function () {
            for (const comet of shown(sky)) {
                if (!behindTheLimb(sky, comet.head))
                    continue;
                behind += 1;
                const c = sky.light.centre;
                verify(c.x !== comet.head.x || c.y !== comet.head.y, "a glint from behind the planet at "
                       + sky.time);
            }
        });
        verify(behind > 0, "no comet fell behind the planet in ten minutes");
    }

    // What the host draws where a comet's head has fallen behind the planet
    // is the planet's face.
    function test_aCometGoesOutBehindThePlanet() {
        const host = makeHost({
                                  glass: false
                              });
        const sky = host.sceneItem;
        tryCompare(sky, "width", 800 / sky.pitch);
        let head = null;
        for (let t = 0; t < 600 && head === null; t += 1 / 30) {
            host.time = t;
            for (const comet of shown(sky)) {
                if (comet.kind === 0 && behindTheLimb(sky, Qt.point(comet.head.x, comet.head.y
                                                                    - 16)))
                    head = comet.head;
            }
        }
        verify(head !== null, "no comet fell behind the planet");
        wait(100);
        const image = grabImage(host);
        const face = Qt.color(sky.roles.face);
        const drawn = Qt.color(image.pixel(Math.round(head.x), Math.round(head.y)));
        verify(Math.abs(drawn.r - face.r) < 0.05 && Math.abs(drawn.g - face.g) < 0.05 && Math.abs(
                   drawn.b - face.b) < 0.05, drawn + " where the face is " + face);
    }

    // Comets and shooting stars fall as Navigator's do: from the upper right
    // toward the lower left, about 25 degrees below the horizontal.
    function test_cometsFallFromTheUpperRight() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        const last = {};
        let steps = 0;
        drive(sky, 240, function () {
            const now = {};
            for (const comet of shown(sky)) {
                const name = comet.slot + "/" + comet.index;
                now[name] = Qt.point(comet.head.x, comet.head.y);
                const before = last[name];
                if (before === undefined)
                    continue;
                steps += 1;
                verify(comet.head.x < before.x, "a comet moving right at " + sky.time);
                verify(comet.head.y > before.y, "a comet moving up at " + sky.time);
                const angle = Math.atan2(comet.head.y - before.y, before.x - comet.head.x) * 180
                      / Math.PI;
                fuzzyCompare(angle, 25, 1);
            }
            for (const name in last)
                delete last[name];
            Object.assign(last, now);
        });
        verify(steps > 0);
    }

    // One comet comes first, then two or three can be in the sky at once,
    // never more, with quiet stretches between them.
    function test_upToThreeCometsAtOnce() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        const counts = {};
        let before = 0;
        let quiet = 0;
        drive(sky, 600, function () {
            const count = shown(sky).length;
            counts[count] = (counts[count] || 0) + 1;
            if (before === 0 && count > 0)
                compare(count, 1, "comets that came together at " + sky.time);
            verify(count <= 3, count + " comets at " + sky.time);
            quiet = count === 0 ? quiet + 1 : quiet;
            before = count;
        });
        verify(counts[2] > 0, "never two comets at once");
        verify(counts[3] > 0, "never three comets at once");
        verify(quiet > 30 * 120, "only " + quiet / 30 + " quiet seconds in ten minutes");
    }

    // The quiet stretches between comets and shooting stars are about half
    // as long as the 22 seconds they once averaged, and there still are some.
    function test_quietStretchesBetweenComets() {
        const sky = makeSky();
        const stretches = [];
        let quiet = 0;
        drive(sky, 3600, function () {
            if (shown(sky).length === 0) {
                quiet += 1;
            } else if (quiet > 0) {
                stretches.push(quiet / 30);
                quiet = 0;
            }
        });
        const mean = stretches.reduce(function (sum, seconds) {
            return sum + seconds;
        }, 0) / stretches.length;
        verify(mean > 6 && mean < 13, "quiet for " + mean + " seconds at a time");
    }

    // Every comet and shooting star goes out behind the planet's limb rather
    // than stopping short in the sky.
    function test_everyCometGoesOutBehindTheLimb() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        let last = {};
        let gone = 0;
        drive(sky, 600, function () {
            const now = {};
            for (const comet of shown(sky))
                now[comet.slot + "/" + comet.index] = Qt.point(comet.head.x, comet.head.y);
            for (const name in last) {
                if (now[name] !== undefined)
                    continue;
                gone += 1;
                verify(behindTheLimb(sky, last[name]), "comet " + name + " went out at "
                       + last[name]);
            }
            last = now;
        });
        verify(gone > 10, gone + " comets");
    }

    // A comet's head is white, with a glow in the palette's accent lifted
    // toward white, which reads pink as Navigator's did, and its tail tapers
    // into the sky from the head.
    function test_aCometsHeadAndTail() {
        const sky = makeSky({
                                reducedMotion: true
                            });
        const head = findChild(sky, "cometHead");
        const halo = findChild(sky, "cometHalo");
        const tail = findChild(sky, "cometTail");
        verify(head !== null && halo !== null && tail !== null);
        compare(Qt.color(head.color), Qt.color("white"));
        const accent = Qt.color(crtRoadKeyColours.dark.theme.accent);
        const glow = Qt.color(halo.color);
        fuzzyCompare(glow.hslHue, accent.hslHue, 0.02);
        verify(glow.hslLightness > accent.hslLightness, glow + " over " + accent);
        verify(tail.endWidth < tail.headWidth, tail.endWidth + " from " + tail.headWidth);
    }

    // A Private window's planet and its limb stay, without their glow.
    function test_anUnlitPlanetHasNoGlow() {
        const lit = makeSky();
        const unlit = makeSky({
                                  unlit: true
                              });
        verify(lit.limbGlows);
        verify(!unlit.limbGlows);
    }

    // The limb's glow pulses on a slow sine of five seconds, from its peak
    // down to 60% of it and back, its reach the same throughout. A commit
    // leaves the pulse alone.
    function test_theLimbsGlowPulses() {
        const sky = makeSky();
        const glow = findChild(sky, "planetGlow");
        verify(glow !== null);
        const reach = sky.glowReach;
        const at = function (time) {
            sky.time = time;
            compare(glow.opacity, sky.glowOpacity);
            compare(sky.glowReach, reach);
            return sky.glowOpacity;
        };
        fuzzyCompare(at(0), 1, 0.001);
        fuzzyCompare(at(2.5), 0.6, 0.001);
        fuzzyCompare(at(5), 1, 0.001);
        const between = at(1.25);
        verify(between > 0.6 && between < 1, between);
        sky.navigating = 1;
        fuzzyCompare(at(7.5), 0.6, 0.001);
    }

    // Under reduced motion the glow holds still at its peak, and a Private
    // window's planet has no glow to pulse.
    function test_theGlowHoldsStill_data() {
        return [
                    {
                        tag: "reduced motion",
                        reducedMotion: true,
                        unlit: false,
                        shown: true
                    },
                    {
                        tag: "unlit",
                        reducedMotion: false,
                        unlit: true,
                        shown: false
                    }
                ];
    }

    function test_theGlowHoldsStill(data) {
        const sky = makeSky({
                                reducedMotion: data.reducedMotion,
                                unlit: data.unlit
                            });
        const glow = findChild(sky, "planetGlow");
        verify(glow !== null);
        const held = sky.glowOpacity;
        compare(held, data.shown ? 1 : 0);
        drive(sky, 6, function () {
            compare(sky.glowOpacity, held);
            compare(glow.opacity, held);
        });
    }

    // The front comet's crossings over `seconds` of the sky's clock: when
    // each came in, where its head stood on the frames it was in the sky,
    // and how bright the planet stood on each frame.
    function crossings(sky, seconds) {
        const seen = [];
        let crossing = null;
        drive(sky, seconds, function () {
            const front = findChild(sky, "frontComet");
            const flare = findChild(sky, "planetFlare");
            compare(flare.opacity, sky.flare);
            if (!sky.frontShown) {
                compare(sky.flare, 0);
                crossing = null;
                return;
            }
            compare(front.opacity, sky.frontTrail);
            if (crossing === null) {
                crossing = {
                    from: sky.time,
                    heads: [],
                    flares: [],
                    trails: [],
                    others: 0
                };
                seen.push(crossing);
            }
            crossing.heads.push(Qt.point(sky.frontHead.x, sky.frontHead.y));
            crossing.flares.push(sky.flare);
            crossing.trails.push(sky.frontTrail);
            crossing.others += shown(sky).length;
        });
        return seen;
    }

    // About every 30 seconds, never while another comet is in the sky, a large
    // comet comes in from the right edge, above the planet in the sky where
    // the window is wide enough, sweeps down and left across its face in
    // front of it, and leaves through the bottom edge. The planet brightens
    // as it passes, and its trail fades over a couple of seconds once it has
    // gone.
    function test_aCometFallsInFrontOfThePlanet_data() {
        return [
                    {
                        tag: "1200 by 800",
                        width: 1200,
                        height: 800,
                        inTheSky: true
                    },
                    {
                        // A tiled column, taller than it is wide: the
                        // diagonal from the crest would leave through the
                        // left edge, so the comet comes in lower.
                        tag: "560 by 1440",
                        width: 560,
                        height: 1440,
                        inTheSky: false
                    }
                ];
    }

    function test_aCometFallsInFrontOfThePlanet(data) {
        const sky = makeSky({
                                width: data.width / 4,
                                height: data.height / 4
                            });
        const seen = crossings(sky, 600);
        verify(seen.length >= 19 && seen.length <= 21, seen.length + " in ten minutes");
        for (let index = 1; index < seen.length; ++index) {
            const gap = seen[index].from - seen[index - 1].from;
            verify(gap > 25 && gap < 35, gap + " seconds apart");
        }
        for (const crossing of seen) {
            compare(crossing.others, 0, "a comet beside the front comet at " + crossing.from);
            const first = crossing.heads[0];
            verify(first.x >= sky.drawWidth - 16, "came in at " + first.x);
            if (data.inTheSky)
                verify(first.y > 0 && first.y < sky.crestY, "came in at " + first.y);
            let over = 0;
            let out = -1;
            for (let frame = 0; frame < crossing.heads.length; ++frame) {
                const head = crossing.heads[frame];
                if (behindTheLimb(sky, head) && head.y < sky.drawHeight) {
                    over += 1;
                    verify(crossing.flares[frame] > 0, "no light on the planet at " + head);
                }
                if (out < 0 && head.y > sky.drawHeight)
                    out = frame;
            }
            verify(over > 30, over + " frames across the face");
            verify(out > 0, "never left through the bottom edge");
            const left = crossing.heads[out];
            verify(left.x > 0 && left.x < sky.drawWidth, "left at " + left);
            compare(Math.max.apply(null, crossing.flares.slice(out)), 0);
            const fading = (crossing.heads.length - out) / 30;
            verify(fading > 1.5 && fading < 2.5, "faded over " + fading + " seconds");
            verify(crossing.trails[crossing.trails.length - 1] < 0.1);
        }
    }

    // A commit while the front comet crosses puts it out with the streaks,
    // and it does not come back part way across once they have gone.
    function test_theStreaksPutTheFrontCometOut() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        drive(sky, 23);
        verify(sky.frontShown, "no comet in front of the planet at " + sky.time);
        sky.navigating = 1;
        drive(sky, 1);
        verify(!sky.frontShown);
        sky.navigating = 0;
        drive(sky, 9, function () {
            verify(!sky.frontShown, "back in front of the planet at " + sky.time);
        });
        // The next one falls as ever.
        compare(crossings(sky, 30).length, 1);
    }

    // On its way to the planet's face the comet in front of it passes the
    // resting Omnibar's right end close enough for its light to catch the
    // rim, its glow never within 16 px of the field. The field is 720 wide,
    // centred, from 50 above where the Omnibar rests to its bottom edge 50
    // below. Each crossing comes in at a place and an angle of its own,
    // between 20 and 32 degrees below the horizontal.
    function test_theFrontCometCatchesTheRim() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4,
                                omnibarReach: 50,
                                omnibarWidth: 720
                            });
        const field = Qt.rect(240, sky.horizonY - 50, 720, 100);
        const clear = sky.front.halo / 2 + 16;
        const seen = [];
        let crossing = null;
        drive(sky, 300, function () {
            if (!sky.frontShown) {
                crossing = null;
                return;
            }
            const head = Qt.point(sky.frontHead.x, sky.frontHead.y);
            if (crossing === null) {
                crossing = {
                    heads: [],
                    near: -1,
                    face: -1
                };
                seen.push(crossing);
            }
            const frame = crossing.heads.length;
            crossing.heads.push(head);
            const dx = Math.max(field.x - head.x, 0, head.x - field.x - field.width);
            const dy = Math.max(field.y - head.y, 0, head.y - field.y - field.height);
            verify(Math.hypot(dx, dy) >= clear, "the comet at " + head + " " + Math.hypot(dx,
                                                                                          dy).toFixed(
                       1) + " from the field");
            if (head.y < sky.drawHeight) {
                fuzzyCompare(sky.light.centre.x, head.x, 0.01);
                fuzzyCompare(sky.light.centre.y, head.y, 0.01);
            }
            if (crossing.near < 0 && lightOnRim(sky, head, field) < 0.5)
                crossing.near = frame;
            if (crossing.face < 0 && behindTheLimb(sky, head))
                crossing.face = frame;
        });
        verify(seen.length >= 9, seen.length + " crossings");
        const angles = [];
        const entries = [];
        for (const one of seen) {
            verify(one.near >= 0, "a comet never caught the rim");
            verify(one.face > one.near, "it reached the face at frame " + one.face
                   + ", the field at " + one.near);
            const a = one.heads[0];
            const b = one.heads[1];
            const angle = Math.atan2(b.y - a.y, a.x - b.x) * 180 / Math.PI;
            verify(angle >= 19.9 && angle <= 32.1, angle + " degrees");
            angles.push(angle);
            entries.push(a.y);
        }
        const spread = function (values) {
            return Math.max.apply(null, values) - Math.min.apply(null, values);
        };
        verify(spread(angles) > 6, "angles within " + spread(angles) + " degrees");
        verify(spread(entries) > 40, "comes in within " + spread(entries) + " px");
    }

    // No comet or shooting star pops into view: each fades in from where it
    // starts, and one already part way across when the clock jumps or the
    // sky changes size stays out of sight. The comet in front of the planet
    // comes in from beyond the right edge.
    function test_noCometPopsIntoView() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        const quiet = function (why) {
            for (const comet of shown(sky))
                verify(comet.shown <= 0.2, "a comet popped in " + why + " at " + sky.time + ", "
                       + comet.shown + " shown");
            verify(!sky.frontShown || sky.frontHead.x > sky.drawWidth, "the front comet popped in "
                   + why + " at " + sky.time + ", " + sky.frontHead);
        };
        let seen = {};
        drive(sky, 300, function () {
            const now = {};
            for (const comet of shown(sky)) {
                const name = comet.slot + "/" + comet.index;
                now[name] = true;
                if (!seen[name])
                    verify(comet.shown <= 0.2, "comet " + name + " came in " + comet.shown
                           + " shown");
            }
            seen = now;
        });
        for (let t = 3; t < 300; t += 7.3) {
            sky.time = t;
            quiet("after the clock jumped");
            drive(sky, 1);
        }
        let full = false;
        for (let t = 300; t < 600 && !full; t += 1 / 30) {
            sky.time = t;
            full = shown(sky).some(function (comet) {
                return comet.shown === 1;
            });
        }
        verify(full, "no comet in full view");
        sky.width = 250;
        quiet("after a resize");
    }

    // About one comet in three is steered past the Omnibar's lower right
    // corner, its glow 16 px off the panel and its light on the rim. Others
    // catch it now and then on their own.
    function test_aThirdOfTheCometsCatchTheRim() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4,
                                omnibarReach: 50,
                                omnibarWidth: 720
                            });
        const field = fieldOf(sky);
        const comets = {};
        drive(sky, 3600, function () {
            for (const comet of shown(sky)) {
                const name = comet.slot + "/" + comet.index;
                const seen = comets[name] || {
                    steered: comet.steered,
                    rim: Infinity
                };
                comets[name] = seen;
                const rim = lightOnRim(sky, comet.head, field);
                seen.rim = Math.min(seen.rim, rim);
                if (comet.steered) {
                    const dx = Math.max(field.x - comet.head.x, 0, comet.head.x - field.x
                                        - field.width);
                    const dy = Math.max(field.y - comet.head.y, 0, comet.head.y - field.y
                                        - field.height);
                    const glow = sky.comets.kinds[comet.kind].halo / 2;
                    verify(Math.hypot(dx, dy) >= glow + 16, "a steered comet at " + comet.head);
                }
            }
        });
        const all = Object.keys(comets).map(function (name) {
            return comets[name];
        });
        const steered = all.filter(function (comet) {
            return comet.steered;
        });
        const share = steered.length / all.length;
        verify(share > 0.25 && share < 0.42, steered.length + " of " + all.length + " steered");
        for (const comet of steered)
            verify(comet.rim < 0.5, "a steered comet missed the rim: " + comet.rim);
        const caught = all.filter(function (comet) {
            return comet.rim < 0.5;
        }).length;
        verify(caught / all.length >= share, caught + " of " + all.length + " caught the rim");
    }

    // What the host draws where the front comet's head crosses the planet's
    // face is the comet, not the face.
    function test_theFrontCometIsDrawnInFront() {
        const host = makeHost({
                                  glass: false
                              });
        const sky = host.sceneItem;
        tryCompare(sky, "width", 800 / sky.pitch);
        let head = null;
        for (let t = 0; t < 120 && head === null; t += 1 / 30) {
            host.time = t;
            if (sky.frontShown && behindTheLimb(sky, Qt.point(sky.frontHead.x, sky.frontHead.y
                                                              - 16)) && sky.frontHead.y
                    < sky.drawHeight - 16)
                head = sky.frontHead;
        }
        verify(head !== null, "no comet crossed the planet's face");
        wait(100);
        const image = grabImage(host);
        const face = Qt.color(sky.roles.face);
        const drawn = Qt.color(image.pixel(Math.round(head.x), Math.round(head.y)));
        verify(drawn.hslLightness > face.hslLightness + 0.3, drawn + " where the face is " + face);
    }

    // The front comet never falls under reduced motion, in a Private window,
    // or while a commit's streaks fall.
    function test_noFrontComet_data() {
        return [
                    {
                        tag: "reduced motion",
                        properties: {
                            reducedMotion: true
                        }
                    },
                    {
                        tag: "unlit",
                        properties: {
                            unlit: true
                        }
                    },
                    {
                        tag: "streaks",
                        properties: {
                            navigating: 1
                        }
                    }
                ];
    }

    function test_noFrontComet(data) {
        const sky = makeSky(data.properties);
        compare(crossings(sky, 180).length, 0);
    }

    // Drives a sky's clock by hand, a frame at the Scene's rate at a time, and
    // calls `each` on every frame.
    function drive(sky, seconds, each) {
        const step = 1 / 30;
        for (let elapsed = 0; elapsed < seconds; elapsed += step) {
            sky.time += step;
            if (each)
                each();
        }
    }

    // What the sky drew over `seconds` of its clock: the share of frames with
    // a comet or a shooting star in them, each kind seen, and the most streaks
    // in one frame.
    function watch(sky, seconds) {
        const seen = {
            frames: 0,
            withComet: 0,
            kinds: {},
            streaks: 0
        };
        drive(sky, seconds, function () {
            seen.frames += 1;
            const comets = shown(sky);
            if (comets.length > 0)
                seen.withComet += 1;
            for (const comet of comets)
                seen.kinds[comet.kind] = true;
            seen.streaks = Math.max(seen.streaks, sky.streaks);
        });
        return seen;
    }

    // At rest a comet or a shooting star crosses now and then, over a star
    // field that holds still.
    function test_aCometFallsNowAndThen() {
        const sky = makeSky();
        verify(sky.stars > 0);
        const seen = watch(sky, 240);
        verify(seen.withComet > 0, "no comet in four minutes");
        verify(seen.withComet < seen.frames / 2, seen.withComet + " of " + seen.frames
               + " frames had a comet");
        verify(seen.kinds[0] && seen.kinds[1], "comets and shooting stars both fall");
        compare(seen.streaks, 0);
    }

    // On commit the stars come thick and fast, as streaks, and they thin out
    // again once the page has painted.
    function test_navigatingBringsStreaks() {
        const sky = makeSky();
        compare(watch(sky, 1).streaks, 0);
        sky.navigating = 1;
        drive(sky, 1.5);
        verify(sky.streaks >= 30, sky.streaks + " streaks");
        sky.navigating = 0;
        drive(sky, 3);
        compare(sky.streaks, 0);
    }

    // A reader who asked for less motion gets one frame that reads on its own:
    // the star field and one comet held part way across, and no streaks.
    function test_aStillSkyHoldsOneComet() {
        const sky = makeSky({
                                reducedMotion: true,
                                navigating: 1
                            });
        // Long enough for slots that would hold no comet in motion.
        drive(sky, 120, function () {
            const comets = shown(sky);
            compare(comets.length, 1);
            verify(!behindTheLimb(sky, comets[0].head));
            compare(sky.streaks, 0);
        });
    }

    // Whether the sky's light falls anywhere on its picture: its ellipse,
    // however wide a plate it lights, reaches some of it.
    function lightFalls(sky) {
        const light = sky.light;
        const c = light.centre;
        return c.x > -sky.drawWidth && c.x < 2 * sky.drawWidth && c.y > -light.reach && c.y
                < sky.drawHeight + light.reach;
    }

    // A passing comet catches the Omnibar's rim: the sky's light follows its
    // head while it is above the planet's limb, and falls nowhere while no
    // comet passes.
    function test_aPassingCometCastsItsLight() {
        const sky = makeSky();
        verify(sky.light !== null);
        let passing = 0;
        let between = 0;
        drive(sky, 240, function () {
            const above = shown(sky).filter(function (comet) {
                return !behindTheLimb(sky, comet.head);
            });
            if (above.length > 0) {
                passing += 1;
                const c = sky.light.centre;
                verify(above.some(function (comet) {
                    return Math.abs(c.x - comet.head.x) < 0.01 && Math.abs(c.y - comet.head.y)
                            < 0.01;

                }), "a glint on no comet's head at " + sky.time);
            } else if (shown(sky).length === 0 && !sky.frontShown) {
                between += 1;
                verify(!lightFalls(sky), "a glint with no comet at " + sky.time);
            }
        });
        verify(passing > 0 && between > 0);
    }

    // Some comets fall past the Omnibar's field, close enough for their light
    // to reach its rim: the field is min(width - 32, 720) wide, centred, its
    // top edge 50 px above where the Omnibar rests and 50 px tall.
    function test_someCometsPassTheField() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        const half = Math.min(sky.drawWidth - 32, 720) / 2;
        const field = Qt.rect(sky.drawWidth / 2 - half, sky.horizonY - 50, half * 2, 50);
        let near = 0;
        drive(sky, 600, function () {
            const c = sky.light.centre;
            const dx = Math.max(field.x - c.x, 0, c.x - field.x - field.width);
            const dy = Math.max(field.y - c.y, 0, c.y - field.y - field.height);
            if (dx < field.width * sky.light.across && dy < sky.light.reach)
                near += 1;
        });
        verify(near > 0, "no comet passed the field in ten minutes");
    }

    // The comet held still for less motion passes nothing, so it lights no
    // rim, and an unlit sky has no comet to.
    function test_noGlintStillOrUnlit_data() {
        return [
                    {
                        tag: "reduced motion",
                        properties: {
                            reducedMotion: true
                        }
                    },
                    {
                        tag: "unlit",
                        properties: {
                            unlit: true
                        }
                    }
                ];
    }

    function test_noGlintStillOrUnlit(data) {
        const sky = makeSky(data.properties);
        drive(sky, 240, function () {
            verify(!lightFalls(sky), "a glint at " + sky.time);
        });
    }

    // A Private window's sky has its lights out: no stars and no comets, even
    // on commit.
    function test_anUnlitSkyHasItsLightsOut() {
        const sky = makeSky({
                                unlit: true
                            });
        compare(sky.stars, 0);
        let seen = watch(sky, 240);
        compare(seen.withComet, 0);
        sky.navigating = 1;
        seen = watch(sky, 3);
        compare(seen.withComet, 0);
        compare(seen.streaks, 0);
    }
}
