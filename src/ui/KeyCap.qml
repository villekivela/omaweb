import QtQuick
import qs.Commons

// A key as the website draws its `kbd`: a 22 px cap, at least 22 px wide, with
// a 1 px border in the accent at 55% and a 3 px bottom edge, the accent at 10%
// over the plate it stands on, and the key in the accent in the mono face at
// weight 500. Its size follows the interface font, 12 px being the size it is
// drawn at on the website.
Rectangle {
    id: root
    objectName: "keycap"

    property var colors
    property string text: ""
    // What the cap stands on, which its ground and border are mixed over.
    property color plate: root.colors ? root.colors.windowOpaque : "black"
    // A cap with no ground of its own, outlined only.
    property bool filled: true

    readonly property real unit: Style.font.body / 12
    readonly property color accent: root.colors ? root.colors.accent : "white"
    readonly property color ground: root.filled ? root.accentOver(root.plate, 0.1) : root.plate

    function accentOver(base, alpha) {
        return Qt.tint(base, Qt.rgba(root.accent.r, root.accent.g, root.accent.b, alpha));
    }

    // The cap's width for a key the face advances by `advance`, which is what
    // a row of caps is measured with before any is drawn.
    function widthFor(advance) {
        return Math.max(22 * root.unit, Math.ceil(advance) + 6 * root.unit);
    }

    width: root.widthFor(label.implicitWidth)
    height: 22 * root.unit
    radius: 4 * root.unit
    color: root.accentOver(root.ground, 0.55)
    Accessible.ignored: true

    Rectangle {
        id: face
        x: 1
        y: 1
        width: root.width - 2
        height: root.height - 4
        radius: Math.max(0, root.radius - 1)
        color: root.ground

        Text {
            id: label
            objectName: "keycapLabel"
            anchors.centerIn: parent
            text: root.text
            color: root.accent
            font.family: Style.font.family
            font.pixelSize: 12 * root.unit
            font.weight: Font.Medium
            Accessible.ignored: true
        }
    }
}
