import QtQuick
import qs.Commons

// The Start page's way to the Shortcut sheet: `?` as a key cap and its word.
// The Omnibar's empty field holds it after the placeholder, and its hint row at
// the right end beside the results' keys.
Row {
    id: root

    property var colors
    property color plate: root.colors ? root.colors.overlayOpaque : "black"

    spacing: 8
    Accessible.role: Accessible.StaticText
    Accessible.name: qsTr("Question mark shows the keyboard shortcuts")

    KeyCap {
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        text: "?"
        plate: root.plate
    }

    Text {
        objectName: "shortcutsCueWord"
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("shortcuts")
        color: root.colors.mutedText
        font.family: Style.font.family
        font.pixelSize: Style.font.bodySmall
        Accessible.ignored: true
    }
}
