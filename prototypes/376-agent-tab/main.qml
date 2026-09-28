import QtQuick
import QtWebEngine

// PROTOTYPE for #376. An Agent tab stacked behind the reader's page, and a scenario that answers
// the spike's four questions, printing one RESULT line of JSON per measurement.
Window {
    id: root

    width: 1200
    height: 800
    visible: true
    title: "omaweb-376-probe"
    color: "black"

    // Each view gets its own off-the-record profile, as each Space has its own, so the two pages
    // are in different renderer processes and their CPU can be told apart.
    property WebEngineProfilePrototype agentPrototype: WebEngineProfilePrototype {
        storageName: ""
    }
    property WebEngineProfilePrototype readerPrototype: WebEngineProfilePrototype {
        storageName: ""
    }

    Item {
        id: pageArea

        anchors.fill: parent

        WebEngineView {
            id: agent

            objectName: "agent"
            anchors.fill: parent
            z: 0
            profile: root.agentPrototype.instance()
            url: "qrc:/qt/qml/AgentTab/agent.html"
        }

        WebEngineView {
            id: reader

            objectName: "reader"
            anchors.fill: parent
            z: 1
            profile: root.readerPrototype.instance()
            url: "qrc:/qt/qml/AgentTab/reader.html"
        }
    }

    // Stands in for the Omnibar: chrome that reacts when it loses keyboard focus.
    TextInput {
        id: chrome

        objectName: "chrome"
        property int focusChanges: 0

        x: 0
        y: 0
        width: 1
        height: 1
        onActiveFocusChanged: focusChanges++
    }

    Component {
        id: timerComponent

        Timer {}
    }

    function emitResult(name, value) {
        probe.emit_("RESULT " + JSON.stringify({
                                                   "name": name,
                                                   "value": value
                                               }));
    }

    function wait(ms) {
        return new Promise(resolve => {
            const timer = timerComponent.createObject(root, {
                                                          "interval": ms
                                                      });
            timer.triggered.connect(() => {
                timer.destroy();
                resolve();
            });
            timer.start();
        });
    }

    function js(view, code) {
        return new Promise(resolve => view.runJavaScript(code, resolve));
    }

    function grab(item) {
        return new Promise(resolve => {
            if (!item.grabToImage(result => resolve(result)))
                resolve(null);
        });
    }

    // The three ways the Agent tab can be out of the reader's sight. `hidden` and `frozen` are
    // what an away Space's page is today; `behind` is the spike's proposal; `opacity0` keeps the
    // view visible to the engine but lets the scene graph skip drawing it. `front` puts the page on
    // show, as the price of the same page when the reader is looking at it.
    function setMode(mode) {
        agent.z = mode === "front" ? 2 : 0;
        if (mode !== "frozen")
            agent.lifecycleState = WebEngineView.LifecycleState.Active;
        agent.opacity = mode === "opacity0" ? 0 : 1;
        agent.visible = mode === "behind" || mode === "opacity0" || mode === "front";
        if (mode === "frozen")
            agent.lifecycleState = WebEngineView.LifecycleState.Frozen;
    }

    function run(generator) {
        const steps = generator();
        function step(value) {
            let next;
            try {
                next = steps.next(value);
            } catch (error) {
                probe.emit_("ERROR " + error + "\n" + error.stack);
                Qt.exit(2);
                return;
            }
            if (next.done) {
                Qt.exit(0);
                return;
            }
            Promise.resolve(next.value).then(step, error => {
                probe.emit_("ERROR " + error);
                Qt.exit(2);
            });
        }
        step();
    }

    function* focusReader() {
        reader.forceActiveFocus();
        yield js(reader, "document.getElementById('r').focus()");
        yield wait(200);
    }

    function* takeLogs() {
        const agentLog = yield js(agent, "probe.take()");
        const readerLog = yield js(reader, "probe.take()");
        return {
            "agent": agentLog,
            "reader": readerLog,
            "qtFocus": probe.activeFocus(root)
        };
    }

    function* rendering() {
        for (const mode of ["hidden", "behind", "opacity0"]) {
            setMode(mode);
            yield wait(1500);
            // A hidden page does not run script promptly, so the counters are started while it is
            // still visible to the engine only for the modes that are; hidden counts from here.
            yield js(agent, "probe.start()");
            yield wait(3000);
            const counts = yield js(agent, "probe.stop()");
            // What the reader sees at the Agent's button and at the middle of the page area.
            counts.windowAtAgentButton = probe.windowPixel(root, 140 / root.width, 70
                                                           / root.height);


            counts.windowAtCentre = probe.windowPixel(root, 0.5, 0.5);
            emitResult("q1.rendering." + mode, counts);
        }
    }

    function* grabbing() {
        for (const mode of ["behind", "opacity0", "hidden"]) {
            setMode("behind");
            yield wait(300);
            for (const colour of ["#ff0000", "#0000ff", "#ff00ff"]) {
                yield js(agent, "probe.setColour('" + colour + "')");
                setMode(mode);
                yield wait(mode === "hidden" ? 1000 : 150);
                const started = probe.milliseconds();
                const result = yield grab(agent);
                const took = probe.milliseconds() - started;
                const seen = result ? probe.pixel(result.image, 0.5, 0.95) : "no image";
                if (result && colour === "#ff00ff")
                    probe.saveImage(result.image, outputDirectory + "/grab-" + mode + ".png");
                emitResult("q2.grab." + mode, {
                               "expected": colour,
                               "seen": seen,
                               "matches": seen === colour,
                               "ms": took
                           });
                setMode("behind");
                yield wait(100);
            }
            yield js(agent, "probe.setColour('#00ff00')");
        }
    }

    function* quietClickBox(label) {
        const centre = yield js(agent, "probe.centre('box')");
        yield* focusReader();
        yield* takeLogs();
        const before = probe.activeFocus(root);
        const accepted = probe.quietClick(agent, centre.x, centre.y);
        yield wait(400);
        const logs = yield* takeLogs();
        logs.qtFocusBefore = before;
        logs.pressAccepted = accepted;
        emitResult(label, logs);
    }

    function* keyActivateBox(label, key) {
        yield* focusReader();
        yield js(agent, "document.getElementById('box').focus()");
        yield* takeLogs();
        const before = probe.activeFocus(root);
        probe.keyTap(agent, key);
        yield wait(400);
        const logs = yield* takeLogs();
        logs.qtFocusBefore = before;
        emitResult(label, logs);
    }

    function* clickBox(label, restoreFocus) {
        const centre = yield js(agent, "probe.centre('box')");
        yield* focusReader();
        yield* takeLogs();
        const before = probe.activeFocus(root);
        const accepted = probe.mouseClick(agent, centre.x, centre.y);
        // Put the reader's focus back in the same turn of the event loop, before Chromium has
        // heard about either change.
        if (restoreFocus)
            reader.forceActiveFocus();
        yield wait(400);
        const logs = yield* takeLogs();
        logs.qtFocusBefore = before;
        logs.pressAccepted = accepted;
        emitResult(label, logs);
    }

    function* typing(label, prepare, text) {
        yield* focusReader();
        yield js(agent, "document.getElementById('field').value = ''");
        yield* takeLogs();
        yield* prepare();
        const focusWhileTyping = probe.activeFocus(root);
        probe.typeText(agent, text);
        yield wait(400);
        const logs = yield* takeLogs();
        logs.qtFocusWhileTyping = focusWhileTyping;
        logs.fieldValue = yield js(agent, "document.getElementById('field').value");
        logs.expected = text;
        emitResult(label, logs);
    }

    function* input() {
        setMode("behind");
        yield wait(1000);
        yield* clickBox("q3.click.behind");
        yield* clickBox("q3.click.behindRestoringFocus", true);
        const field = yield js(agent, "probe.centre('field')");
        yield* quietClickBox("q3.click.quiet");
        yield* keyActivateBox("q3.activate.return", Qt.Key_Return);
        yield* keyActivateBox("q3.activate.space", Qt.Key_Space);
        // A quiet click into the field, then typing, with the reader's focus never visibly moved.
        yield* typing("q3.keys.afterQuietClick", function* () {
            probe.quietClick(agent, field.x, field.y);
            yield wait(300);
        }, "quiet");
        // The reader's focus is in chrome rather than a page when the Agent clicks.
        chrome.forceActiveFocus();
        yield wait(200);
        chrome.focusChanges = 0;
        yield* takeLogs();
        probe.quietClick(agent, field.x, field.y);
        yield wait(300);
        const inChrome = yield* takeLogs();
        inChrome.chromeActiveFocusChanges = chrome.focusChanges;
        emitResult("q3.click.quietWhileChromeFocused", inChrome);
        yield* focusReader();
        // The reader goes on typing after the quiet click: their keys still reach their own field.
        yield js(reader, "document.getElementById('r').value = ''");
        yield* takeLogs();
        probe.quietClick(agent, field.x, field.y);
        probe.typeText(reader, "mine");
        yield wait(400);
        const afterQuiet = yield* takeLogs();
        afterQuiet.agentField = yield js(agent, "document.getElementById('field').value");
        emitResult("q3.readerKeysAfterQuietClick", afterQuiet);

        // The Agent clicks the field, which takes Qt's focus, and types with it.
        yield* typing("q3.keys.afterClickKeepingFocus", function* () {
            probe.mouseClick(agent, field.x, field.y);
            yield wait(300);
        }, "hello");
        // The Agent clicks the field and the reader's focus is put back before it types.
        yield* typing("q3.keys.afterClickFocusRestored", function* () {
            probe.mouseClick(agent, field.x, field.y);
            yield wait(300);
            yield* focusReader();
        }, "world");
        // No click at all: the field is focused by script and Qt's focus never moves.
        yield* typing("q3.keys.scriptFocusOnly", function* () {
            yield js(agent, "document.getElementById('field').focus()");
            yield wait(200);
        }, "abc");
        // A FocusIn sent to the delegate tells Chromium the page has focus without moving Qt's.
        yield* typing("q3.keys.focusInEventOnly", function* () {
            probe.sendFocusIn(agent);
            yield js(agent, "document.getElementById('field').focus()");
            yield wait(200);
        }, "xyz");
        yield* focusReader();

        agent.activeFocusOnPress = false;
        yield* clickBox("q3.click.activeFocusOnPressOff");
        agent.activeFocusOnPress = true;

        setMode("opacity0");
        yield wait(1000);
        yield* clickBox("q3.click.opacity0");

        setMode("hidden");
        yield wait(1000);
        yield* clickBox("q3.click.hidden");
        setMode("behind");
        yield wait(500);
    }

    function* cost() {
        const states = [["frozen", "static"], ["hidden", "static"], ["behind", "static"], ["behind",
                                                                                           "spinner"],
                        ["behind", "raf"], ["opacity0", "static"], ["opacity0", "spinner"],
                        ["opacity0", "raf"], ["front", "spinner"], ["front", "raf"]];
        yield* focusReader();
        for (let round = 0; round < rounds; round++) {
            for (const [mode, load] of states) {
                setMode("behind");
                yield wait(200);
                yield js(agent, "probe.setLoad('" + load + "')");
                setMode(mode);
                yield wait(2000);
                const pids = [agent.renderProcessPid || 0, reader.renderProcessPid || 0];
                const before = probe.cpuSeconds(pids[0], pids[1]);
                probe.watchFrames(root);
                const started = probe.milliseconds();
                yield wait(5000);
                const seconds = (probe.milliseconds() - started) / 1000;
                const frames = probe.frameReport();
                const after = probe.cpuSeconds(pids[0], pids[1]);
                const cpuPercent = {};
                for (const type in after)
                    cpuPercent[type] = Math.round((after[type] - (before[type] || 0)) / seconds
                                                  * 1000) / 10;
                emitResult("q4.cost." + mode + "." + load, {
                               "round": round,
                               "cpuPercentOfOneCore": cpuPercent,
                               "framesPerSecond": Math.round(frames.frames / seconds * 10) / 10,
                               "qtCpuMsPerFrame": frames.cpuMsPerFrame,
                               "qtGpuMsPerFrame": frames.gpuMsPerFrame
                           });
            }
        }
        setMode("behind");
        yield js(agent, "probe.setLoad('static')");
    }

    // The whole window out of sight: the harness moves it to another workspace while this runs.
    function* unseen() {
        setMode("behind");
        yield wait(1000);
        for (let interval = 0; interval < 9; interval++) {
            yield js(agent, "probe.start()");
            yield wait(3000);
            const counts = yield js(agent, "probe.stop()");
            const started = probe.milliseconds();
            const result = yield Promise.race([grab(agent), wait(3000).then(() => null)]);
            counts.grabMs = result ? probe.milliseconds() - started : "timed out after 3000";
            counts.second = Math.round(probe.milliseconds() / 1000);
            emitResult("unseen." + interval, counts);
        }
    }

    function* scenario() {
        while (agent.loading || reader.loading || agent.loadProgress < 100 || reader.loadProgress
               < 100)
            yield wait(100);
        yield wait(500);
        emitResult("info", {
                       "platform": Qt.platform.pluginName,
                       "os": Qt.platform.os,
                       "graphicsApi": GraphicsInfo.api,
                       "delegate": probe.delegateClass(agent),
                       "agentRenderer": agent.renderProcessPid,
                       "readerRenderer": reader.renderProcessPid,
                       "devicePixelRatio": Screen.devicePixelRatio,
                       "size": [root.width, root.height]
                   });
        const only = Qt.application.arguments.slice(1);
        const parts = {
            "rendering": rendering,
            "grab": grabbing,
            "input": input,
            "cost": cost,
            "unseen": unseen
        };
        for (const name in parts) {
            if ((only.length === 0 && name !== "unseen") || only.indexOf(name) >= 0)
                yield* parts[name]();
        }
    }

    Component.onCompleted: run(scenario)
}
