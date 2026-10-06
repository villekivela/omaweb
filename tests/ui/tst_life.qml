import QtQuick
import QtQuick.Window
import QtTest
import Omaweb

// The rules the Game of Life Scene's board evolves by, on boards small
// enough to write out: a pattern is rows of `O` for a live cell and `.` for a
// dead one.
TestCase {
    id: testCase
    name: "Life"
    when: windowShown

    Component {
        id: boardComponent

        LifeBoard {}
    }

    // A board in a window of its own on screen, so what it draws is drawn.
    Component {
        id: shownComponent

        Window {
            property alias board: board

            width: 200
            height: 200
            visible: true

            LifeBoard {
                id: board
            }
        }
    }

    function boardOf(lines) {
        const board = createTemporaryObject(boardComponent, testCase, {
                                                columns: lines[0].length,
                                                rows: lines.length
                                            });
        verify(board !== null);
        board.place(lines, 0, 0);
        return board;
    }

    function test_aBlinkerTurnsAndTurnsBack() {
        const board = boardOf([".....", "..O..", "..O..", "..O..", "....."]);
        board.step();
        compare(board.picture(), [".....", ".....", ".OOO.", ".....", "....."]);
        board.step();
        compare(board.picture(), [".....", "..O..", "..O..", "..O..", "....."]);
    }

    // A glider comes back to its own shape every four generations, a cell
    // further down and to the right.
    function test_aGliderTravelsDiagonally() {
        const board = boardOf([".O....", "..O...", "OOO...", "......", "......", "......"]);
        for (let generation = 0; generation < 4; ++generation)
            board.step();
        compare(board.picture(), ["......", "..O...", "...O..", ".OOO..", "......", "......"]);
    }

    // The board wraps: what leaves one edge comes back at the opposite one,
    // and cells on opposite edges are neighbours.
    function test_theBoardWraps() {
        const board = boardOf(["......", "......", "......", "....O.", ".....O", "...OOO"]);
        for (let generation = 0; generation < 4; ++generation)
            board.step();
        compare(board.picture(), ["O...OO", "......", "......", "......", ".....O", "O....."]);

        // A block across the four corners stays.
        const corners = boardOf(["O...O", ".....", ".....", ".....", "O...O"]);
        corners.step();
        compare(corners.picture(), ["O...O", ".....", ".....", ".....", "O...O"]);
        const across = boardOf([".....", ".....", "O..OO", ".....", "....."]);
        across.step();
        compare(across.picture(), [".....", "....O", "....O", "....O", "....."]);
    }

    // A cell that has just died leaves a trail for `trail` generations, which
    // the picture numbers by the generations since it died.
    function test_aCellThatDiesLeavesAShortTrail() {
        const board = boardOf(["...", ".O.", "..."]);
        board.trail = 2;
        board.step();
        compare(board.picture(), ["...", ".1.", "..."]);
        board.step();
        compare(board.picture(), ["...", ".2.", "..."]);
        board.step();
        compare(board.picture(), ["...", "...", "..."]);

        // A cell that comes alive again has no trail.
        const blinker = boardOf([".....", "..O..", "..O..", "..O..", "....."]);
        blinker.trail = 2;
        blinker.step();
        blinker.step();
        compare(blinker.picture(), [".....", "..O..", ".1O1.", "..O..", "....."]);
    }

    function near(drawn, colour) {
        const a = Qt.color(drawn);
        const b = Qt.color(colour);
        return Math.abs(a.r - b.r) < 0.02 && Math.abs(a.g - b.g) < 0.02 && Math.abs(a.b - b.b)
                < 0.02;
    }

    // The board draws a cell to each of its pixels, stretched over the item
    // sharp-edged: a live cell in `live`, a trail in the colour for the
    // generations since the cell died, and every other cell in `ground`.
    function test_theBoardDrawsItsCells() {
        const window = createTemporaryObject(shownComponent, testCase);
        verify(window !== null);
        const board = window.board;
        board.columns = 4;
        board.rows = 3;
        board.place(["....", ".O..", "...."], 0, 0);
        board.width = 40;
        board.height = 30;
        board.trail = 2;
        board.ground = "#101010";
        board.live = "#ff8000";
        board.trailColours = ["#804000", "#402000"];
        waitForRendering(board);
        let image = grabImage(board);
        verify(near(image.pixel(15, 15), "#ff8000"), image.pixel(15, 15));
        verify(near(image.pixel(10, 10), "#ff8000"), image.pixel(10, 10));
        verify(near(image.pixel(19, 19), "#ff8000"), image.pixel(19, 19));
        verify(near(image.pixel(9, 15), "#101010"), image.pixel(9, 15));
        verify(near(image.pixel(35, 25), "#101010"), image.pixel(35, 25));

        board.step();
        waitForRendering(board);
        image = grabImage(board);
        verify(near(image.pixel(15, 15), "#804000"), image.pixel(15, 15));
        board.step();
        waitForRendering(board);
        image = grabImage(board);
        verify(near(image.pixel(15, 15), "#402000"), image.pixel(15, 15));
    }
}
