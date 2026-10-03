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
    property color plate: root.colors ? root.colors.windowOpaque : "black"
    property bool filled: true

    // The row as caps and the dots between alternatives.
    readonly property var entries: root.entriesFor(root.keys)
    // The keys alone, one to a cap.
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
                                           metrics.advanceWidth(entry.key));
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
        model: root.entries

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
