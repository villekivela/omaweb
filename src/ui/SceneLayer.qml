import QtQuick

// A canvas a Scene draws what does not move into, once per size, theme and
// option rather than on every tick. It is drawn in logical pixels and scaled to
// the Scene's own, `pitch` to one of them. It paints only while it can be
// seen; a paint asked for while it cannot is kept for when it can.
Canvas {
    id: layer

    property int pitch: 1
    property var paintWith: function (context) {}
    property bool stale: true

    anchors.fill: parent
    renderTarget: Canvas.Image
    renderStrategy: Canvas.Immediate

    function draw() {
        layer.stale = true;
        if (layer.visible)
            layer.requestPaint();
    }

    onVisibleChanged: if (visible && stale)
                          requestPaint()
    onPaint: {
        layer.stale = false;
        const context = getContext("2d");
        context.reset();
        context.scale(1 / layer.pitch, 1 / layer.pitch);
        layer.paintWith(context);
    }
}
