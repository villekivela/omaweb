import QtQuick
import QtQuick.Shapes
import Omaweb
import "DevicePixels.mjs" as DevicePixels

// Telemetry behind the Start page (#814): a retro-futuristic CRT dashboard of
// live readings that wraps around the resting Omnibar, the Cockpit edge to
// edge. Over the Omnibar, the CPU and network charts and a two-by-two cluster
// of gauges; under it, a row of large figures, and under those, sections for
// memory, power, the pages loaded today and Omaweb's memory per Space.
//
// Every reading is real and local (share/scenes/telemetry.json). The Start
// page hands them in: `readings` from the Telemetry sampler, `downloads` and
// `spaces` from the window. The modules are display only: nothing here takes
// a click or the keyboard, so the Omnibar is the one thing on the page that
// does.
//
// A small page area keeps the Omnibar and drops whole blocks rather than
// squeezing them: over the Omnibar the gauges go first, then the network
// chart, then the CPU chart; the figures keep as many as fit, the earlier
// modules first; the sections go last first. It keeps NightRoad.qml's
// contract with SceneHost.qml.
Item {
    id: root
    objectName: "telemetry"

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
    // A Private window: the lights are dimmed, and nothing from History or
    // other Spaces is shown.
    property bool unlit: false
    property real omnibarReach: 26
    property real omnibarWidth: 0

    // ---- what the Start page hands it

    // TelemetrySampler.readings() for the window's Spaces.
    property var readings: ({})
    // The window's running downloads: fileName and fraction, 0 to 1, or
    // below 0 while the size is not known.
    property var downloads: []
    // Omaweb's memory per Space of the window: name and bytes.
    property var spaces: []
    // The modules the reader turned off.
    property var hidden: []
    // "large" or "small".
    property string textSize: "large"

    // ---- what it declares

    readonly property var parameters: Scenes.parameters("telemetry")
    readonly property int pitch: root.parameters.pitch
    readonly property int fps: root.parameters.fps
    readonly property string glass: root.parameters.glass
    readonly property string screen: root.parameters.screen
    readonly property bool devicePixels: root.parameters.devicePixels === true
    readonly property var crt: root.parameters.crt
    readonly property var declaredOptions: root.parameters.options
    readonly property real horizonY: Math.round(root.height * root.parameters.rest)

    // ---- palette

    readonly property color ground: root.dark ? root.colors.windowOpaque : Qt.darker(root.colors.text,
                                                                                     3)
    readonly property color ink: root.dark ? root.colors.text : root.colors.windowOpaque
    readonly property color glow: root.unlit ? Qt.darker(root.ink, 1.6) : root.colors.accent
    readonly property color hot: root.colors.syntax && root.colors.syntax.keyword
                                 ? root.colors.syntax.keyword : Qt.lighter(root.glow, 1.4)
    readonly property color dim: Qt.rgba(root.ink.r, root.ink.g, root.ink.b, 0.82)
    readonly property color faint: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.16)

    // ---- text

    readonly property var textSizes: root.parameters.text[root.textSize === "small" ? "small" :
                                                                                      "large"]
    readonly property int captionSize: root.textSizes.caption
    readonly property int readingSize: root.textSizes.reading

    // ---- where the Omnibar rests, which every block keeps clear

    readonly property real barTop: root.horizonY - 50 - 16
    readonly property real barBottom: root.horizonY + root.omnibarReach + 16

    // ---- the readings, each with a value while there is none yet

    readonly property var cores: root.readings.cores || []
    readonly property var cpu: root.readings.cpu || []
    readonly property var down: root.readings.down || []
    readonly property var up: root.readings.up || []
    readonly property var memory: root.readings.memory || ({})
    readonly property real omawebKib: root.readings.omaweb || 0
    readonly property var battery: root.readings.battery || null
    readonly property var blocked: root.readings.blocked || ({
                                                                 lastMinute: 0,
                                                                 total: 0,
                                                                 hosts: []
                                                             })
    readonly property var pagesToday: root.readings.pages || []
    readonly property real memoryUsedKib: Math.max(0, (root.memory.total || 0) - (
                                                       root.memory.available || 0))
    readonly property real cpuNow: root.readings.cpuNow || 0
    readonly property real cpuPeak: root.readings.cpuPeak || 0
    readonly property real downPeak: root.readings.downPeak || 0
    readonly property real upPeak: root.readings.upPeak || 0
    readonly property var downUnit: root.rateUnit(root.downPeak)
    readonly property var upUnit: root.rateUnit(root.upPeak)
    // The rates the gauges and figures show, eased: a number that changes four
    // times a second cannot be read.
    property real downEased: root.readings.downNow || 0
    property real upEased: root.readings.upNow || 0

    Behavior on downEased {
        NumberAnimation {
            duration: 900
            easing.type: Easing.OutCubic
        }
    }
    Behavior on upEased {
        NumberAnimation {
            duration: 900
            easing.type: Easing.OutCubic
        }
    }

    // ---- the modules in drop order, and which of them this window has

    // Kept apart from the readings, so a new reading changes them only when
    // a battery or a download comes or goes, and the lists of what is shown
    // below are rebuilt only then.
    readonly property bool batteryPresent: !!root.readings.battery
    readonly property bool downloading: root.downloads.length > 0

    function has(key) {
        if (root.hidden.indexOf(key) >= 0)
            return false;
        if (key === "downloads")
            return root.downloading;
        if (key === "battery")
            return root.batteryPresent;
        if (key === "pages" || key === "spaces")
            return !root.unlit;
        return true;
    }

    // ---- the display's pixels

    // The Scene is drawn at the display's own pixels, so a position worked
    // out by halving is put on them, or text and borders stand between two
    // and draw soft at a fractional scale.
    readonly property real pixelRatio: Window.window ? Window.window.devicePixelRatio : 1

    function snap(value) {
        return DevicePixels.snap(value, root.pixelRatio);
    }

    // ---- formatting

    // A rate's unit, chosen once for a whole series from its peak, so it
    // changes only when the traffic changes scale, never between samples:
    // kB/s or MB/s, never B/s.
    function rateUnit(peak) {
        if (peak >= 1048576)
            return {
                name: qsTr("MB/s", "megabytes a second"),
                per: 1048576,
                places: 1
            };
        return {
            name: qsTr("kB/s", "kilobytes a second"),
            per: 1024,
            places: peak >= 10240 ? 0 : 1
        };
    }
    function inUnit(bytes, unit) {
        return (bytes / unit.per).toFixed(unit.places);
    }
    function size(kib) {
        return kib >= 1048576 ? qsTr("%1 GiB").arg((kib / 1048576).toFixed(1)) : qsTr("%1 MiB").arg((
                                                                                                        kib / 1024).toFixed(
                                                                                                        0));
    }
    function percent(fraction) {
        return qsTr("%1%", "a percentage").arg(Math.round(fraction * 100));
    }
    function lastOf(series) {
        return series && series.length ? series[series.length - 1] : 0;
    }
    // The largest of `series`, at least 1. A loop, not Math.max(...series):
    // a spread takes stack for every element.
    function peakOf(series) {
        let peak = 1;
        for (let i = 0; series && i < series.length; ++i)
            if (series[i] > peak)
                peak = series[i];
        return peak;
    }
    function sum(series) {
        let total = 0;
        for (let i = 0; series && i < series.length; ++i)
            total += series[i];
        return total;
    }

    // ---- the gauges and figures shown

    // Which are shown, by key: the models of their Repeaters, which change
    // only with the room and the modules, so a reading moves what the
    // delegates show rather than making them again.
    readonly property var gaugeKeys: {
        if (!layout.gaugesShown)
            return [];
        const keys = [];
        if (root.has("cpu"))
            keys.push("cpu");
        if (root.has("memory"))
            keys.push("memory");
        if (root.has("network"))
            keys.push("down", "up");
        return keys;
    }
    readonly property var figureKeys: {
        const keys = [];
        if (root.has("cpu"))
            keys.push("cpu");
        if (root.has("network"))
            keys.push("down", "up");
        if (root.has("memory"))
            keys.push("memory");
        if (root.has("blocked"))
            keys.push("blocked");
        if (root.has("battery"))
            keys.push("battery");
        if (root.has("memory"))
            keys.push("swap", "omaweb");
        return keys.slice(0, layout.figuresFit);
    }
    readonly property var gauges: root.gaugeKeys.map(key => root.gauge(key))
    readonly property var figures: root.figureKeys.map(key => root.figure(key))

    function gauge(key) {
        if (key === "cpu")
            return {
                key: key,
                value: root.cpuNow,
                peak: root.cpuPeak,
                reading: root.percent(root.cpuNow),
                unit: "",
                scaleTop: "100",
                label: qsTr("CPU", "telemetry: the processor")
            };
        if (key === "memory") {
            const used = root.memory.total ? root.memoryUsedKib / root.memory.total : 0;
            return {
                key: key,
                value: used,
                peak: -1,
                reading: root.percent(used),
                unit: "",
                scaleTop: "100",
                label: qsTr("MEM", "telemetry gauge: memory in use")
            };
        }
        const down = key === "down";
        const eased = down ? root.downEased : root.upEased;
        const top = down ? root.downPeak : root.upPeak;
        const unit = down ? root.downUnit : root.upUnit;
        return {
            key: key,
            value: top > 0 ? eased / top : 0,
            peak: -1,
            reading: root.inUnit(eased, unit),
            unit: unit.name,
            scaleTop: root.inUnit(top, unit),
            label: down ? qsTr("▼ DOWN", "telemetry: received") : qsTr("▲ UP", "telemetry: sent")
        };
    }

    function figure(key) {
        switch (key) {
        case "cpu":
            return {
                key: key,
                label: qsTr("CPU", "telemetry: the processor"),
                value: root.percent(root.cpuNow)
            };
        case "down":
            return {
                key: key,
                label: qsTr("▼ DOWN · %1", "telemetry: received, in a unit").arg(root.downUnit.name),
                value: root.inUnit(root.downEased, root.downUnit)
            };
        case "up":
            return {
                key: key,
                label: qsTr("▲ UP · %1", "telemetry: sent, in a unit").arg(root.upUnit.name),
                value: root.inUnit(root.upEased, root.upUnit)
            };
        case "memory":
            return {
                key: key,
                label: qsTr("MEMORY USED"),
                value: root.size(root.memoryUsedKib)
            };
        case "blocked":
        {
            const hosts = root.blocked.hosts || [];
            return {
                key: key,
                label: qsTr("BLOCKED · %1/MIN", "telemetry: requests refused a minute").arg(
                           root.blocked.lastMinute || 0),
                value: String(root.blocked.total || 0),
                // The hosts refused most over the five minutes, most first.
                note: hosts.map(refused => refused.host).join(" · "),
                small: true
            };
        }
        case "battery":
            return {
                key: key,
                label: qsTr("BATTERY"),
                value: root.battery ? root.percent(root.battery.capacity) : ""
            };
        case "swap":
            return {
                key: key,
                label: qsTr("SWAP"),
                value: root.memory.swapTotal ? root.percent(1 - root.memory.swapFree
                                                            / root.memory.swapTotal) : "—"
            };
        default:
            return {
                key: key,
                label: qsTr("OMAWEB UI", "telemetry: the browser's own process"),
                value: root.size(root.omawebKib)
            };
        }
    }

    // ---- parts

    // Text as a CRT draws it: the glyph in its colour, and on large text its
    // red and blue a pixel apart either side, as a tube's three beams converge
    // a little off. The glass adds the bloom, scanlines and mask over it.
    component CrtText: Item {
        id: crtText
        property alias text: main.text
        property color color: root.ink
        property int pixelSize: root.captionSize
        property bool bold: false
        property int elide: Text.ElideNone
        property real maxWidth: -1
        // None on small text, where it smears the strokes.
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
            // The desktop's monospace: JetBrains Mono on Omarchy.
            font.family: "monospace"
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

    // A chart: dotted gridlines with their values at the right, time along
    // the bottom, `values` as a hatched area under a bright line ("area") or
    // as hatched bars ("bars"), `second` as a line over it, and the latest
    // value in a lit box at the line's head.
    component Chart: Item {
        id: chart
        property var values: []
        property var second: null
        property real max: 0
        // The scale `second` is drawn on, or 0 for the same as `values`.
        property real secondMax: 0
        // A point a second over the five minutes.
        property int span: TelemetrySampler.slowKept
        // The highest of the values where it is known, so a chart scans
        // nothing to find its scale.
        property real peak: 0
        property string style: "area"
        // Bars: how many the span is grouped into.
        property int bars: 60
        property var format: function (v) {
            return String(Math.round(v));
        }
        property var timeLabels: [qsTr("-5m", "five minutes ago"), qsTr("-4m", "four minutes ago"),
            qsTr("-3m", "three minutes ago"), qsTr("-2m", "two minutes ago"), qsTr("-1m",
                                                                                   "a minute ago"),
            qsTr("now", "telemetry chart: the latest sample")]
        property bool callout: true
        readonly property real axisW: root.captionSize * 4.5
        readonly property real axisH: root.captionSize * 1.6
        readonly property real plotW: Math.max(10, chart.width - chart.axisW)
        // Room over the plot for half the top scale value.
        readonly property real padTop: Math.round(root.captionSize * 0.7)
        readonly property real plotH: Math.max(10, chart.height - chart.axisH - chart.padTop)
        // The scale's top: the given one, or the peak rounded up.
        readonly property real scaleTop: chart.max > 0 ? chart.max : chart.nice(Math.max(chart.peak
                                                                                         > 0 ? chart.peak :
                                                                                               root.peakOf(
                                                                                                   chart.values),
                                                                                         chart.secondMax
                                                                                         > 0 ? 0 : root.peakOf(
                                                                                                   chart.second))
                                                                                * 1.15)
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
            y: chart.padTop
            width: chart.plotW
            height: chart.plotH
            property var values: chart.values
            property var second: chart.second
            property real scaleTop: chart.scaleTop
            property color glow: root.glow
            onValuesChanged: requestPaint()
            onScaleTopChanged: requestPaint()
            onGlowChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onPaint: {
                const c = getContext("2d");
                c.reset();
                const w = width, h = height, top = plot.scaleTop;
                const glow = plot.glow;
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
                const vs = plot.values || [];
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
                    // The bars from the newest back, each the mean of its
                    // samples.
                    const tops = [];
                    for (let b = 0; b < chart.bars; ++b) {
                        const end = vs.length - b * per;
                        if (end <= 0)
                            break;
                        let total = 0, n = 0;
                        for (let i = Math.max(0, Math.floor(end - per)); i < end; ++i) {
                            total += vs[i];
                            ++n;
                        }
                        tops.push([w - (b + 1) * bw, yOf(total / n)]);
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
                    for (let i = 0; i < vs.length; ++i) {
                        if (i === 0)
                            c.moveTo(x0, yOf(vs[0]));
                        else
                            c.lineTo(x0 + i * step, yOf(vs[i]));
                    }
                    c.strokeStyle = glow;
                    c.lineWidth = 2.5;
                    c.stroke();
                }
                const ss = plot.second;
                if (ss && ss.length > 1) {
                    const step = w / (chart.span - 1);
                    const x0 = w - (ss.length - 1) * step;
                    const y2 = v => chart.secondMax > 0 ? h - Math.min(1, v / chart.secondMax) * h :
                                                          yOf(v);
                    c.beginPath();
                    for (let i = 0; i < ss.length; ++i) {
                        if (i === 0)
                            c.moveTo(x0, y2(ss[0]));
                        else
                            c.lineTo(x0 + i * step, y2(ss[i]));
                    }
                    c.strokeStyle = root.hot;
                    c.lineWidth = 2;
                    c.stroke();
                }
            }
        }
        // The scale's values at the right of the gridlines: every one, every
        // other or the ends only, as the chart's height has room.
        readonly property int labelStep: chart.plotH / 4 >= root.captionSize * 1.4 ? 1 :
                                                                                     chart.plotH
                                                                                     / 2 >= root.captionSize
                                                                                     * 1.4 ? 2 : 4
        Repeater {
            model: 5
            Caption {
                required property int index
                visible: index % chart.labelStep === 0
                x: chart.plotW + 8
                y: root.snap(chart.padTop + chart.plotH - chart.plotH * index / 4 - height / 2)
                text: chart.format(chart.scaleTop * index / 4)
            }
        }
        // Time along the bottom.
        Repeater {
            model: chart.timeLabels
            Caption {
                required property string modelData
                required property int index
                x: root.snap(Math.min(chart.plotW - width, Math.max(0, chart.plotW * index / (
                                                                        chart.timeLabels.length
                                                                        - 1) - width / 2)))
                y: chart.padTop + chart.plotH + 4
                text: modelData
            }
        }
        // The latest value, lit, at the line's head.
        Rectangle {
            visible: chart.callout && chart.values.length > 0
            x: chart.plotW - width - 6
            y: chart.padTop + Math.max(0, Math.min(chart.plotH - height, chart.headY - height - 10))
            width: calloutText.width + 16
            height: calloutText.height + 8
            color: root.glow
            CrtText {
                id: calloutText
                x: root.snap((parent.width - width) / 2)
                y: root.snap((parent.height - height) / 2)
                color: root.ground
                bold: true
                pixelSize: root.readingSize * 0.8
                text: chart.format(chart.latest)
            }
        }
    }

    // A rule of dots, as the titles and figures are underlined.
    component DottedRule: Canvas {
        id: rule
        property color glow: root.glow
        height: 2
        onWidthChanged: requestPaint()
        onGlowChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            c.fillStyle = Qt.rgba(rule.glow.r, rule.glow.g, rule.glow.b, 0.6);
            for (let x = 0; x < width; x += 5)
                c.fillRect(x, 0, 2, 2);
        }
    }

    // A section: its title over a dotted rule, its content under.
    component Section: Item {
        id: section
        property string title
        default property alias content: body.data
        Caption {
            id: heading
            text: section.title
            color: root.glow
            maxWidth: section.width
            elide: Text.ElideRight
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
            clip: true
        }
    }

    // A figure: a small label over a large value over a dotted rule, and a
    // note under it where the module has one.
    component Figure: Column {
        id: figure
        property string label
        property string value
        property string note
        property bool small: false
        spacing: 4
        Caption {
            text: figure.label
            color: root.glow
            maxWidth: figure.width
            elide: Text.ElideRight
        }
        Reading {
            text: figure.value
            pixelSize: figure.small ? root.readingSize : Math.round(root.readingSize * 1.45)
        }
        DottedRule {
            width: figure.width
        }
        Caption {
            visible: figure.note !== ""
            text: figure.note
            maxWidth: figure.width
            elide: Text.ElideRight
        }
    }

    // A round gauge, as a HUD draws one: a 270 degree scale of forty segments
    // that light up to `value` (0 to 1), a fine ring of ticks outside it with a
    // lit marker at the value and a dim one at `peak` (none below 0), a thin
    // ring inside it turning as a scanner, brackets at the corners, and the
    // reading and label in the middle. No needle. The scale is painted once;
    // the lit segments, markers and scanner are scene-graph items, so they
    // glide.
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
            property color glow: root.glow
            onWidthChanged: requestPaint()
            onGlowChanged: requestPaint()
            onPaint: {
                const g = getContext("2d");
                g.reset();
                const cx = gauge.cx, r = gauge.r;
                const from = Math.PI * 0.75, sweep = Math.PI * 1.5;
                const glow = face.glow;
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

        // The scanner: a thin broken ring inside, turning with the Scene's
        // clock, so it stands still where the Scene does.
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
            x: root.snap((gauge.size - width) / 2)
            y: root.snap((gauge.size - height) / 2)
            spacing: 2
            Reading {
                x: root.snap((parent.width - width) / 2)
                text: gauge.reading
                color: gauge.tone
                pixelSize: Math.max(root.captionSize, Math.min(root.readingSize * 1.3, gauge.size
                                                               / 6.5))
            }
            Caption {
                x: root.snap((parent.width - width) / 2)
                text: gauge.label
                color: root.glow
            }
        }
        // The scale's ends, under the gap.
        Caption {
            x: root.snap(gauge.cx - gauge.r * 0.62 - width / 2)
            y: root.snap(gauge.cx + gauge.r * 0.62)
            text: "0"
        }
        Caption {
            x: root.snap(gauge.cx + gauge.r * 0.62 - width / 2)
            y: root.snap(gauge.cx + gauge.r * 0.62)
            text: gauge.scaleTop
        }
        Reading {
            visible: gauge.unit !== ""
            x: root.snap((gauge.size - width) / 2)
            y: root.snap(gauge.cx + gauge.r * 0.6)
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
            x: root.snap((ring.width - width) / 2)
            y: root.snap((ring.height - height) / 2)
            text: donut.reading
            pixelSize: Math.max(root.captionSize, Math.min(root.readingSize, donut.size / 5))
        }
        Caption {
            id: labelText
            x: root.snap((ring.width - width) / 2)
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

    // ---- the cockpit

    Item {
        id: layout
        anchors.fill: parent

        readonly property real gap: Math.round(Math.max(20, root.width * 0.012))
        readonly property real innerW: Math.max(0, root.width - 2 * gap)
        // Over the Omnibar: CPU, the gauges, network. Under it: the figures,
        // then the sections.
        readonly property real upperY: gap + status.height + gap
        readonly property real upperH: Math.max(0, root.barTop - upperY - gap)
        readonly property real figuresY: root.barBottom + gap * 0.5
        readonly property real lowerY: figuresY + figures.height + gap * 1.5
        readonly property real lowerH: Math.max(0, root.height - lowerY - gap)

        // What fits, each block's least size in text-size units.
        readonly property real minChart: root.captionSize * 16
        readonly property real minGauge: root.captionSize * 6
        readonly property real minSection: root.captionSize * 18
        // A figure holds "22.4 GiB" at its size.
        readonly property real minFigure: root.readingSize * 1.45 * 0.62 * 9
        readonly property bool upperShown: upperH >= root.captionSize * 9
        readonly property bool cpuShown: upperShown && root.has("cpu") && innerW >= minChart
        readonly property bool networkShown: upperShown && root.has("network") && (innerW - gap)
                                             / 2 >= minChart
        readonly property int charts: (cpuShown ? 1 : 0) + (networkShown ? 1 : 0)
        // The gauges go before the network chart: while the reader has the
        // network on, they stand only beside it.
        readonly property bool gaugesShown: upperShown && (networkShown || !root.has("network")) && (
                                                innerW - charts * minChart - (charts + 1) * gap) / 2
                                            >= minGauge && (upperH - gap * 0.5) / 2 >= minGauge
        readonly property real chartW: gaugesShown ? Math.round((innerW - 2 * gap) * 0.39) : charts
                                                     > 0 ? (innerW - (charts - 1) * gap) / charts :
                                                           0

        readonly property real midW: innerW - charts * (chartW + gap)
        readonly property int figuresFit: Math.max(0, Math.floor((innerW + gap) / (minFigure
                                                                                   + gap)))
        readonly property bool figuresShown: root.height - figuresY >= figures.implicitHeight + gap
        readonly property bool lowerShown: lowerH >= root.captionSize * 9
        readonly property var lowerKeys: {
            if (!lowerShown)
                return [];
            const keys = ["memory", "battery", "pages", "spaces"].filter(k => root.has(k));
            const fit = Math.floor((innerW + gap * 1.5) / (minSection + gap * 1.5));
            return keys.slice(0, Math.max(0, fit));
        }
        function lowerHas(key) {
            return layout.lowerKeys.indexOf(key) >= 0;
        }

        // Status: the title and the clock, and downloads while one runs.
        Row {
            id: status
            x: layout.gap
            y: layout.gap
            spacing: 32
            Caption {
                color: root.glow
                // Read again with every sample, so it keeps the time while
                // the Scene stands still.
                text: {
                    root.readings;
                    return qsTr("OMAWEB TELEMETRY  %1").arg(Qt.formatTime(new Date(), "hh:mm:ss"));
                }
            }
            Row {
                objectName: "telemetryDownloads"
                visible: root.has("downloads")
                spacing: 32
                Repeater {
                    model: root.has("downloads") ? root.downloads.length : 0
                    Row {
                        required property int index
                        readonly property var modelData: root.downloads[index] || ({
                                                                                       fileName: "",
                                                                                       fraction: 0
                                                                                   })
                        readonly property real fraction: modelData.fraction
                        objectName: "telemetryDownload-" + index
                        spacing: 10
                        Caption {
                            text: qsTr("▼ %1", "telemetry: a download, by its file's name").arg(
                                      modelData.fileName)
                            color: root.ink
                            maxWidth: root.captionSize * 16
                            elide: Text.ElideMiddle
                        }
                        Rectangle {
                            y: root.snap((parent.height - height) / 2)
                            width: root.captionSize * 7
                            height: root.captionSize / 2
                            color: root.faint
                            Rectangle {
                                width: parent.width * Math.max(0, modelData.fraction)
                                height: parent.height
                                color: root.glow
                            }
                        }
                        Caption {
                            visible: modelData.fraction >= 0
                            text: root.percent(Math.max(0, modelData.fraction))
                            color: root.ink
                        }
                    }
                }
            }
        }

        // CPU: the whole processor over five minutes, and a square per core
        // under it, lit by its load.
        Section {
            id: cpuSection
            objectName: "telemetryCpu"
            visible: layout.cpuShown
            x: layout.gap
            y: layout.upperY
            width: layout.chartW
            height: layout.upperH
            // English has no catalogue, so the singular is a string of its own.
            title: root.cores.length === 1 ? qsTr("CPU · 1 CORE · TOTAL", "telemetry chart") : qsTr("CPU · %1 CORES · TOTAL",
                                                                                                    "telemetry chart").arg(
                                                 root.cores.length)

            readonly property real squareSide: Math.max(10, Math.min(root.readingSize * 2.6, (width
                                                                                              - (root.cores.length
                                                                                                 - 1) * 4)
                                                                     / Math.max(1,
                                                                                root.cores.length)))
            readonly property int perRow: Math.max(1, Math.floor((width + 4) / (squareSide + 4)))
            readonly property real squaresH: Math.ceil(root.cores.length / perRow) * (squareSide
                                                                                      + 4)
            Chart {
                width: parent.width
                height: Math.max(0, parent.height - cpuSection.squaresH - 12)
                values: root.cpu
                max: 1
                format: v => root.percent(v)
            }
            Flow {
                y: parent.height - cpuSection.squaresH
                width: parent.width
                spacing: 4
                Repeater {
                    model: root.cores.length
                    Rectangle {
                        required property int index
                        readonly property real modelData: root.cores[index] || 0
                        width: cpuSection.squareSide
                        height: cpuSection.squareSide
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

        // The gauges over the Omnibar, two by two.
        Grid {
            id: gaugeGrid
            objectName: "telemetryGauges"
            visible: layout.gaugesShown && root.gaugeKeys.length > 0
            readonly property real cell: Math.max(60, Math.min((layout.midW - columnSpacing) / 2, (
                                                                   layout.upperH - rowSpacing) / 2))
            x: root.snap(layout.gap + (layout.cpuShown ? layout.chartW + layout.gap : 0) + (
                             layout.midW - width) / 2)
            y: root.snap(layout.upperY + (layout.upperH - height) / 2)
            columns: 2
            columnSpacing: layout.gap
            rowSpacing: layout.gap * 0.5
            Repeater {
                model: root.gaugeKeys
                Gauge {
                    required property string modelData
                    readonly property var shown: root.gauge(modelData)
                    size: gaugeGrid.cell
                    value: shown.value
                    peak: shown.peak
                    reading: shown.reading
                    unit: shown.unit
                    scaleTop: shown.scaleTop
                    label: shown.label
                }
            }
        }

        // Network: down as bars, up as a line.
        Section {
            id: networkSection
            objectName: "telemetryNetwork"
            visible: layout.networkShown
            x: root.width - layout.gap - width
            y: layout.upperY
            width: layout.chartW
            height: layout.upperH
            readonly property var unit: root.rateUnit(Math.max(root.downPeak, root.upPeak))
            title: qsTr("NETWORK · %1 · ▼ BARS  ▲ LINE", "telemetry chart, in a unit").arg(
                       unit.name)
            Chart {
                width: parent.width
                height: parent.height
                style: "bars"
                bars: 72
                values: root.down
                second: root.up
                peak: Math.max(root.downPeak, root.upPeak)
                format: v => root.inUnit(v, networkSection.unit)
            }
        }

        // The figures under the Omnibar, across the page.
        Row {
            id: figures
            objectName: "telemetryFigures"
            x: layout.gap
            y: layout.figuresY
            spacing: layout.gap
            visible: layout.figuresShown && root.figureKeys.length > 0
            readonly property real each: (layout.innerW - (root.figureKeys.length - 1) * spacing) / Math.max(
                                             1, root.figureKeys.length)
            Repeater {
                model: root.figureKeys
                Figure {
                    required property string modelData
                    readonly property var shown: root.figure(modelData)
                    width: figures.each
                    label: shown.label
                    value: shown.value
                    note: shown.note || ""
                    small: shown.small === true
                }
            }
        }

        // The sections under the figures, widths by weight among those shown.
        Row {
            id: lower
            x: layout.gap
            y: layout.lowerY
            spacing: layout.gap * 1.5
            readonly property var weights: ({
                                                pages: layout.lowerHas("pages") ? 3 : 0,
                                                memory: layout.lowerHas("memory") ? 2.4 : 0,
                                                power: layout.lowerHas("battery") ? 2.6 : 0,
                                                spaces: layout.lowerHas("spaces") ? 2.6 : 0
                                            })
            readonly property int count: Object.values(lower.weights).filter(w => w > 0).length
            readonly property real total: Object.values(lower.weights).reduce((s, w) => s + w, 0)
                                          || 1
            function widthOf(key) {
                return (layout.innerW - (lower.count - 1) * lower.spacing) * lower.weights[key]
                        / lower.total;
            }

            // Pages loaded today, an hour a bar.
            Section {
                objectName: "telemetrySection-pages"
                visible: layout.lowerHas("pages")
                width: lower.widthOf("pages")
                height: layout.lowerH
                title: qsTr("PAGES TODAY · %1").arg(root.sum(root.pagesToday))
                Chart {
                    objectName: "telemetryPagesChart"
                    width: parent.width
                    height: parent.height
                    style: "bars"
                    bars: 24
                    span: 24
                    values: root.pagesToday
                    // A whole number of pages at every gridline.
                    max: Math.max(4, Math.ceil(root.peakOf(root.pagesToday) / 4) * 4)
                    timeLabels: ["00", "06", "12", "18", "24"]
                    callout: false
                }
            }

            // Memory: RAM as used, cache and free, and swap.
            Section {
                id: memorySection
                objectName: "telemetrySection-memory"
                visible: layout.lowerHas("memory")
                width: lower.widthOf("memory")
                height: layout.lowerH
                title: qsTr("MEMORY · %1").arg(root.size(root.memory.total || 0))
                readonly property real used: Math.max(0, (root.memory.total || 0) - (root.memory.cache
                                                                                     || 0) - (root.memory.free
                                                                                              || 0))
                readonly property real ring: Math.max(60, Math.min(layout.lowerH - root.captionSize
                                                                   * 6, (width - root.captionSize
                                                                         * 14) / 1.2))
                Row {
                    spacing: 20
                    Donut {
                        size: memorySection.ring
                        parts: [
                            {
                                value: memorySection.used,
                                colour: root.glow
                            },
                            {
                                value: root.memory.cache || 0,
                                colour: Qt.rgba(root.glow.r, root.glow.g, root.glow.b, 0.45)
                            },
                            {
                                value: root.memory.free || 0,
                                colour: root.faint
                            }
                        ]
                        reading: root.percent(memorySection.used / (root.memory.total || 1))
                        label: qsTr("RAM")
                    }
                    Column {
                        spacing: 10
                        Caption {
                            text: qsTr("■ USED %1").arg(root.size(memorySection.used))
                            color: root.glow
                        }
                        Caption {
                            text: qsTr("■ CACHE %1").arg(root.size(root.memory.cache || 0))
                        }
                        Caption {
                            text: qsTr("□ FREE %1").arg(root.size(root.memory.free || 0))
                        }
                        Item {
                            width: 1
                            height: root.captionSize
                        }
                        Donut {
                            readonly property real total: root.memory.swapTotal || 0
                            readonly property real used: total - (root.memory.swapFree || 0)
                            size: memorySection.ring * 0.55
                            // Whole or not at all: under the legend, if it fits.
                            visible: memorySection.height - root.captionSize - 16
                                     >= root.captionSize * 6.5 + memorySection.ring * 0.55
                                     + root.captionSize * 1.6 + 30
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
                            reading: total > 0 ? root.percent(used / total) : "—"
                            label: qsTr("SWAP")
                        }
                    }
                }
            }

            // Power: the draw over five minutes, and the charge as a line.
            Section {
                objectName: "telemetrySection-power"
                visible: layout.lowerHas("battery")
                width: lower.widthOf("power")
                height: layout.lowerH
                title: root.battery ? qsTr("POWER · %1 · W BARS  % LINE",
                                           "telemetry chart: the battery's charge").arg(root.percent(
                                                                                            root.battery.capacity)) :
                                      ""
                Chart {
                    width: parent.width
                    height: parent.height
                    style: "bars"
                    bars: 50
                    values: root.battery ? root.battery.watts : []
                    second: root.battery ? root.battery.charge : []
                    secondMax: 1
                    format: v => qsTr("%1 W", "watts").arg(v.toFixed(v < 10 ? 1 : 0))
                }
            }

            // Memory per Space: a hatched bar each, against a common scale.
            Section {
                id: spacesSection
                objectName: "telemetrySection-spaces"
                visible: layout.lowerHas("spaces")
                width: lower.widthOf("spaces")
                height: layout.lowerH
                title: qsTr("MEMORY PER SPACE")
                // The scale's top: the largest Space rounded up to a whole
                // GiB.
                readonly property real scaleTop: Math.max(1, Math.ceil(root.spaces.reduce((m, s)
                                                                                          => Math.max(
                                                                                                 m, s.bytes),
                                                                                          0) / 1073741824))
                                                 * 1073741824
                readonly property real labelW: root.captionSize * 7
                readonly property real valueW: root.readingSize * 1.45 * 3.2
                readonly property real barW: Math.max(20, width - labelW - valueW - 24)
                // The rows share the section's height, up to a generous size.
                readonly property real rowH: Math.min(root.readingSize * 4.5, (height
                                                                               - root.captionSize
                                                                               * 5) / Math.max(1,
                                                                                               root.spaces.length)
                                                      - 8)
                Column {
                    spacing: 8
                    // The scale over the bars.
                    Item {
                        width: spacesSection.width
                        height: root.captionSize * 1.4
                        Repeater {
                            model: 3
                            Caption {
                                required property int index
                                x: root.snap(spacesSection.labelW + 12 + spacesSection.barW * index
                                             / 2 - (index === 2 ? width : index === 0 ? 0 : width
                                                                                        / 2))
                                text: qsTr("%1G", "gibibytes").arg(spacesSection.scaleTop * index
                                                                   / 2 / 1073741824)
                            }
                        }
                    }
                    Repeater {
                        model: root.spaces.length
                        Item {
                            required property int index
                            readonly property var modelData: root.spaces[index] || ({
                                                                                        name: "",
                                                                                        bytes: 0
                                                                                    })
                            width: spacesSection.width
                            height: spacesSection.rowH
                            Caption {
                                maxWidth: spacesSection.labelW
                                elide: Text.ElideRight
                                y: root.snap((parent.height - height) / 2)
                                text: modelData.name.toUpperCase()
                                color: root.ink
                            }
                            Canvas {
                                id: bar
                                x: spacesSection.labelW + 12
                                y: parent.height * 0.2
                                width: spacesSection.barW
                                height: parent.height * 0.6
                                property real share: Math.min(1, modelData.bytes
                                                              / spacesSection.scaleTop)
                                property color glow: root.glow
                                onShareChanged: requestPaint()
                                onGlowChanged: requestPaint()
                                onWidthChanged: requestPaint()
                                onHeightChanged: requestPaint()
                                onPaint: {
                                    const c = getContext("2d");
                                    c.reset();
                                    const g = bar.glow;
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
                                x: spacesSection.labelW + 12 + spacesSection.barW + 12
                                y: root.snap((parent.height - height) / 2)
                                pixelSize: Math.round(root.readingSize * 1.45)
                                text: qsTr("%1G", "gibibytes").arg((modelData.bytes
                                                                    / 1073741824).toFixed(1))
                            }
                        }
                    }
                }
            }
        }
    }
}
