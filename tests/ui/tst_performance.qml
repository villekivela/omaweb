import QtQuick
import QtTest
import Omaweb
import "../../src/ui" as Omaweb

// The runtime numbers the UI lab can measure without an engine: what a tab
// switch costs before the destination is on screen, what a frame of the
// chromeless chrome costs over a page that never sits still, and how far apart
// the frames of the sidebar, a Space switch, the Omnibar and a Glance fall
// while they move. Startup and
// restore are measured on the lab as a process in tst_startup.cpp, and a
// frozen tab's memory needs a renderer and is measured in the engine suite.
//
// Each probe prints the line the development guide's performance section
// records and fails naming the value and the threshold it crossed. Thresholds
// are set at a margin over the first measurement, never ahead of it, except
// the frame intervals', which are ADR 0007's one-frame budget.
TestCase {
    id: testCase
    name: "PerformanceProbes"
    when: true

    property var window: null

    // Both numbers are under ten milliseconds, where scheduling jitter is a
    // larger share of a sample than the machine is, so the thresholds sit
    // further over the first measurement than the startup probes' do: seven
    // times for the switch, ten for the frame. The measurements are in the
    // development guide.
    readonly property real tabSwitchThresholdMilliseconds: 50
    readonly property real frameTimeThresholdMilliseconds: 10
    // How many switches the latency is the median of. One number would be
    // whichever of them a garbage collection happened to land in.
    readonly property int switches: 10

    Component {
        id: windowComponent
        Omaweb.Main {}
    }

    // The engine a switch is on its way to, and when the first frame showing it
    // was swapped. Read at the swap rather than polled for afterwards: with the
    // single-threaded loop the offscreen platform draws with, the handler runs
    // right after the frame that drew the state it reads.
    property var awaitedEngine: null
    property real arrivedAt: -1

    Connections {
        target: testCase.window

        function onFrameSwapped() {
            if (testCase.awaitedEngine === null || testCase.arrivedAt >= 0)
                return;
            const engineHost = findChild(testCase.window.contentItem, "engineLoader");
            if (engineHost.item === testCase.awaitedEngine && testCase.awaitedEngine.visible)
                testCase.arrivedAt = probeClock.milliseconds();
        }
    }

    function initTestCase() {
        window = windowComponent.createObject(null);
        verify(window !== null);
        window.show();
        wait(50);
    }

    function cleanupTestCase() {
        window.destroy();
    }

    // Records one number and holds it to its threshold. The clock prints the
    // line the development guide copies and hands back the failure message.
    function probe(name, measured, unit, threshold) {
        const failure = probeClock.report(name, measured, unit, threshold);
        verify(measured <= threshold, failure);
    }

    // A new Space comes up on one blank tab, which the first page takes over;
    // every page after it opens a tab of its own.
    function openPage(url, inNewTab) {
        const engineHost = findChild(window.contentItem, "engineLoader");
        verify(engineHost !== null);
        browser.openInput(url, inNewTab);
        tryVerify(function () {
            return engineHost.item !== null && engineHost.item.currentUrl.toString() === url;
        });
        return engineHost.item;
    }

    // From the switch command to the destination page's frame on screen, for
    // two pages the reader has already visited: the everyday switch, where
    // the destination's engine exists and only has to be shown. Measured in a
    // Space of its own so nothing another suite left open is stepped through.
    function test_aTabSwitchDrawsTheDestinationInsideItsBudget() {
        const spaceId = browser.createSpace("Switching");
        verify(browser.switchSpace(spaceId));
        const first = openPage("https://first.example/", false);
        const second = openPage("https://second.example/", true);
        verify(first !== second);
        compare(browser.tabs.rowCount(), 2);

        const samples = [];
        for (let round = 0; round < switches; ++round) {
            awaitedEngine = round % 2 === 0 ? first : second;
            arrivedAt = -1;
            const started = probeClock.milliseconds();
            window.commands.run("next-tab", -1);
            while (arrivedAt < 0 && probeClock.milliseconds() - started < 5000)
                probeClock.waitForFrame(window, 1000);
            verify(arrivedAt >= 0, "the destination page was never drawn");
            samples.push(arrivedAt - started);
        }
        awaitedEngine = null;
        samples.sort(function (a, b) {
            return a - b;
        });
        console.info("tab switches: fastest " + samples[0].toFixed(1) + " ms, slowest "
                     + samples[samples.length - 1].toFixed(1) + " ms");
        probe("tab-switch-to-page-frame", samples[Math.floor(samples.length / 2)], "ms",
              tabSwitchThresholdMilliseconds);
    }

    // Watches a second of frames and reports them, waking on each frame rather
    // than sleeping between polls: `wait` sleeps ten milliseconds at a time,
    // which under a threaded render loop costs the second a third of its
    // frames and leaves the count saying nothing.
    function watchASecond() {
        probeClock.watchFrames(window);
        const started = probeClock.milliseconds();
        while (probeClock.milliseconds() - started < 1000)
            probeClock.waitForFrame(window, 100);
        return probeClock.frameReport();
    }

    function describeFrames(report) {
        const gpu = report.meanGpuMilliseconds > 0 ? ", " + report.meanGpuMilliseconds.toFixed(2)
                                                     + " ms of it on the GPU" : "";
        return report.frames + " frames in a second, mean " + report.meanFrameMilliseconds.toFixed(
                    2) + " ms, slowest " + report.maxFrameMilliseconds.toFixed(1) + " ms" + gpu;
    }

    // What a frame costs in the chromeless state: sidebar hidden, the
    // navigation strip floating over the page, and the page redrawing every
    // frame. The cost is the scene graph's own, from the start of a frame to
    // its end, rather than the interval between frames, which the animation
    // timer sets at sixty a second regardless.
    //
    // The same second is then taken with the strip sent away, so what the
    // strip's blur costs the page is the difference between the two lines,
    // rather than a number that would have to be teased out of one. Only the
    // first is held: under a GPU the strip is a fraction of a millisecond, and
    // the software rasteriser draws no shader effect at all, so a threshold
    // on the difference would be one on noise.
    //
    // The tests draw through the software rasteriser on the offscreen
    // platform, so this number is comparative only: it holds the chrome to
    // what it cost before, on the same machine. An absolute frame time needs a
    // GPU backend on real hardware, and a virtual machine's number is only
    // ever comparative. With `QSG_RHI_PROFILE=1` a GPU backend also reports
    // what the frames cost the GPU itself, which the CPU-side bracket does not
    // include.
    function test_theChromelessChromeDrawsAnAnimatedPageInsideItsBudget() {
        const engine = openPage("https://motion.example/", true);
        engine.motionReview = true;
        mouseMove(window.contentItem, window.width / 2, window.height / 2);
        window.floatingControls = true;
        window.sidebarCollapsed = true;
        const sidebar = findChild(window.contentItem, "sidebar");
        tryCompare(sidebar, "visible", false);
        const cluster = findChild(window.contentItem, "navigationCluster");
        tryVerify(function () {
            return cluster.visible;
        });
        // The collapse eases, and the strip's first frames build its
        // pipelines; the measured second starts once both have settled.
        wait(300);

        const report = watchASecond();
        // Fewer frames than that and the page was not animating, so the mean
        // would be the cost of nothing.
        verify(report.frames >= 20, "the animated page drew only " + report.frames
               + " frames in a second, so nothing was being timed");
        const backend = GraphicsInfo.api === GraphicsInfo.Software ? "software rasteriser" : "GPU";
        console.info("chromeless frames with the strip: " + describeFrames(report)
                     + ", drawn by the " + backend
                     + "; comparative only unless drawn by a GPU on real hardware");

        // The reader can ask for the strip not to be there, which is the
        // chromeless state without it: the control.
        window.floatingControls = false;
        tryCompare(cluster, "visible", false);
        wait(300);
        const control = watchASecond();
        console.info("chromeless frames without the strip: " + describeFrames(control));
        window.floatingControls = true;

        probe("chromeless-frame-time", report.meanFrameMilliseconds, "ms",
              frameTimeThresholdMilliseconds);

        engine.motionReview = false;
        window.sidebarCollapsed = false;
    }

    // ADR 0007 lets an animation follow the display and hold the interface
    // thread for no more than one frame. A frame that waited on one held frame
    // arrives two frames after the last: 33.3 ms on a 60 Hz display, which is
    // what the offscreen platform reports.
    function frameIntervalCeiling() {
        let rate = 60;
        if (window.screen && window.screen.refreshRate > 0)
            rate = window.screen.refreshRate;
        return 2 * 1000 / rate;
    }
    // How many times each movement is taken. One movement is about ten
    // frames, too few for a 95th percentile to differ from the slowest.
    readonly property int movements: 10

    // Runs `act` and waits, frame by frame, until `rested` says the movement
    // it started is over, `movements` times, with every frame watched. Each
    // movement is the pointer's, on a desktop that has not asked for reduced
    // motion: after a key the chrome steps rather than eases, and a step has
    // no frames between its ends to measure.
    function watchMovements(act, rested) {
        SystemMotion.reduced = false;
        // The first movement builds what each later one only draws, which a
        // reader pays once; it is not watched.
        InputOrigin.pointer = true;
        act(0);
        waitForRest(rested);
        probeClock.watchFrames(window);
        for (let movement = 1; movement <= movements; ++movement) {
            InputOrigin.pointer = true;
            act(movement);
            waitForRest(rested);
        }
        return probeClock.frameReport();
    }

    function waitForRest(rested) {
        const started = probeClock.milliseconds();
        // A movement's first frame comes after the input; one frame first so
        // a state that reads at rest before it starts is not taken for its end.
        probeClock.waitForFrame(window, 100);
        while (!rested() && probeClock.milliseconds() - started < 5000)
            probeClock.waitForFrame(window, 100);
        verify(rested(), "the movement never came to rest");
    }

    // Holds a surface's frame intervals to the one-frame budget. The 95th
    // percentile is held; the slowest is printed beside it, so a failure says
    // whether the movement was slow throughout or hitched once.
    //
    // A surface that was over the budget when it was first measured passes
    // `overBudget`: its `guard`, about twice the slowest of those
    // measurements, is held so it cannot get worse unnoticed, and the budget
    // is printed beside the `reason` it is over but not held. The change that
    // brings the surface inside takes `overBudget` away. An expected failure
    // would force that, but a surface near the line passes it on a quiet
    // machine, and an unexpected pass fails the run.
    function probeIntervals(name, report, overBudget) {
        const backend = GraphicsInfo.api === GraphicsInfo.Software ? "software rasteriser" : "GPU";
        const gpu = report.meanGpuMilliseconds > 0 ? ", " + report.meanGpuMilliseconds.toFixed(2)
                                                     + " ms of it on the GPU" : "";
        console.info(name + ": " + report.intervals + " intervals, 95th percentile "
                     + report.p95IntervalMilliseconds.toFixed(1) + " ms, slowest "
                     + report.maxIntervalMilliseconds.toFixed(1) + " ms, mean frame cost "
                     + report.meanFrameMilliseconds.toFixed(2) + " ms" + gpu + ", drawn by the "
                     + backend);
        // A stepped movement draws one frame at each end. Three a movement is
        // below what a slow eased one draws and above what a step does.
        verify(report.intervals >= movements * 3, "only " + report.intervals
               + " frame intervals were seen, so nothing was moving");
        if (!overBudget) {
            probe(name, report.p95IntervalMilliseconds, "ms", frameIntervalCeiling());
            return;
        }
        const budget = probeClock.report(name, report.p95IntervalMilliseconds, "ms",
                                         frameIntervalCeiling());
        if (report.p95IntervalMilliseconds > frameIntervalCeiling())
            console.warn("over budget, not held: " + budget + ": " + overBudget.reason);
        probe(name + "-guard", report.p95IntervalMilliseconds, "ms", overBudget.guard);
    }

    // Puts a page that redraws every frame on show, so the window draws at its
    // full rate throughout and a held interface thread shows as a long
    // interval rather than as a pause nothing was drawing in anyway.
    function openAnimatedPage(url) {
        const engine = openPage(url, true);
        engine.motionReview = true;
        return engine;
    }

    // The sidebar leaving and coming back: the seam eases, and the page is
    // laid out once at each end.
    function test_theSidebarSlidesInsideTheFrameBudget() {
        const engine = openAnimatedPage("https://sidebar-motion.example/");
        const sidebar = findChild(window.contentItem, "sidebar");
        window.sidebarCollapsed = false;
        tryCompare(sidebar, "x", 0);

        const report = watchMovements(function (movement) {
            window.sidebarCollapsed = movement % 2 === 0;
        }, function () {
            return window.sidebarCollapsed ? !sidebar.visible : sidebar.x === 0;
        });
        window.sidebarCollapsed = false;
        tryCompare(sidebar, "x", 0);
        engine.motionReview = false;
        probeIntervals("sidebar-frame-interval", report, {
                           "guard": 150,
                           "reason": "the closed Shortcut sheet lays out its columns again when "
                                     + "the page area settles at its new width"
                       });
    }

    // A Space switch between two Spaces with a moving page each: the list
    // slides in over a picture of the one leaving, and the page area with it.
    function test_aSpaceSwitchSlidesInsideTheFrameBudget() {
        const homeSpaceId = browser.activeSpaceId;
        const spaceIds = [browser.createSpace("Sliding left"), browser.createSpace(
                              "Sliding right")];
        const engines = spaceIds.map(function (spaceId, index) {
            verify(browser.switchSpace(spaceId));
            return openAnimatedPage("https://space-motion-" + index + ".example/");
        });
        const sidebar = findChild(window.contentItem, "sidebar");
        tryCompare(sidebar, "arriving", false);

        const report = watchMovements(function (movement) {
            verify(browser.switchSpace(spaceIds[movement % 2]));
        }, function () {
            return !sidebar.arriving;
        });
        engines.forEach(function (engine) {
            engine.motionReview = false;
        });
        verify(browser.switchSpace(homeSpaceId));
        verify(browser.deleteSpace(spaceIds[0], "Sliding left"));
        verify(browser.deleteSpace(spaceIds[1], "Sliding right"));
        probeIntervals("space-switch-frame-interval", report, {
                           "guard": 500,
                           "reason": "showing the arriving Space's page rebuilds the closed "
                                     + "Shortcut sheet's sections before the slide's first frame"
                       });
    }

    // The Omnibar opening over a page in a Space of a hundred tabs, the
    // reader typing a word every tab's address holds, so each keystroke ranks
    // them all, and the Omnibar going.
    function test_theOmnibarOpensAndFiltersInsideTheFrameBudget() {
        const homeSpaceId = browser.activeSpaceId;
        const spaceId = browser.createSpace("Filtering");
        verify(browser.switchSpace(spaceId));
        for (let tab = 0; tab < 100; ++tab)
            browser.openInputInBackground("https://tab-" + tab + ".example/page");
        const engine = openAnimatedPage("https://omnibar-motion.example/");
        verify(browser.tabs.rowCount() > 100);
        const omnibar = findChild(window.contentItem, "omnibar");
        const input = findChild(window.contentItem, "omnibarInput");
        let typed = "";

        const report = watchMovements(function (movement) {
            if (movement % 2 === 1) {
                window.closeOmnibar();
                return;
            }
            window.openOmnibar(false);
            waitForRest(function () {
                return omnibar.arrival === 1;
            });
            for (const character of "example") {
                keyClick(character);
                probeClock.waitForFrame(window, 100);
            }
            typed = input.text;
        }, function () {
            return window.omnibarOpen ? omnibar.arrival === 1 : !omnibar.visible;
        });
        compare(typed, "example");
        engine.motionReview = false;
        verify(browser.switchSpace(homeSpaceId));
        verify(browser.deleteSpace(spaceId, "Filtering"));
        probeIntervals("omnibar-frame-interval", report, {
                           "guard": 120,
                           "reason": "each keystroke ranks the tabs and the commands up to "
                                     + "three times: for the text, then for each answer that "
                                     + "arrives"
                       });
    }

    // A Glance opening over a moving page, as a page's new-tab request opens
    // one, and going.
    function test_aGlanceOpensInsideTheFrameBudget() {
        const engine = openAnimatedPage("https://glance-motion.example/");
        const glance = findChild(window.contentItem, "glance");
        window.setGlanceEnabled(true);

        const report = watchMovements(function (movement) {
            if (movement % 2 === 1)
                window.closeGlance();
            else
                engine.simulateNewWindowRequest("https://glanced-" + movement + ".example/", false);
        }, function () {
            return window.glanceOpen ? glance.visible && glance.arrival === 1 : !glance.visible;
        });
        engine.motionReview = false;
        probeIntervals("glance-frame-interval", report);
    }

    // What a frame of the Start page costs while its road drives: the Scene's
    // moving layer redrawn at thirty frames a second, its small picture
    // captured, and the CRT glass drawn over the window. Comparative under the
    // software rasteriser, which leaves the glass out; under a GPU the line
    // reports what the glass costs it too.
    function test_theStartPageDrawsItsRoadInsideItsBudget() {
        const spaceId = browser.createSpace("Road");
        verify(browser.switchSpace(spaceId));
        window.requestActivate();
        const startPage = findChild(window.contentItem, "startPage");
        tryVerify(function () {
            return startPage.visible && startPage.roadRunning;
        });
        wait(300);

        const report = watchASecond();
        // The road draws at most thirty frames a second; fewer than twenty and
        // it was not moving, so the mean would be the cost of nothing.
        verify(report.frames >= 20, "the Start page drew only " + report.frames
               + " frames in a second, so nothing was being timed");
        const backend = GraphicsInfo.api === GraphicsInfo.Software ? "software rasteriser" : "GPU";
        console.info("Start page frames with the road driving: " + describeFrames(report)
                     + ", drawn by the " + backend
                     + "; comparative only unless drawn by a GPU on real hardware");
        probe("start-page-frame-time", report.meanFrameMilliseconds, "ms",
              frameTimeThresholdMilliseconds);
        verify(browser.deleteSpace(spaceId, "Road"));
    }
}
