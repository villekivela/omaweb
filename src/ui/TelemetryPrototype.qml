import QtQuick
import QtQuick.Shapes
import Omaweb

// PROTOTYPE (#814), throwaway: the Telemetry Scene, a retro CRT dashboard
// around the resting Omnibar, for the maintainer to judge. Never merged; it
// lives on prototype/020-telemetry.
//
// Round 2, after the first verdict: the Cockpit layout only, under the real
// CRT glass with real scanlines (TelemetryHostPrototype.qml takes the picture
// at full resolution and sets the scanline pitch), text in two sizes to
// compare, CPU cores as squares, more gauges, donuts for memory and swap, and
// blocked requests as a small module.
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
                                        shade: 0.55,
                                        narrowest: 0.55,
                                        widest: 1.15,
                                        falloff: 2.2,
                                        lift: 1.2
                                    },
                                    mask: {
                                        dim: 0.78,
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
    readonly property color dim: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.6)
    readonly property color faint: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.16)
    readonly property string mono: "monospace"

    // ---- text: two sizes, each a glyph over at least six scanlines of three
    // device pixels (TelemetryHostPrototype.qml)

    readonly property bool large: root.textSize === "L"
    readonly property int captionSize: root.large ? 20 : 16
    readonly property int readingSize: root.large ? 30 : 22

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
        property var up: []
        property var lastNet: null
        property var meminfo: ({})
        property real chromeKib: 0
        property var battery: null
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

        function readBattery() {
            for (const name of Scenes.systemEntries("/sys/class/power_supply")) {
                const dir = "/sys/class/power_supply/" + name + "/";
                if (Scenes.systemFile(dir + "type").trim() !== "Battery")
                    continue;
                probe.battery = {
                    capacity: Number(Scenes.systemFile(dir + "capacity")) / 100,
                    status: Scenes.systemFile(dir + "status").trim(),
                    watts: Number(Scenes.systemFile(dir + "power_now")) / 1e6
                };
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
        readonly property real split: Math.max(1, Math.round(crtText.pixelSize / 14))
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
            color: "#ff2a3a"
            opacity: 0.45
            font: main.font
        }
        Text {
            x: crtText.split
            width: parent.width
            text: main.text
            elide: crtText.elide
            color: "#2a6aff"
            opacity: 0.45
            font: main.font
        }
        Text {
            id: main
            width: parent.width
            elide: crtText.elide
            color: crtText.color
            font.family: root.mono
            font.pixelSize: crtText.pixelSize
            font.bold: crtText.bold
            font.letterSpacing: crtText.pixelSize / 12
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

    // A trace of `values` (0..max, or 0..its own peak), newest at the right.
    component Trace: Canvas {
        id: trace
        property var values: []
        property var second: null
        property real max: 0
        property color colour: root.glow
        property color secondColour: root.hot
        property int span: 300
        onValuesChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            const peak = trace.max > 0 ? trace.max : Math.max(root.peakOf(trace.values), root.peakOf(
                                                                  trace.second || []));
            const step = trace.width / (trace.span - 1);
            const line = (values, colour, fill) => {
                if (values.length < 2)
                    return;
                c.beginPath();
                const x0 = trace.width - (values.length - 1) * step;
                for (let i = 0; i < values.length; ++i) {
                    const x = x0 + i * step;
                    const y = trace.height - 2 - Math.min(1, values[i] / peak) * (trace.height - 4);
                    if (i === 0)
                        c.moveTo(x, y);
                    else
                        c.lineTo(x, y);
                }
                c.strokeStyle = colour;
                c.lineWidth = 2;
                c.stroke();
                if (fill) {
                    c.lineTo(trace.width, trace.height);
                    c.lineTo(x0, trace.height);
                    c.closePath();
                    c.fillStyle = Qt.rgba(colour.r, colour.g, colour.b, 0.2);
                    c.fill();
                }
            };
            line(trace.values, trace.colour, true);
            if (trace.second)
                line(trace.second, trace.secondColour, false);
        }
    }

    // A round gauge: a 270 degree scale, the needle at `value` (0..1), its
    // reading in the face and its label under it. The scale is painted once;
    // the arc and the needle are scene-graph items that glide to each new
    // reading, so the gauge moves at the display's rate, not the sample's.
    component Gauge: Item {
        id: gauge
        property real value: 0
        property string reading
        property string label
        property bool fake: false
        property real size: 140
        property real eased: Math.max(0, Math.min(1, gauge.value))
        readonly property real r: gauge.size / 2 - 3
        readonly property real thick: Math.max(4, gauge.r * 0.1)
        width: gauge.size
        height: gauge.size + labelText.height + 4

        Behavior on eased {
            NumberAnimation {
                duration: 450
                easing.type: Easing.OutCubic
            }
        }

        Canvas {
            id: face
            width: gauge.size
            height: gauge.size
            property color tone: root.glow
            onWidthChanged: requestPaint()
            onToneChanged: requestPaint()
            onPaint: {
                const g = getContext("2d");
                g.reset();
                const cx = width / 2, cy = height / 2, r = gauge.r;
                const from = Math.PI * 0.75, sweep = Math.PI * 1.5;
                g.lineWidth = gauge.thick;
                g.strokeStyle = root.faint;
                g.beginPath();
                g.arc(cx, cy, r - g.lineWidth / 2, from, from + sweep);
                g.stroke();
                g.lineWidth = 2;
                g.strokeStyle = Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.6);
                for (let t = 0; t <= 10; ++t) {
                    const a = from + sweep * t / 10;
                    const inner = r * (t % 5 ? 0.78 : 0.72), outer = r * 0.84;
                    g.beginPath();
                    g.moveTo(cx + Math.cos(a) * inner, cy + Math.sin(a) * inner);
                    g.lineTo(cx + Math.cos(a) * outer, cy + Math.sin(a) * outer);
                    g.stroke();
                }
            }
        }
        Shape {
            anchors.fill: face
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: gauge.eased > 0.85 ? root.hot : root.glow
                strokeWidth: gauge.thick
                fillColor: "transparent"
                capStyle: ShapePath.FlatCap
                PathAngleArc {
                    centerX: gauge.size / 2
                    centerY: gauge.size / 2
                    radiusX: gauge.r - gauge.thick / 2
                    radiusY: gauge.r - gauge.thick / 2
                    startAngle: 135
                    sweepAngle: 270 * gauge.eased
                }
            }
        }
        // The needle, pointing up from the centre, turned to the reading.
        Item {
            x: gauge.size / 2
            y: gauge.size / 2
            rotation: 135 + 270 * gauge.eased + 90
            Rectangle {
                x: -1.5
                y: -gauge.r * 0.7
                width: 3
                height: gauge.r * 0.8
                antialiasing: true
                color: root.hot
            }
        }
        Rectangle {
            x: gauge.size / 2 - 5
            y: gauge.size / 2 - 5
            width: 10
            height: 10
            radius: 5
            color: root.hot
        }
        Reading {
            anchors.horizontalCenter: face.horizontalCenter
            y: face.height * 0.62
            text: gauge.reading
            pixelSize: Math.max(root.captionSize, Math.min(root.readingSize, gauge.size / 6))
        }
        Caption {
            id: labelText
            anchors.horizontalCenter: face.horizontalCenter
            y: face.height + 2
            text: gauge.label + (gauge.fake ? " [FAKE]" : "")
            color: gauge.fake ? root.hot : root.glow
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

    // A framed panel with a title in its top edge.
    component Panel: Item {
        id: panel
        property string title
        property bool fake: false
        default property alias content: body.data
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.55)
            border.width: 2
        }
        Rectangle {
            x: 14
            y: -heading.height / 2
            width: heading.width + 14
            height: heading.height
            color: root.ground
            Caption {
                id: heading
                x: 7
                text: panel.title + (panel.fake ? "  [FAKE]" : "")
                color: panel.fake ? root.hot : root.glow
            }
        }
        Item {
            id: body
            anchors.fill: parent
            anchors.margins: 16
            anchors.topMargin: heading.height / 2 + 12
        }
    }

    // ---- ground

    Rectangle {
        anchors.fill: parent
        color: root.ground
    }

    // ---- the cockpit: four panels frame the Omnibar, a status strip over
    // them and a footer under them

    Item {
        id: a
        anchors.fill: parent

        readonly property real gap: 20
        readonly property real colW: (root.width - 3 * gap) / 2
        readonly property real stripH: status.height
        readonly property real footH: root.has("pages") || root.has("spaces") ? Math.min(
                                                                                    root.height
                                                                                    * 0.2, root.readingSize
                                                                                    * 6) : 0
        readonly property real upperY: gap + stripH + gap
        readonly property real upperH: Math.max(80, root.barTop - upperY - gap)
        readonly property real lowerY: root.barBottom + gap
        readonly property real lowerH: Math.max(80, root.height - lowerY - gap - (footH > 0 ? footH
                                                                                              + gap : 0))
        // A gauge as big as a panel's height holds, over its label.
        readonly property real upperGauge: Math.max(60, Math.min(220, upperH - root.captionSize * 2
                                                                 - 36))
        readonly property real lowerGauge: Math.max(60, Math.min(220, lowerH - root.captionSize * 2
                                                                 - 36))

        // Status strip: the title, the clock, downloads while one runs.
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
                        width: 140
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

        // CPU: a square per core, lit by its load, and the total as a gauge.
        Panel {
            id: cpuPanel
            visible: root.has("cpu")
            x: a.gap
            y: a.upperY
            width: root.has("net") ? a.colW : root.width - 2 * a.gap
            height: a.upperH
            title: "CPU  " + probe.cores.length + " CORES"

            Grid {
                id: squares
                readonly property int count: Math.max(1, probe.cores.length)
                readonly property real roomW: cpuPanel.width - 32 - a.upperGauge - 24
                readonly property real roomH: a.upperH - root.captionSize - 28
                // As many across as makes the squares largest.
                readonly property int across: {
                    let best = 1, bestSide = 0;
                    for (let n = 1; n <= squares.count; ++n) {
                        const side = Math.min(squares.roomW / n, squares.roomH / Math.ceil(
                                                  squares.count / n));
                        if (side > bestSide) {
                            bestSide = side;
                            best = n;
                        }
                    }
                    return best;
                }
                readonly property real side: Math.max(8, Math.min(squares.roomW / squares.across,
                                                                  squares.roomH / Math.ceil(
                                                                      squares.count
                                                                      / squares.across)) - spacing)
                columns: squares.across
                spacing: 6
                Repeater {
                    model: probe.cores
                    Rectangle {
                        required property real modelData
                        width: squares.side
                        height: squares.side
                        color: modelData > 0.85 ? root.hot : Qt.rgba(root.glow.r, root.glow.g,
                                                                     root.glow.b, 0.08 + 0.92
                                                                     * modelData)
                        border.color: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.7)
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 300
                            }
                        }
                        CrtText {
                            visible: squares.side >= root.captionSize * 2.2
                            anchors.centerIn: parent
                            pixelSize: root.captionSize
                            color: modelData > 0.5 ? root.ground : root.ink
                            text: Math.round(modelData * 100)
                        }
                    }
                }
            }
            Gauge {
                anchors.right: parent.right
                size: a.upperGauge
                value: root.lastOf(probe.cpuTotal)
                reading: root.pct(root.lastOf(probe.cpuTotal))
                label: "TOTAL"
            }
        }

        // Network: down and up as gauges against the five minutes' peak, and
        // the trace of both.
        Panel {
            visible: root.has("net")
            x: root.width - a.gap - width
            y: a.upperY
            width: root.has("cpu") ? a.colW : root.width - 2 * a.gap
            height: a.upperH
            title: "NETWORK"

            Row {
                id: netGauges
                spacing: 16
                Gauge {
                    size: a.upperGauge
                    value: root.lastOf(probe.down) / root.peakOf(probe.down)
                    reading: root.rate(root.lastOf(probe.down))
                    label: "▼ DOWN"
                }
                Gauge {
                    size: a.upperGauge
                    value: root.lastOf(probe.up) / root.peakOf(probe.up)
                    reading: root.rate(root.lastOf(probe.up))
                    label: "▲ UP"
                }
            }
            Trace {
                x: netGauges.width + 20
                width: Math.max(0, parent.width - x)
                height: parent.height
                values: probe.down
                second: probe.up
                span: probe.fastKeep
            }
        }

        // Memory: RAM as used, cache and free, swap as used and free, and the
        // browser's own.
        Panel {
            id: memPanel
            visible: root.has("mem")
            x: a.gap
            y: a.lowerY
            width: root.has("blocked") || root.has("battery") ? a.colW : root.width - 2 * a.gap
            height: a.lowerH
            title: "MEMORY  " + root.size(probe.meminfo.MemTotal || 0)

            // RAM, its legend and swap across the panel, shrunk to fit it.
            readonly property real fit: Math.max(50, Math.min(a.lowerGauge, (width - 32
                                                                             - root.captionSize
                                                                             * 10 - 48) / 1.8))

            Row {
                spacing: 24
                Donut {
                    readonly property real total: probe.meminfo.MemTotal || 1
                    readonly property real cache: (probe.meminfo.Cached || 0) + (
                                                      probe.meminfo.Buffers || 0) + (
                                                      probe.meminfo.SReclaimable || 0)
                    readonly property real free: probe.meminfo.MemFree || 0
                    readonly property real used: Math.max(0, total - cache - free)
                    size: memPanel.fit
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
                    spacing: 6
                    y: 10
                    Caption {
                        text: "■ USED  " + root.size(Math.max(0, (probe.meminfo.MemTotal || 0) - (
                                                                  probe.meminfo.MemFree || 0) - (
                                                                  probe.meminfo.Cached || 0) - (
                                                                  probe.meminfo.Buffers || 0) - (
                                                                  probe.meminfo.SReclaimable || 0)))
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
                }
                Donut {
                    readonly property real total: probe.meminfo.SwapTotal || 0
                    readonly property real used: total - (probe.meminfo.SwapFree || 0)
                    size: memPanel.fit * 0.8
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

        // The browser's own memory and the battery as gauges, and blocked
        // requests as a small module.
        Panel {
            visible: root.has("blocked") || root.has("battery")
            x: root.width - a.gap - width
            y: a.lowerY
            width: root.has("mem") ? a.colW : root.width - 2 * a.gap
            height: a.lowerH
            title: root.has("battery") ? "BROWSER · POWER" : "BROWSER"

            Row {
                spacing: 24
                Gauge {
                    size: a.lowerGauge
                    value: probe.chromeKib / 2097152
                    reading: root.size(probe.chromeKib)
                    label: "OMAWEB UI"
                }
                Gauge {
                    visible: root.has("battery")
                    size: a.lowerGauge
                    value: probe.battery ? probe.battery.capacity : 0
                    reading: probe.battery ? root.pct(probe.battery.capacity) : ""
                    label: probe.battery ? probe.battery.status.toUpperCase() + " "
                                           + probe.battery.watts.toFixed(1) + " W" : ""
                }
            }

            // Blocked: small, in the panel's corner.
            Column {
                visible: root.has("blocked")
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 4
                Caption {
                    anchors.right: parent.right
                    text: "BLOCKED " + probe.blockedOpen + " · " + probe.blockedSession + " SESSION"
                }
                Trace {
                    anchors.right: parent.right
                    width: 160
                    height: root.captionSize * 1.5
                    values: probe.blockedRate
                    colour: root.dim
                }
                Caption {
                    anchors.right: parent.right
                    visible: probe.topHosts.length > 0
                    maxWidth: 260
                    elide: Text.ElideLeft
                    text: probe.topHosts.length ? probe.topHosts[0].host + " ×"
                                                  + probe.topHosts[0].count : ""
                }
            }
        }

        // Footer: pages today and memory per Space, both fake.
        Panel {
            visible: root.has("pages")
            x: a.gap
            y: root.height - a.gap - a.footH
            width: root.has("spaces") ? a.colW : root.width - 2 * a.gap
            height: a.footH
            fake: true
            title: "PAGES TODAY  " + probe.pagesToday.reduce((s, v) => s + v, 0)
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
                spacing: 20
                Repeater {
                    model: probe.spaces
                    Gauge {
                        required property var modelData
                        size: Math.max(50, a.footH - root.captionSize * 2 - 36)
                        value: modelData.mib / 2048
                        reading: modelData.mib + "M"
                        label: modelData.name.toUpperCase()
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
        opacity: 0.06
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
