import QtQuick
import QtQuick.Shapes
import Omaweb
import "DevicePixels.mjs" as DevicePixels

// A touchpad seen from above, the back of a right hand on it with `fingers`
// fingers down and the rest curled, and a two-way arrow for the sideways
// swipe, drawn in the theme's accent (#933). While `active` the hand slides
// across the pad and back, and under reduced motion it stays in the middle.
//
// The hand is drawn in units of a hand about 80 wide whose middle fingertip
// is at 8, scaled onto the pad. It is outlined by stroking every part wide and
// filling them all again with `fill` on top, which leaves the outer edge and
// the lines between the fingers; `fill` is what the drawing stands on, so the
// pad's edge passes behind the hand.
Item {
    id: root

    property var colors
    property int fingers: 2
    property color fill
    property bool active: false
    readonly property bool moving: active && !SystemMotion.reduced
    // Where the hand is across the pad, from -1 to 1.
    property real slide: 0

    implicitHeight: Math.round(width * 0.5)
    clip: true

    onMovingChanged: if (!moving)
                         slide = 0

    SequentialAnimation on slide {
        running: root.moving
        loops: Animation.Infinite
        NumberAnimation {
            to: 1
            duration: 700
            easing.type: Easing.InOutSine
        }
        PauseAnimation {
            duration: 250
        }
        NumberAnimation {
            to: -1
            duration: 1100
            easing.type: Easing.InOutSine
        }
        PauseAnimation {
            duration: 250
        }
        NumberAnimation {
            to: 0
            duration: 500
            easing.type: Easing.InOutSine
        }
    }

    readonly property real stroke: Math.max(1.5, width / 220)
    readonly property real padWidth: width * 0.62
    readonly property real padHeight: height * 0.78
    readonly property real padX: DevicePixels.snap((width - padWidth) / 2, pixelRatio)
    readonly property real padY: DevicePixels.snap(height * 0.1, pixelRatio)
    readonly property real reach: padWidth * 0.17
    readonly property real pixelRatio: Window.window ? Window.window.devicePixelRatio : 1

    // A capsule standing on (x0..x1, bottom) with its round end at tipY, drawn
    // clockwise as every part is, so where parts overlap they fill as one.
    function capsule(point, k, x0, x1, tipY, bottom) {
        const r = (x1 - x0) / 2;
        return "M" + point(x0, bottom) + " L" + point(x0, tipY + r) + " A " + r * k + " " + r * k
                + " 0 0 1" + point(x1, tipY + r) + " L" + point(x1, bottom) + " Z ";
    }

    // The palm running into the wrist off the foot of the drawing, bulging
    // out under the thumb; the fingers across the palm's whole width, those
    // touching straight and the rest curled to the knuckle; and the thumb
    // pointing up along them, leaning out.
    function handPath(fingers, k, ox, oy) {
        const point = (x, y) => " " + (ox + x * k) + " " + (oy + y * k);
        let path = "M" + point(30, 66) + " L" + point(72, 66) + " Q" + point(80, 66) + point(80,
                                                                                             76) + " L"
            + point(80, 106) + " Q" + point(80, 120) + point(70, 129) + " L" + point(68, 175) + " L"
            + point(32, 175) + " L" + point(30, 133) + " Q" + point(12, 124) + point(12, 106) + " Q"
            + point(12, 92) + point(20, 86) + " L" + point(20, 76) + " Q" + point(20, 66) + point(30,
                                                                                                  66) + " Z ";
        // Index, middle, ring and little finger: left edge, right edge and
        // the tip when the finger is down.
        const columns = [[20, 33.9, 16], [35.4, 49.3, 8], [50.8, 64.6, 14], [66.1, 80, 28]];
        for (let finger = 0; finger < columns.length; ++finger) {
            const column = columns[finger];
            path += root.capsule(point, k, column[0], column[1], finger < fingers ? column[2] : 57,
                                 80);
        }
        const baseX = 19, baseY = 108, radius = 6.5, length = 40;
        const lean = 22 * Math.PI / 180;
        const tipX = baseX - length * Math.sin(lean), tipY = baseY - length * Math.cos(lean);
        const normalX = -Math.cos(lean) * radius, normalY = Math.sin(lean) * radius;
        path += "M" + point(baseX + normalX, baseY + normalY) + " L" + point(tipX + normalX, tipY
                                                                             + normalY) + " A "
                + radius * k + " " + radius * k + " 0 0 1" + point(tipX - normalX, tipY - normalY)
                + " L" + point(baseX - normalX, baseY - normalY) + " Z";
        return path;
    }

    Rectangle {
        x: root.padX
        y: root.padY
        width: Math.round(root.padWidth)
        height: Math.round(root.padHeight)
        radius: root.padHeight * 0.09
        color: "transparent"
        border.width: root.stroke
        border.color: root.colors.accent
    }

    // The swipe, both ways, above the fingertips.
    Shape {
        id: arrow
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        readonly property real level: root.padY + root.padHeight * 0.11
        readonly property real from: DevicePixels.snap(root.width / 2 - root.padWidth * 0.3,
                                                       root.pixelRatio)
        readonly property real to: DevicePixels.snap(root.width / 2 + root.padWidth * 0.3,
                                                     root.pixelRatio)

        readonly property real head: root.padWidth * 0.047

        ShapePath {
            strokeColor: root.colors.accent
            strokeWidth: root.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: {
                    const point = (x, y) => " " + x + " " + y;
                    return "M" + point(arrow.from + arrow.head, arrow.level - arrow.head) + " L"
                            + point(arrow.from, arrow.level) + " L" + point(arrow.from + arrow.head,
                                                                            arrow.level
                                                                            + arrow.head) + " M"
                            + point(arrow.from, arrow.level) + " L" + point(arrow.to, arrow.level)
                            + " M" + point(arrow.to - arrow.head, arrow.level - arrow.head) + " L"
                            + point(arrow.to, arrow.level) + " L" + point(arrow.to - arrow.head,
                                                                          arrow.level + arrow.head);
                }
            }
        }
    }

    Shape {
        id: hand
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        readonly property real k: root.height / 172
        // At rest on the display's pixels; the slide moves it from there.
        readonly property real restingX: DevicePixels.snap(root.width / 2 - 47 * k, root.pixelRatio)
        readonly property string path: root.handPath(root.fingers, k, restingX + root.slide
                                                     * root.reach, root.padY + root.padHeight * 0.3
                                                     - 8 * k)

        ShapePath {
            fillRule: ShapePath.WindingFill
            strokeColor: root.colors.accent
            strokeWidth: root.stroke * 2
            fillColor: root.fill
            joinStyle: ShapePath.RoundJoin
            PathSvg {
                path: hand.path
            }
        }
        ShapePath {
            fillRule: ShapePath.WindingFill
            strokeWidth: -1
            strokeColor: "transparent"
            fillColor: root.fill
            PathSvg {
                path: hand.path
            }
        }
    }
}
