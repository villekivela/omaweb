import QtQuick
import qs.Commons

// The key a control answers to, laid over it while Primary is held on its own.
// Primary is already down, so a chord on it is labelled by the key that
// finishes it, on the key cap's own ground; a key pressed on its own is
// labelled as itself, outlined without the ground, because it is not what the
// held key leads to.
Item {
    id: root

    // The binding as the keymap displays it, such as "Ctrl+L" or "r". Empty
    // for a control the keymap gives no key.
    property string keys: ""
    property bool shown: false
    property var colors

    readonly property bool chord: keys.indexOf("Ctrl+") === 0
    readonly property string text: chord ? keys.slice("Ctrl+".length) : keys

    visible: shown && keys.length > 0
    z: 50
    width: cap.width
    height: cap.height
    Accessible.ignored: true

    KeyCap {
        id: cap
        colors: root.colors
        text: root.text
        filled: root.chord
    }
}
