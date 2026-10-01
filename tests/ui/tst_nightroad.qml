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
