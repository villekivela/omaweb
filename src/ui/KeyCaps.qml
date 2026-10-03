import QtQuick
import qs.Commons

// A binding as a row of caps, one per key, as the website sets its `.keys`:
// "Ctrl+Shift+T" is three caps 4 px apart. A `+` that is the key itself, as in
// "Ctrl++", stays one.
Row {
    id: root
    objectName: "keycaps"

    property var colors
    property string keys: ""
    property color plate: root.colors ? root.colors.windowOpaque : "black"
    property bool filled: true

    readonly property var parts: root.split(root.keys)
    // The face `widthOf` measures in. A caller reads these for the dependency
    // alone: the measuring is done in C++ off a font the binding never
    // otherwise touches, so a change of type would leave it on the last size.
    readonly property real measuredPixelSize: metrics.font.pixelSize
    readonly property string measuredFamily: metrics.font.family

    function split(keys) {
        return keys.length === 0 ? [] : keys.split(/\+(?=.)/);
    }

    // The width the row of a binding takes, measured in the face the caps are
    // drawn in.
    function widthOf(keys) {
        const keyParts = root.split(keys);
        let width = Math.max(0, keyParts.length - 1) * root.spacing;
        for (const part of keyParts)
            width += ruler.widthFor(metrics.advanceWidth(part));
        return width;
    }

    spacing: 4

    KeyCap {
        id: ruler
        visible: false
    }

    FontMetrics {
        id: metrics
        font.family: Style.font.family
        font.pixelSize: 12 * ruler.unit
        font.weight: Font.Medium
    }

    Repeater {
        model: root.parts

        KeyCap {
            required property string modelData
            colors: root.colors
            text: modelData
            plate: root.plate
            filled: root.filled
        }
    }
}
