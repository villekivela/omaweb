import QtQuick
import QtQuick.Layouts

// The outline footer's marks, in the order they stand from the Spaces to
// Settings: a running Problem recording, a newer release, the hosted
// extensions, and downloads. A mark that has nothing to say is hidden and the
// others close up, so adding or moving one is a change to this list alone.
RowLayout {
    id: root

    property var colors
    property string iconFontFamily
    property var problemRecording: null
    property var releaseWatch: null
    property bool privateWindow: false
    property var hostedExtensions: null
    property var downloads: null
    property bool savedFileNoticeShowing: false

    readonly property int markSize: 26
    readonly property alias downloadItem: downloadMark
    readonly property bool anyShown: problemMark.visible || releaseMark.visible
                                     || extensionMark.visible || downloadMark.visible

    signal problemReportRequested
    signal releaseNotesRequested(url notes)
    signal extensionsClicked
    signal downloadsRequested

    function extensionRect() {
        const corner = extensionMark.mapToItem(null, 0, 0);
        return Qt.rect(corner.x, corner.y, extensionMark.width, extensionMark.height);
    }

    spacing: 4

    ProblemMark {
        id: problemMark
        objectName: "problemRecordingMark"
        Layout.preferredWidth: root.markSize
        Layout.preferredHeight: root.markSize
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        recording: root.problemRecording
        onReportRequested: root.problemReportRequested()
    }

    ReleaseMark {
        id: releaseMark
        objectName: "releaseMark"
        Layout.preferredWidth: root.markSize
        Layout.preferredHeight: root.markSize
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        watch: root.releaseWatch
        privateWindow: root.privateWindow
        onNotesRequested: function (notes) {
            root.releaseNotesRequested(notes);
        }
    }

    ExtensionMark {
        id: extensionMark
        objectName: "extensionMark"
        Layout.preferredWidth: root.markSize
        Layout.preferredHeight: root.markSize
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        hosted: root.hostedExtensions
        onClicked: root.extensionsClicked()
    }

    DownloadMark {
        id: downloadMark
        objectName: "downloadMark"
        Layout.preferredWidth: root.markSize
        Layout.preferredHeight: root.markSize
        colors: root.colors
        iconFontFamily: root.iconFontFamily
        model: root.downloads
        savedFileNoticeShowing: root.savedFileNoticeShowing
        onClicked: root.downloadsRequested()
    }
}
