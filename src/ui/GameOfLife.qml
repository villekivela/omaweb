import QtQuick
import Omaweb
import "SceneColour.mjs" as Colour

// Conway's Game of Life, a Start page Scene: a board of cells in the palette,
// a cell to each of the Scene's pixels, evolving behind the Omnibar. At rest
// it moves a generation now and then; from a commit until the page paints,
// a generation a frame. Random soups and a glider gun or two keep it alive,
// and it is seeded again when it dies out or stalls.
//
// It keeps NightRoad.qml's contract with SceneHost.qml: it receives `colors`,
// `dark`, `time`, `navigating`, `beat`, `options`, `reducedMotion` and
// `unlit`, and declares `pitch`, `fps`, `glass` with the glass's amounts as
// `crt`, its options, and `horizonY`, where the Omnibar rests. It casts no
// light. What it draws by is share/scenes/game-of-life.json; the board and
// its rules are LifeBoard's.
Item {
    id: root
    objectName: "gameOfLife"

    // ---- what the host hands it

    // The theme's palette: the board reads `windowOpaque`, `text` and
    // `accent`.
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
    // A still board: a few gliders mid-flight, each with its trail.
    property bool reducedMotion: false
    // A Private window's board is frozen, its cells dimmed.
    property bool unlit: false

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("game-of-life")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options

    // ---- geometry, in logical pixels: the Scene's pixels times its pitch

    readonly property real drawWidth: root.width * root.pitch
    readonly property real drawHeight: root.height * root.pitch
    // Where the Omnibar rests, as it rests on the road's horizon.
    readonly property real horizonY: Math.round(root.drawHeight * root.parameters.rest)

    // ---- the board, a cell to each of the Scene's pixels

    readonly property int columns: Math.max(1, Math.round(root.width))
    readonly property int rows: Math.max(1, Math.round(root.height))

    // ---- what it drew, for the tests

    readonly property int population: board.population
    // Generations the board has moved since it was made.
    property int generation: 0
    // How many times it has been seeded again.
    property int reseeds: 0

    // ---- palette

    // A night in the theme, as the road's. A light theme's night is drawn
    // from its dark text, so the ground stays dark.
    readonly property var roles: {
        const night = root.parameters.night;
        const ground = root.dark ? Qt.color(root.colors.windowOpaque) : Colour.mix(root.colors.text,
                                                                                   "black", night.lightThemeGround);
        const light = Qt.color(root.dark ? root.colors.text : root.colors.windowOpaque);
        const accent = Qt.color(root.colors.accent);
        const cells = root.parameters.cells;
        return {
            ground: ground,
            live: root.unlit ? Colour.mix(light, ground, cells.unlit) : accent,
            trail: root.unlit ? [] : cells.trail.map(function (amount) {
                return Colour.mix(accent, ground, amount);
            })
        };
    }

    // ---- seeding

    // Whether the board has been seeded for the size it has.
    property bool seeded: false
    // Where the soups fall and what is in them: a launch of its own each
    // time, and the same board for the same seed, as a test needs.
    property int seedNumber: Math.floor(Math.random() * 100000)
    // The hashes of the boards it has been through lately, to tell a stall.
    property var lately: []

    // Random soups and a glider gun or two, run a few generations so the
    // board is shown as Life rather than noise. A reader who asked for less
    // motion gets the still frame instead.
    function seed() {
        board.clear();
        root.lately = [];
        root.seeded = root.columns > 1 && root.rows > 1;
        if (!root.seeded)
            return;
        if (root.reducedMotion) {
            root.layStill();
            return;
        }
        const guns = root.placeGuns();
        const s = root.parameters.soups;
        const count = Math.max(1, Math.round(root.columns * root.rows / s.every));
        // Each seeding scatters from indices of its own, past the most soups
        // a seeding tries.
        const tries = count * s.tries;
        const base = (root.seedNumber + root.reseeds) * tries;
        // A soup stands wholly on the board rather than wrapping over an
        // edge, so it is kept clear of a gun however close to an edge.
        const across = Math.max(0, root.columns - s.size);
        const down = Math.max(0, root.rows - s.size);
        let placed = 0;
        for (let index = 0; placed < count && index < tries; ++index) {
            const x = Math.floor(Colour.scatter(base + index, s.across) * across);
            const y = Math.floor(Colour.scatter(base + index, s.down) * down);
            if (guns.some(function (gun) {
                return x + s.size > gun.x && x < gun.x + gun.width && y + s.size > gun.y && y
                        < gun.y + gun.height;
            }))
                continue;
            board.soup(x, y, s.size, s.density, base + index);
            ++placed;
        }
        for (let generation = 0; generation < s.settle; ++generation)
            board.step();
    }

    // Places the glider guns the board has room for, and returns where each
    // stands with its clearance around it.
    function placeGuns() {
        const g = root.parameters.guns;
        const width = g.pattern[0].length;
        const height = g.pattern.length;
        const places = root.columns < width || root.rows < height ? [] : root.columns < g.twoFrom
                                                                    ? g.places.slice(0, 1) :
                                                                      g.places;
        return places.map(function (place) {
            const x = Math.min(root.columns - width, Math.floor(place.across * root.columns));
            const y = Math.min(root.rows - height, Math.floor(place.down * root.rows));
            board.place(g.pattern, x, y, place.turned, place.turned);
            return {
                x: x - g.clear,
                y: y - g.clear,
                width: width + 2 * g.clear,
                height: height + 2 * g.clear
            };
        });
    }

    // The still frame: a few gliders, run a generation or two so each leaves
    // its trail behind it.
    function layStill() {
        const st = root.parameters.still;
        for (const glider of st.gliders)
            board.place(st.glider, Math.floor(glider.across * root.columns), Math.floor(glider.down
                                                                                        * root.rows),
                        glider.mirrored, glider.flipped);
        for (let generation = 0; generation < st.run; ++generation)
            board.step();
    }

    // Moves the board on a generation, and seeds it again if it has died out
    // or come back to a board it was lately.
    function advance() {
        board.step();
        root.generation += 1;
        if (board.population === 0 || root.lately.indexOf(board.hash) >= 0) {
            root.reseeds += 1;
            root.seed();
            return;
        }
        root.lately.push(board.hash);
        if (root.lately.length > root.parameters.stall)
            root.lately.shift();
    }

    // ---- the clock
    //
    // How fast the board moves follows what `navigating` asks: a generation
    // now and then at rest, and one a frame from a commit until the page
    // paints, easing back down after. A still or unlit board does not move.

    property real pace: 0
    // Generations owed: the board moves one when a whole one is due.
    property real due: 0
    property real last: -1

    function move() {
        const g = root.parameters.generations;
        if (root.last < 0)
            root.last = root.time;
        const step = Math.max(0, Math.min(g.longestStep, root.time - root.last));
        root.last = root.time;
        if (root.reducedMotion || root.unlit || !root.seeded) {
            root.pace = 0;
            root.due = 0;
            return;
        }
        // Flat out from the commit's first frame, slowing with the clock
        // once the page has painted.
        if (root.navigating > root.pace)
            root.pace = root.navigating;
        else
            root.pace += (root.navigating - root.pace) * Math.min(1, step * g.ease);
        root.due += step * (g.rest + (g.rush - g.rest) * root.pace);
        // A frame's worth of a whole generation is due within rounding.
        if (root.due >= 1 - 1e-6) {
            root.due = Math.min(1, root.due - 1);
            root.advance();
        }
    }

    onTimeChanged: root.move()
    onReducedMotionChanged: root.seed()
    Component.onCompleted: root.seed()

    LifeBoard {
        id: board
        objectName: "lifeBoard"

        anchors.fill: parent
        columns: root.columns
        rows: root.rows
        ground: root.roles.ground
        live: root.roles.live
        trailColours: root.roles.trail
        // A board given a new size is seeded for it, once its columns and
        // rows have both arrived. SceneHost holds the width while a dragged
        // seam moves, so this is once a resize, not once a frame of it.
        onBoardSizeChanged: if (board.columns === root.columns && board.rows === root.rows)
                                root.seed()
    }
}
