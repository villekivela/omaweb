import QtQuick
import qs.Commons

ChromeButton {
    id: root

    property var colors
    property string iconFontFamily
    property var activity: null
    property int dwellMilliseconds: 4200

    readonly property int running: root.activity ? root.activity.running : 0
    readonly property real fraction: root.activity ? root.activity.fraction : -1
    readonly property int finished: root.activity ? root.activity.finished : 0
    readonly property var downloads: root.activity ? root.activity.downloads : []

    readonly property bool measured: root.fraction >= 0
    readonly property bool holding: root.running === 0 && dwell.running
    readonly property bool detailRequested: root.visible && (root.hot || root.activeFocus)

    readonly property string summary: root.holding ? (root.finished === 1 ? "1 download finished" : String(
                                                                                root.finished)
                                                                            + " downloads finished") :
                                                     (root.running === 1 ? "1 download · "
                                                                           + root.progressLabel :
                                                                           String(root.running)
                                                                           + " downloads · "
                                                                           + root.progressLabel)
    readonly property string progressLabel: root.progressLabelFor(root.fraction)

    function progressLabelFor(fraction) {
        return fraction >= 0 ? Math.round(fraction * 100) + "%" : "size unknown"
    }

    function fileSummary() {
        const names = []
        for (let index = 0; index < root.downloads.length; ++index) {
            names.push(root.downloads[index].name + " · " + root.progressLabelFor(
                           root.downloads[index].fraction))
        }
        return names.join(", ")
    }

    visible: root.running > 0 || root.holding
    icon: root.holding ? "download_done" : "download"
    foreground: root.colors.text
    accent: root.colors.accent
    fontFamily: root.iconFontFamily
    focusable: root.visible
    accessibleName: root.summary
    Accessible.description: root.detailRequested ? root.fileSummary() : ""

    onRunningChanged: {
        if (root.running > 0)
            dwell.stop()
        else if (root.finished > 0)
            dwell.restart()
    }

    onFinishedChanged: {
        if (root.running === 0 && root.finished > 0)
            dwell.restart()
    }

    Timer {
        id: dwell
        interval: root.dwellMilliseconds
    }

    Text {
        objectName: "downloadMarkCount"
        anchors.right: parent.right
        anchors.top: parent.top
        visible: root.running > 1
        text: String(root.running)
        color: root.colors.text
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        Accessible.ignored: true
    }

    Rectangle {
        id: track
        objectName: "downloadMarkTrack"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 3
        radius: 1
        color: root.colors.border

        Rectangle {
            objectName: "downloadMarkFill"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            radius: parent.radius
            width: root.holding ? track.width : (root.measured ? Math.round(track.width
                                                                            * root.fraction) : 0)
            color: root.colors.accent
        }
    }
}
