import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// The CRT road under the Start page, as a Scene: the colours it shares with the
// website, what a Private window's road leaves out, and how it moves for the
// clock and the reader the host hands it.
TestCase {
    id: testCase
    name: "NightRoad"
    when: windowShown
    width: 800
    height: 500

    Component {
        id: roadComponent

        Omaweb.NightRoad {
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
                    Omaweb.NightRoad {}
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

    // Drives a road's clock by hand, a frame at the Scene's rate at a time.
    function drive(road, seconds) {
        const step = 1 / 30;
        for (let elapsed = 0; elapsed < seconds; elapsed += step)
            road.time += step;
    }

    function makeRoad(properties) {
        const road = createTemporaryObject(roadComponent, testCase, properties || {});
        verify(road !== null);
        return road;
    }

    function hex(colour) {
        const c = Qt.color(colour);
        return [c.r, c.g, c.b];
    }

    // Within one step of a channel: the website rounds to bytes as it draws,
    // and Qt keeps a little more.
    function compareColour(actual, expected, what) {
        const a = hex(actual);
        const e = hex(expected);
        for (let channel = 0; channel < 3; ++channel)
            verify(Math.abs(a[channel] - e[channel]) <= 1.01 / 255, what + ": " + actual
                   + " against " + expected);
    }

    function keyColours(road) {
        const p = road.parameters;
        return {
            skyTop: road.roles.skyTop,
            skyLow: road.roles.skyLow,
            light: road.roles.light,
            sunTop: road.roles.sunTop,
            sunLow: road.roles.sunLow,
            ridges: p.ridges.layers.map(function (layer) {
                return road.colour(layer.tone);
            }),
            desertNear: road.colour(p.desert.stops[0][1]),
            centreLine: road.colour(p.centreLine.colour),
            roadside: road.colour(p.roadside.colour)
        };
    }

    // tests/scenes/crt-road-key-colours.json is what the website draws, held
    // there by its own test; the app's road, reading the same parameter file,
    // has to draw the same.
    function test_theKeyColoursAreTheWebsites_data() {
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

    function test_theKeyColoursAreTheWebsites(data) {
        const expected = crtRoadKeyColours[data.theme];
        const road = makeRoad({
                                  colors: expected.theme,
                                  dark: data.dark
                              });
        const actual = keyColours(road);
        for (const name in expected.colours) {
            if (name === "ridges") {
                for (let index = 0; index < 3; ++index)
                    compareColour(actual.ridges[index], expected.colours.ridges[index], "ridge "
                                  + index);
            } else {
                compareColour(actual[name], expected.colours[name], name);
            }
        }
    }

    // The road lights the Omnibar's rim as the website's does, from the same
    // file: the same stops on the same ellipse centred on the sun, and the
    // same bloom, at rest and on a full beat.
    function test_theRimLightIsTheWebsites_data() {
        return test_theKeyColoursAreTheWebsites_data();
    }

    function test_theRimLightIsTheWebsites(data) {
        const expected = crtRoadKeyColours[data.theme].rim;
        const road = makeRoad({
                                  colors: crtRoadKeyColours[data.theme].theme,
                                  dark: data.dark
                              });
        const light = road.light;
        compare(light.centre, Qt.point(road.drawWidth / 2, road.horizonY));
        fuzzyCompare(light.across, expected.across, 0.001);
        fuzzyCompare(light.reach / road.drawHeight, expected.reach, 0.001);
        compare(light.stops.length, expected.stops.length);
        for (let index = 0; index < expected.stops.length; ++index) {
            const stop = light.stops[index];
            fuzzyCompare(stop.position, expected.stops[index][0], 0.001);
            compareColour(stop.colour, expected.stops[index][1], "stop " + index);
            fuzzyCompare(stop.colour.a, expected.stops[index][2], 0.01);
        }
        fuzzyCompare(light.bloom.opacity, expected.bloom.rest, 0.001);
        compare(light.bloom.width, expected.bloom.width);
        compare(light.bloom.blur, expected.bloom.blur);
        road.beat = 1;
        fuzzyCompare(road.light.bloom.opacity, expected.bloom.lifted, 0.001);
    }

    Component {
        id: litPlateComponent

        Window {
            property alias plate: plate
            property alias rim: rim

            width: 800
            height: 500
            visible: true

            Rectangle {
                id: plate
                x: 200
                y: 220
                width: 400
                height: 60
            }

            Omaweb.RimLight {
                id: rim
                plate: plate
                plateRadius: 3
                sun: Qt.point(400, 252)
            }
        }
    }

    // What is drawn is the road's light on the plate it falls on: the
    // ellipse 78% of the plate's width across and two sun radii down,
    // centred on the sun, and the website's first stop, premultiplied as the
    // shader blends it.
    function test_theRimIsDrawnOnTheWebsitesEllipse() {
        const expected = crtRoadKeyColours.dark.rim;
        const road = makeRoad({
                                  height: 125
                              });
        const lit = createTemporaryObject(litPlateComponent, testCase);
        const rim = lit.rim;
        rim.light = road.light;
        verify(rim.visible);
        const reach = rim.reach;
        compare(rim.x, 200 - reach);
        compare(rim.y, 220 - reach);
        compare(rim.plateArea, Qt.vector4d(reach, reach, 400, 60));
        compare(rim.sunCentre, Qt.point(400 - rim.x, 252 - rim.y));
        fuzzyCompare(rim.radii.width, expected.across * 400, 0.01);
        fuzzyCompare(rim.radii.height, expected.reach * road.drawHeight, 0.01);
        const first = Qt.color(expected.stops[0][1]);
        fuzzyCompare(rim.stop0.x, first.r * expected.stops[0][2], 1.01 / 255);
        fuzzyCompare(rim.stop0.w, expected.stops[0][2], 0.001);
        const last = expected.stops[expected.stops.length - 1];
        fuzzyCompare(rim.stop4.w, last[2], 0.01);
        fuzzyCompare(rim.stop4.x, Qt.color(last[1]).r * last[2], 1.01 / 255);
        compare(rim.stopCount, expected.stops.length);
        compare(rim.bloomOpacity, expected.bloom.rest);

        // The bloom's inner half is drawn from inside the plate.
        rim.inner = true;
        compare(rim.x, -reach);
        compare(rim.innerHalf, 1);

        rim.light = null;
        verify(!rim.visible);
    }

    // The browser plays no music, but a beat handed to the road lifts the
    // rim's bloom; reduced motion holds it at rest whatever the beat.
    function test_reducedMotionHoldsTheRimStill() {
        const road = makeRoad({
                                  beat: 0.8
                              });
        const rest = road.parameters.light.bloom.rest;
        verify(road.light.bloom.opacity > rest);
        road.reducedMotion = true;
        fuzzyCompare(road.light.bloom.opacity, rest, 0.001);
    }

    // The road on screen, not only its recipes: the centre line's nearest
    // full mark and the top of the sun, drawn in the website's colours.
    Component {
        id: shownRoadComponent

        Window {
            property alias road: road

            width: 200
            height: 125
            visible: true

            Omaweb.NightRoad {
                id: road
                width: 200
                height: 125
                colors: crtRoadKeyColours.dark.theme
            }
        }
    }

    function test_theRoadIsDrawnInTheWebsitesKeyColours() {
        const window = createTemporaryObject(shownRoadComponent, testCase);
        const road = window.road;
        const expected = crtRoadKeyColours.dark.colours;
        tryVerify(function () {
            return window.visible && road.width === 200;
        });
        wait(200);
        const image = grabImage(window.contentItem);
        const scale = image.width / window.width;
        const pixel = function (x, y) {
            return image.pixel(Math.floor(x * scale), Math.floor(y * scale));
        };
        // At rest the second mark is the nearest that is fully lit, from
        // depth 2.15 to 2.57.
        const line = road.parameters.centreLine;
        const depth = 1 + line.spacing + line.length / 2;
        const markY = (road.horizonY + road.depth / depth) / road.pitch;
        compareColour(pixel(road.width / 2, markY), expected.centreLine, "centre line");
        // A Scene pixel down from the sun's top, where its gradient has barely
        // begun.
        const sunTop = (road.horizonY - road.drawHeight * road.parameters.sun.radius) / road.pitch
              + 1.5;
        const drawn = Qt.color(pixel(road.width / 2, sunTop));
        const sun = Qt.color(expected.sunTop);
        verify(Math.abs(drawn.r - sun.r) < 0.04 && Math.abs(drawn.g - sun.g) < 0.04 && Math.abs(
                   drawn.b - sun.b) < 0.04, "sun top: " + drawn + " against " + sun);
    }

    // The host sizes the Scene in its own pixels and hands it the palette and
    // the options it declares.
    function test_theHostSizesTheSceneAtItsPitch() {
        const host = makeHost();
        const road = host.sceneItem;
        tryCompare(road, "width", 200);
        compare(road.height, 125);
        compare(road.colors, host.colors);
        verify(road.dark);
        compare(road.options.bands, "4");
        compare(road.options.road, "widest");
    }

    function test_theHostKeepsTheClockOnlyWhileRunning() {
        const host = makeHost();
        wait(200);
        compare(host.frames, 0);
        compare(host.sceneItem.time, 0);

        host.running = true;
        tryVerify(function () {
            return host.frames > 2;
        });
        verify(host.sceneItem.time > 0);
        verify(host.sceneItem.travel > 0);

        host.running = false;
        const frames = host.frames;
        const time = host.sceneItem.time;
        wait(200);
        compare(host.frames, frames);
        compare(host.sceneItem.time, time);
    }

    // The road needs thirty frames a second and the host draws no more.
    function test_theHostDrawsAtTheScenesRate() {
        const host = makeHost({
                                  running: true
                              });
        tryVerify(function () {
            return host.frames > 0;
        });
        const frames = host.frames;
        wait(1000);
        verify(host.frames - frames <= 34, host.frames - frames + " frames in a second");
    }

    // Navigating speeds the drive up and lights the sun; it eases back after.
    function test_navigatingSpeedsTheDriveUp() {
        const road = makeRoad();
        drive(road, 0.5);
        compare(road.speed, 1);
        road.navigating = 1;
        drive(road, 1.5);
        verify(road.speed > 3, road.speed);
        verify(road.sunUp > 0.9, road.sunUp);
        road.navigating = 0;
        drive(road, 3);
        verify(road.speed < 1.1, road.speed);
    }

    // A reader who asked for less motion gets one frame that reads on its own:
    // the host keeps no clock and hands the Scene no navigating.
    function test_aStillRoadHoldsItsPlace() {
        const host = makeHost({
                                  running: true,
                                  reducedMotion: true,
                                  navigating: 1
                              });
        const road = host.sceneItem;
        verify(road.reducedMotion);
        compare(road.navigating, 0);
        wait(200);
        compare(host.frames, 0);
        compare(road.travel, road.parameters.motion.stillTravel);
    }

    // The glass is the Scene's to declare and the reader's to turn off; off,
    // the road is its plain pixels.
    function test_theGlassCanBeTurnedOff() {
        const host = makeHost();
        const glass = findChild(host, "crtGlass");
        const plain = findChild(host, "sceneDisplay");
        verify(glass.visible);
        verify(!plain.visible);
        host.glass = false;
        verify(!glass.visible);
        verify(plain.visible);
    }

    // A Scene that declares the glass with amounts of its own, which no
    // amount in the host could match by chance.
    Component {
        id: oddGlassScene

        Item {
            property var colors
            property bool dark
            property real time
            property real navigating
            property real beat
            property var options
            property bool reducedMotion
            property bool unlit
            readonly property int pitch: 4
            readonly property int fps: 30
            readonly property string glass: "crt"
            readonly property var declaredOptions: ({})
            readonly property var crt: ({
                                            bloom: {
                                                scale: 5,
                                                opacity: 0.31
                                            },
                                            scanlines: {
                                                every: 4,
                                                shade: 0.27
                                            },
                                            vignette: {
                                                clear: 0.61,
                                                shade: 0.43
                                            },
                                            band: {
                                                every: 9,
                                                from: -0.1,
                                                travel: 1.2,
                                                reach: 0.11,
                                                reachAtLeast: 3,
                                                strength: 0.08
                                            },
                                            flicker: {
                                                least: 0.07,
                                                range: 0.01,
                                                frequencies: [5, 3]
                                            }
                                        })
        }
    }

    // The glass draws by the amounts its Scene declares, and the road declares
    // the shared file's `crt` block, which the website's glass draws by too.
    function test_theGlassTakesItsAmountsFromTheScene() {
        compare(makeRoad().crt, makeRoad().parameters.crt);

        const host = makeHost({
                                  running: true,
                                  scene: oddGlassScene
                              });
        tryVerify(function () {
            return host.sceneItem !== null && host.sceneItem.crt !== undefined;
        });
        const glass = findChild(host, "crtGlass");
        tryCompare(host, "sceneHeight", 125);
        compare(glass.bloomMix, 0.31);
        compare(glass.scanEvery, 4);
        compare(glass.scanShade, 0.27);
        compare(glass.vignetteClear, 0.61);
        compare(glass.vignetteShade, 0.43);
        compare(glass.bandStrength, 0.08);
        compare(glass.bandReach, 125 * 0.11);
        compare(findChild(host, "bloomPicture").textureSize.width, 40);
        tryVerify(function () {
            return host.frames > 2;
        });
        verify(glass.flicker >= 0.07 && glass.flicker <= 0.08, glass.flicker);
    }

    // A Private window's road has its lights off: no sun, no stars and no
    // centre line, and the rest of the night still there.
    function test_anUnlitRoadHasItsLightsOff() {
        const lit = makeRoad();
        const unlit = makeRoad({
                                   unlit: true
                               });
        verify(lit.sunShown);
        verify(lit.stars > 0);
        verify(lit.centreMarks > 0);
        verify(!unlit.sunShown);
        compare(unlit.stars, 0);
        compare(unlit.centreMarks, 0);
        verify(unlit.roadsideMarks > 0);
    }
}
