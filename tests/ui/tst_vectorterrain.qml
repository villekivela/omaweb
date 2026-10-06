import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// The vector terrain under the Start page, as a Scene: where its mountains
// stand against the Omnibar, how its ridges drift and its grid rushes for the
// clock and the reader the host hands it, and what a Private window's terrain
// and a still one show.
TestCase {
    id: testCase
    name: "VectorTerrain"
    when: windowShown
    width: 800
    height: 500

    Component {
        id: terrainComponent

        Omaweb.VectorTerrain {
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
                    Omaweb.VectorTerrain {}
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

    function makeTerrain(properties) {
        const terrain = createTemporaryObject(terrainComponent, testCase, properties || {});
        verify(terrain !== null);
        return terrain;
    }

    // Drives the terrain's clock by hand, a frame at the Scene's rate at a
    // time, and calls `each` on every frame.
    function drive(terrain, seconds, each) {
        const step = 1 / 30;
        for (let elapsed = 0; elapsed < seconds; elapsed += step) {
            terrain.time += step;
            if (each)
                each();
        }
    }

    function lightness(colour) {
        const c = Qt.color(colour);
        return (Math.max(c.r, c.g, c.b) + Math.min(c.r, c.g, c.b)) / 2;
    }

    // Where each ridge stands now and the grid's depth lines: what moves.
    function pose(terrain) {
        const ridges = terrain.parameters.ridges.layers.map(function (layer, index) {
            return terrain.ridgeOffset(index);
        });
        const lines = [];
        for (let index = 0; index < terrain.depthLines; ++index)
            lines.push(terrain.depthLineY(index));
        return {
            ridges: ridges,
            lines: lines
        };
    }

    // The terrain is a Scene the host shows as it shows the road: in its own
    // pixels, through the CRT glass it declares, with the Omnibar resting on
    // the page area's middle. It casts no light on the Omnibar's rim.
    function test_theHostShowsTheTerrainAsAScene() {
        const host = makeHost();
        const terrain = host.sceneItem;
        compare(terrain.objectName, "vectorTerrain");
        compare(terrain.parameters.id, "vector-terrain");
        tryCompare(terrain, "width", 800 / terrain.pitch);
        compare(terrain.horizonY, 250);
        compare(terrain.glass, "crt");
        compare(terrain.crt, terrain.parameters.crt);
        verify(findChild(host, "crtGlass").visible);
        compare(host.light, null);
    }

    // The terrain is drawn from the theme's palette and is always night: a
    // light theme's night is drawn from its dark text, so the sky stays dark
    // and the lines are the accent, and a theme change redraws it.
    function test_theTerrainIsAlwaysNight_data() {
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

    function test_theTerrainIsAlwaysNight(data) {
        const terrain = makeTerrain({
                                        colors: crtRoadKeyColours[data.theme].theme,
                                        dark: data.dark
                                    });
        verify(lightness(terrain.roles.skyTop) < 0.25, terrain.roles.skyTop);
        verify(lightness(terrain.roles.groundNear) < 0.25, terrain.roles.groundNear);
        verify(lightness(terrain.roles.light) > 0.75, terrain.roles.light);
        compare(terrain.roles.glow, Qt.color(crtRoadKeyColours[data.theme].theme.accent));

        const other = data.theme === "dark" ? "light" : "dark";
        terrain.colors = crtRoadKeyColours[other].theme;
        terrain.dark = !data.dark;
        compare(terrain.roles.glow, Qt.color(crtRoadKeyColours[other].theme.accent));
    }

    // The mountains stand on a horizon below the Omnibar, as the sky's planet
    // does: the highest peak a small gap under the resting Omnibar's hint
    // row, and every ridge between there and the ground's horizon, at any
    // size. The grid lies on the ground below them, and the moon hangs low in
    // the sky above them.
    function test_thePeaksStayBelowWhereTheOmnibarRests_data() {
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
                        tag: "thumbnail",
                        width: 512 / 2,
                        height: 320 / 2
                    }
                ];
    }

    function test_thePeaksStayBelowWhereTheOmnibarRests(data) {
        const terrain = makeTerrain({
                                        width: data.width,
                                        height: data.height
                                    });
        const peaks = terrain.parameters.peaks;
        compare(terrain.horizonY, Math.round(terrain.drawHeight / 2));
        compare(terrain.peaksTop, terrain.horizonY + peaks.omnibarReach + peaks.gap);
        verify(terrain.groundY > terrain.peaksTop + 10, terrain.groundY);
        verify(terrain.groundY < terrain.drawHeight - 20, terrain.groundY);
        let highest = terrain.drawHeight;
        terrain.parameters.ridges.layers.forEach(function (layer, index) {
            for (const point of terrain.ridge(index)) {
                verify(point[1] >= terrain.peaksTop, "a peak at " + point[1]);
                verify(point[1] <= terrain.groundY, "a valley at " + point[1]);
                highest = Math.min(highest, point[1]);
            }
        });
        // The mountains rise most of the way to the line they keep under.
        verify(highest < terrain.peaksTop + 0.25 * (terrain.groundY - terrain.peaksTop), highest);

        verify(terrain.moonShown);
        const moon = terrain.moonCentre;
        const r = terrain.moonRadius;
        verify(moon.y + r < terrain.peaksTop, "the moon behind the peaks");
        verify(moon.x + r < terrain.drawWidth, "the moon off the right edge");
    }

    // The moon hangs low in the sky beside the Omnibar where there is room,
    // and otherwise just above it, never behind it.
    function test_theMoonHangsLowBesideTheOmnibar_data() {
        return [
                    {
                        tag: "page area",
                        width: 1360 / 2,
                        height: 860 / 2,
                        beside: true
                    },
                    {
                        tag: "wide window",
                        width: 1800 / 2,
                        height: 1100 / 2,
                        beside: true
                    },
                    {
                        tag: "small window",
                        width: 800 / 2,
                        height: 500 / 2,
                        beside: false
                    },
                    {
                        tag: "tall window",
                        width: 900 / 2,
                        height: 1400 / 2,
                        beside: false
                    }
                ];
    }

    function test_theMoonHangsLowBesideTheOmnibar(data) {
        const terrain = makeTerrain({
                                        width: data.width,
                                        height: data.height
                                    });
        const moon = terrain.moonCentre;
        const r = terrain.moonRadius;
        const field = terrain.omnibar;
        const half = Math.min(terrain.drawWidth - 32, 720) / 2;
        compare(field.x, terrain.drawWidth / 2 - half);
        compare(field.y, terrain.horizonY - 50);
        compare(field.y + field.height, terrain.horizonY + terrain.omnibarReach);
        const clear = moon.x - r > field.x + field.width || moon.y + r < field.y;
        verify(clear, "the moon at " + moon + " behind the Omnibar at " + field);
        verify(moon.x + r < terrain.drawWidth, "the moon off the right edge");
        if (data.beside) {
            verify(moon.x - r > field.x + field.width, "the moon not beside the Omnibar");
            // Low: level with the Omnibar, not above it.
            verify(moon.y > field.y, "the moon high in the sky");
        } else {
            verify(moon.y + r < field.y, "the moon not above the Omnibar");
            verify(moon.y + r > field.y - 2 * terrain.parameters.peaks.gap, "the moon high above "
                   + "the Omnibar");
        }
    }

    // At rest the ridges drift by in slow parallax, the nearer faster than
    // the farther, and the grid holds still.
    function test_theRidgesDriftSlowlyAtRest() {
        const terrain = makeTerrain();
        drive(terrain, 1);
        const before = pose(terrain);
        drive(terrain, 10);
        const after = pose(terrain);
        compare(after.lines, before.lines);
        const moved = after.ridges.map(function (offset, index) {
            const travelled = offset - before.ridges[index];
            return travelled < 0 ? travelled + terrain.drawWidth : travelled;
        });
        for (let index = 0; index < moved.length; ++index) {
            verify(moved[index] > 0, "ridge " + index + " stood still");
            verify(moved[index] < terrain.drawWidth * 0.2, "ridge " + index + " moved "
                   + moved[index] + " in ten seconds");
            if (index > 0)
                verify(moved[index] > moved[index - 1], "ridge " + index
                       + " no faster than the one behind it");
        }
    }

    // From a commit until the page paints, the grid's lines rush toward the
    // reader while the mountains hold the horizon, drifting as they did at
    // rest. Once the page has painted the grid slows to a stop again.
    function test_navigatingRushesTheGridTowardTheReader() {
        const terrain = makeTerrain();
        drive(terrain, 1);
        const ground = terrain.groundY;
        const ridge = String(terrain.ridge(1));
        const resting = terrain.ridgeOffset(1);
        drive(terrain, 1);
        const restingDrift = terrain.ridgeOffset(1) - resting;

        terrain.navigating = 1;
        const start = terrain.travel;
        drive(terrain, 1.5);
        verify(terrain.travel - start > terrain.parameters.grid.rush * 0.6, terrain.travel - start
               + " in a second and a half");
        // A line not about to wrap round moves down, toward the reader, on
        // each frame.
        let toward = 0;
        for (let frame = 0; frame < 10; ++frame) {
            const before = pose(terrain).lines;
            drive(terrain, 1 / 30);
            const after = pose(terrain).lines;
            for (let index = 0; index < before.length; ++index)
                if (after[index] > before[index])
                    toward += 1;
                else
                    verify(after[index] < terrain.groundY + (terrain.drawHeight - terrain.groundY)
                           * 0.2, "a line came back from the reader to " + after[index]);
        }
        verify(toward > terrain.depthLines * 5, toward + " lines moved toward the reader");

        compare(terrain.groundY, ground);
        compare(String(terrain.ridge(1)), ridge);
        const rushing = terrain.ridgeOffset(1);
        drive(terrain, 1);
        fuzzyCompare(terrain.ridgeOffset(1) - rushing, restingDrift, 0.01);

        terrain.navigating = 0;
        drive(terrain, 3);
        verify(terrain.speed < terrain.parameters.grid.rush * 0.05, terrain.speed);
    }

    // A reader who asked for less motion gets one frame that reads on its own:
    // the mountains, the grid and the moon, and nothing moves, even on
    // commit.
    function test_aStillTerrainHoldsOneFrame() {
        const terrain = makeTerrain({
                                        reducedMotion: true,
                                        navigating: 1
                                    });
        verify(terrain.moonShown);
        verify(terrain.glowing);
        verify(terrain.depthLines > 0);
        const still = pose(terrain);
        drive(terrain, 30);
        compare(pose(terrain), still);
    }

    // A Private window's terrain has its lights out: the wireframe alone, no
    // moon and no glow, its lines dimmed from the accent toward the ground,
    // and it holds still, even on commit.
    function test_anUnlitTerrainIsTheWireframeOnly() {
        const lit = makeTerrain();
        const terrain = makeTerrain({
                                        unlit: true
                                    });
        verify(!terrain.moonShown);
        verify(!terrain.glowing);
        verify(terrain.depthLines > 0);
        const still = pose(terrain);
        drive(terrain, 30);
        terrain.navigating = 1;
        drive(terrain, 3);
        compare(pose(terrain), still);

        const ground = lightness(terrain.roles.ground);
        const dimmed = lightness(terrain.roles.glow);
        verify(dimmed > ground, "lines as dark as the ground");
        verify(Math.abs(lightness(lit.roles.glow) - dimmed) > 0.05, "lines in the accent");
    }

    // What the host draws: the moon as a thin line, its lit limb bright,
    // with a vector monitor's glow around it on the night sky; and in a
    // Private window, no moon at all.
    function test_theLinesGlowOnTheNight() {
        const host = makeHost({
                                  glass: false
                              });
        const terrain = host.sceneItem;
        tryCompare(terrain, "width", 800 / terrain.pitch);
        wait(100);
        let image = grabImage(host);
        // The grab is in the display's pixels.
        const scale = image.width / host.width;
        const sample = function (x, y) {
            return lightness(image.pixel(Math.round(x * scale), Math.round(y * scale)));
        };
        const centre = terrain.moonCentre;
        const radius = terrain.moonRadius;
        const pixel = terrain.pitch;
        // Across the lit limb, on the moon's middle row.
        let brightest = 0;
        for (let x = centre.x - radius - 2 * pixel; x <= centre.x - radius + 2 * pixel; x += 1)
            brightest = Math.max(brightest, sample(x, centre.y));
        const sky = sample(centre.x - radius - 30 * pixel, centre.y);
        const glow = sample(centre.x - radius - 3 * pixel, centre.y);
        verify(brightest > sky + 0.3, "a limb " + brightest + " on a sky " + sky);
        verify(glow > sky + 0.02, "a glow " + glow + " beside the limb, on a sky " + sky);

        host.unlit = true;
        wait(100);
        image = grabImage(host);
        brightest = 0;
        for (let x = centre.x - radius - 2 * pixel; x <= centre.x - radius + 2 * pixel; x += 1)
            brightest = Math.max(brightest, sample(x, centre.y));
        verify(brightest < sample(centre.x - radius - 30 * pixel, centre.y) + 0.02,
               "a moon in a Private window");
    }
}
