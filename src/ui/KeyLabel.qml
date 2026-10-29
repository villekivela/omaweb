import QtQuick
import qs.Commons

// The key a control answers to, laid over it while Primary is held on its own.
// Primary is already down, so a chord on it alone is labelled by the key that
// finishes it, on the key cap's own ground; a key pressed on its own is
// labelled as itself, outlined without the ground, because it is not what the
// held key leads to. A chord with more than Primary in it is labelled whole:
// labelled Shift and Return, Primary+Shift+Return was pressed without Primary
// (#863). Return is named Enter, the name on the key.
Item {
    id: root

    // The binding as the keymap displays it, such as "Ctrl+L" or "r". Empty
    // for a control the keymap gives no key.
    property string keys: ""
    property bool shown: false
    property var colors
    property string iconFontFamily: ""

    readonly property bool chord: keys.indexOf("Ctrl+") === 0
    readonly property string afterPrimary: chord ? keys.slice("Ctrl+".length) : keys
    // More than one key still to press after Primary, such as Shift and Return,
    // drawn as a cap a key.
    readonly property bool severalKeys: /\+./.test(afterPrimary)
    readonly property string text: (severalKeys ? keys : afterPrimary).replace(/(^|\+)Return$/,
                                                                               "$1Enter")

    visible: shown && keys.length > 0
    z: 50
    width: severalKeys ? caps.width : cap.width
    height: severalKeys ? caps.height : cap.height
    Accessible.ignored: true

    KeyCap {
        id: cap
        visible: !root.severalKeys
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        text: root.severalKeys ? "" : root.text
        filled: root.chord
    }

    KeyCaps {
        id: caps
        visible: root.severalKeys
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        keys: root.severalKeys ? root.text : ""
        filled: root.chord
    }
}
