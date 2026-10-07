import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// The radar under the Start page, as a Scene: how it fills the page area from
// behind the Omnibar, where each tab's blip stands, how its sweep turns and lights
// them for the clock and the reader the host hands it, the light it casts on
// the Omnibar's rim, and what a Private window's radar and a still one show.
TestCase {
    id: testCase
    name: "Radar"
    when: windowShown
    width: 800
    height: 500

    Component {
        id: radarComponent

        Omaweb.Radar {
            width: 400
            height: 250
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
                    Omaweb.Radar {}
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

    function makeRadar(properties) {
        const radar = createTemporaryObject(radarComponent, testCase, properties || {});
        verify(radar !== null);
        return radar;
    }

    // Drives the radar's clock by hand, a frame at the Scene's rate at a
    // time, and calls `each` on every frame.
    function drive(radar, seconds, each) {
        const step = 1 / 30;
        for (let elapsed = 0; elapsed < seconds - 1e-6; elapsed += step) {
            radar.time += step;
            if (each)
                each();
        }
    }

    function lightness(colour) {
        const c = Qt.color(colour);
        return (Math.max(c.r, c.g, c.b) + Math.min(c.r, c.g, c.b)) / 2;
    }

    // Degrees from `from` clockwise to `to`, 0 to 360.
    function clockwise(from, to) {
        return ((to - from) % 360 + 360) % 360;
    }

    function places(radar) {
        return radar.blips.map(function (blip) {
            return blip.site + "@" + blip.x.toFixed(3) + "," + blip.y.toFixed(3);
        });
    }

    // The radar is a Scene the host shows as it shows the road: in its own
    // pixels, through the CRT glass it declares, with the Omnibar resting on
    // the page area's middle, and it casts a light on the Omnibar's rim.
    function test_theHostShowsTheRadarAsAScene() {
        const host = makeHost();
        const radar = host.sceneItem;
        compare(radar.objectName, "radar");
        compare(radar.parameters.id, "radar");
        tryCompare(radar, "width", 800 / radar.pitch);
        compare(radar.horizonY, 250);
        compare(radar.glass, "crt");
        compare(radar.crt, radar.parameters.crt);
        verify(findChild(host, "crtGlass").visible);
        verify(host.light !== null, "no light on the rim");
    }

    // The radar is drawn from the theme's palette and is always night: a
    // light theme's night is drawn from its dark text, so the face stays dark
    // and the sweep is the accent.
    function test_theRadarIsAlwaysNight_data() {
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

    function test_theRadarIsAlwaysNight(data) {
        const radar = makeRadar({
                                    colors: crtRoadKeyColours[data.theme].theme,
                                    dark: data.dark
                                });
        verify(lightness(radar.roles.face) < 0.25, radar.roles.face);
        verify(lightness(radar.roles.light) > 0.75, radar.roles.light);
        compare(radar.roles.glow, Qt.color(crtRoadKeyColours[data.theme].theme.accent));
    }

    // Whether a point stands on the page area, `margin` in from its edges,
    // and clear of the resting Omnibar's field.
    function onPageClearOfTheField(radar, point, margin) {
        const f = radar.field;
        const inField = point.x > f.x && point.x < f.x + f.width && point.y > f.y && point.y < f.y
              + f.height;
        return !inField && point.x >= margin && point.x <= radar.drawWidth - margin && point.y
                >= margin && point.y <= radar.drawHeight - margin;
    }

    // The radar fills the page area: the sweep turns from a centre behind the
    // Omnibar's field and reaches the farthest corner, and the field it keeps
    // its blips clear of is where the Omnibar rests, at any size.
    function test_theRadarFillsThePageArea_data() {
        return [
                    {
                        tag: "page area",
                        width: 1360 / 2,
                        height: 860 / 2
                    },
                    {
                        tag: "small window",
                        width: 800 / 2,
                        height: 500 / 2
                    },
                    {
                        tag: "narrow window",
                        width: 500 / 2,
                        height: 1200 / 2
                    },
                    {
                        tag: "thumbnail",
                        width: 512 / 2,
                        height: 320 / 2
                    }
                ];
    }

    function test_theRadarFillsThePageArea(data) {
        const radar = makeRadar({
                                    width: data.width,
                                    height: data.height
                                });
        const w = radar.drawWidth;
        const h = radar.drawHeight;
        compare(radar.horizonY, Math.round(h / 2));
        compare(radar.centre, Qt.point(w / 2, radar.horizonY));
        for (const corner of [Qt.point(0, 0), Qt.point(w, 0), Qt.point(0, h), Qt.point(w, h)])
            verify(radar.reach >= Math.hypot(corner.x - radar.centre.x, corner.y - radar.centre.y)
                   - 0.001, "a corner the sweep does not reach");
        const field = radar.field;
        const width = Math.min(w - 32, 720);
        compare(field.x, w / 2 - width / 2);
        compare(field.width, width);
        compare(field.y, radar.horizonY - 50);
        compare(field.y + field.height, radar.horizonY + radar.omnibarReach);

        compare(radar.blips.length, radar.parameters.blips.sample);
        for (const blip of radar.blips)
            verify(onPageClearOfTheField(radar, blip, 1), "a blip at " + blip.x + ", " + blip.y);
    }

    // Each open tab in the Space has a blip, placed by a scatter of its site:
    // where it stands does not hang on the tab's path, on its place in the
    // list or on the other tabs, so a tab's blip stays put. A page with no
    // host, such as a file, has one by its scheme, a blank page has none,
    // and two tabs of one site have one each.
    function test_aBlipForEachOpenTab() {
        const radar = makeRadar({
                                    pages: ["https://alpha.example/one", "https://beta.example/",
                                        "about:blank", "", "file:///home/reader/notes.html"]
                                });
        compare(radar.blips.length, 3);
        compare(radar.blips[0].site, "alpha.example");
        compare(radar.blips[1].site, "beta.example");
        compare(radar.blips[2].site, "file");
        for (const blip of radar.blips)
            verify(onPageClearOfTheField(radar, blip, 1), blip.site + " at " + blip.x + ", "
                   + blip.y);
        const before = places(radar);
        verify(before[0].split("@")[1] !== before[1].split("@")[1], "two sites in one place");

        radar.pages = ["https://gamma.example/", "https://www.beta.example/elsewhere",
                       "https://alpha.example/two?x=1"];
        const after = places(radar);
        compare(after.length, 3);
        verify(after.indexOf(before[0]) >= 0, before[0] + " moved: " + after);
        verify(after.indexOf(before[1]) >= 0, before[1] + " moved: " + after);

        radar.pages = ["https://alpha.example/one", "https://alpha.example/two"];
        compare(radar.blips.length, 2);
        verify(places(radar)[0].split("@")[1] !== places(radar)[1].split("@")[1],
               "two tabs of one site in one place");
        compare(places(radar)[0], before[0]);
    }

    // A picture with no Space behind it, such as the picker's thumbnail,
    // shows a few blips all the same.
    function test_aPictureWithNoSpaceShowsSampleBlips() {
        const radar = makeRadar();
        compare(radar.pages, null);
        compare(radar.blips.length, radar.parameters.blips.sample);
        radar.pages = [];
        compare(radar.blips.length, 0);
    }

    // At rest the sweep turns clockwise once every six seconds.
    function test_theSweepTurnsOnceEverySixSeconds() {
        const radar = makeRadar();
        drive(radar, 1);
        const start = radar.sweep;
        drive(radar, 6);
        fuzzyCompare(radar.sweep - start, 360, 4);
        compare(radar.parameters.sweep.turn, 6);
    }

    // The sweep lights a blip as it passes over it, and the blip fades with
    // the afterglow, about two seconds behind the beam.
    function test_theSweepLightsABlipThatFadesWithTheAfterglow() {
        const radar = makeRadar({
                                    pages: ["https://alpha.example/"]
                                });
        const blip = radar.blips[0];
        const floor = radar.parameters.blips.floor;
        drive(radar, 0.1);
        // Wait until the beam is just short of the blip.
        let guard = 0;
        while (clockwise(radar.sweep % 360, blip.bearing) > 6 && guard++ < 400)
            drive(radar, 1 / 30);
        fuzzyCompare(radar.blipLevel(0), floor, 0.02);
        drive(radar, 0.2);
        verify(radar.blipLevel(0) > 0.8, "a blip at " + radar.blipLevel(0) + " as it was swept");
        const swept = radar.blipLevel(0);
        drive(radar, 1);
        const fading = radar.blipLevel(0);
        verify(fading < swept && fading > floor + 0.02, "a blip at " + fading + " a second on");
        drive(radar, 1.1);
        fuzzyCompare(radar.blipLevel(0), floor, 0.02);
    }

    // From a commit until the page paints, the sweep spins fast and the page
    // being opened has a blip of its own that brightens. Once the page has
    // painted, the sweep slows to its pace again.
    function test_navigatingSpinsTheSweepAndBrightensTheArrivingBlip() {
        const radar = makeRadar({
                                    pages: ["https://alpha.example/"]
                                });
        drive(radar, 1);
        compare(radar.arrivingBlip, null);
        const turn = radar.parameters.sweep.turn;

        radar.arriving = "https://new.example/page";
        radar.navigating = 1;
        verify(radar.arrivingBlip !== null);
        compare(radar.arrivingBlip.site, "new.example");
        const levels = [];
        drive(radar, 1.5, function () {
            levels.push(radar.arrival);
        });
        for (let index = 1; index < levels.length; ++index)
            verify(levels[index] >= levels[index - 1], "the arriving blip dimmed");
        verify(levels[0] < 0.2, levels[0]);
        compare(radar.arrival, 1);
        const start = radar.sweep;
        drive(radar, 1);
        verify(radar.sweep - start > 360 / turn * radar.parameters.sweep.spin * 0.8, radar.sweep
               - start + " degrees in a second");

        // Another page being opened brightens from dark.
        radar.arriving = "https://next.example/";
        compare(radar.arrival, 0);
        drive(radar, 0.5);
        verify(radar.arrival > 0.2 && radar.arrival < 1, radar.arrival);

        radar.navigating = 0;
        drive(radar, 1 / 30);
        compare(radar.arrival, 0);
        drive(radar, 3);
        const resting = radar.sweep;
        drive(radar, 1);
        fuzzyCompare(radar.sweep - resting, 360 / turn, 6);
    }

    // A reader who asked for less motion gets one frame that reads on its own:
    // the sweep at one bearing with its afterglow, and every blip lit, and
    // nothing moves, even on commit.
    function test_aStillRadarHoldsOneFrame() {
        const radar = makeRadar({
                                    reducedMotion: true,
                                    navigating: 1,
                                    arriving: "https://new.example/",
                                    pages: ["https://alpha.example/", "https://beta.example/"]
                                });
        compare(radar.sweep, radar.parameters.sweep.still);
        compare(radar.blipLevel(0), 1);
        compare(radar.blipLevel(1), 1);
        drive(radar, 30);
        compare(radar.sweep, radar.parameters.sweep.still);
        compare(radar.arrival, 0);
        compare(radar.blipLevel(0), 1);
        verify(radar.glowing);
    }

    // A Private window's radar has its lights out: the sweep and the rings,
    // with no blips and no glow, its lines the theme's light dimmed toward
    // the ground, casting no light, and it holds still, even on commit.
    function test_anUnlitRadarHasNoBlips() {
        const lit = makeRadar();
        const radar = makeRadar({
                                    unlit: true,
                                    pages: ["https://alpha.example/"]
                                });
        compare(radar.blips.length, 0);
        compare(radar.arrivingBlip, null);
        verify(!radar.glowing);
        compare(radar.light, null);
        compare(radar.sweep, radar.parameters.sweep.still);
        drive(radar, 10);
        radar.arriving = "https://new.example/";
        radar.navigating = 1;
        drive(radar, 3);
        compare(radar.sweep, radar.parameters.sweep.still);
        compare(radar.arrivingBlip, null);
        compare(radar.arrival, 0);

        const ground = lightness(radar.roles.ground);
        const dimmed = lightness(radar.roles.glow);
        verify(dimmed > ground, "lines as dark as the ground");
        verify(Math.abs(lightness(lit.roles.glow) - dimmed) > 0.05, "lines in the accent");
    }

    // The sweep's light on the Omnibar's rim stands where the beam leaves the
    // field, so it rides round the rim as the sweep turns behind it: on the
    // top edge as the beam points up and on the bottom edge as it points down.
    function test_theSweepCatchesTheRimWhereItLeavesTheField() {
        const radar = makeRadar({
                                    width: 1360 / 2,
                                    height: 860 / 2
                                });
        const field = radar.field;
        let top = false;
        let bottom = false;
        drive(radar, 7, function () {
            const bearing = radar.sweep % 360;
            const sun = radar.light.centre;
            const toward = Qt.point(Math.sin(bearing * Math.PI / 180), -Math.cos(bearing * Math.PI
                                                                                 / 180));
            const out = Qt.point(sun.x - radar.centre.x, sun.y - radar.centre.y);
            verify(Math.abs(out.x * toward.y - out.y * toward.x) < 0.01, "a light off the beam");
            verify(out.x * toward.x + out.y * toward.y > 0, "a light behind the beam");
            const edge = Math.min(Math.abs(sun.x - field.x), Math.abs(sun.x - field.x - field.width),
                                  Math.abs(sun.y - field.y), Math.abs(sun.y - field.y
                                                                      - field.height));
            verify(edge < 0.01, "a light " + edge + " off the rim");
            if (bearing < 4 || bearing > 356)
                top = top || Math.abs(sun.y - field.y) < 0.01;
            if (Math.abs(bearing - 180) < 4)
                bottom = bottom || Math.abs(sun.y - field.y - field.height) < 0.01;
        });
        verify(top && bottom, "the light did not come round the rim");
        compare(radar.light.reach, radar.parameters.light.reach);
        compare(radar.light.across, radar.parameters.light.across);
    }

    // What the host draws: the afterglow lit behind the beam and dark ahead
    // of it, a blip bright on the face, and no blip in a Private window.
    function test_theAfterglowTrailsTheBeam() {
        const host = makeHost({
                                  glass: false,
                                  reducedMotion: true
                              });
        const radar = host.sceneItem;
        tryCompare(radar, "width", 800 / radar.pitch);
        radar.pages = ["https://alpha.example/"];
        wait(100);
        let image = grabImage(host);
        // The grab is in the display's pixels.
        const scale = image.width / host.width;
        const sample = function (x, y) {
            return lightness(image.pixel(Math.round(x * scale), Math.round(y * scale)));
        };
        // Between two rings, clear of the rings and the spokes.
        const at = function (bearing) {
            return radar.along(bearing, radar.parameters.grid.ringEvery * 1.5);
        };
        const still = radar.parameters.sweep.still;
        const behind = at(still - 15);
        const ahead = at(still + 105);
        verify(sample(behind.x, behind.y) > sample(ahead.x, ahead.y) + 0.05, "an afterglow "
               + sample(behind.x, behind.y) + " on a face " + sample(ahead.x, ahead.y));

        const blip = radar.blips[0];
        // Beside it, toward the centre, so on the page.
        const toward = Math.hypot(radar.centre.x - blip.x, radar.centre.y - blip.y);
        const face = sample(blip.x + (radar.centre.x - blip.x) / toward * 8 * radar.pitch, blip.y + (
                                radar.centre.y - blip.y) / toward * 8 * radar.pitch);
        verify(sample(blip.x, blip.y) > face + 0.2, "a blip " + sample(blip.x, blip.y)
               + " on a face " + face);

        host.unlit = true;
        wait(100);
        image = grabImage(host);
        verify(sample(blip.x, blip.y) < face + 0.1, "a blip in a Private window");
    }
}
