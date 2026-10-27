import QtQuick
import Omaweb

// PROTOTYPE (#814), throwaway: the Telemetry Scene, a retro CRT dashboard
// around the resting Omnibar, in four structurally different layouts switched
// live, for the maintainer to pick one. Never merged; it lives on
// prototype/020-telemetry.
//
// Real: CPU per core, network, memory and swap (/proc), battery (/sys), the
// chrome process's memory, requests blocked on the Space's open tabs, running
// downloads. Fake, and labelled FAKE on screen: pages loaded today, memory per
// Space.
//
//   A  Cockpit      panels framing the Omnibar on four sides   display only
//   B  Strips       full-width oscilloscope lanes, time first  display only
//   C  Cluster      round gauges over the Omnibar, lamps below display only
//   D  Console      dense side columns; rows can be clicked    clickable
//
// It keeps NightRoad.qml's contract with SceneHost.qml.
Item {
    id: root
    objectName: "telemetryPrototype"

    // ---- what the host hands it

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
    property bool reducedMotion: false
    property bool unlit: false
    property real omnibarReach: 26
    property real omnibarWidth: 0

    // ---- what the Start page hands it (prototype)

    readonly property bool telemetry: true
    property string layout: "A"
    property var blocker: null
    property var browser: null
    property var tabs: null
    property var downloads: null
    // Ctrl+Alt+D: pretend a download runs, to see the module.
    property bool fakeDownload: false
    signal activated(string kind, string value)

    // ---- what it declares

    readonly property int pitch: 1
    readonly property int fps: 30
    readonly property string glass: "crt"
    readonly property string screen: "vector"
    readonly property var crt: ({
                                    bloom: {
                                        scale: 3,
                                        opacity: 0.25
                                    },
                                    halation: {
                                        threshold: 0.35,
                                        strength: 0.3,
                                        reach: 32,
                                        spacing: 24
                                    },
                                    grade: {
                                        top: 0.15,
                                        depth: 0.55,
                                        highlights: {
                                            from: 0.25,
                                            to: 0.85,
                                            strength: 0.5
                                        },
                                        midtones: {
                                            from: 0.08,
                                            to: 0.5,
                                            strength: 0.4
                                        }
                                    },
                                    trail: {
                                        fade: 0.82
                                    },
                                    vignette: {
                                        clear: 0.6,
                                        shade: 0.55
                                    },
                                    flicker: {
                                        least: 0.02,
                                        range: 0.02,
                                        frequencies: [37.1, 11.3]
                                    }
                                })
    readonly property var declaredOptions: ({})
    readonly property real horizonY: Math.round(root.layout === "D" ? Math.max(90, root.height * 0.2) :
                                                                      root.height * (root.layout
                                                                                     === "C" ? 0.7 :
                                                                                               0.5))

    // ---- palette

    readonly property color ground: root.dark ? root.colors.windowOpaque : Qt.darker(root.colors.text,
                                                                                     3)
    readonly property color ink: root.dark ? root.colors.text : root.colors.windowOpaque
    readonly property color glow: root.unlit ? Qt.darker(root.ink, 1.6) : root.colors.accent
    readonly property color hot: root.colors.syntax && root.colors.syntax.keyword
                                 ? root.colors.syntax.keyword : Qt.lighter(root.glow, 1.4)
    readonly property color dim: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.45)
    readonly property color faint: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.14)
    readonly property string mono: "monospace"

    // ---- where the Omnibar rests, which every layout keeps clear

    readonly property real barWidth: root.omnibarWidth > 0 ? root.omnibarWidth : Math.min(720,
                                                                                          root.width
                                                                                          - 32)
    readonly property real barLeft: Math.round((root.width - root.barWidth) / 2)
    readonly property real barTop: root.horizonY - 50 - 14
    readonly property real barBottom: root.horizonY + root.omnibarReach + 14

    // ---- the modules, in drop order, and how many the size holds

    readonly property var order: ["cpu", "net", "mem", "blocked", "downloads", "battery", "pages",
        "spaces"]
    readonly property var present: root.order.filter(key => {
        if (key === "downloads")
            return probe.downloadsRunning.length > 0;
        if (key === "battery")
            return probe.battery !== null;
        if (key === "pages" || key === "spaces")
            return !root.unlit;
        return true;
    })
    // The share of a full-size page area this one is.
    readonly property real room: Math.min(root.width / 1280, root.height / 760)
    readonly property bool compact: root.room < 0.85
    readonly property int holds: root.room >= 0.85 ? 8 : root.room >= 0.65 ? 6 : root.room >= 0.5
                                                                             ? 4 : root.room
                                                                               >= 0.38 ? 2 : 0
    readonly property var shown: root.present.slice(0, root.holds)
    function has(key) {
        return root.shown.indexOf(key) >= 0;
    }

    // ---- formatting

    function rate(bytes) {
        if (bytes >= 1048576)
            return (bytes / 1048576).toFixed(1) + " MB/s";
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(0) + " kB/s";
        return Math.round(bytes) + " B/s";
    }
    function size(kib) {
        return kib >= 1048576 ? (kib / 1048576).toFixed(1) + " GiB" : (kib / 1024).toFixed(0)
                                + " MiB";

    }
    function pct(f) {
        return Math.round(f * 100) + "%";
    }

    // ---- the probe: one sample a second, five minutes kept, nothing on disk

    QtObject {
        id: probe

        readonly property int keep: 300
        property var cores: []          // [[fraction...]] per core, oldest first
        property var cpuTotal: []
        property var last: null
        property var down: []
        property var up: []
        property var lastNet: null
        property var memUsed: []
        property var swapUsed: []
        property var meminfo: ({})
        property real chromeKib: 0
        property var battery: null      // {capacity, status, watts}
        property var batteryHistory: []
        property int blockedOpen: 0
        property int blockedSession: 0
        property var blockedRate: []
        property var topHosts: []       // [{host, count}]
        property var downloadsRunning: []
        // FAKE: pages loaded per hour today, and memory per Space.
        readonly property var pagesToday: [0, 0, 0, 0, 0, 0, 0, 4, 18, 31, 27, 22, 9, 25, 33, 29, 17,
            12, 6, 0, 0, 0, 0, 0]
        readonly property var spaces: [
            {
                name: "Personal",
                mib: 812
            },
            {
                name: "Work",
                mib: 1430
            },
            {
                name: "Omaweb dev",
                mib: 655
            }
        ]

        function push(series, value) {
            const next = series.slice(Math.max(0, series.length - probe.keep + 1));
            next.push(value);
            return next;
        }

        function sample() {
            probe.readCpu();
            probe.readNet();
            probe.readMem();
            probe.readBattery();
            probe.readBlocked();
            probe.readDownloads();
        }

        function readCpu() {
            const now = [];
            for (const line of Scenes.systemFile("/proc/stat").split("\n")) {
                const m = /^cpu(\d*)\s+(.*)$/.exec(line);
                if (!m)
                    continue;
                const f = m[2].trim().split(/\s+/).map(Number);
                const total = f.slice(0, 8).reduce((a, b) => a + b, 0);
                now.push({
                             core: m[1] === "" ? -1 : Number(m[1]),
                             total: total,
                             idle: f[3] + f[4]
                         });
            }
            if (probe.last && probe.last.length === now.length) {
                const cores = probe.cores.slice();
                for (let i = 0; i < now.length; ++i) {
                    const dt = now[i].total - probe.last[i].total;
                    const busy = dt > 0 ? 1 - (now[i].idle - probe.last[i].idle) / dt : 0;
                    if (now[i].core < 0) {
                        probe.cpuTotal = probe.push(probe.cpuTotal, busy);
                    } else {
                        cores[now[i].core] = probe.push(cores[now[i].core] || [], busy);
                    }
                }
                probe.cores = cores;
            }
            probe.last = now;
        }

        function readNet() {
            let rx = 0, tx = 0;
            for (const line of Scenes.systemFile("/proc/net/dev").split("\n").slice(2)) {
                const parts = line.split(":");
                if (parts.length < 2 || parts[0].trim() === "lo")
                    continue;
                const f = parts[1].trim().split(/\s+/).map(Number);
                rx += f[0];
                tx += f[8];
            }
            const now = Date.now();
            if (probe.lastNet) {
                const s = Math.max(0.2, (now - probe.lastNet.at) / 1000);
                probe.down = probe.push(probe.down, Math.max(0, (rx - probe.lastNet.rx) / s));
                probe.up = probe.push(probe.up, Math.max(0, (tx - probe.lastNet.tx) / s));
            }
            probe.lastNet = {
                at: now,
                rx: rx,
                tx: tx
            };
        }

        function readMem() {
            const info = {};
            for (const line of Scenes.systemFile("/proc/meminfo").split("\n")) {
                const m = /^(\w+):\s+(\d+)/.exec(line);
                if (m)
                    info[m[1]] = Number(m[2]);
            }
            probe.meminfo = info;
            probe.memUsed = probe.push(probe.memUsed, info.MemTotal ? 1 - info.MemAvailable
                                                                      / info.MemTotal : 0);
            probe.swapUsed = probe.push(probe.swapUsed, info.SwapTotal ? 1 - info.SwapFree
                                                                         / info.SwapTotal : 0);
            const self = /VmRSS:\s+(\d+)/.exec(Scenes.systemFile("/proc/self/status"));
            probe.chromeKib = self ? Number(self[1]) : 0;
        }

        function readBattery() {
            for (const name of Scenes.systemEntries("/sys/class/power_supply")) {
                const dir = "/sys/class/power_supply/" + name + "/";
                if (Scenes.systemFile(dir + "type").trim() !== "Battery")
                    continue;
                const capacity = Number(Scenes.systemFile(dir + "capacity")) / 100;
                const power = Number(Scenes.systemFile(dir + "power_now")) / 1e6;
                probe.battery = {
                    capacity: capacity,
                    status: Scenes.systemFile(dir + "status").trim(),
                    watts: power
                };
                probe.batteryHistory = probe.push(probe.batteryHistory, power);
                return;
            }
            probe.battery = null;
        }

        function readBlocked() {
            let open = 0;
            const hosts = {};
            if (root.blocker && root.browser && root.tabs) {
                const space = root.browser.sessionSpaceId;
                for (let row = 0; row < root.tabs.rowCount(); ++row) {
                    const url = root.tabs.data(root.tabs.index(row, 0), Qt.UserRole + 3);
                    open += root.blocker.refusalTally(space, url);
                    for (const refused of root.blocker.refusedRequests(space, url)) {
                        const m = /^[a-z-]+:\/\/([^\/:?#]+)/.exec(String(refused.address));
                        const host = m ? m[1] : String(refused.address);
                        hosts[host] = (hosts[host] || 0) + 1;
                    }
                }
            }
            const delta = probe.blockedRate.length > 0 || probe.blockedOpen > 0 ? Math.max(0, open
                                                                                           - probe.blockedOpen) :
                                                                                  0;
            probe.blockedSession += delta;
            probe.blockedRate = probe.push(probe.blockedRate, delta);
            probe.blockedOpen = open;
            probe.topHosts = Object.keys(hosts).map(host => ({
                host: host,
                count: hosts[host]
            })).sort((a, b) => b.count - a.count).slice(0, 5);
        }

        function readDownloads() {
            const running = root.downloads ? root.downloads.runningDownloads.slice() : [];
            if (root.fakeDownload)
                running.push({
                                 fileName: "FAKE-ubuntu-24.04.iso",
                                 fraction: (root.time / 40) % 1
                             });
            probe.downloadsRunning = running;
        }
    }

    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: probe.sample()
    }

    // ---- small parts every layout draws with

    // A trace of `values` (0..max, or 0..its own peak), newest at the right.
    component Trace: Canvas {
        id: trace
        property var values: []
        property var second: null
        property real max: 0
        property color colour: root.glow
        property color secondColour: root.hot
        property bool area: true
        property int span: 300
        onValuesChanged: requestPaint()
        onWidthChanged: requestPaint()
        onColourChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            const peak = trace.max > 0 ? trace.max : Math.max(1, ...trace.values, ...(trace.second
                                                                                      || []));
            const step = trace.width / (trace.span - 1);
            const line = (values, colour, fill) => {
                if (values.length < 2)
                    return;
                c.beginPath();
                const x0 = trace.width - (values.length - 1) * step;
                for (let i = 0; i < values.length; ++i) {
                    const x = x0 + i * step;
                    const y = trace.height - 1 - Math.min(1, values[i] / peak) * (trace.height - 2);
                    if (i === 0)
                        c.moveTo(x, y);
                    else
                        c.lineTo(x, y);
                }
                c.strokeStyle = colour;
                c.lineWidth = 1.25;
                c.stroke();
                if (fill) {
                    c.lineTo(trace.width, trace.height);
                    c.lineTo(x0, trace.height);
                    c.closePath();
                    c.fillStyle = Qt.rgba(colour.r, colour.g, colour.b, 0.18);
                    c.fill();
                }
            };
            line(trace.values, trace.colour, trace.area);
            if (trace.second)
                line(trace.second, trace.secondColour, false);
        }
    }

    // Vertical bars, one per value, 0..1.
    component Bars: Row {
        id: bars
        property var values: []
        property real barHeight: 60
        spacing: 2
        Repeater {
            model: bars.values.length
            Item {
                required property int index
                width: Math.max(2, (bars.width - (bars.values.length - 1) * bars.spacing) / Math.max(
                                    1, bars.values.length))
                height: bars.barHeight
                Rectangle {
                    anchors.fill: parent
                    color: root.faint
                }
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: Math.max(1, parent.height * bars.values[parent.index])
                    color: bars.values[parent.index] > 0.85 ? root.hot : root.glow
                }
            }
        }
    }

    // A horizontal meter, 0..1.
    component Meter: Item {
        property real value: 0
        property color colour: root.glow
        height: 6
        Rectangle {
            anchors.fill: parent
            color: root.faint
        }
        Rectangle {
            width: parent.width * Math.max(0, Math.min(1, parent.value))
            height: parent.height
            color: parent.colour
        }
    }

    component Caption: Text {
        color: root.dim
        font.family: root.mono
        font.pixelSize: root.compact ? 10 : 11
        font.letterSpacing: 1.5
    }
    component Reading: Text {
        color: root.ink
        font.family: root.mono
        font.pixelSize: root.compact ? 13 : 16
    }
    component Fake: Text {
        text: "FAKE"
        color: root.hot
        font.family: root.mono
        font.pixelSize: 9
        font.bold: true
    }

    // A framed panel with a title.
    component Panel: Item {
        id: panel
        property string title
        property bool fake: false
        default property alias content: body.data
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.5)
            border.width: 1
        }
        Rectangle {
            x: 10
            y: -6
            width: heading.width + 10
            height: 12
            color: root.ground
            Caption {
                id: heading
                x: 5
                anchors.verticalCenter: parent.verticalCenter
                text: panel.title + (panel.fake ? "  [FAKE]" : "")
                color: panel.fake ? root.hot : root.glow
            }
        }
        Item {
            id: body
            anchors.fill: parent
            anchors.margins: 12
            anchors.topMargin: 14
        }
    }

    readonly property var latestCores: probe.cores.map(series => series.length
                                                                 ? series[series.length - 1] : 0)
    function lastOf(series) {
        return series.length ? series[series.length - 1] : 0;
    }

    // ---- ground

    Rectangle {
        anchors.fill: parent
        color: root.ground
    }
    // A faint grid, as a scope's graticule.
    Canvas {
        anchors.fill: parent
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        property color line: root.faint
        onLineChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            c.strokeStyle = Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.06);
            c.lineWidth = 1;
            for (let x = 0.5; x < width; x += 40) {
                c.beginPath();
                c.moveTo(x, 0);
                c.lineTo(x, height);
                c.stroke();
            }
            for (let y = 0.5; y < height; y += 40) {
                c.beginPath();
                c.moveTo(0, y);
                c.lineTo(width, y);
                c.stroke();
            }
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: ({
                              "A": cockpit,
                              "B": strips,
                              "C": cluster,
                              "D": consoleLayout
                          })[root.layout] || cockpit
    }

    // ================================================================ A
    // Cockpit: four panels frame the Omnibar, a status strip over them and a
    // footer strip under them. Display only.
    Component {
        id: cockpit

        Item {
            id: a
            readonly property real gap: root.compact ? 10 : 18
            readonly property real side: Math.max(0, root.barLeft - 2 * gap)
            readonly property real colW: Math.max(220, (root.width - 3 * gap) / 2)
            readonly property real upperH: Math.max(60, root.barTop - 2 * gap - stripH)
            readonly property real lowerH: Math.max(60, root.height - root.barBottom - 3 * gap
                                                    - footH)
            readonly property real stripH: 28
            readonly property real footH: root.has("pages") || root.has("spaces") ? (root.compact
                                                                                     ? 46 : 70) : 0

            // Status strip: battery, downloads.
            Row {
                x: a.gap
                y: a.gap
                spacing: 28
                Caption {
                    text: "OMAWEB TELEMETRY  " + Qt.formatTime(new Date(root.time * 0 + Date.now()),
                                                               "hh:mm:ss")
                    color: root.glow
                }
                Row {
                    visible: root.has("battery")
                    spacing: 8
                    Caption {
                        text: "BAT"
                    }
                    Meter {
                        width: 60
                        y: 3
                        value: probe.battery ? probe.battery.capacity : 0
                        colour: probe.battery && probe.battery.capacity < 0.2 ? root.hot : root.glow
                    }
                    Caption {
                        color: root.ink
                        text: probe.battery ? root.pct(probe.battery.capacity) + " "
                                              + probe.battery.status.toLowerCase() + " "
                                              + probe.battery.watts.toFixed(1) + " W" : ""
                    }
                }
                Repeater {
                    model: root.has("downloads") ? probe.downloadsRunning : []
                    Row {
                        required property var modelData
                        spacing: 8
                        Caption {
                            text: "DL " + modelData.fileName
                            color: root.ink
                        }
                        Meter {
                            width: 80
                            y: 3
                            value: modelData.fraction
                        }
                    }
                }
            }

            Panel {
                visible: root.has("cpu")
                x: a.gap
                y: a.gap + a.stripH + a.gap
                width: root.has("net") ? a.colW : root.width - 2 * a.gap
                height: a.upperH
                title: "CPU " + probe.cores.length + " CORES  " + root.pct(root.lastOf(
                                                                               probe.cpuTotal))
                Bars {
                    width: parent.width
                    barHeight: Math.max(20, parent.height * 0.55)
                    values: root.latestCores
                }
                Trace {
                    y: parent.height * 0.6
                    width: parent.width
                    height: parent.height * 0.4
                    values: probe.cpuTotal
                    max: 1
                }
            }
            Panel {
                visible: root.has("net")
                x: root.width - a.gap - width
                y: a.gap + a.stripH + a.gap
                width: a.colW
                height: a.upperH
                title: "NETWORK"
                Column {
                    Reading {
                        text: "▼ " + root.rate(root.lastOf(probe.down))
                    }
                    Reading {
                        text: "▲ " + root.rate(root.lastOf(probe.up))
                        color: root.hot
                    }
                }
                Trace {
                    x: 150
                    width: parent.width - 150
                    height: parent.height
                    values: probe.down
                    second: probe.up
                }
            }
            Panel {
                visible: root.has("mem")
                x: a.gap
                y: root.barBottom + a.gap
                width: root.has("blocked") ? a.colW : root.width - 2 * a.gap
                height: a.lowerH
                title: "MEMORY  " + root.size(probe.meminfo.MemTotal - probe.meminfo.MemAvailable)
                       + " / " + root.size(probe.meminfo.MemTotal || 0)
                Column {
                    width: 140
                    spacing: 6
                    Caption {
                        text: "RAM " + root.pct(root.lastOf(probe.memUsed))
                    }
                    Meter {
                        width: 120
                        value: root.lastOf(probe.memUsed)
                    }
                    Caption {
                        text: "SWAP " + root.pct(root.lastOf(probe.swapUsed))
                    }
                    Meter {
                        width: 120
                        value: root.lastOf(probe.swapUsed)
                        colour: root.hot
                    }
                    Caption {
                        text: "OMAWEB UI " + root.size(probe.chromeKib)
                    }
                }
                Trace {
                    x: 150
                    width: parent.width - 150
                    height: parent.height
                    values: probe.memUsed
                    second: probe.swapUsed
                    max: 1
                }
            }
            Panel {
                visible: root.has("blocked")
                x: root.width - a.gap - width
                y: root.barBottom + a.gap
                width: a.colW
                height: a.lowerH
                title: "BLOCKED  " + probe.blockedOpen + " ON OPEN TABS  " + probe.blockedSession
                       + " THIS SESSION"
                Column {
                    width: parent.width * 0.5
                    Repeater {
                        model: probe.topHosts
                        Caption {
                            required property var modelData
                            width: parent.width
                            elide: Text.ElideRight
                            color: root.ink
                            text: String(modelData.count).padStart(4) + "  " + modelData.host
                        }
                    }
                    Caption {
                        visible: probe.topHosts.length === 0
                        text: "nothing blocked on open tabs"
                    }
                }
                Trace {
                    x: parent.width * 0.52
                    width: parent.width * 0.48
                    height: parent.height
                    values: probe.blockedRate
                    colour: root.hot
                }
            }

            // Footer: pages today and memory per Space.
            Panel {
                visible: root.has("pages")
                x: a.gap
                y: root.height - a.gap - a.footH
                width: root.has("spaces") ? a.colW : root.width - 2 * a.gap
                height: a.footH
                fake: true
                title: "PAGES TODAY " + probe.pagesToday.reduce((s, v) => s + v, 0)
                Trace {
                    width: parent.width
                    height: parent.height
                    values: probe.pagesToday
                    span: 24
                }
            }
            Panel {
                visible: root.has("spaces")
                x: root.width - a.gap - width
                y: root.height - a.gap - a.footH
                width: a.colW
                height: a.footH
                fake: true
                title: "MEMORY PER SPACE"
                Row {
                    spacing: 18
                    Repeater {
                        model: probe.spaces
                        Column {
                            required property var modelData
                            Caption {
                                text: modelData.name.toUpperCase()
                            }
                            Reading {
                                text: modelData.mib + " MiB"
                            }
                        }
                    }
                }
            }
        }
    }

    // ================================================================ B
    // Strips: every module is a lane as wide as the page, time running left
    // to right across it, as a chart recorder's paper. The lanes stack above
    // and below the Omnibar, which cuts through the middle. Display only.
    Component {
        id: strips

        Item {
            id: b
            readonly property var lanes: root.shown.filter(key => key !== "spaces")
            readonly property var above: b.lanes.filter((key, i) => i % 2 === 0)
            readonly property var below: b.lanes.filter((key, i) => i % 2 === 1).concat(root.has(
                                                                                            "spaces")
                                                                                        ? ["spaces"] :
                                                                                          [])
            readonly property real laneH: Math.min(root.compact ? 54 : 84, (root.barTop - 20) / Math.max(1,
                                                                                                         b.above.length))

            // The sweep, the paper's pen: a line at the newest sample.
            Rectangle {
                x: root.width - 24
                width: 1
                height: root.height
                color: root.glow
                opacity: 0.5 + 0.5 * Math.abs(Math.sin(root.time * 3))
            }

            Column {
                x: 0
                width: root.width
                y: root.barTop - height - 6
                Repeater {
                    model: b.above
                    Loader {
                        required property string modelData
                        width: root.width
                        height: b.laneH
                        sourceComponent: lane
                        property string key: modelData
                    }
                }
            }
            Column {
                x: 0
                width: root.width
                y: root.barBottom + 6
                Repeater {
                    model: b.below
                    Loader {
                        required property string modelData
                        width: root.width
                        height: b.laneH
                        sourceComponent: lane
                        property string key: modelData
                    }
                }
            }

            Component {
                id: lane
                Item {
                    id: l
                    readonly property string key: parent ? parent.key : ""
                    Rectangle {
                        width: parent.width
                        height: 1
                        y: parent.height - 1
                        color: root.faint
                    }
                    Column {
                        x: 16
                        y: 8
                        width: 170
                        Caption {
                            color: l.key === "pages" ? root.hot : root.glow
                            text: ({
                                       cpu: "CPU / CORE",
                                       net: "NET ▼ ▲",
                                       mem: "MEM / SWAP",
                                       blocked: "BLOCKED /S",
                                       downloads: "DOWNLOADS",
                                       battery: "BATTERY W",
                                       pages: "PAGES/H [FAKE]",
                                       spaces: "SPACES [FAKE]"
                                   })[l.key] || l.key
                        }
                        Reading {
                            text: l.key === "cpu" ? root.pct(root.lastOf(probe.cpuTotal)) : l.key
                                                    === "net" ? root.rate(root.lastOf(probe.down)) :
                                                                l.key === "mem" ? root.pct(
                                                                                      root.lastOf(
                                                                                          probe.memUsed)) :
                                                                                  l.key === "blocked"
                                                                                  ? probe.blockedOpen
                                                                                    + " open / "
                                                                                    + probe.blockedSession :
                                                                                    l.key === "battery"
                                                                                    && probe.battery
                                                                                    ? root.pct(
                                                                                          probe.battery.capacity) :
                                                                                      l.key === "pages"
                                                                                      ? probe.pagesToday.reduce(
                                                                                            (s, v) => s
                                                                                                      + v, 0) + " today" :
                                                                                        l.key === "downloads"
                                                                                        ? probe.downloadsRunning.length
                                                                                          + " running" :
                                                                                          ""
                        }
                    }
                    // CPU: a heat map, a row per core, a column per second.
                    Canvas {
                        id: heat
                        visible: l.key === "cpu"
                        x: 200
                        y: 4
                        width: root.width - 232
                        height: parent.height - 8
                        property var cores: probe.cores
                        onCoresChanged: if (visible)
                                            requestPaint()
                        onPaint: {
                            const c = getContext("2d");
                            c.reset();
                            const rows = heat.cores.length;
                            if (!rows)
                                return;
                            const rh = heat.height / rows;
                            const cw = heat.width / 300;
                            for (let r = 0; r < rows; ++r) {
                                const s = heat.cores[r];
                                const x0 = heat.width - s.length * cw;
                                for (let i = 0; i < s.length; ++i) {
                                    c.fillStyle = Qt.rgba(root.glow.r, root.glow.g, root.glow.b,
                                                          0.08 + 0.92 * s[i]);
                                    c.fillRect(x0 + i * cw, r * rh, Math.ceil(cw), rh - 1);
                                }
                            }
                        }
                    }
                    Trace {
                        visible: l.key !== "cpu" && l.key !== "spaces" && l.key !== "downloads"
                        x: 200
                        y: 4
                        width: root.width - 232
                        height: parent.height - 8
                        values: l.key === "net" ? probe.down : l.key === "mem" ? probe.memUsed : l.key
                                                                                 === "blocked"
                                                                                 ? probe.blockedRate :
                                                                                   l.key === "battery"
                                                                                   ? probe.batteryHistory :
                                                                                     l.key === "pages"
                                                                                     ? probe.pagesToday :
                                                                                       []
                        second: l.key === "net" ? probe.up : l.key === "mem" ? probe.swapUsed : null
                        max: l.key === "mem" ? 1 : 0
                        span: l.key === "pages" ? 24 : 300
                        colour: l.key === "blocked" || l.key === "pages" ? root.hot : root.glow
                    }
                    Row {
                        visible: l.key === "downloads"
                        x: 200
                        y: 14
                        spacing: 24
                        Repeater {
                            model: probe.downloadsRunning
                            Column {
                                required property var modelData
                                Caption {
                                    text: modelData.fileName
                                    color: root.ink
                                }
                                Meter {
                                    width: 200
                                    value: modelData.fraction
                                }
                            }
                        }
                    }
                    Row {
                        visible: l.key === "spaces"
                        x: 200
                        y: 14
                        spacing: 30
                        Repeater {
                            model: probe.spaces
                            Column {
                                required property var modelData
                                Caption {
                                    text: modelData.name.toUpperCase()
                                }
                                Meter {
                                    width: 140
                                    value: modelData.mib / 2048
                                    colour: root.hot
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ================================================================ C
    // Cluster: an instrument cluster. Round gauges stand in an arc over the
    // Omnibar, which rests low; each dial carries its last five minutes as a
    // ring. Warning lamps and small readouts sit under it. Display only.
    Component {
        id: cluster

        Item {
            id: c
            readonly property var dials: root.shown.filter(key => ["cpu", "net", "mem", "blocked"].indexOf(
                key) >= 0)
            readonly property real dialSize: Math.min((root.width - 80) / Math.max(1,
                                                                                   c.dials.length)
                                                      - 24, root.barTop - 70, 260)

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(20, root.barTop - c.dialSize - 40 - (c.dials.length - 1) / 2
                            * c.dialSize * 0.12)
                spacing: 24
                Repeater {
                    model: c.dials
                    Item {
                        id: dial
                        required property string modelData
                        required property int index
                        width: c.dialSize
                        height: c.dialSize + 30
                        // The outer dials stand lower, so the row reads as an arc.
                        y: (Math.abs(index - (c.dials.length - 1) / 2)) * c.dialSize * 0.12
                        readonly property var series: modelData === "cpu" ? probe.cpuTotal :
                                                                            modelData === "net"
                                                                            ? probe.down :
                                                                              modelData === "mem"
                                                                              ? probe.memUsed :
                                                                                probe.blockedRate
                        readonly property real peak: modelData === "cpu" || modelData === "mem" ? 1 :
                                                                                                  Math.max(1,
                                                                                                           ...dial.series)
                        readonly property real value: root.lastOf(dial.series) / dial.peak
                        Canvas {
                            id: face
                            width: c.dialSize
                            height: c.dialSize
                            property real value: dial.value
                            property var series: dial.series
                            property var cores: dial.modelData === "cpu" ? root.latestCores : []
                            onValueChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onPaint: {
                                const g = getContext("2d");
                                g.reset();
                                const cx = width / 2, cy = height / 2, r = width / 2 - 4;
                                const from = Math.PI * 0.75, sweep = Math.PI * 1.5;
                                // Rim and scale.
                                g.strokeStyle = Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.5);
                                g.lineWidth = 1;
                                g.beginPath();
                                g.arc(cx, cy, r, from, from + sweep);
                                g.stroke();
                                for (let t = 0; t <= 10; ++t) {
                                    const a = from + sweep * t / 10;
                                    g.beginPath();
                                    g.moveTo(cx + Math.cos(a) * r, cy + Math.sin(a) * r);
                                    g.lineTo(cx + Math.cos(a) * (r - (t % 5 ? 6 : 12)), cy
                                             + Math.sin(a) * (r - (t % 5 ? 6 : 12)));
                                    g.stroke();
                                }
                                // History as a ring inside the scale.
                                const s = face.series;
                                const peak = dial.peak;
                                for (let i = 0; i < s.length; ++i) {
                                    const a = from + sweep * (i + 300 - s.length) / 300;
                                    const len = (r * 0.28) * Math.min(1, s[i] / peak);
                                    g.strokeStyle = Qt.rgba(root.glow.r, root.glow.g, root.glow.b,
                                                            0.35);
                                    g.beginPath();
                                    g.moveTo(cx + Math.cos(a) * (r * 0.6), cy + Math.sin(a) * (r
                                                                                               * 0.6));
                                    g.lineTo(cx + Math.cos(a) * (r * 0.6 + len), cy + Math.sin(a) * (
                                                 r * 0.6 + len));
                                    g.stroke();
                                }
                                // Per-core ticks round the rim.
                                for (let k = 0; k < face.cores.length; ++k) {
                                    const a = from + sweep * (k + 0.5) / face.cores.length;
                                    g.strokeStyle = face.cores[k] > 0.85 ? root.hot : root.glow;
                                    g.lineWidth = 3;
                                    g.beginPath();
                                    g.moveTo(cx + Math.cos(a) * (r + 2), cy + Math.sin(a) * (r
                                                                                             + 2));
                                    g.lineTo(cx + Math.cos(a) * (r + 2 + 10 * face.cores[k]), cy
                                             + Math.sin(a) * (r + 2 + 10 * face.cores[k]));
                                    g.stroke();
                                }
                                // Needle.
                                const a = from + sweep * Math.min(1, face.value);
                                g.strokeStyle = root.hot;
                                g.lineWidth = 2;
                                g.beginPath();
                                g.moveTo(cx, cy);
                                g.lineTo(cx + Math.cos(a) * (r - 14), cy + Math.sin(a) * (r - 14));
                                g.stroke();
                                g.fillStyle = root.hot;
                                g.beginPath();
                                g.arc(cx, cy, 4, 0, Math.PI * 2);
                                g.fill();
                            }
                        }
                        Column {
                            anchors.horizontalCenter: face.horizontalCenter
                            y: face.height * 0.62
                            Reading {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: dial.modelData === "cpu" ? root.pct(root.lastOf(
                                                                              probe.cpuTotal)) :
                                                                 dial.modelData === "net"
                                                                 ? root.rate(root.lastOf(
                                                                                 probe.down)) :
                                                                   dial.modelData === "mem"
                                                                   ? root.pct(root.lastOf(
                                                                                  probe.memUsed)) :
                                                                     probe.blockedOpen + ""
                            }
                            Caption {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: dial.modelData === "net" ? "▲ " + root.rate(root.lastOf(
                                                                                      probe.up)) :
                                                                 dial.modelData === "blocked"
                                                                 ? "BLOCKED "
                                                                   + probe.blockedSession
                                                                   + " SESSION" : dial.modelData
                                                                   === "mem" ? "SWAP " + root.pct(
                                                                                   root.lastOf(
                                                                                       probe.swapUsed)) :
                                                                               probe.cores.length
                                                                               + " CORES"
                            }
                        }
                        Caption {
                            anchors.horizontalCenter: face.horizontalCenter
                            y: face.height + 4
                            color: root.glow
                            text: ({
                                       cpu: "CPU",
                                       net: "NETWORK",
                                       mem: "MEMORY",
                                       blocked: "BLOCKED"
                                   })[dial.modelData]
                        }
                    }
                }
            }

            // Lamps and small readouts under the Omnibar.
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.barBottom + 16
                spacing: 36
                Column {
                    visible: root.has("battery")
                    spacing: 4
                    Caption {
                        text: "● BATTERY"
                        color: probe.battery && probe.battery.status === "Charging" ? root.glow :
                                                                                      root.dim
                    }
                    Reading {
                        text: probe.battery ? root.pct(probe.battery.capacity) + "  "
                                              + probe.battery.watts.toFixed(1) + " W" : ""
                    }
                }
                Column {
                    visible: root.has("downloads")
                    spacing: 4
                    Caption {
                        text: "● DOWNLOADING"
                        color: root.glow
                        opacity: 0.5 + 0.5 * Math.abs(Math.sin(root.time * 4))
                    }
                    Repeater {
                        model: probe.downloadsRunning
                        Meter {
                            required property var modelData
                            width: 120
                            value: modelData.fraction
                        }
                    }
                }
                Column {
                    visible: root.has("pages")
                    spacing: 4
                    Caption {
                        text: "PAGES TODAY [FAKE]"
                        color: root.hot
                    }
                    Trace {
                        width: 160
                        height: 28
                        values: probe.pagesToday
                        span: 24
                    }
                }
                Column {
                    visible: root.has("spaces")
                    spacing: 2
                    Caption {
                        text: "SPACES [FAKE]"
                        color: root.hot
                    }
                    Repeater {
                        model: probe.spaces
                        Caption {
                            required property var modelData
                            color: root.ink
                            text: modelData.name + "  " + modelData.mib + " MiB"
                        }
                    }
                }
            }
        }
    }

    // ================================================================ D
    // Console: two dense columns flank the Omnibar, a monitor's panes, with a
    // ticker over it. Rows can be clicked: a host shows the pages it was
    // blocked on, a download or the pages sparkline opens its list, a Space
    // says what it would do.
    Component {
        id: consoleLayout

        Item {
            id: d
            // Two columns under the Omnibar, which rests high as their header.
            readonly property real colW: (root.width - 48) / 2
            readonly property bool sides: true
            property string openHost: ""
            property string note: ""

            // Ticker over the Omnibar.
            Caption {
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.barTop - 22
                color: root.glow
                text: "CPU " + root.pct(root.lastOf(probe.cpuTotal)) + "   ▼ " + root.rate(
                          root.lastOf(probe.down)) + "  ▲ " + root.rate(root.lastOf(probe.up)) + "   MEM "
                      + root.pct(root.lastOf(probe.memUsed)) + "   BLOCKED " + probe.blockedOpen + (
                          probe.battery ? "   BAT " + root.pct(probe.battery.capacity) : "")
            }
            Caption {
                visible: d.note !== ""
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.barBottom + 8
                color: root.hot
                text: d.note
            }
            Timer {
                running: d.note !== ""
                interval: 3000
                onTriggered: d.note = ""
            }

            // Left column: machine.
            Column {
                visible: d.sides
                x: 16
                y: root.barBottom + 20
                spacing: 8
                Caption {
                    visible: root.has("cpu")
                    color: root.glow
                    text: "── CPU ────────── " + probe.cores.length + " cores"
                }
                Grid {
                    columns: 2
                    columnSpacing: 16
                    rowSpacing: 3
                    Repeater {
                        model: root.has("cpu") ? root.latestCores : []
                        Row {
                            required property real modelData
                            required property int index
                            spacing: 6
                            Caption {
                                width: 26
                                text: String(index).padStart(2, "0")
                            }
                            Meter {
                                y: 3
                                width: d.colW / 2 - 88
                                value: modelData
                                colour: modelData > 0.85 ? root.hot : root.glow
                            }
                            Caption {
                                color: root.ink
                                text: root.pct(modelData).padStart(4)
                            }
                        }
                    }
                }
                Caption {
                    visible: root.has("net")
                    color: root.glow
                    text: "── NET ──────────────────"
                }
                Trace {
                    visible: root.has("net")
                    width: d.colW
                    height: 50
                    values: probe.down
                    second: probe.up
                }
                Caption {
                    visible: root.has("net")
                    color: root.ink
                    text: "▼ " + root.rate(root.lastOf(probe.down)) + "   ▲ " + root.rate(root.lastOf(
                                                                                              probe.up))
                }
                Caption {
                    visible: root.has("mem")
                    color: root.glow
                    text: "── MEM ──────────────────"
                }
                Trace {
                    visible: root.has("mem")
                    width: d.colW
                    height: 40
                    values: probe.memUsed
                    second: probe.swapUsed
                    max: 1
                }
                Caption {
                    visible: root.has("mem")
                    color: root.ink
                    text: "RAM " + root.pct(root.lastOf(probe.memUsed)) + "  SWAP " + root.pct(
                              root.lastOf(probe.swapUsed)) + "  UI " + root.size(probe.chromeKib)
                }
                Column {
                    visible: root.has("battery")
                    Caption {
                        color: root.glow
                        text: "── BATTERY ──────────────"
                    }
                    Caption {
                        color: root.ink
                        text: probe.battery ? root.pct(probe.battery.capacity) + "  "
                                              + probe.battery.status + "  "
                                              + probe.battery.watts.toFixed(1) + " W" : ""
                    }
                }
            }

            // Right column: browser. Click rows.
            Column {
                visible: d.sides
                x: root.width - 16 - d.colW
                y: root.barBottom + 20
                spacing: 4
                Caption {
                    visible: root.has("blocked")
                    color: root.glow
                    text: "── BLOCKED ── " + probe.blockedOpen + " open · " + probe.blockedSession
                          + " session"
                }
                Trace {
                    visible: root.has("blocked")
                    width: d.colW
                    height: 30
                    values: probe.blockedRate
                    colour: root.hot
                }
                Repeater {
                    model: root.has("blocked") ? probe.topHosts : []
                    Column {
                        required property var modelData
                        Row2 {
                            leftText: modelData.host
                            rightText: String(modelData.count)
                            onClicked: d.openHost = d.openHost === modelData.host ? "" :
                                                                                    modelData.host
                        }
                        Caption {
                            visible: d.openHost === modelData.host
                            x: 20
                            width: d.colW - 20
                            wrapMode: Text.Wrap
                            text: "blocked " + modelData.count
                                  + "× on this Space's open tabs. Full impl: list the pages, or open Site information."
                        }
                    }
                }
                Caption {
                    visible: root.has("blocked") && probe.topHosts.length === 0
                    text: "  nothing blocked on open tabs"
                }
                Caption {
                    visible: root.has("downloads")
                    color: root.glow
                    text: "── DOWNLOADS ────────────"
                }
                Repeater {
                    model: root.has("downloads") ? probe.downloadsRunning : []
                    Row2 {
                        required property var modelData
                        leftText: modelData.fileName
                        rightText: root.pct(Math.max(0, modelData.fraction))
                        onClicked: root.activated("downloads", modelData.fileName)
                    }
                }
                Caption {
                    visible: root.has("pages")
                    color: root.hot
                    text: "── PAGES TODAY [FAKE] ─── " + probe.pagesToday.reduce((s, v) => s + v, 0)
                }
                Item {
                    visible: root.has("pages")
                    width: d.colW
                    height: 40
                    Trace {
                        anchors.fill: parent
                        values: probe.pagesToday
                        span: 24
                    }
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: root.activated("history", "")
                    }
                }
                Caption {
                    visible: root.has("spaces")
                    color: root.hot
                    text: "── MEMORY PER SPACE [FAKE] "
                }
                Repeater {
                    model: root.has("spaces") ? probe.spaces : []
                    Row2 {
                        required property var modelData
                        leftText: modelData.name
                        rightText: modelData.mib + " MiB"
                        onClicked: d.note = "Full impl: switch to the " + modelData.name + " Space"
                    }
                }
            }

            // Too narrow for side columns: everything under the Omnibar.
            Caption {
                visible: !d.sides
                anchors.horizontalCenter: parent.horizontalCenter
                y: root.barBottom + 30
                color: root.dim
                text: "(narrow: Console keeps only the ticker)"
            }
        }
    }

    component Row2: Rectangle {
        id: row2
        property string leftText
        property string rightText
        property bool clickable: true
        property color tint: root.ink
        signal clicked
        width: d.colW
        height: 20
        color: hover.hovered && clickable ? Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.25) :
                                            "transparent"
        Caption {
            x: 4
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 80
            elide: Text.ElideRight
            color: row2.tint
            text: (row2.clickable ? "› " : "  ") + row2.leftText
        }
        Caption {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            color: row2.tint
            text: row2.rightText
        }
        HoverHandler {
            id: hover
            cursorShape: row2.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
        TapHandler {
            enabled: row2.clickable
            onTapped: row2.clicked()
        }
    }
}
