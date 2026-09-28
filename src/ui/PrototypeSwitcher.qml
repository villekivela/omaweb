import QtQuick
import qs.Commons

// PROTOTYPE (ui-language): throwaway. Cycles the interface variants. Not part
// of any design being judged, so it is drawn loud and plain on purpose.
Rectangle {
    id: root
    required property var window
    readonly property var variants: ["0", "A", "B", "C"]
    readonly property var names: ({
                                      "0": "Today",
                                      "A": "Tiled",
                                      "B": "Ledger",
                                      "C": "Keys"
                                  })
    function step(delta) {
        const at = root.variants.indexOf(root.window.uiVariant);
        const next = (at + delta + root.variants.length) % root.variants.length;
        root.window.uiVariant = root.variants[next];
    }

    z: 1000
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 18
    width: bar.implicitWidth + 24
    height: 36
    radius: 18
    color: "#f5f5f5"
    border.color: "#111111"
    border.width: 2

    Shortcut {
        sequences: ["Ctrl+Alt+Right"]
        context: Qt.ApplicationShortcut
        onActivated: root.step(1)
    }
    Shortcut {
        sequences: ["Ctrl+Alt+Left"]
        context: Qt.ApplicationShortcut
        onActivated: root.step(-1)
    }

    Row {
        id: bar
        anchors.centerIn: parent
        spacing: 14

        Text {
            text: "◀"
            color: "#111111"
            font.pixelSize: 14
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                onClicked: root.step(-1)
            }
        }
        Text {
            text: root.window.uiVariant + "  " + root.names[root.window.uiVariant]
            color: "#111111"
            font.family: Style.font.family
            font.pixelSize: 13
            font.bold: true
        }
        Text {
            text: "▶"
            color: "#111111"
            font.pixelSize: 14
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                onClicked: root.step(1)
            }
        }
        Text {
            visible: root.window.uiVariant === "C"
            text: root.window.protoKeysPinned ? "[keys on]" : "[keys]"
            color: "#111111"
            font.family: Style.font.family
            font.pixelSize: 12
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                onClicked: root.window.protoKeysPinned = !root.window.protoKeysPinned
            }
        }
    }
}
