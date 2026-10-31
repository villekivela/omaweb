import QtQuick

// PROTOTYPE (#906): the pill at the bottom that flips between the switcher's
// drawings. Not part of any design; F1 to F3 or Ctrl+[ and Ctrl+] do the same.
Rectangle {
    id: bar
    property var core

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 24
    width: row.implicitWidth + 24
    height: 36
    radius: 18
    color: "#ff2d95"
    border.color: "white"
    border.width: 2

    MouseArea {
        anchors.fill: parent
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 14
        Text {
            text: "◀"
            color: "white"
            font.pixelSize: 15
            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                onClicked: bar.core.stepVariant(-1)
            }
        }
        Text {
            text: "PROTOTYPE  " + String.fromCharCode(65 + bar.core.variant) + "  " + bar.core.variants[bar.core.variant].name
                  + "    (F1 to F" + bar.core.variants.length + ")"
            color: "white"
            font.pixelSize: 14
            font.bold: true
        }
        Text {
            text: "▶"
            color: "white"
            font.pixelSize: 15
            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                onClicked: bar.core.stepVariant(1)
            }
        }
    }
}
