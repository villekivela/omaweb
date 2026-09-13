import QtQuick
import QtTest
import "../../src/ui" as Omaweb

// The runtime numbers the UI lab can measure without an engine: what a tab
// switch costs before the destination is on screen, and what a frame of the
// chromeless chrome costs over a page that never sits still. Startup and
// restore are measured on the lab as a process in tst_startup.cpp, and a
// frozen tab's memory needs a renderer and is measured in the engine suite.
//
// Each probe prints the line the development guide's performance section
// records and fails naming the value and the threshold it crossed. Thresholds
// are set at a margin over the first measurement, never ahead of it.
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

    // What a frame costs in the chromeless state: sidebar hidden, the
    // navigation strip floating over the page, and the page redrawing every
    // frame. The cost is the scene graph's own, from the start of a frame to
    // its end, rather than the interval between frames, which the animation
    // timer sets at sixty a second regardless.
    //
    // The tests draw through the software rasteriser on the offscreen
    // platform, so this number is comparative only: it holds the chrome to
    // what it cost before, on the same machine. An absolute frame time needs a
    // GPU backend on real hardware, and a virtual machine's number is only
    // ever comparative.
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
        // The collapse eases; the measured second starts once it has settled.
        wait(300);

        probeClock.watchFrames(window);
        wait(1000);
        const report = probeClock.frameReport();
        // Fewer frames than that and the page was not animating, so the mean
        // would be the cost of nothing.
        verify(report.frames >= 20, "the animated page drew only " + report.frames
               + " frames in a second, so nothing was being timed");
        const backend = GraphicsInfo.api === GraphicsInfo.Software ? "software rasteriser" : "GPU";
        console.info("chromeless frames: " + report.frames + " in a second, slowest "
                     + report.maxFrameMilliseconds.toFixed(1) + " ms, drawn by the " + backend
                     + "; comparative only unless drawn by a GPU on real hardware");
        probe("chromeless-frame-time", report.meanFrameMilliseconds, "ms",
              frameTimeThresholdMilliseconds);

        engine.motionReview = false;
        window.sidebarCollapsed = false;
    }
}
