import QtQuick
import qs.Commons

ChromeButton {
    id: root

    property var colors
    property string iconFontFamily
    // The window's downloads. The mark reports the Download activity the list
    // publishes rather than a summary somebody else keeps up to date.
    property var model: null
    // The glossary's rule: after the last download finishes the mark remains
    // until the saved-file notice closes. Following the notice itself is what
    // makes that hold, rather than a duration copied from it.
    property bool savedFileNoticeShowing: false

    readonly property int running: root.model ? root.model.running : 0
    readonly property real fraction: root.model ? root.model.fraction : -1
    readonly property int finished: root.model ? root.model.finished : 0

    readonly property bool measured: root.fraction >= 0
    readonly property bool holding: root.running === 0 && root.finished > 0
                                    && root.savedFileNoticeShowing

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
        return fraction >= 0 ? Math.round(fraction * 100) + "%" : "size unknown";
    }

    function fileSummary(rows) {
        const names = [];
        for (let index = 0; index < rows.length; ++index)
            names.push(rows[index].fileName + " · " + root.progressLabelFor(rows[index].fraction));
        return names.join(", ");
    }

    visible: root.running > 0 || root.holding
    icon: root.holding ? "download_done" : "download"
    foreground: root.colors.text
    accent: root.colors.accent
    fontFamily: root.iconFontFamily
    focusable: root.visible
    accessibleName: root.summary
    // The list answers for its running rows rather than keeping a list of them,
    // and this binding only asks while the mark is being read: until then the
    // condition is false and nothing depends on the answer.
    Accessible.description: root.detailRequested && root.model ? root.fileSummary(
                                                                     root.model.runningDownloads) :
                                                                 ""

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
