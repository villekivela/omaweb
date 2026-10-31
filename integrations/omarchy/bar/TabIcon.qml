import QtQuick

// A tab's favicon for the switcher (#906), its host's first letters where
// the page gave none, and a spinner in its place while the page loads.
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
        visible: !icon.tab.loading && status === Image.Ready
    }
    Rectangle {
        anchors.fill: parent
        radius: icon.size / 4
        visible: !icon.tab.loading && favicon.status !== Image.Ready
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
    Text {
        visible: !!icon.tab.loading
        anchors.centerIn: parent
        text: "󰔟"
        color: icon.fg
        opacity: 0.6
        font.family: icon.font
        font.pixelSize: Math.round(icon.size * 0.8)
        RotationAnimation on rotation {
            running: !!icon.tab.loading
            loops: Animation.Infinite
            from: 0
            to: 360
            duration: 1200
        }
    }
}
