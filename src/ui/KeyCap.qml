import QtQuick
import qs.Commons

// A key as the website draws its `kbd`: a 22 px cap, at least 22 px wide, with
// a 1 px border in the accent at 55% and a 3 px bottom edge, the accent at 10%
// over the plate it stands on, and the key in the accent in the mono face at
// weight 500. Its size follows the interface font, 12 px being the size it is
// drawn at on the website.
//
// `text` is the key's name, "Return" or "Shift", and a special key is drawn as
// a symbol: a Material Symbols icon where the font carries a matching glyph,
// the Unicode symbol in the interface face where it does not, and that symbol
// for all of them where no icon font is given. Ctrl, Alt, Escape and the
// navigation keys with no clear symbol stay words.
Rectangle {
    id: root
    objectName: "keycap"

    property var colors
    // The key's name.
    property string text: ""
    // The Material Symbols family, where the window has loaded it.
    property string iconFontFamily: ""
    // What the cap stands on, which its ground and border are mixed over.
    property color plate: root.colors ? root.colors.windowOpaque : "black"
    // A cap with no ground of its own, outlined only.
    property bool filled: true
    // A cap filled with the accent, its key in the plate's colour: the cap of
    // what Return would do.
    property bool accented: false

    // How a special key is drawn: the icon the font names it by and the
    // Unicode symbol that stands in for it. A key without a clear symbol is not
    // here and is drawn as its name.
    readonly property var faces: ({
                                      "Return": {
                                          "icon": "keyboard_return",
                                          "glyph": "\u21b5"
                                      },
                                      "Enter": {
                                          "icon": "keyboard_return",
                                          "glyph": "\u21b5"
                                      },
                                      "Up": {
                                          "icon": "arrow_upward",
                                          "glyph": "\u2191"
                                      },
                                      "Down": {
                                          "icon": "arrow_downward",
                                          "glyph": "\u2193"
                                      },
                                      "Left": {
                                          "icon": "arrow_back",
                                          "glyph": "\u2190"
                                      },
                                      "Right": {
                                          "icon": "arrow_forward",
                                          "glyph": "\u2192"
                                      },
                                      "Tab": {
                                          "icon": "keyboard_tab",
                                          "glyph": "\u21e5"
                                      },
                                      "Backspace": {
                                          "icon": "backspace",
                                          "glyph": "\u232b"
                                      },
                                      "Shift": {
                                          "icon": "shift",
                                          "glyph": "\u21e7"
                                      },
                                      "Space": {
                                          "icon": "space_bar",
                                          "glyph": "\u2423"
                                      },
                                      "Delete": {
                                          "glyph": "\u2326"
                                      },
                                      "Del": {
                                          "glyph": "\u2326"
                                      },
                                      "Esc": {
                                          "word": "Esc"
                                      },
                                      "Escape": {
                                          "word": "Esc"
                                      }
                                  })
    readonly property var names: ({
                                      "Esc": "Escape",
                                      "Del": "Delete"
                                  })

    // What a key is drawn as: its `text`, and whether that is an icon of the
    // icon font, which is drawn in that font.
    function faceFor(key) {
        const face = root.faces[key];
        if (!face)
            return {
                "text": key,
                "icon": false
            };
        if (face.word)
            return {
                "text": face.word,
                "icon": false
            };
        if (face.icon && root.iconFontFamily.length > 0)
            return {
                "text": face.icon,
                "icon": true
            };
        return {
            "text": face.glyph,
            "icon": false
        };
    }

    // What a screen reader is told for a key.
    function spokenName(key) {
        return root.names[key] || key;
    }

    readonly property var shown: root.faceFor(root.text)
    readonly property real unit: Style.font.body / 12
    readonly property color accent: root.colors ? root.colors.accent : "white"
    readonly property color ground: root.accented ? root.accent : (root.filled ? root.accentOver(
                                                                                     root.plate,
                                                                                     0.1) : root.plate)

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
    color: root.accented ? root.accent : root.accentOver(root.ground, 0.55)
    Accessible.ignored: true
    Accessible.name: root.spokenName(root.text)

    Rectangle {
        id: inner
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
            text: root.shown.text
            color: root.accented ? root.plate : root.accent
            font.family: root.shown.icon ? root.iconFontFamily : Style.font.family
            font.pixelSize: (root.shown.icon ? 14 : 12) * root.unit
            font.weight: Font.Medium
            Accessible.ignored: true
        }
    }
}
