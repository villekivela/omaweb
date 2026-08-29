// THROWAWAY PROTOTYPE — the variant switcher. Deliberately not part of any
// design under evaluation: high contrast, floating, obviously bolted on.
import QtQuick

Item {
    id: root

    property var colors
    property string variantKey: "A"
    property string variantName: ""
    property string stateText: ""

    signal step(int delta)

    implicitWidth: pill.width
    implicitHeight: pill.height

    Rectangle {
        id: pill
        width: Math.max(360, layout.implicitWidth + 28)
        height: 52
        radius: 26
        color: "#f2101014"
        border.width: 1
        border.color: "#5cffffff"

        Row {
            id: layout
            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                width: 30
                height: 30
                radius: 15
                anchors.verticalCenter: parent.verticalCenter
                color: previousMouse.containsMouse ? "#33ffffff" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "‹"
                    color: "#ffffff"
                    font.pixelSize: 20
                }

                MouseArea {
                    id: previousMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.step(-1)
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.variantKey + " — " + root.variantName
                    color: "#ffffff"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.stateText
                    color: "#b9b4c4"
                    font.pixelSize: 10
                    font.family: "Menlo"
                }
            }

            Rectangle {
                width: 30
                height: 30
                radius: 15
                anchors.verticalCenter: parent.verticalCenter
                color: nextMouse.containsMouse ? "#33ffffff" : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "›"
                    color: "#ffffff"
                    font.pixelSize: 20
                }

                MouseArea {
                    id: nextMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.step(1)
                }
            }
        }
    }
}
