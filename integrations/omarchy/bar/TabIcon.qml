import QtQuick
import qs.Commons as Commons

// A tab's favicon for the switcher (#906), its host's first letters where
// the page gave none, an accent dot at its top left for an Agent tab, and a
// short line running under it while the page loads.
Item {
    id: icon

    required property var tab
    property color fg: "white"
    property string font: ""
    property int size: 16

    width: size
    height: size

    Image {
        id: favicon
        anchors.fill: parent
        source: icon.tab && icon.tab.icon ? "file://" + icon.tab.icon : ""
        sourceSize: Qt.size(icon.size * 2, icon.size * 2)
        smooth: true
        asynchronous: true
        visible: status === Image.Ready
    }
    Rectangle {
        anchors.fill: parent
        radius: icon.size / 4
        visible: favicon.status !== Image.Ready
        color: Qt.alpha(icon.fg, 0.16)
        Text {
            anchors.centerIn: parent
            text: String(icon.tab.host || "").replace(/^www\./, "").slice(0, 2)
            color: icon.fg
            font.family: icon.font
            font.pixelSize: Math.round(icon.size / 2)
            font.bold: true
        }
    }
    Rectangle {
        visible: !!icon.tab.agent
        x: -2
        y: -2
        width: 6
        height: 6
        radius: 3
        color: Commons.Color.accent
    }
    Rectangle {
        id: runner
        visible: !!icon.tab.loading
        y: icon.height + 2
        width: Math.round(icon.width / 2)
        height: 2
        radius: 1
        color: icon.fg
        opacity: 0.8
        SequentialAnimation on x {
            running: runner.visible
            loops: Animation.Infinite
            NumberAnimation {
                from: 0
                to: icon.width - runner.width
                duration: 700
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                from: icon.width - runner.width
                to: 0
                duration: 700
                easing.type: Easing.InOutSine
            }
        }
    }
}
