import QtQuick
import qs.Commons

// The key a control answers to, laid over it while Primary is held on its own.
// Primary is already down, so a chord on it is labelled by the key that
// finishes it, filled; a key pressed on its own is labelled as itself,
// outlined, because it is not what the held key leads to.
Rectangle {
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
    width: label.implicitWidth + 8
    height: label.implicitHeight + 2
    radius: 2
    color: chord ? colors.accent : colors.windowOpaque
    border.width: chord ? 0 : 1
    border.color: colors.accent
    Accessible.ignored: true

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.chord ? root.colors.windowOpaque : root.colors.accent
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.bold: true
    }
}
