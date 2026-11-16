import QtQuick
import "DevicePixels.mjs" as DevicePixels

// A mark only: it never takes input or focus from the page, and follows the fingers directly.
Item {
    id: root
    objectName: "historySwipeFeedback"

    required property var engine
    readonly property real progress: engine && engine.historySwipeProgress !== undefined
                                     ? engine.historySwipeProgress : 0
    readonly property real amount: Math.abs(progress)
    readonly property real ratio: root.Window.window ? root.Window.window.devicePixelRatio : 1
    readonly property color ink: engine && engine.pageControlAccent.a > 0
                                 ? engine.pageControlAccent : palette.highlight

    anchors.fill: parent
    z: 100
    visible: amount > 0
    enabled: false
    Accessible.ignored: true

    SystemPalette {
        id: palette
        colorGroup: SystemPalette.Active
    }

    Rectangle {
        id: circle
        objectName: "historySwipeArrow"
        width: 48
        height: 48
        radius: 24
        y: DevicePixels.snap((root.height - height) / 2, root.ratio)
        x: DevicePixels.snap(root.progress > 0 ? 12 + Math.min(root.amount, 1) * 20 : root.width
                                                 - width - 12 - Math.min(root.amount, 1) * 20,
                             root.ratio)

        color: root.engine ? root.engine.pageBackgroundColor : "transparent"
        border.color: root.ink
        border.width: root.amount >= 1 ? 3 : 1
        opacity: 0.95

        Canvas {
            id: arrow
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                ctx.strokeStyle = root.ink;
                ctx.lineWidth = 2.5;
                ctx.lineCap = "round";
                ctx.lineJoin = "round";
                if (root.progress < 0) {
                    ctx.translate(width, 0);
                    ctx.scale(-1, 1);
                }
                ctx.beginPath();
                ctx.moveTo(31, 24);
                ctx.lineTo(17, 24);
                ctx.moveTo(24, 17);
                ctx.lineTo(17, 24);
                ctx.lineTo(24, 31);
                ctx.stroke();
            }
            Connections {
                target: root
                function onProgressChanged() {
                    arrow.requestPaint();
                }
                function onInkChanged() {
                    arrow.requestPaint();
                }
            }
        }
    }
}
