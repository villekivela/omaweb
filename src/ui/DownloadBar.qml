import QtQuick
import Omaweb
import qs.Commons

// How far a download has come, drawn as a thin track. With a known size the
// fill is the share received; without one a segment slides along the track
// instead of inventing a percentage. The segment rests in the middle under
// reduced motion, and its animation exists only while it shows, so a window
// with no download running pays nothing for it.
Rectangle {
    id: root

    property var colors
    // Names the track and its parts: `<name>Track`, `<name>Fill`, `<name>Slide`.
    property string name: "download"
    // The share received, or below zero while the size is unknown.
    property real fraction: -1
    // Whether a download is running, which is when an unknown size slides.
    property bool running: true
    // Drawn full, for a finished download the surface still holds.
    property bool full: false

    readonly property bool measured: root.fraction >= 0

    objectName: root.name + "Track"
    height: 3
    radius: 1
    color: root.colors.border

    Rectangle {
        objectName: root.name + "Fill"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        radius: parent.radius
        width: root.full ? root.width : (root.measured ? Math.round(root.width * root.fraction) : 0)
        color: root.colors.accent
    }

    Rectangle {
        id: slide
        objectName: root.name + "Slide"

        readonly property real restingX: Math.round((root.width - width) / 2)
        readonly property bool animating: sliding.running

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Math.round(root.width / 3)
        radius: parent.radius
        color: root.colors.accent
        visible: root.running && !root.measured && !root.full
        x: slide.restingX

        SequentialAnimation on x {
            id: sliding
            running: slide.visible && !SystemMotion.reduced
            loops: Animation.Infinite
            onRunningChanged: if (!running)
                                  slide.x = Qt.binding(function () {
                                      return slide.restingX;
                                  })
            NumberAnimation {
                to: root.width - slide.width
                duration: 900
                easing.type: Easing.InOutSine
            }
            NumberAnimation {
                to: 0
                duration: 900
                easing.type: Easing.InOutSine
            }
        }
    }
}
