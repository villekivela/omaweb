import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// Conway's Game of Life under the Start page, as a Scene: how fast its board
// evolves for the clock and the reader the host hands it, how it stays alive,
// and what a Private window's board and a still one show.
TestCase {
    id: testCase
    name: "GameOfLife"
    when: windowShown
    width: 800
    height: 500

    Component {
        id: lifeComponent

        Omaweb.GameOfLife {
            width: 200
            height: 125
            colors: crtRoadKeyColours.dark.theme
            seedNumber: 0
        }
    }

    // A board as a launch makes it, with a seed of its own.
    Component {
        id: launchedComponent

        Omaweb.GameOfLife {
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
                    Omaweb.GameOfLife {
                        seedNumber: 0
                    }
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

    function makeLife(properties) {
        const life = createTemporaryObject(lifeComponent, testCase, properties || {});
        verify(life !== null);
        return life;
    }

    // The board is a Scene the host shows as it shows the road: in its own
    // pixels, a cell to each, through the CRT glass it declares, with the
    // Omnibar resting on the page area's middle. It casts no light on the
    // Omnibar's rim.
    function test_theHostShowsLifeAsAScene() {
        const host = makeHost();
        const life = host.sceneItem;
        compare(life.objectName, "gameOfLife");
        compare(life.parameters.id, "game-of-life");
        tryCompare(life, "width", 800 / life.pitch);
        compare(life.columns, 800 / life.pitch);
        compare(life.rows, Math.ceil(500 / life.pitch));
        compare(life.horizonY, 250);
        compare(life.glass, "crt");
        compare(life.crt, life.parameters.crt);
        verify(findChild(host, "crtGlass").visible);
        compare(host.light, null);
    }

    // Drives the board's clock by hand, a frame at the Scene's rate at a
    // time, and calls `each` on every frame.
    function drive(life, seconds, each) {
        const step = 1 / 30;
        for (let elapsed = 0; elapsed < seconds; elapsed += step) {
            life.time += step;
            if (each)
                each();
        }
    }

    // At rest the board evolves about two generations a second.
    function test_theBoardEvolvesSlowlyAtRest() {
        const life = makeLife();
        verify(life.population > 0);
        const start = life.generation;
        drive(life, 10);
        const generations = life.generation - start;
        verify(generations >= 18 && generations <= 22, generations + " generations in ten seconds");
    }

    // Generations the board moves over `seconds` of its clock.
    function generationsIn(life, seconds) {
        const start = life.generation;
        drive(life, seconds);
        return life.generation - start;
    }

    // From a commit until the page paints, generations run flat out, one a
    // frame from the first, and once it has painted they slow to the resting
    // pace again.
    function test_navigatingRunsGenerationsFlatOut() {
        const life = makeLife();
        drive(life, 1);
        life.navigating = 1;
        let frames = 0;
        const start = life.generation;
        drive(life, 1, function () {
            frames += 1;
        });
        compare(life.generation - start, frames);
        life.navigating = 0;
        drive(life, 4);
        const resting = generationsIn(life, 5);
        verify(resting >= 9 && resting <= 11, resting + " generations in five seconds");
    }

    // Drives the clock until the board has moved a generation.
    function nextGeneration(life) {
        const start = life.generation;
        for (let frame = 0; frame < 60 && life.generation === start; ++frame)
            life.time += 1 / 30;
        compare(life.generation, start + 1);
    }

    // A board that dies out is seeded again.
    function test_aBoardThatDiesOutIsSeededAgain() {
        const life = makeLife();
        const board = findChild(life, "lifeBoard");
        board.clear();
        board.place(["O"], 10, 10);
        nextGeneration(life);
        compare(life.reseeds, 1);
        verify(life.population > 0);
    }

    // A board that stalls, coming back to a board it was lately, is seeded
    // again: a still life on its second generation, and a blinker on its
    // third.
    function test_aBoardThatStallsIsSeededAgain() {
        const life = makeLife();
        const board = findChild(life, "lifeBoard");
        board.clear();
        board.place(["OO", "OO"], 10, 10);
        nextGeneration(life);
        compare(life.reseeds, 0);
        nextGeneration(life);
        compare(life.reseeds, 1);
        verify(board.picture().join("").indexOf("O") >= 0);
        verify(life.population !== 4);

        board.clear();
        board.place(["OOO"], 10, 10);
        nextGeneration(life);
        nextGeneration(life);
        compare(life.reseeds, 1);
        nextGeneration(life);
        compare(life.reseeds, 2);
    }

    // Whether `picture` holds `pattern`'s live cells with its top left corner
    // at `x`, `y`, turned about when `turned`.
    function holds(picture, pattern, x, y, turned) {
        const height = pattern.length;
        const width = pattern[0].length;
        for (let row = 0; row < height; ++row)
            for (let column = 0; column < width; ++column) {
                const live = pattern[turned ? height - 1 - row : row][turned ? width - 1 - column :
                                                                               column] === "O";
                if (live !== (picture[y + row][x + column] === "O"))
                    return false;
            }
        return true;
    }

    // The soup is fed by Gosper glider guns: two on a page area's board, one
    // where there is room for only one, the second turned about to fire the
    // other way. A gun's period is thirty generations, so the board shows it
    // as placed once it has settled.
    function test_gliderGunsFeedTheSoup() {
        const gun = makeLife().parameters.guns.pattern;
        const wide = makeLife({
                                  width: 300,
                                  height: 200
                              });
        let picture = findChild(wide, "lifeBoard").picture();
        verify(holds(picture, gun, 18, 20, false), "no gun at the top left");
        verify(holds(picture, gun, 210, 156, true), "no gun turned about at the bottom right");

        const narrow = makeLife({
                                    width: 150,
                                    height: 100
                                });
        picture = findChild(narrow, "lifeBoard").picture();
        verify(holds(picture, gun, 9, 10, false), "no gun at the top left");
        verify(!holds(picture, gun, 105, 78, true), "a second gun");
    }

    // A reader who asked for less motion gets one board that reads on its
    // own: a few gliders mid-flight, each with the trail it leaves, and
    // nothing moves, even on commit.
    function test_aStillBoardHoldsAFewGlidersMidFlight() {
        const life = makeLife({
                                  reducedMotion: true,
                                  navigating: 1
                              });
        const board = findChild(life, "lifeBoard");
        const picture = board.picture();
        const gliders = life.parameters.still.gliders.length;
        verify(gliders >= 3 && gliders <= 6, gliders + " gliders");
        // A glider is five live cells, whatever its phase.
        compare(life.population, gliders * 5);
        verify(picture.join("").indexOf("1") >= 0, "no trail");
        drive(life, 60);
        compare(life.generation, 0);
        compare(board.picture(), picture);
    }

    function lightness(colour) {
        const c = Qt.color(colour);
        return (Math.max(c.r, c.g, c.b) + Math.min(c.r, c.g, c.b)) / 2;
    }

    // A Private window's board has its lights out: frozen, even on commit,
    // its cells dimmed from the accent to a faint light, with no trail.
    function test_anUnlitBoardIsFrozenAndDimmed() {
        const lit = makeLife();
        const life = makeLife({
                                  unlit: true
                              });
        const board = findChild(life, "lifeBoard");
        verify(life.population > 0);
        const picture = board.picture();
        drive(life, 30);
        life.navigating = 1;
        drive(life, 3);
        compare(life.generation, 0);
        compare(board.picture(), picture);

        const ground = lightness(board.ground);
        const dimmed = lightness(board.live);
        verify(dimmed > ground, "cells as dark as the ground");
        verify(dimmed - ground < lightness(findChild(lit, "lifeBoard").live) - ground,
               "cells as bright as the accent");
        compare(board.trail, 0);
    }

    // The board is drawn from the theme's palette and is always night: a
    // light theme's ground is drawn from its dark text, live cells are the
    // accent and a trail fades from it toward the ground, and a theme change
    // redraws it.
    function test_theBoardIsAlwaysNight_data() {
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

    function test_theBoardIsAlwaysNight(data) {
        const life = makeLife({
                                  colors: crtRoadKeyColours[data.theme].theme,
                                  dark: data.dark
                              });
        const board = findChild(life, "lifeBoard");
        verify(lightness(board.ground) < 0.25, board.ground);
        compare(board.live, Qt.color(crtRoadKeyColours[data.theme].theme.accent));
        compare(board.trail, board.trailColours.length);
        verify(board.trail >= 1 && board.trail <= 3, board.trail + " generations of trail");
        let brighter = lightness(board.live);
        for (const colour of board.trailColours) {
            const fading = lightness(colour);
            verify(fading < brighter && fading > lightness(board.ground), colour);
            brighter = fading;
        }

        const other = data.theme === "dark" ? "light" : "dark";
        life.colors = crtRoadKeyColours[other].theme;
        life.dark = !data.dark;
        compare(board.live, Qt.color(crtRoadKeyColours[other].theme.accent));
    }

    // The host shows each cell as a square of the Scene's pitch, sharp-edged,
    // in the accent on the ground.
    function test_theHostDrawsCellsAsSquarePixels() {
        const host = makeHost({
                                  glass: false
                              });
        const life = host.sceneItem;
        tryCompare(life, "width", 800 / life.pitch);
        const board = findChild(life, "lifeBoard");
        board.clear();
        board.place(["O"], 20, 10);
        wait(100);
        const image = grabImage(host);
        const pitch = life.pitch;
        const live = Qt.color(board.live);
        const ground = Qt.color(board.ground);
        const near = function (x, y, colour) {
            const drawn = Qt.color(image.pixel(x, y));
            return Math.abs(drawn.r - colour.r) < 0.05 && Math.abs(drawn.g - colour.g) < 0.05
                    && Math.abs(drawn.b - colour.b) < 0.05;
        };
        for (const corner of [[0, 0], [pitch - 1, 0], [0, pitch - 1], [pitch - 1, pitch - 1]])
            verify(near(20 * pitch + corner[0], 10 * pitch + corner[1], live), corner);
        verify(near(20 * pitch - 1, 10 * pitch, ground));
        verify(near(21 * pitch, 10 * pitch, ground));
        verify(near(20 * pitch, 11 * pitch, ground));
    }

    // A board given a new size, as a page area laid out late or a window
    // resized, is seeded again for it, so it is never a small board in a
    // corner of an empty one.
    function test_aResizedBoardIsSeededForItsSize() {
        const gun = makeLife().parameters.guns.pattern;
        const life = makeLife({
                                  width: 40,
                                  height: 30
                              });
        life.width = 300;
        life.height = 200;
        const picture = findChild(life, "lifeBoard").picture();
        compare(picture.length, 200);
        verify(holds(picture, gun, 18, 20, false), "no gun at the top left");
        verify(holds(picture, gun, 210, 156, true), "no gun turned about at the bottom right");
    }

    // Each launch opens on a board of its own: the soups fall in other
    // places, with other cells in them.
    function test_eachLaunchOpensOnABoardOfItsOwn() {
        const one = createTemporaryObject(launchedComponent, testCase);
        const other = createTemporaryObject(launchedComponent, testCase);
        verify(one.population > 0 && other.population > 0);
        const picture = function (life) {
            return String(findChild(life, "lifeBoard").picture());
        };
        verify(picture(one) !== picture(other));
    }
}
