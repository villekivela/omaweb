import QtQuick
import qs.Commons

// A binding as a row of caps, one per key, as the website sets its `.keys`:
// "Ctrl+Shift+T" is three caps 4 px apart. A binding with alternatives, which
// the key map joins with a dot, is one run of caps for each, the dot between
// them. A `+` that is the key itself, as in "Ctrl++", stays one.
Row {
    id: root
    objectName: "keycaps"

    property var colors
    property string keys: ""
    // The Material Symbols family, where the window has loaded it: a special
    // key is an icon of it, and measured in it.
    property string iconFontFamily: ""
    property color plate: root.colors ? root.colors.windowOpaque : "black"
    property bool filled: true
    // Whether the caps are made. The Shortcut sheet holds a row for every
    // command whether it is open or not, and a closed sheet that drew them all
    // slowed the rest of the window down.
    property bool drawn: true

    readonly property var entries: root.entriesFor(root.keys)
    readonly property var parts: root.entries.filter(function (entry) {
        return !entry.separator;
    }).map(function (entry) {
        return entry.key;
    })
    // The face `widthOf` measures in. A caller reads these for the dependency
    // alone: the measuring is done in C++ off a font the binding never
    // otherwise touches, so a change of type would leave it on the last size.
    readonly property real measuredPixelSize: metrics.font.pixelSize
    readonly property string measuredFamily: metrics.font.family
    readonly property string measuredIconFamily: root.iconFontFamily + "/"
                                                 + iconMetrics.font.pixelSize

    function entriesFor(keys) {
        const entries = [];
        if (keys.length === 0)
            return entries;
        for (const alternative of keys.split(/\s+\u00b7\s+/)) {
            if (entries.length > 0)
                entries.push({
                                 "separator": true,
                                 "key": "\u00b7"
                             });
            for (const key of alternative.split(/\+(?=.)/))
                entries.push({
                                 "separator": false,
                                 "key": key
                             });
        }
        return entries;
    }

    // The width the row of a binding takes, measured in the face the caps are
    // drawn in.
    function widthOf(keys) {
        const row = root.entriesFor(keys);
        let width = Math.max(0, row.length - 1) * root.spacing;
        for (const entry of row)
            width += entry.separator ? Math.ceil(metrics.advanceWidth(entry.key)) : ruler.widthFor(
                                           root.advanceOf(entry.key));
        return width;
    }

    // How far a key's face advances, in the face it is drawn in.
    function advanceOf(key) {
        const face = ruler.faceFor(key);
        return face.icon ? iconMetrics.advanceWidth(face.text) : metrics.advanceWidth(face.text);
    }

    spacing: 4

    KeyCap {
        id: ruler
        visible: false
        iconFontFamily: root.iconFontFamily
    }

    FontMetrics {
        id: iconMetrics
        font.family: root.iconFontFamily
        font.pixelSize: 14 * ruler.unit
    }

    FontMetrics {
        id: metrics
        font.family: Style.font.family
        font.pixelSize: 12 * ruler.unit
        font.weight: Font.Medium
    }

    Repeater {
        model: root.drawn ? root.entries : []

        Item {
            id: entry
            required property var modelData
            width: modelData.separator ? dot.implicitWidth : cap.width
            height: cap.height

            KeyCap {
                id: cap
                visible: !entry.modelData.separator
                colors: root.colors
                text: entry.modelData.separator ? "" : entry.modelData.key
                iconFontFamily: root.iconFontFamily
                plate: root.plate
                filled: root.filled
            }

            Text {
                id: dot
                anchors.centerIn: parent
                visible: entry.modelData.separator
                text: entry.modelData.key
                color: root.colors ? root.colors.mutedText : "gray"
                font.family: Style.font.family
                font.pixelSize: 12 * cap.unit
                font.weight: Font.Medium
            }
        }
    }
}
