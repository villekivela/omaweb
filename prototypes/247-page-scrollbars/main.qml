import QtQuick
import QtQuick.Window
import QtWebEngine

// PROTOTYPE — throwaway. Answers: can Omaweb draw a webpage's scrollbar itself?
//
// Hides only the root scrollbar (`scrollbar-width: none` on html, which leaves
// every inner scroller's bar alone), reports the page's scroll metrics over the
// console-message channel EngineView already uses for __omaweb_* messages, and
// draws the bar in QML. Shown only while the pointer is in the gutter.
Window {
    id: win
    width: 900
    height: 700
    visible: true
    title: "scrollproto"

    property real pageY: 0
    property real pageHeight: 1
    property real viewportHeight: 1
    property int updates: 0
    property real lastArrival: 0
    property real worstGap: 0
    property real lastLatency: 0

    readonly property bool scrollable: pageHeight > viewportHeight + 1

    // PROTOTYPE escape hatch: `scrollproto show` pins the bar visible, because
    // a warped cursor does not produce a Wayland pointer-enter and the hover
    // gate cannot be driven synthetically.
    readonly property bool forceShow: Qt.application.arguments.indexOf("show") >= 0

    function report(text) {
        const now = Date.now();
        const data = JSON.parse(text);
        win.pageY = data.y;
        win.pageHeight = data.h;
        win.viewportHeight = data.v;
        win.lastLatency = now - data.t;
        if (win.lastArrival > 0)
            win.worstGap = Math.max(win.worstGap, now - win.lastArrival);
        win.lastArrival = now;
        win.updates++;
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        url: "https://en.wikipedia.org/wiki/Foo_Fighters"

        readonly property string protoSource: "(() => {"
            + "const css = ':where(html){scrollbar-width:none}';"
            + "const apply = () => {"
            + "  const parent = document.head || document.documentElement;"
            + "  if (!parent) return false;"
            + "  let s = document.getElementById('__proto_sb');"
            + "  if (!s) { s = document.createElement('style'); s.id = '__proto_sb'; parent.append(s); }"
            + "  s.textContent = css; return true;"
            + "};"
            + "if (!apply()) { const o = new MutationObserver(() => { if (apply()) o.disconnect(); });"
            + "  o.observe(document, {childList:true, subtree:true}); }"
            + "const send = () => {"
            + "  const d = document.documentElement;"
            + "  console.log('__omaweb_scroll__' + JSON.stringify({"
            + "    y: window.scrollY, h: d.scrollHeight, v: window.innerHeight, t: Date.now()}));"
            + "};"
            + "addEventListener('scroll', send, {passive:true});"
            + "addEventListener('resize', send, {passive:true});"
            + "addEventListener('load', send);"
            + "setInterval(send, 500);"
            + "send();"
            + "})()"

        Component.onCompleted: {
            const script = WebEngine.script();
            script.name = "proto scrollbar";
            script.injectionPoint = WebEngineScript.DocumentCreation;
            script.worldId = WebEngineScript.MainWorld;
            script.runsOnSubFrames = false;
            script.sourceCode = view.protoSource;
            view.userScripts.collection = [script];
        }

        onJavaScriptConsoleMessage: function (level, message) {
            if (message.startsWith("__omaweb_scroll__"))
                win.report(message.substring("__omaweb_scroll__".length));
        }
    }

    // ---- the bar -----------------------------------------------------------

    readonly property int gutter: 14
    readonly property int restingThumb: 6
    readonly property int hoveredThumb: 9

    MouseArea {
        id: gutterArea
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: win.gutter
        hoverEnabled: true
        visible: win.scrollable
        cursorShape: Qt.ArrowCursor

        property bool dragging: false
        property real grabOffset: 0

        readonly property real trackLength: height
        readonly property real thumbLength: Math.max(32, trackLength * (win.viewportHeight / Math.max(win.pageHeight, 1)))
        readonly property real maxScroll: Math.max(win.pageHeight - win.viewportHeight, 1)
        readonly property real thumbTop: (win.pageY / maxScroll) * (trackLength - thumbLength)

        onPressed: function (mouse) {
            if (mouse.y >= thumbTop && mouse.y <= thumbTop + thumbLength) {
                dragging = true;
                grabOffset = mouse.y - thumbTop;
            } else {
                dragging = true;
                grabOffset = thumbLength / 2;
                scrollTo(mouse.y);
            }
        }
        onReleased: dragging = false
        onPositionChanged: function (mouse) {
            if (dragging)
                scrollTo(mouse.y);
        }

        function scrollTo(pointerY) {
            const top = Math.max(0, Math.min(trackLength - thumbLength, pointerY - grabOffset));
            const target = (top / Math.max(trackLength - thumbLength, 1)) * maxScroll;
            view.runJavaScript("window.scrollTo(0," + Math.round(target) + ")");
        }

        Rectangle {
            id: thumb
            width: (gutterArea.containsMouse || gutterArea.dragging) ? win.hoveredThumb : win.restingThumb
            height: gutterArea.thumbLength
            radius: width / 2
            anchors.right: parent.right
            anchors.rightMargin: 2
            y: gutterArea.thumbTop
            color: gutterArea.dragging ? "#9b87ff" : (gutterArea.containsMouse ? "#aaa5b7" : "#4a4658")
            opacity: (gutterArea.containsMouse || gutterArea.dragging || win.forceShow) ? 1 : 0
            Behavior on width { NumberAnimation { duration: 120 } }
            Behavior on opacity { NumberAnimation { duration: 160 } }
            Behavior on color { ColorAnimation { duration: 120 } }
        }
    }

    // ---- instrumentation ---------------------------------------------------

    Rectangle {
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 360
        height: 74
        color: "#cc16151d"

        Text {
            anchors.centerIn: parent
            color: "#f3f1fa"
            font.family: "monospace"
            font.pixelSize: 12
            text: "y " + Math.round(win.pageY) + " / " + Math.round(win.pageHeight)
                + "   updates " + win.updates
                + "\nlast latency " + Math.round(win.lastLatency) + " ms"
                + "\nworst gap between updates " + Math.round(win.worstGap) + " ms"
                + "\nhover " + gutterArea.containsMouse + "  drag " + gutterArea.dragging
                + "  thumb y " + Math.round(gutterArea.thumbTop) + " len " + Math.round(gutterArea.thumbLength)
        }
    }
}
