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
    // Whether alternatives are joined by a dot. Off, they are set apart by a
    // quiet gap alone.
    property bool dotted: true
    // Whether the alternative with the fewest keys, the bare one, comes first.
    property bool bareFirst: false
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
        const alternatives = keys.split(/\s+\u00b7\s+/).map(function (alternative) {
            return alternative.split(/\+(?=.)/);
        });
        if (root.bareFirst) {
            // The sort is stable, so alternatives of one length keep their order.
            alternatives.sort(function (a, b) {
                return a.length - b.length;
            });
        }
        for (const alternative of alternatives) {
            if (entries.length > 0)
                entries.push({
                                 "separator": true,
                                 "key": root.dotted ? "\u00b7" : ""
                             });
            for (const key of alternative)
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
            width += entry.separator ? (root.dotted ? Math.ceil(metrics.advanceWidth(entry.key)) : 6
                                                      * ruler.unit) : ruler.widthFor(root.advanceOf(
                                                                                         entry.key));
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
        model: root.entries

        Item {
            id: entry
            required property var modelData
            width: modelData.separator ? (root.dotted ? dot.implicitWidth : 6 * cap.unit) :
                                         cap.width
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
                objectName: "keycapSeparator"
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
