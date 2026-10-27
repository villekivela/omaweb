import QtQuick

// A canvas a Scene draws into, in logical pixels scaled to the Scene's own,
// `pitch` to one of them, each edge covering its share of a pixel as a
// browser's canvas draws it. It paints only while it can be seen; a paint
// asked for while it cannot is kept for when it can.
Canvas {
    id: layer

    property int pitch: 1
    property var paintWith: function (context) {}
    property bool stale: true

    anchors.fill: parent
    antialiasing: true
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
