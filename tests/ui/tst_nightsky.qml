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

    // Whether a point lies on the planet's face.
    function behindTheLimb(sky, point) {
        return Math.hypot(point.x - sky.planetCentre.x, point.y - sky.planetCentre.y)
                < sky.planetRadius;
    }

    // A comet that has fallen behind the limb has gone out: it casts no light.
    function test_aCometBehindThePlanetCastsNoLight() {
        const sky = makeSky({
                                width: 1200 / 4,
                                height: 800 / 4
                            });
        let behind = 0;
        drive(sky, 600, function () {
            if (sky.cometShown && behindTheLimb(sky, sky.cometHead)) {
                behind += 1;
                verify(!lightFalls(sky), "a glint from behind the planet at " + sky.time);
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
        let found = false;
        for (let t = 0; t < 600 && !found; t += 1 / 30) {
            host.time = t;
            const head = sky.cometHead;
            found = sky.cometShown && sky.cometKind === 0 && behindTheLimb(sky, Qt.point(head.x,
                                                                                         head.y - 16));
        }
        verify(found, "no comet fell behind the planet");
        wait(100);
        const image = grabImage(host);
        const head = sky.cometHead;
        const face = Qt.color(sky.roles.face);
        const drawn = Qt.color(image.pixel(Math.round(head.x), Math.round(head.y)));
        verify(Math.abs(drawn.r - face.r) < 0.05 && Math.abs(drawn.g - face.g) < 0.05 && Math.abs(
                   drawn.b - face.b) < 0.05, drawn + " where the face is " + face);
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
            if (sky.cometShown) {
                seen.withComet += 1;
                seen.kinds[sky.cometKind] = true;
            }
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
        const seen = watch(sky, 120);
        compare(seen.withComet, seen.frames);
        compare(seen.streaks, 0);
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
            if (sky.cometShown && !behindTheLimb(sky, sky.cometHead)) {
                passing += 1;
                fuzzyCompare(sky.light.centre.x, sky.cometHead.x, 0.01);
                fuzzyCompare(sky.light.centre.y, sky.cometHead.y, 0.01);
            } else if (!sky.cometShown) {
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
