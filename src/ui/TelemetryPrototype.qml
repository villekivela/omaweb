import QtQuick
import QtQuick.Shapes
import Omaweb

// PROTOTYPE (#814), throwaway: the Telemetry Scene, a retro CRT dashboard
// around the resting Omnibar, for the maintainer to judge. Never merged; it
// lives on prototype/020-telemetry.
//
// Round 3: the Cockpit edge to edge, as the maintainer's reference draws a
// CRT dashboard: charts with dotted gridlines, axis values and a lit callout,
// hatched bars, a row of figures under the Omnibar, and sections without
// frames. Under the real CRT glass with real scanlines
// (TelemetryHostPrototype.qml), text in two sizes to compare.
//
// Real: CPU per core, network, memory, cache and swap (/proc), battery (/sys),
// the chrome process's memory, requests blocked on the Space's open tabs,
// running downloads. Fake, and labelled FAKE on screen: pages loaded today,
// memory per Space.
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
    // "S" or "L": the two text sizes to compare.
    property string textSize: "S"
    property var blocker: null
    property var browser: null
    property var tabs: null
    property var downloads: null
    // Ctrl+Alt+D: pretend a download runs, to see the module.
    property bool fakeDownload: false

    // ---- what it declares

    readonly property int pitch: 1
    readonly property int fps: 30
    readonly property string glass: "crt"
    readonly property string screen: "raster"
    // The road's glass, its bloom stronger and wider for glyphs.
    readonly property var crt: ({
                                    bloom: {
                                        scale: 4,
                                        opacity: 0.4
                                    },
                                    scanlines: {
                                        shade: 0.38,
                                        narrowest: 0.8,
                                        widest: 1.15,
                                        falloff: 2.2,
                                        lift: 1.2
                                    },
                                    mask: {
                                        dim: 0.88,
                                        lift: 1.18
                                    },
                                    halation: {
                                        threshold: 0.3,
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
                                            strength: 0.45
                                        },
                                        midtones: {
                                            from: 0.08,
                                            to: 0.5,
                                            strength: 0.35
                                        }
                                    },
                                    vignette: {
                                        clear: 0.6,
                                        shade: 0.55
                                    },
                                    band: {
                                        every: 7,
                                        from: -0.2,
                                        travel: 1.4,
                                        reach: 0.07,
                                        reachAtLeast: 4,
                                        strength: 0.05
                                    },
                                    flicker: {
                                        least: 0.02,
                                        range: 0.02,
                                        frequencies: [37.1, 11.3]
                                    }
                                })
    readonly property var declaredOptions: ({})
    readonly property real horizonY: Math.round(root.height * 0.5)

    // ---- palette

    readonly property color ground: root.dark ? root.colors.windowOpaque : Qt.darker(root.colors.text,
                                                                                     3)
    readonly property color ink: root.dark ? root.colors.text : root.colors.windowOpaque
    readonly property color glow: root.unlit ? Qt.darker(root.ink, 1.6) : root.colors.accent
    readonly property color hot: root.colors.syntax && root.colors.syntax.keyword
                                 ? root.colors.syntax.keyword : Qt.lighter(root.glow, 1.4)
    readonly property color dim: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.82)
    readonly property color faint: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.16)
    // The desktop's monospace, JetBrains Mono on Omarchy.
    readonly property string mono: "JetBrainsMono Nerd Font"

    // ---- text: two sizes, each a glyph over at least six scanlines of three
    // device pixels (TelemetryHostPrototype.qml)

    readonly property bool large: root.textSize === "L"
    readonly property int captionSize: root.large ? 22 : 18
    readonly property int readingSize: root.large ? 32 : 26

    // ---- where the Omnibar rests, which the panels keep clear

    readonly property real barTop: root.horizonY - 50 - 16
    readonly property real barBottom: root.horizonY + root.omnibarReach + 16

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
    // The share of a 1700 by 1000 page area this one is, text size counted.
    readonly property real room: Math.min(root.width / 1700, root.height / 1000) * (root.large ? 0.8 :
                                                                                                 1)
    readonly property int holds: root.room >= 0.75 ? 8 : root.room >= 0.6 ? 6 : root.room >= 0.45
                                                                            ? 4 : root.room >= 0.3
                                                                              ? 2 : 0
    readonly property var shown: root.present.slice(0, root.holds)
    function has(key) {
        return root.shown.indexOf(key) >= 0;
    }

    // ---- formatting

    // A rate's unit, chosen once for a whole series from its peak, so it
    // changes only when the traffic changes scale, never between samples:
    // kB/s or MB/s, never B/s.
    function rateUnit(series) {
        let peak = 0;
        for (let i = 0; i < series.length; ++i)
            peak = Math.max(peak, series[i]);
        if (peak >= 1048576)
            return {
                name: "MB/s",
                per: 1048576,
                places: 1
            };
        return {
            name: "kB/s",
            per: 1024,
            places: peak >= 10240 ? 0 : 1
        };
    }
    function inUnit(bytes, unit) {
        return (bytes / unit.per).toFixed(unit.places);
    }
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
    function lastOf(series) {
        return series.length ? series[series.length - 1] : 0;
    }
    // The largest of `series`, at least 1. A loop, not Math.max(...series):
    // a spread takes stack for every element.
    function peakOf(series) {
        let peak = 1;
        for (let i = 0; i < series.length; ++i)
            if (series[i] > peak)
                peak = series[i];
        return peak;
    }

    // ---- the probe: CPU and network four times a second, the rest once a
    // second, five minutes kept, nothing on disk

    QtObject {
        id: probe

        readonly property int keep: 300
        // Four samples a second for five minutes.
        readonly property int fastKeep: 1200
        property var cores: []
        property var cpuTotal: []
        property var last: null
        property var down: []
        // The rates eased, for the readings: a number that changes four
        // times a second cannot be read.
        property real downEased: 0
        property real upEased: 0
        property var up: []
        property var lastNet: null
        property var meminfo: ({})
        property real chromeKib: 0
        property var battery: null
        property var watts: []
        property var charge: []
        property int blockedOpen: 0
        property int blockedSession: 0
        property var blockedRate: []
        property var topHosts: []
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
                name: "Dev",
                mib: 655
            }
        ]

        function push(series, value, keep) {
            const next = series.slice(Math.max(0, series.length - (keep || probe.keep) + 1));
            next.push(value);
            return next;
        }

        function fastSample() {
            probe.readCpu();
            probe.readNet();
        }

        function sample() {
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
                now.push({
                             core: m[1] === "" ? -1 : Number(m[1]),
                             total: f.slice(0, 8).reduce((a, b) => a + b, 0),
                             idle: f[3] + f[4]
                         });
            }
            if (probe.last && probe.last.length === now.length) {
                const cores = probe.cores.slice();
                for (let i = 0; i < now.length; ++i) {
                    const dt = now[i].total - probe.last[i].total;
                    const busy = dt > 0 ? 1 - (now[i].idle - probe.last[i].idle) / dt : 0;
                    if (now[i].core < 0)
                        probe.cpuTotal = probe.push(probe.cpuTotal, busy, probe.fastKeep);
                    else
                        cores[now[i].core] = busy;
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
                probe.down = probe.push(probe.down, Math.max(0, (rx - probe.lastNet.rx) / s),
                                        probe.fastKeep);
                probe.up = probe.push(probe.up, Math.max(0, (tx - probe.lastNet.tx) / s),
                                      probe.fastKeep);
                probe.downEased += (root.lastOf(probe.down) - probe.downEased) * 0.15;
                probe.upEased += (root.lastOf(probe.up) - probe.upEased) * 0.15;
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
            const self = /VmRSS:\s+(\d+)/.exec(Scenes.systemFile("/proc/self/status"));
            probe.chromeKib = self ? Number(self[1]) : 0;
        }

        // The machine's own battery, not a device's (a trackpad's has scope
        // Device). Its draw is power_now, or current_now times voltage_now
        // where the firmware gives those instead.
        function readBattery() {
            for (const name of Scenes.systemEntries("/sys/class/power_supply")) {
                const dir = "/sys/class/power_supply/" + name + "/";
                if (Scenes.systemFile(dir + "type").trim() !== "Battery" || Scenes.systemFile(dir
                                                                                              + "scope").trim(
                            ) === "Device")
                    continue;
                const power = Scenes.systemFile(dir + "power_now");
                const watts = power !== "" ? Number(power) / 1e6 : Number(Scenes.systemFile(dir
                                                                                            + "current_now"))
                                             * Number(Scenes.systemFile(dir + "voltage_now"))
                                             / 1e12;
                probe.battery = {
                    capacity: Number(Scenes.systemFile(dir + "capacity")) / 100,
                    status: Scenes.systemFile(dir + "status").trim(),
                    watts: watts
                };
                probe.watts = probe.push(probe.watts, watts);
                probe.charge = probe.push(probe.charge, probe.battery.capacity);
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
            const delta = probe.blockedRate.length > 0 ? Math.max(0, open - probe.blockedOpen) : 0;
            probe.blockedSession += delta;
            probe.blockedRate = probe.push(probe.blockedRate, delta);
            probe.blockedOpen = open;
            probe.topHosts = Object.keys(hosts).map(host => ({
                host: host,
                count: hosts[host]
            })).sort((a, b) => b.count - a.count).slice(0, 3);
        }

        function readDownloads() {
            const running = root.downloads ? root.downloads.runningDownloads.slice() : [];
            if (root.fakeDownload)
                running.push({
                                 fileName: "FAKE-ubuntu-24.04.iso",
                                 fraction: (Date.now() / 40000) % 1
                             });
            probe.downloadsRunning = running;
        }
    }

    Timer {
        interval: 250
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: probe.fastSample()
    }
    Timer {
        interval: 1000
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: probe.sample()
    }

    // ---- parts

    // Text as a CRT draws it: the glyph in its colour, and its red and blue
    // a little apart either side, as a tube's three beams converge a little
    // off. The glass adds the bloom, scanlines and mask over it.
    component CrtText: Item {
        id: crtText
        property alias text: main.text
        property color color: root.ink
        property int pixelSize: root.captionSize
        property bool bold: false
        property int elide: Text.ElideNone
        property real maxWidth: -1
        // The red and blue beams' offset: a pixel on large text, none on
        // small text, where it smears the strokes.
        readonly property real split: crtText.pixelSize >= 24 ? 1 : 0
        implicitWidth: main.implicitWidth
        implicitHeight: main.implicitHeight
        width: crtText.maxWidth > 0 ? Math.min(crtText.maxWidth, main.implicitWidth) :
                                      main.implicitWidth
        height: main.implicitHeight

        Text {
            x: -crtText.split
            width: parent.width
            text: main.text
            elide: crtText.elide
            visible: crtText.split > 0
            color: "#ff2a3a"
            opacity: 0.3
            font: main.font
        }
        Text {
            x: crtText.split
            width: parent.width
            text: main.text
            elide: crtText.elide
            visible: crtText.split > 0
            color: "#2a6aff"
            opacity: 0.3
            font: main.font
        }
        Text {
            id: main
            width: parent.width
            elide: crtText.elide
            color: crtText.color
            font.family: root.mono
            font.pixelSize: crtText.pixelSize
            font.weight: crtText.bold ? Font.Bold : Font.Medium
            font.letterSpacing: crtText.pixelSize / 20
        }
    }
    component Caption: CrtText {
        color: root.dim
        pixelSize: root.captionSize
    }
    component Reading: CrtText {
        color: root.ink
        pixelSize: root.readingSize
        bold: true
    }

    // A chart as the reference draws one: dotted gridlines with their values
    // at the right, time along the bottom, `values` as a hatched area under a
    // bright line ("area") or as hatched bars ("bars"), `second` as a line
    // over it, and the latest value in a lit box at the line's head.
    component Chart: Item {
        id: chart
        property var values: []
        property var second: null
        property real max: 0
        // The scale `second` is drawn on, or 0 for the same as `values`.
        property real secondMax: 0
        property int span: 300
        property string style: "area"
        // Bars: how many the span is grouped into.
        property int bars: 60
        property var format: function (v) {
            return String(Math.round(v));
        }
        property var timeLabels: ["-5m", "-4m", "-3m", "-2m", "-1m", "now"]
        property bool callout: true
        readonly property real axisW: root.captionSize * 4.5
        readonly property real axisH: root.captionSize * 1.6
        readonly property real plotW: Math.max(10, chart.width - chart.axisW)
        readonly property real plotH: Math.max(10, chart.height - chart.axisH)
        // The scale's top: the given one, or the peak rounded up.
        readonly property real scaleTop: chart.max > 0 ? chart.max : chart.nice(Math.max(root.peakOf(
                                                                                             chart.values),
                                                                                         chart.secondMax
                                                                                         > 0 ? 0 : root.peakOf(
                                                                                                   chart.second
                                                                                                   || [])) * 1.15)
        readonly property real latest: root.lastOf(chart.values)
        // The callout eases, so it glides as the line moves.
        property real headY: chart.plotH - Math.min(1, chart.latest / chart.scaleTop) * chart.plotH

        Behavior on headY {
            NumberAnimation {
                duration: 300
                easing.type: Easing.OutCubic
            }
        }

        function nice(v) {
            const step = Math.pow(10, Math.floor(Math.log10(Math.max(1e-9, v))));
            for (const k of [1, 2, 2.5, 5, 10])
                if (k * step >= v)
                    return k * step;
            return 10 * step;
        }

        Canvas {
            id: plot
            width: chart.plotW
            height: chart.plotH
            property var values: chart.values
            property var second: chart.second
            property real scaleTop: chart.scaleTop
            onValuesChanged: requestPaint()
            onScaleTopChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d");
                c.reset();
                const w = width, h = height, top = plot.scaleTop;
                const glow = root.glow;
                // Gridlines, dotted.
                c.strokeStyle = Qt.rgba(glow.r, glow.g, glow.b, 0.35);
                c.lineWidth = 1;
                c.setLineDash([1, 4]);
                for (let t = 0; t <= 4; ++t) {
                    const y = Math.round(h - h * t / 4) + 0.5;
                    c.beginPath();
                    c.moveTo(0, y);
                    c.lineTo(w, y);
                    c.stroke();
                }
                for (let t = 0; t < chart.timeLabels.length; ++t) {
                    const x = Math.round(w * t / (chart.timeLabels.length - 1)) + 0.5;
                    c.beginPath();
                    c.moveTo(x, 0);
                    c.lineTo(x, h);
                    c.stroke();
                }
                c.setLineDash([]);
                const vs = plot.values;
                if (vs.length < 1)
                    return;
                const yOf = v => h - Math.min(1, v / top) * h;
                // Hatching, clipped to the shape: vertical lines every 3 px.
                const hatch = () => {
                    c.save();
                    c.clip();
                    c.strokeStyle = Qt.rgba(glow.r, glow.g, glow.b, 0.45);
                    c.lineWidth = 1;
                    c.beginPath();
                    for (let x = 0.5; x < w; x += 3) {
                        c.moveTo(x, 0);
                        c.lineTo(x, h);
                    }
                    c.stroke();
                    c.restore();
                };
                if (chart.style === "bars") {
                    const per = chart.span / chart.bars;
                    const bw = w / chart.bars;
                    // The bars from the newest back, each the mean of its samples.
                    const tops = [];
                    for (let b = 0; b < chart.bars; ++b) {
                        const end = vs.length - b * per;
                        if (end <= 0)
                            break;
                        let sum = 0, n = 0;
                        for (let i = Math.max(0, Math.floor(end - per)); i < end; ++i) {
                            sum += vs[i];
                            ++n;
                        }
                        const x = w - (b + 1) * bw;
                        tops.push([x, yOf(sum / n)]);
                    }
                    c.beginPath();
                    for (const [x, y] of tops)
                        c.rect(x + 2, y, bw - 4, h - y);
                    c.fillStyle = Qt.rgba(glow.r, glow.g, glow.b, 0.12);
                    c.fill();
                    hatch();
                    c.strokeStyle = glow;
                    c.lineWidth = 2;
                    c.beginPath();
                    for (const [x, y] of tops) {
                        c.moveTo(x + 2, y);
                        c.lineTo(x + bw - 2, y);
                    }
                    c.stroke();
                } else {
                    const step = w / (chart.span - 1);
                    const x0 = w - (vs.length - 1) * step;
                    c.beginPath();
                    c.moveTo(x0, h);
                    for (let i = 0; i < vs.length; ++i)
                        c.lineTo(x0 + i * step, yOf(vs[i]));
                    c.lineTo(w, h);
                    c.closePath();
                    c.fillStyle = Qt.rgba(glow.r, glow.g, glow.b, 0.12);
                    c.fill();
                    hatch();
                    c.beginPath();
                    for (let i = 0; i < vs.length; ++i)
                        if (i === 0)
                            c.moveTo(x0, yOf(vs[0]));
                        else
                            c.lineTo(x0 + i * step, yOf(vs[i]));
                    c.strokeStyle = glow;
                    c.lineWidth = 2.5;
                    c.stroke();
                }
                const ss = plot.second;
                if (ss && ss.length > 1) {
                    const step = w / (chart.span - 1);
                    const x0 = w - (ss.length - 1) * step;
                    c.beginPath();
                    const y2 = v => chart.secondMax > 0 ? h - Math.min(1, v / chart.secondMax) * h :
                                                          yOf(v);
                    for (let i = 0; i < ss.length; ++i)
                        if (i === 0)
                            c.moveTo(x0, y2(ss[0]));
                        else
                            c.lineTo(x0 + i * step, y2(ss[i]));
                    c.strokeStyle = root.hot;
                    c.lineWidth = 2;
                    c.stroke();
                }
            }
        }
        // The scale's values at the right of each gridline.
        Repeater {
            model: 5
            Caption {
                required property int index
                x: chart.plotW + 8
                y: chart.plotH - chart.plotH * index / 4 - height / 2
                text: chart.format(chart.scaleTop * index / 4)
            }
        }
        // Time along the bottom.
        Repeater {
            model: chart.timeLabels
            Caption {
                required property string modelData
                required property int index
                x: Math.min(chart.plotW - width, Math.max(0, chart.plotW * index / (
                                                              chart.timeLabels.length - 1) - width
                                                          / 2))
                y: chart.plotH + 4
                text: modelData
            }
        }
        // The latest value, lit, at the line's head.
        Rectangle {
            visible: chart.callout && chart.values.length > 0
            x: chart.plotW - width - 6
            y: Math.max(0, Math.min(chart.plotH - height, chart.headY - height - 10))
            width: calloutText.width + 16
            height: calloutText.height + 8
            color: root.glow
            CrtText {
                id: calloutText
                anchors.centerIn: parent
                color: root.ground
                bold: true
                pixelSize: root.readingSize * 0.8
                text: chart.format(chart.latest)
            }
        }
    }

    // A rule of dots, as the reference underlines its titles and figures.
    component DottedRule: Canvas {
        height: 2
        onWidthChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            c.fillStyle = Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.6);
            for (let x = 0; x < width; x += 5)
                c.fillRect(x, 0, 2, 2);
        }
    }

    // A section: its title over a dotted rule, its content under.
    component Section: Item {
        id: section
        property string title
        property bool fake: false
        default property alias content: body.data
        Caption {
            id: heading
            text: section.title + (section.fake ? "  [FAKE]" : "")
            color: section.fake ? root.hot : root.glow
        }
        DottedRule {
            y: heading.height + 4
            width: section.width
        }
        Item {
            id: body
            y: heading.height + 16
            width: section.width
            height: Math.max(0, section.height - y)
        }
    }

    // A figure: a small label over a large value over a dotted rule.
    component Figure: Column {
        id: figure
        property string label
        property string value
        property bool small: false
        property bool fake: false
        spacing: 4
        Caption {
            text: figure.label + (figure.fake ? " [FAKE]" : "")
            color: figure.fake ? root.hot : root.glow
        }
        Reading {
            text: figure.value
            pixelSize: figure.small ? root.readingSize : Math.round(root.readingSize * 1.45)
        }
        DottedRule {
            width: figure.width
        }
    }

    // A round gauge, as a HUD draws one: a 270 degree scale of segments that
    // light up to `value` (0..1), a fine ring of ticks outside it with a lit
    // marker at the value and a dim one at `peak` (or none below 0), a thin
    // ring inside it turning as a scanner, brackets at the corners, and the
    // reading and label in the middle. The scale is painted once; the lit
    // segments, markers and scanner are scene-graph items, so they glide.
    component Gauge: Item {
        id: gauge
        property real value: 0
        property real peak: -1
        property string reading
        property string label
        // The unit, under the reading in the scale's gap, and the scale's top
        // in it; a percentage carries its unit in the reading.
        property string unit
        property string scaleTop: "100"
        property bool fake: false
        property real size: 140
        property real eased: Math.max(0, Math.min(1, gauge.value))
        property real easedPeak: Math.max(0, Math.min(1, gauge.peak))
        readonly property real cx: gauge.size / 2
        readonly property real r: gauge.size / 2 - 4
        // The segmented scale's radius and thickness.
        readonly property real band: Math.max(6, gauge.r * 0.13)
        readonly property real scaleR: gauge.r * 0.8 - gauge.band / 2
        // Forty segments, each 70% lit and 30% gap, in stroke widths.
        readonly property real segment: gauge.scaleR * (Math.PI * 1.5) / 40 / gauge.band
        readonly property color tone: gauge.eased > 0.85 ? root.hot : root.glow
        width: gauge.size
        height: gauge.size

        Behavior on eased {
            NumberAnimation {
                duration: 450
                easing.type: Easing.OutCubic
            }
        }
        Behavior on easedPeak {
            NumberAnimation {
                duration: 450
                easing.type: Easing.OutCubic
            }
        }

        // The fine outer ring: a tick every 4.5 degrees, longer every 45.
        Canvas {
            id: face
            anchors.fill: parent
            onWidthChanged: requestPaint()
            onPaint: {
                const g = getContext("2d");
                g.reset();
                const cx = gauge.cx, r = gauge.r;
                const from = Math.PI * 0.75, sweep = Math.PI * 1.5;
                const glow = root.glow;
                g.strokeStyle = Qt.rgba(glow.r, glow.g, glow.b, 0.5);
                g.lineWidth = 1;
                for (let t = 0; t <= 60; ++t) {
                    const a = from + sweep * t / 60;
                    const inner = r * (t % 10 ? 0.93 : 0.88);
                    g.beginPath();
                    g.moveTo(cx + Math.cos(a) * inner, cx + Math.sin(a) * inner);
                    g.lineTo(cx + Math.cos(a) * r, cx + Math.sin(a) * r);
                    g.stroke();
                }
                // A hairline inside the scale.
                g.strokeStyle = Qt.rgba(glow.r, glow.g, glow.b, 0.3);
                g.beginPath();
                g.arc(cx, cx, gauge.scaleR - gauge.band / 2 - 5, from, from + sweep);
                g.stroke();
            }
        }

        // The scale: every segment dim, and those up to the value lit.
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: root.faint
                strokeWidth: gauge.band
                strokeStyle: ShapePath.DashLine
                dashPattern: [gauge.segment * 0.7, gauge.segment * 0.3]
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: gauge.cx
                    centerY: gauge.cx
                    radiusX: gauge.scaleR
                    radiusY: gauge.scaleR
                    startAngle: 135
                    sweepAngle: 270
                }
            }
            ShapePath {
                strokeColor: gauge.tone
                strokeWidth: gauge.band
                strokeStyle: ShapePath.DashLine
                dashPattern: [gauge.segment * 0.7, gauge.segment * 0.3]
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: gauge.cx
                    centerY: gauge.cx
                    radiusX: gauge.scaleR
                    radiusY: gauge.scaleR
                    startAngle: 135
                    sweepAngle: 270 * gauge.eased
                }
            }
        }

        // The scanner: a thin broken ring inside, turning.
        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            rotation: (root.time * 24) % 360
            ShapePath {
                strokeColor: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.55)
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: gauge.cx
                    centerY: gauge.cx
                    radiusX: gauge.r * 0.5
                    radiusY: gauge.r * 0.5
                    startAngle: 0
                    sweepAngle: 70
                }
            }
            ShapePath {
                strokeColor: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.55)
                strokeWidth: 2
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: gauge.cx
                    centerY: gauge.cx
                    radiusX: gauge.r * 0.5
                    radiusY: gauge.r * 0.5
                    startAngle: 180
                    sweepAngle: 70
                }
            }
        }

        // Markers on the outer ring, pointing in: the peak, dim, and the
        // value, lit.
        Item {
            visible: gauge.peak >= 0
            x: gauge.cx
            y: gauge.cx
            rotation: 135 + 270 * gauge.easedPeak + 90
            Rectangle {
                x: -1
                y: -gauge.r - 2
                width: 2
                height: gauge.r * 0.16
                antialiasing: true
                color: Qt.rgba(root.hot.r, root.hot.g, root.hot.b, 0.6)
            }
        }
        Item {
            x: gauge.cx
            y: gauge.cx
            rotation: 135 + 270 * gauge.eased + 90
            Rectangle {
                x: -1.5
                y: -gauge.r - 2
                width: 3
                height: gauge.r * 0.26
                antialiasing: true
                color: root.hot
            }
        }

        // Brackets at the corners.
        Repeater {
            model: 4
            Item {
                required property int index
                readonly property real arm: gauge.size * 0.1
                x: index % 2 ? gauge.size - arm : 0
                y: index > 1 ? gauge.size - arm : 0
                width: arm
                height: arm
                Rectangle {
                    y: parent.index > 1 ? parent.height - 2 : 0
                    width: parent.width
                    height: 2
                    color: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.7)
                }
                Rectangle {
                    x: parent.index % 2 ? parent.width - 2 : 0
                    width: 2
                    height: parent.height
                    color: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.7)
                }
            }
        }

        Column {
            anchors.centerIn: parent
            spacing: 2
            Reading {
                anchors.horizontalCenter: parent.horizontalCenter
                text: gauge.reading
                color: gauge.tone
                pixelSize: Math.max(root.captionSize, Math.min(root.readingSize * 1.3, gauge.size
                                                               / 6.5))
            }
            Caption {
                anchors.horizontalCenter: parent.horizontalCenter
                text: gauge.label + (gauge.fake ? " [FAKE]" : "")
                color: gauge.fake ? root.hot : root.glow
            }
        }
        // The scale's ends, under the gap.
        Caption {
            x: gauge.cx - gauge.r * 0.62 - width / 2
            y: gauge.cx + gauge.r * 0.62
            text: "0"
        }
        Caption {
            x: gauge.cx + gauge.r * 0.62 - width / 2
            y: gauge.cx + gauge.r * 0.62
            text: gauge.scaleTop
        }
        Reading {
            visible: gauge.unit !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            y: gauge.cx + gauge.r * 0.6
            pixelSize: root.captionSize
            color: root.glow
            text: gauge.unit
        }
    }

    // A donut: up to three `parts` [{value, colour}] round the ring, its
    // reading in the middle, its label under it. Each part's share glides to
    // its new size.
    component Donut: Item {
        id: donut
        property var parts: []
        property string reading
        property string label
        property real size: 140
        readonly property real total: donut.parts.reduce((s, p) => s + p.value, 0) || 1
        property real share0: donut.parts.length > 0 ? donut.parts[0].value / donut.total : 0
        property real share1: donut.parts.length > 1 ? donut.parts[1].value / donut.total : 0
        property real share2: donut.parts.length > 2 ? donut.parts[2].value / donut.total : 0
        readonly property real r: donut.size / 2 - 3
        readonly property real thick: donut.r * 0.28
        width: donut.size
        height: donut.size + labelText.height + 4

        Behavior on share0 {
            NumberAnimation {
                duration: 450
                easing.type: Easing.OutCubic
            }
        }
        Behavior on share1 {
            NumberAnimation {
                duration: 450
                easing.type: Easing.OutCubic
            }
        }
        Behavior on share2 {
            NumberAnimation {
                duration: 450
                easing.type: Easing.OutCubic
            }
        }

        function colourOf(index) {
            return donut.parts.length > index ? donut.parts[index].colour : "transparent";
        }

        Shape {
            id: ring
            width: donut.size
            height: donut.size
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: donut.colourOf(0)
                strokeWidth: donut.thick
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: donut.size / 2
                    centerY: donut.size / 2
                    radiusX: donut.r - donut.thick / 2
                    radiusY: donut.r - donut.thick / 2
                    startAngle: -90
                    sweepAngle: Math.max(0, 360 * donut.share0 - 2)
                }
            }
            ShapePath {
                strokeColor: donut.colourOf(1)
                strokeWidth: donut.thick
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: donut.size / 2
                    centerY: donut.size / 2
                    radiusX: donut.r - donut.thick / 2
                    radiusY: donut.r - donut.thick / 2
                    startAngle: -90 + 360 * donut.share0
                    sweepAngle: Math.max(0, 360 * donut.share1 - 2)
                }
            }
            ShapePath {
                strokeColor: donut.colourOf(2)
                strokeWidth: donut.thick
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: donut.size / 2
                    centerY: donut.size / 2
                    radiusX: donut.r - donut.thick / 2
                    radiusY: donut.r - donut.thick / 2
                    startAngle: -90 + 360 * (donut.share0 + donut.share1)
                    sweepAngle: Math.max(0, 360 * donut.share2 - 2)
                }
            }
        }
        Reading {
            anchors.centerIn: ring
            text: donut.reading
            pixelSize: Math.max(root.captionSize, Math.min(root.readingSize, donut.size / 5))
        }
        Caption {
            id: labelText
            anchors.horizontalCenter: ring.horizontalCenter
            y: ring.height + 2
            text: donut.label
            color: root.glow
        }
    }

    // ---- ground

    Rectangle {
        anchors.fill: parent
        color: root.ground
    }

    // ---- the cockpit, edge to edge as the reference: two charts and a
    // cluster of gauges over the Omnibar, a row of figures under it, and a
    // row of sections under that.

    Item {
        id: a
        anchors.fill: parent

        readonly property real gap: Math.round(Math.max(20, root.width * 0.012))
        readonly property real innerW: root.width - 2 * gap
        // Over the Omnibar: CPU, gauges, network. The two charts flank the
        // Omnibar's width; the gauges stand over it.
        readonly property real chartW: Math.round((innerW - 2 * gap) * 0.39)
        readonly property real midW: innerW - 2 * chartW - 2 * gap
        readonly property real upperY: gap + status.height + gap
        readonly property real upperH: Math.max(80, root.barTop - upperY - gap)
        // Under it: the figures, then the sections.
        readonly property real figuresY: root.barBottom + gap * 0.5
        readonly property real lowerY: figuresY + figures.height + gap * 1.5
        readonly property real lowerH: Math.max(60, root.height - lowerY - gap)

        // Status: the title, the clock, the text size, downloads while one
        // runs.
        Row {
            id: status
            x: a.gap
            y: a.gap
            spacing: 32
            Caption {
                id: clock
                color: root.glow
                text: "OMAWEB TELEMETRY  " + Qt.formatTime(new Date(), "hh:mm:ss")
                Timer {
                    interval: 1000
                    repeat: true
                    running: true
                    onTriggered: clock.text = "OMAWEB TELEMETRY  " + Qt.formatTime(new Date(),
                                                                                   "hh:mm:ss")
                }
            }
            Caption {
                text: "TEXT " + (root.large ? "L" : "S") + " " + root.captionSize + "/"
                      + root.readingSize + "px"
            }
            Repeater {
                model: root.has("downloads") ? probe.downloadsRunning : []
                Row {
                    required property var modelData
                    spacing: 10
                    Caption {
                        text: "▼ " + modelData.fileName
                        color: root.ink
                    }
                    Rectangle {
                        y: (parent.height - height) / 2
                        width: 160
                        height: root.captionSize / 2
                        color: root.faint
                        Rectangle {
                            width: parent.width * Math.max(0, modelData.fraction)
                            height: parent.height
                            color: root.glow
                        }
                    }
                    Caption {
                        text: root.pct(Math.max(0, modelData.fraction))
                        color: root.ink
                    }
                }
            }
        }

        // CPU: the total over five minutes, and a square per core under it.
        Section {
            id: cpu
            visible: root.has("cpu")
            x: a.gap
            y: a.upperY
            width: a.chartW
            height: a.upperH
            title: "CPU · " + probe.cores.length + " CORES · TOTAL"

            readonly property real squareSide: Math.max(10, Math.min(root.readingSize * 2.6, (width
                                                                                              - (probe.cores.length
                                                                                                 - 1) * 4)
                                                                     / Math.max(1,
                                                                                probe.cores.length)))
            readonly property int perRow: Math.max(1, Math.floor((width + 4) / (squareSide + 4)))
            readonly property real squaresH: Math.ceil(probe.cores.length / perRow) * (squareSide
                                                                                       + 4)
            Chart {
                width: parent.width
                height: parent.height - cpu.squaresH - 12
                values: probe.cpuTotal
                span: probe.fastKeep
                max: 1
                format: v => Math.round(v * 100) + "%"
            }
            Flow {
                y: parent.height - cpu.squaresH
                width: parent.width
                spacing: 4
                Repeater {
                    model: probe.cores
                    Rectangle {
                        required property real modelData
                        width: cpu.squareSide
                        height: cpu.squareSide
                        color: modelData > 0.85 ? root.hot : Qt.rgba(root.glow.r, root.glow.g,
                                                                     root.glow.b, 0.06 + 0.94
                                                                     * modelData)
                        border.color: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.7)
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 300
                            }
                        }
                    }
                }
            }
        }

        // Gauges over the Omnibar, two by two.
        Grid {
            id: gauges
            visible: root.has("cpu") || root.has("net")
            readonly property real cell: Math.max(60, Math.min((a.midW - columnSpacing) / 2, (
                                                                   a.upperH - rowSpacing) / 2))
            x: a.gap + a.chartW + a.gap + (a.midW - width) / 2
            y: a.upperY + (a.upperH - height) / 2
            columns: 2
            columnSpacing: a.gap
            rowSpacing: a.gap * 0.5
            Gauge {
                size: gauges.cell
                value: root.lastOf(probe.cpuTotal)
                peak: root.peakOf(probe.cpuTotal.map(v => v * 1000)) / 1000
                reading: root.pct(root.lastOf(probe.cpuTotal))
                label: "CPU"
            }
            Gauge {
                size: gauges.cell
                value: probe.meminfo.MemTotal ? 1 - probe.meminfo.MemAvailable
                                                / probe.meminfo.MemTotal : 0
                reading: probe.meminfo.MemTotal ? root.pct(1 - probe.meminfo.MemAvailable
                                                           / probe.meminfo.MemTotal) : ""
                label: "MEMORY"
            }
            Gauge {
                size: gauges.cell
                readonly property var u: root.rateUnit(probe.down)
                readonly property real scaleMax: Math.max(1, root.peakOf(probe.down))
                value: probe.downEased / scaleMax
                reading: root.inUnit(probe.downEased, u)
                unit: u.name
                scaleTop: root.inUnit(scaleMax, u)
                label: "▼ DOWN"
            }
            Gauge {
                size: gauges.cell
                readonly property var u: root.rateUnit(probe.up)
                readonly property real scaleMax: Math.max(1, root.peakOf(probe.up))
                value: probe.upEased / scaleMax
                reading: root.inUnit(probe.upEased, u)
                unit: u.name
                scaleTop: root.inUnit(scaleMax, u)
                label: "▲ UP"
            }
        }

        // Network: down as bars, up as a line.
        Section {
            id: netSection
            visible: root.has("net")
            x: root.width - a.gap - width
            y: a.upperY
            width: a.chartW
            height: a.upperH
            readonly property var u: root.rateUnit(probe.down.concat(probe.up))
            title: "NETWORK · " + u.name + " · ▼ BARS  ▲ LINE"
            Chart {
                width: parent.width
                height: parent.height
                style: "bars"
                bars: 72
                values: probe.down
                second: probe.up
                span: probe.fastKeep
                format: v => root.inUnit(v, netSection.u)
            }
        }

        // Figures under the Omnibar, across the page.
        Row {
            id: figures
            x: a.gap
            y: a.figuresY
            spacing: a.gap
            readonly property var items: {
                const out = [];
                if (root.has("cpu"))
                    out.push(["CPU", root.pct(root.lastOf(probe.cpuTotal)), false]);
                if (root.has("net")) {
                    const du = root.rateUnit(probe.down), uu = root.rateUnit(probe.up);
                    out.push(["▼ DOWN · " + du.name, root.inUnit(probe.downEased, du), false]);
                    out.push(["▲ UP · " + uu.name, root.inUnit(probe.upEased, uu), false]);
                }
                if (root.has("mem")) {
                    out.push(["MEMORY USED", root.size(Math.max(0, (probe.meminfo.MemTotal || 0) - (
                                                                    probe.meminfo.MemAvailable
                                                                    || 0))), false]);
                    out.push(["SWAP", probe.meminfo.SwapTotal ? root.pct(1 - probe.meminfo.SwapFree
                                                                         / probe.meminfo.SwapTotal) :
                                                                "—", false]);
                    out.push(["OMAWEB UI", root.size(probe.chromeKib), false]);
                }
                if (root.has("battery") && probe.battery)
                    out.push(["BATTERY", root.pct(probe.battery.capacity), false]);
                if (root.has("blocked"))
                    out.push(["BLOCKED", probe.blockedOpen + " · " + probe.blockedSession, true]);
                return out;
            }
            readonly property real each: (a.innerW - (items.length - 1) * spacing) / Math.max(1,
                                                                                              items.length)
            Repeater {
                model: figures.items
                Figure {
                    required property var modelData
                    width: figures.each
                    label: modelData[0]
                    value: modelData[1]
                    small: modelData[2]
                }
            }
        }

        // The sections under the figures, widths by weight among those shown.
        Row {
            id: lower
            x: a.gap
            y: a.lowerY
            spacing: a.gap * 1.5
            readonly property var weights: ({
                                                pages: root.has("pages") ? 3 : 0,
                                                mem: root.has("mem") ? 2.4 : 0,
                                                power: root.has("battery") ? 2.6 : 0,
                                                spaces: root.has("spaces") ? 2.6 : 0
                                            })
            readonly property int count: Object.values(lower.weights).filter(w => w > 0).length
            readonly property real total: Object.values(lower.weights).reduce((s, w) => s + w, 0)
                                          || 1
            function widthOf(key) {
                return (a.innerW - (lower.count - 1) * lower.spacing) * lower.weights[key]
                        / lower.total;
            }

            // Pages loaded today, an hour a bar.
            Section {
                visible: root.has("pages")
                width: lower.widthOf("pages")
                height: a.lowerH
                fake: true
                title: "PAGES TODAY · " + probe.pagesToday.reduce((s, v) => s + v, 0)
                Chart {
                    width: parent.width
                    height: parent.height
                    style: "bars"
                    bars: 24
                    span: 24
                    values: probe.pagesToday
                    timeLabels: ["00", "06", "12", "18", "24"]
                    callout: false
                }
            }

            // Memory: RAM as used, cache and free, and swap.
            Section {
                id: memory
                visible: root.has("mem")
                width: lower.widthOf("mem")
                height: a.lowerH
                title: "MEMORY · " + root.size(probe.meminfo.MemTotal || 0)
                readonly property real ring: Math.max(60, Math.min(a.lowerH - root.captionSize * 6, (width
                                                                                                     - root.captionSize
                                                                                                     * 14) / 1.2))
                Row {
                    spacing: 20
                    Donut {
                        readonly property real total: probe.meminfo.MemTotal || 1
                        readonly property real cache: (probe.meminfo.Cached || 0) + (
                                                          probe.meminfo.Buffers || 0) + (
                                                          probe.meminfo.SReclaimable || 0)
                        readonly property real free: probe.meminfo.MemFree || 0
                        readonly property real used: Math.max(0, total - cache - free)
                        size: memory.ring
                        parts: [
                            {
                                value: used,
                                colour: root.glow
                            },
                            {
                                value: cache,
                                colour: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.45)
                            },
                            {
                                value: free,
                                colour: root.faint
                            }
                        ]
                        reading: root.pct(used / total)
                        label: "RAM"
                    }
                    Column {
                        spacing: 10
                        Caption {
                            text: "■ USED  " + root.size(Math.max(0, (probe.meminfo.MemTotal || 0)
                                                                  - (probe.meminfo.MemFree || 0) - (
                                                                      probe.meminfo.Cached || 0) - (
                                                                      probe.meminfo.Buffers || 0) - (
                                                                      probe.meminfo.SReclaimable
                                                                      || 0)))
                            color: root.glow
                        }
                        Caption {
                            text: "■ CACHE " + root.size((probe.meminfo.Cached || 0) + (
                                                             probe.meminfo.Buffers || 0) + (
                                                             probe.meminfo.SReclaimable || 0))
                        }
                        Caption {
                            text: "□ FREE  " + root.size(probe.meminfo.MemFree || 0)
                        }
                        Item {
                            width: 1
                            height: root.captionSize
                        }
                        Donut {
                            readonly property real total: probe.meminfo.SwapTotal || 0
                            readonly property real used: total - (probe.meminfo.SwapFree || 0)
                            size: memory.ring * 0.55
                            parts: [
                                {
                                    value: used,
                                    colour: root.hot
                                },
                                {
                                    value: Math.max(total > 0 ? 0 : 1, total - used),
                                    colour: root.faint
                                }
                            ]
                            reading: total > 0 ? root.pct(used / total) : "—"
                            label: "SWAP"
                        }
                    }
                }
            }

            // Power: the draw over five minutes, and the charge as a line.
            Section {
                visible: root.has("battery")
                width: lower.widthOf("power")
                height: a.lowerH
                title: probe.battery ? "POWER · " + probe.battery.status.toUpperCase() + " · "
                                       + root.pct(probe.battery.capacity) + " · W BARS  % LINE" :
                                       "POWER"
                Chart {
                    width: parent.width
                    height: parent.height
                    style: "bars"
                    bars: 50
                    values: probe.watts
                    second: probe.charge
                    secondMax: 1
                    format: v => v.toFixed(v < 10 ? 1 : 0) + " W"
                }
            }

            // Memory per Space: a hatched bar each, against a common scale.
            Section {
                id: spaces
                visible: root.has("spaces")
                width: lower.widthOf("spaces")
                height: a.lowerH
                fake: true
                title: "MEMORY PER SPACE"
                readonly property real scaleTop: 2048
                readonly property real labelW: root.captionSize * 7
                readonly property real valueW: root.readingSize * 1.45 * 3.2
                readonly property real barW: Math.max(20, width - labelW - valueW - 24)
                // The rows share the section's height, up to a generous size.
                readonly property real rowH: Math.min(root.readingSize * 4.5, (height
                                                                               - root.captionSize
                                                                               * 5) / Math.max(1,
                                                                                               probe.spaces.length)
                                                      - 8)
                Column {
                    spacing: 8
                    // The scale over the bars.
                    Item {
                        width: spaces.width
                        height: root.captionSize * 1.4
                        Repeater {
                            model: 3
                            Caption {
                                required property int index
                                x: spaces.labelW + 12 + spaces.barW * index / 2 - (index === 2
                                                                                   ? width : index
                                                                                     === 0 ? 0 :
                                                                                             width / 2)
                                text: (spaces.scaleTop * index / 2 / 1024).toFixed(0) + "G"
                            }
                        }
                    }
                    Repeater {
                        model: probe.spaces
                        Item {
                            required property var modelData
                            width: spaces.width
                            height: spaces.rowH
                            Caption {
                                width: spaces.labelW
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name.toUpperCase()
                                color: root.ink
                            }
                            Canvas {
                                id: bar
                                x: spaces.labelW + 12
                                y: parent.height * 0.2
                                width: spaces.barW
                                height: parent.height * 0.6
                                property real share: Math.min(1, modelData.mib / spaces.scaleTop)
                                onShareChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                onPaint: {
                                    const c = getContext("2d");
                                    c.reset();
                                    const g = root.glow;
                                    c.strokeStyle = Qt.rgba(g.r, g.g, g.b, 0.35);
                                    c.setLineDash([1, 4]);
                                    for (let t = 0; t <= 4; ++t) {
                                        const x = Math.round((width - 1) * t / 4) + 0.5;
                                        c.beginPath();
                                        c.moveTo(x, 0);
                                        c.lineTo(x, height);
                                        c.stroke();
                                    }
                                    c.setLineDash([]);
                                    const w = width * bar.share;
                                    c.fillStyle = Qt.rgba(g.r, g.g, g.b, 0.12);
                                    c.fillRect(0, 0, w, height);
                                    c.strokeStyle = Qt.rgba(g.r, g.g, g.b, 0.5);
                                    c.beginPath();
                                    for (let x = 0.5; x < w; x += 3) {
                                        c.moveTo(x, 0);
                                        c.lineTo(x, height);
                                    }
                                    c.stroke();
                                    c.fillStyle = g;
                                    c.fillRect(Math.max(0, w - 3), 0, 3, height);
                                }
                            }
                            Reading {
                                x: spaces.labelW + 12 + spaces.barW + 12
                                anchors.verticalCenter: parent.verticalCenter
                                pixelSize: Math.round(root.readingSize * 1.45)
                                text: (modelData.mib / 1024).toFixed(1) + "G"
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- faint noise over everything, moved every frame: one tile of
    // random grey, written in one putImageData, shown as copies of its
    // texture across the Scene.

    Canvas {
        id: noiseTile
        width: 256
        height: 256
        renderTarget: Canvas.Image
        onPaint: {
            const c = getContext("2d");
            const image = c.createImageData(256, 256);
            const data = image.data;
            for (let i = 0; i < data.length; i += 4) {
                const v = Math.floor(Math.random() * 256);
                data[i] = v;
                data[i + 1] = v;
                data[i + 2] = v;
                data[i + 3] = 255;
            }
            c.putImageData(image, 0, 0);
        }
    }
    ShaderEffectSource {
        id: noiseTexture
        sourceItem: noiseTile
        hideSource: true
        smooth: false
    }
    Item {
        x: -Math.floor((root.time * 7919) % 256)
        y: -Math.floor((root.time * 9973) % 256)
        opacity: 0.035
        Grid {
            columns: Math.ceil(root.width / 256) + 1
            Repeater {
                model: (Math.ceil(root.width / 256) + 1) * (Math.ceil(root.height / 256) + 1)
                ShaderEffect {
                    width: 256
                    height: 256
                    property var source: noiseTexture
                }
            }
        }
    }
}
