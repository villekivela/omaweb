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
                    Omaweb.GameOfLife {}
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
}
