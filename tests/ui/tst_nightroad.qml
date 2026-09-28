import QtQuick
import QtTest
import "../../src/ui" as Omaweb

// The road under the Start page: what it draws from the palette, what a
// Private window's road leaves out, and that it moves only while it is asked
// to.
TestCase {
    id: testCase
    name: "NightRoad"
    when: windowShown
    width: 800
    height: 500

    readonly property var darkColors: ({
                                           text: "#f3f1fa",
                                           accent: "#9b87ff",
                                           windowOpaque: "#16151d"
                                       })
    readonly property var lightColors: ({
                                            text: "#1c1b22",
                                            accent: "#3366cc",
                                            windowOpaque: "#f4f3f8"
                                        })

    Component {
        id: roadComponent

        Omaweb.NightRoad {
            width: testCase.width
            height: testCase.height
            colors: testCase.darkColors
        }
    }

    function makeRoad(properties) {
        const road = createTemporaryObject(roadComponent, testCase, properties || {});
        verify(road !== null);
        return road;
    }

    function framesOver(road, milliseconds) {
        const before = road.frames;
        wait(milliseconds);
        return road.frames - before;
    }

    function test_theDisplayIsLitInTheAccentAndFollowsIt() {
        const road = makeRoad();
        compare(String(road.glow), String(testCase.darkColors.accent));
        road.colors = {
            text: "#f3f1fa",
            accent: "#ff5500",
            windowOpaque: "#16151d"
        };
        compare(String(road.glow), "#ff5500");
    }

    // A light theme's road is still night: its ground comes from the dark
    // text and its lit lines from the light window colour.
    function test_aLightThemeDrawsANightRoad() {
        const road = makeRoad({
                                  colors: testCase.lightColors
                              });
        verify(road.lightTheme);
        verify(Qt.color(road.ground).hslLightness < 0.3);
        verify(Qt.color(road.light).hslLightness > 0.8);
    }

    function test_aPrivateRoadHasItsLightsOff() {
        const lit = makeRoad();
        const unlit = makeRoad({
                                   privateWindow: true
                               });
        verify(findChild(unlit, "nightRoadDisplay") !== null);
        // No stars, no lane marks, no posts: only the grid still moves.
        verify(lit.movingMarks > unlit.movingMarks);
        compare(unlit.stars, 0);
        verify(lit.stars > 0);
        verify(String(unlit.glow) !== String(testCase.darkColors.accent));
    }

    function test_theRoadMovesOnlyWhileRunning() {
        const road = makeRoad();
        compare(framesOver(road, 200), 0);

        road.running = true;
        tryVerify(function () {
            return road.frames > 2;
        });
        verify(road.travel > 0);

        road.running = false;
        wait(50);
        const stopped = road.travel;
        compare(framesOver(road, 200), 0);
        compare(road.travel, stopped);
    }

    function test_drivingSpeedsTheRoadUpAndStoppingSettlesIt() {
        const road = makeRoad({
                                  running: true
                              });
        road.driving = true;
        tryVerify(function () {
            return road.speed > 3;
        });
        road.running = false;
        compare(road.speed, 1);
    }
}
