import QtQuick
import QtQuick.Window
import QtTest
import "../../src/ui" as Omaweb

// Telemetry under the Start page, as a Scene: the modules it draws from the
// readings it is handed, which of them a small page area keeps, what a
// reader's settings and a Private window leave out, and how the host takes its
// picture at the display's own pixels with its scanlines apart from its rows.
TestCase {
    id: testCase
    name: "Telemetry"
    when: windowShown
    width: 800
    height: 500

    // The Scene alone, in a window of its own on screen, as large as the
    // largest page area it is tried at.
    Component {
        id: sceneComponent

        Window {
            property alias scene: scene

            width: 2560
            height: 1440
            visible: true

            Omaweb.Telemetry {
                id: scene
                colors: crtRoadKeyColours.dark.theme
            }
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
                    Omaweb.Telemetry {}
                }
            }
        }
    }

    // Five minutes of a machine at work: eight cores, a download running at
    // about 2 MB/s, a battery draining, refusals in the Space asked for, and
    // pages loaded through the morning.
    function readings(changes) {
        const series = (count, value) => Array.from({
                                                        length: count
                                                    }, () => value);
        return Object.assign({
                                 cores: [0.1, 0.9, 0.3, 0.5, 0.2, 0.4, 0.6, 0.05],
                                 cpu: series(300, 0.38),
                                 cpuNow: 0.38,
                                 cpuPeak: 0.92,
                                 down: series(300, 2.1 * 1048576),
                                 downNow: 2.1 * 1048576,
                                 downPeak: 2.4 * 1048576,
                                 up: series(300, 30 * 1024),
                                 upNow: 30 * 1024,
                                 upPeak: 41 * 1024,
                                 memory: {
                                     total: 16 * 1048576,
                                     available: 9 * 1048576,
                                     free: 4 * 1048576,
                                     cache: 5 * 1048576,
                                     swapTotal: 4 * 1048576,
                                     swapFree: 3 * 1048576
                                 },
                                 omaweb: 420 * 1024,
                                 battery: {
                                     capacity: 0.64,
                                     status: "Discharging",
                                     watts: series(300, 9.5),
                                     charge: series(300, 0.64)
                                 },
                                 blocked: {
                                     lastMinute: 60,
                                     total: 1234,
                                     hosts: [
                                         {
                                             host: "tracker.example",
                                             count: 120
                                         }
                                     ]
                                 },
                                 pages: [0, 0, 0, 0, 0, 0, 0, 4, 18, 31, 27, 22, 9, 25, 33, 29, 17,
                                     12, 6, 0, 0, 0, 0, 0]
                             }, changes || {});
    }

    function makeScene(properties) {
        const window = createTemporaryObject(sceneComponent, testCase);
        verify(window !== null);
        const scene = window.scene;
        const given = Object.assign({
                                        width: 2560,
                                        height: 1440,
                                        readings: readings(),
                                        spaces: [
                                            {
                                                name: "Personal",
                                                bytes: 812 * 1048576
                                            }
                                        ]
                                    }, properties || {});
        for (const name in given)
            scene[name] = given[name];
        return scene;
    }

    function shown(scene, name) {
        const item = findChild(scene, name);
        verify(item !== null, name);
        return item.visible;
    }

    // What a reader sees of a block: shown, and where on the page.
    function box(scene, name) {
        const item = findChild(scene, name);
        return item.mapToItem(scene, 0, 0, item.width, item.height);
    }

    function figureLabels(scene) {
        return scene.figures.map(figure => figure.key);
    }

    // Telemetry is a Scene the host shows as it shows the road: the Omnibar
    // rests on the page area's middle, and the Scene declares the CRT glass.
    function test_theHostShowsTelemetryAsAScene() {
        const window = createTemporaryObject(hostComponent, testCase);
        const host = window.host;
        tryVerify(() => host.sceneItem !== null);
        const scene = host.sceneItem;
        compare(scene.objectName, "telemetry");
        compare(scene.parameters.id, "telemetry");
        tryCompare(scene, "horizonY", 250);
        compare(scene.glass, "crt");
        compare(scene.screen, "raster");
        verify(findChild(host, "crtGlass").visible);
        compare(host.light, null);
    }

    // The host takes a Scene that asks for it at the display's own pixels,
    // and stands the beam's lines its pitch of device pixels apart rather
    // than one to each of the Scene's rows. The road keeps both as they were.
    function test_theHostDrawsTelemetryAtDevicePixels() {
        const window = createTemporaryObject(hostComponent, testCase);
        const host = window.host;
        tryVerify(() => host.sceneItem !== null);
        const ratio = host.devicePixelRatio;
        verify(ratio > 0);
        const picture = findChild(host, "sceneDisplay");
        tryVerify(() => picture.textureSize.width === Math.ceil(800 * ratio)
                        && picture.textureSize.height === Math.ceil(500 * ratio), 5000, "picture "
        + picture.textureSize);
        const glass = findChild(host, "crtGlass");
        compare(glass.scanPeriod, 6 / ratio);

        host.scene = Qt.createComponent("../../src/ui/NightRoad.qml");
        tryVerify(() => host.sceneItem && host.sceneItem.objectName !== "telemetry");
        verify(picture.textureSize.width === host.sceneWidth && picture.textureSize.height
               === host.sceneHeight, "picture " + picture.textureSize);
        compare(glass.scanPeriod, 500 / host.sceneHeight);
    }

    // On a large page area every module has room: the CPU and network charts
    // and the gauges over the Omnibar, the figures under it, and the memory,
    // power, pages and Spaces sections under those.
    function test_aLargePageAreaShowsEveryModule() {
        const scene = makeScene();
        verify(shown(scene, "telemetryCpu"));
        verify(shown(scene, "telemetryGauges"));
        verify(shown(scene, "telemetryNetwork"));
        verify(shown(scene, "telemetryFigures"));
        for (const section of ["memory", "power", "pages", "spaces"])
            verify(shown(scene, "telemetrySection-" + section), section);
        compare(figureLabels(scene), ["cpu", "down", "up", "memory", "blocked", "battery", "swap",
                                      "omaweb"]);
    }

    // The readings are the machine's, each in its unit: the CPU's load, the
    // download's rate in MB/s chosen from its peak, the memory in use and the
    // refusals' total.
    function test_theFiguresReadTheReadings() {
        const scene = makeScene();
        const value = key => scene.figures.find(figure => figure.key === key).value;
        compare(value("cpu"), "38%");
        // A rate glides to its reading.
        tryVerify(() => value("down") === "2.1");
        compare(value("memory"), "7.0 GiB");
        compare(value("blocked"), "1234");
        compare(value("battery"), "64%");
        compare(scene.figures.find(figure => figure.key === "down").label, "▼ DOWN · MB/s");
    }

    // A rate's unit is chosen once for its whole series from its peak, so it
    // changes only when the traffic changes scale: kB/s or MB/s, never B/s.
    function test_aRateIsInKilobytesOrMegabytesNeverBytes_data() {
        return [
                    {
                        tag: "idle",
                        peak: 300,
                        unit: "kB/s"
                    },
                    {
                        tag: "browsing",
                        peak: 80 * 1024,
                        unit: "kB/s"
                    },
                    {
                        tag: "downloading",
                        peak: 3 * 1048576,
                        unit: "MB/s"
                    }
                ];
    }

    function test_aRateIsInKilobytesOrMegabytesNeverBytes(data) {
        compare(makeScene().rateUnit(data.peak).name, data.unit);
    }

    // The Omnibar always stays, and nothing is drawn over the band it rests
    // in, at any size: its field's top edge 50 px over the horizon with 16 px
    // of padding, down to as far as it reaches under the horizon.
    function test_nothingStandsWhereTheOmnibarRests_data() {
        const rows = [];
        for (const size of [[2560, 1440], [1280, 720], [853, 480], [600, 360], [400, 300]]) {
            const row = {};
            row.tag = size.join("x");
            row.width = size[0];
            row.height = size[1];
            rows.push(row);
        }
        return rows;
    }

    function test_nothingStandsWhereTheOmnibarRests(data) {
        const reach = 40;
        const scene = makeScene({
                                    width: data.width,
                                    height: data.height,
                                    omnibarReach: reach
                                });
        const omnibarTop = scene.horizonY - 50 - 16;
        const omnibarBottom = scene.horizonY + reach;
        for (const name of ["telemetryCpu", "telemetryGauges", "telemetryNetwork",
                            "telemetryFigures", "telemetrySection-memory", "telemetrySection-power",
                            "telemetrySection-pages", "telemetrySection-spaces"]) {
            const item = findChild(scene, name);
            if (!item.visible)
                continue;
            const b = box(scene, name);
            verify(b.y + b.height <= omnibarTop || b.y >= omnibarBottom, name
                   + " over the Omnibar at " + data.tag);
            verify(b.x >= 0 && b.x + b.width <= data.width + 0.5, name + " off the page");
            verify(b.y + b.height <= data.height + 0.5, name + " off the bottom");
        }
    }

    // A smaller page area drops whole blocks rather than squeezing them, in
    // the order #814 settled. Over the Omnibar the gauges go first, then the
    // network chart, then the CPU chart; under it the sections go last
    // first: Spaces, pages, power, then memory. The figures, compact
    // readouts, keep as many as fit, the earlier modules first.
    function test_aSmallerPageAreaDropsBlocksInOrder_data() {
        return [
                    {
                        tag: "a window narrowing at 16:9",
                        tall: false
                    },
                    {
                        tag: "a tall pane narrowing",
                        tall: true
                    }
                ];
    }

    function test_aSmallerPageAreaDropsBlocksInOrder(data) {
        const scene = makeScene();
        const order = ["telemetryGauges", "telemetryNetwork", "telemetryCpu"];
        const sections = ["spaces", "pages", "power", "memory"].map(s => "telemetrySection-" + s);
        let lastFigures = 99;
        const dropped = {};
        for (let width = 2560; width >= 360; width -= 40) {
            scene.width = width;
            scene.height = data.tall ? 1440 : Math.round(width * 9 / 16);
            for (const list of [order, sections]) {
                for (let index = 0; index < list.length; ++index) {
                    if (shown(scene, list[index])) {
                        verify(!dropped[list[index]], list[index] + " came back at " + width);
                        for (let later = index + 1; later < list.length; ++later)
                            verify(shown(scene, list[later]), list[later] + " dropped before "
                                   + list[index] + " at " + width);
                    } else {
                        dropped[list[index]] = true;
                    }
                }
            }
            const figures = figureLabels(scene);
            verify(figures.length <= lastFigures, "figures came back at " + width);
            lastFigures = figures.length;
            const all = ["cpu", "down", "up", "memory", "blocked", "battery", "swap", "omaweb"];
            compare(figures, all.slice(0, figures.length), "figures at " + width);
        }
        for (const name of order)
            verify(dropped[name], name + " never dropped");
        if (!data.tall) {
            for (const name of sections)
                verify(dropped[name], name + " never dropped");
        }
    }

    // A module the reader turned off in Settings is gone wherever it would
    // stand: its chart, its gauges and its figures.
    function test_aModuleTurnedOffIsGone() {
        const scene = makeScene({
                                    hidden: ["network", "spaces"]
                                });
        verify(!shown(scene, "telemetryNetwork"));
        verify(!shown(scene, "telemetrySection-spaces"));
        verify(shown(scene, "telemetryCpu"));
        compare(figureLabels(scene), ["cpu", "memory", "blocked", "battery", "swap", "omaweb"]);
        compare(scene.gauges.map(gauge => gauge.key), ["cpu", "memory"]);
    }

    // A machine with no battery has no power section and no battery figure.
    function test_noBatteryNoPower() {
        const scene = makeScene({
                                    readings: readings({
                                                           battery: null
                                                       })
                                });
        verify(!shown(scene, "telemetrySection-power"));
        verify(figureLabels(scene).indexOf("battery") < 0);
    }

    // Downloads are shown only while one runs, each with how far it has come.
    function test_downloadsShowOnlyWhileOneRuns() {
        const scene = makeScene();
        verify(!shown(scene, "telemetryDownloads"));
        scene.downloads = [
                    {
                        fileName: "omarchy.iso",
                        fraction: 0.42
                    }
                ];
        verify(shown(scene, "telemetryDownloads"));
        compare(findChild(scene, "telemetryDownload-0").fraction, 0.42);
        scene.downloads = [];
        verify(!shown(scene, "telemetryDownloads"));
    }

    // A Private window shows its own downloads and refusals and the machine's
    // readings, and nothing from History or other Spaces.
    function test_aPrivateWindowShowsNothingStored() {
        const scene = makeScene({
                                    unlit: true
                                });
        verify(!shown(scene, "telemetrySection-pages"));
        verify(!shown(scene, "telemetrySection-spaces"));
        verify(shown(scene, "telemetrySection-memory"));
        verify(figureLabels(scene).indexOf("blocked") >= 0);
    }

    // Pages are whole: the pages chart's scale stands a whole number of pages
    // at each of its four gridlines.
    function test_thePagesChartCountsWholePages_data() {
        return [
                    {
                        tag: "a quiet day",
                        peak: 1,
                        top: 4
                    },
                    {
                        tag: "a busy one",
                        peak: 33,
                        top: 36
                    }
                ];
    }

    function test_thePagesChartCountsWholePages(data) {
        const pages = Array.from({
                                     length: 24
                                 }, () => 0);
        pages[9] = data.peak;
        const scene = makeScene({
                                    readings: readings({
                                                           pages: pages
                                                       })
                                });
        compare(findChild(scene, "telemetryPagesChart").scaleTop, data.top);
    }

    // Large text by default, 22 px captions and 32 px readings, and small
    // text, 18 and 26, the reader's other choice.
    function test_textIsLargeUnlessSmallIsChosen() {
        const scene = makeScene();
        compare(scene.captionSize, 22);
        compare(scene.readingSize, 32);
        scene.textSize = "small";
        compare(scene.captionSize, 18);
        compare(scene.readingSize, 26);
    }

    // The modules are display only: nothing in the Scene takes a click or
    // the keyboard, so the Omnibar is the one thing on the page that does.
    function test_nothingInTheSceneTakesFocusOrClicks() {
        const scene = makeScene();
        const visit = item => {
            verify(!item.activeFocusOnTab, item);
            verify(!(item instanceof MouseArea), item);
            for (const child of item.children)
                visit(child);
        };
        visit(scene);
    }
}
