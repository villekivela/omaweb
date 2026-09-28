import QtQuick
import qs.Commons

// PROTOTYPE (ui-language): the chord a control answers to, drawn over it
// while Ctrl is held in the Keys variant.
Rectangle {
    id: root
    property string keys: ""
    // Ctrl is already held, so a chord on it is labelled by the key that
    // finishes it, filled; a key pressed on its own is outlined.
    readonly property bool chord: keys.indexOf("Ctrl+") === 0
    readonly property string shownKeys: chord ? keys.substring(5) : keys
    property var colors
    property bool shown: false
    visible: shown && keys.length > 0
    opacity: shown ? 1 : 0
    z: 50
    width: label.implicitWidth + 8
    height: label.implicitHeight + 2
    radius: 2
    color: chord ? (root.colors ? root.colors.accent : "#9b87ff") : (root.colors
                                                                     ? root.colors.windowOpaque :
                                                                       "#16151d")
    border.width: chord ? 0 : 1
    border.color: root.colors ? root.colors.accent : "#9b87ff"

    Text {
        id: label
        anchors.centerIn: parent
        text: root.shownKeys
        color: root.chord ? (root.colors ? root.colors.windowOpaque : "#16151d") : (root.colors
                                                                                    ? root.colors.accent :
                                                                                      "#9b87ff")
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.bold: true
    }
}
