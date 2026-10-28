import QtQuick
import qs.Commons
import "DevicePixels.mjs" as DevicePixels

// The status island: a capsule above the footer's Space row that hugs the
// marks of what is happening now. Left to right they are a running Problem
// recording, downloads and a newer release, each only while it applies, so
// the island is there only while one of them is (#838). One mark wears a
// short live label beside its icon; several are icons alone.
//
// Pressing downloads or the release opens the capsule in place, upward, into
// that mark's detail, and pressing it again or Escape closes it. The detail
// keeps its content until the capsule has shrunk back, so nothing empties
// while it is still drawn. The recording's mark ends the recording and opens
// the report, as it always has: there is nothing more to show of it.
Rectangle {
    id: root
    objectName: "statusIsland"

    property var colors
    property string iconFontFamily
    property var problemRecording: null
    property var releaseWatch: null
    property bool privateWindow: false
    property var downloads: null
    property bool savedFileNoticeShowing: false
    // Whether anything may move: off under reduced motion, where the width,
    // the opening and the rise each change in one step.
    property bool motion: true
    // The widest the capsule may be, which it takes while open.
    property real room: 200
    property real pixelRatio: 1

    // The mark the capsule is open on, "downloads" or "release", or empty.
    property string expanded: ""
    // What the detail shows. It follows `expanded` as the capsule opens and
    // empties only once the capsule has shrunk back.
    property string detail: ""

    readonly property int markSize: 26
    readonly property int closedHeight: 30
    readonly property int count: (problemMark.running ? 1 : 0) + (downloadMark.applies ? 1 : 0) + (
                                     releaseMark.announcing ? 1 : 0)
    readonly property bool anyShown: root.count > 0
    readonly property bool single: root.count === 1
    readonly property bool open: root.expanded.length > 0
    readonly property alias downloadItem: downloadMark

    // What the release detail says, held past the dismissal its own button
    // makes, so the detail does not empty while the capsule shrinks.
    property string releaseHeadline: ""
    property string releaseInstruction: ""
    property url releaseNotes: ""

    signal problemReportRequested
    signal releaseNotesRequested(url notes)
    signal downloadsRequested

    function toggle(key) {
        root.expanded = root.expanded === key ? "" : key;
    }

    function close() {
        const opener = root.expanded === "downloads" ? downloadMark : root.expanded === "release" ? releaseMark :
                                                                                                    null;
        root.expanded = "";
        if (opener && opener.visible)
            opener.forceActiveFocus();
    }

    // Read from `expanded` itself: `open` is not yet brought up to date when
    // a handler of a change to `expanded` runs.
    function settleDetail() {
        if (root.expanded.length === 0 && root.height <= root.closedHeight)
            root.detail = "";
    }

    onExpandedChanged: {
        if (root.expanded.length > 0) {
            root.detail = root.expanded;
            root.forceActiveFocus();
        }
        root.settleDetail();
    }
    onHeightChanged: root.settleDetail()

    Binding {
        target: root
        property: "releaseHeadline"
        when: releaseMark.announcing
        value: releaseMark.announcing ? qsTr("Omaweb %1 is out").arg(root.releaseWatch.release) : ""
        restoreMode: Binding.RestoreNone
    }
    Binding {
        target: root
        property: "releaseInstruction"
        when: releaseMark.announcing
        value: releaseMark.announcing ? root.releaseWatch.instruction : ""
        restoreMode: Binding.RestoreNone
    }
    Binding {
        target: root
        property: "releaseNotes"
        when: releaseMark.announcing
        value: releaseMark.announcing ? root.releaseWatch.notes : ""
        restoreMode: Binding.RestoreNone
    }

    // A mark that goes while the capsule is open on it closes it.
    Connections {
        target: downloadMark
        function onAppliesChanged() {
            if (!downloadMark.applies && root.expanded === "downloads")
                root.expanded = "";
        }
    }
    Connections {
        target: releaseMark
        function onAnnouncingChanged() {
            if (!releaseMark.announcing && root.expanded === "release")
                root.expanded = "";
        }
    }

    // Summed here rather than read from the row, which places its marks only
    // when it next polishes: the capsule's width follows a mark at once.
    readonly property real marksWidth: (problemMark.running ? problemMark.width : 0) + (
                                           downloadMark.applies ? downloadMark.width : 0) + (
                                           releaseMark.announcing ? releaseMark.width : 0)
                                       + Math.max(0, root.count - 1) * marks.spacing

    width: root.open ? root.room : Math.min(root.room, Math.max(root.closedHeight, root.marksWidth
                                                                + 4))
    height: root.open ? root.closedHeight + detailBody.height + 16 : root.closedHeight
    radius: root.open ? 12 : root.closedHeight / 2
    clip: true
    color: root.colors.overlay
    border.width: 1
    border.color: root.colors.separator
    visible: lift.showing
    opacity: lift.progress
    transform: SheetLift {
        id: lift
        shown: root.anyShown
        ease: root.motion
        distance: 8
    }

    Keys.onEscapePressed: function (event) {
        if (!root.open) {
            event.accepted = false;
            return;
        }
        root.close();
        event.accepted = true;
    }

    Behavior on width {
        enabled: root.motion
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
    Behavior on height {
        enabled: root.motion
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }
    Behavior on radius {
        enabled: root.motion
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    // Above the marks, which stay where they were as the capsule opens.
    Item {
        id: detailBody
        objectName: "statusIslandDetail"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 10
        height: root.detail === "downloads" ? downloadDetail.height : root.detail === "release" ? releaseDetail.height :
                                                                                                  0
        visible: root.detail.length > 0

        Column {
            id: downloadDetail
            objectName: "downloadDetail"
            width: parent.width
            visible: root.detail === "downloads"
            spacing: 4

            Repeater {
                // The whole list, settled rows and all, but only while the
                // detail shows it: a Space's Download records are in here too
                // and none of them is a running download.
                model: root.detail === "downloads" ? root.downloads : null

                Item {
                    id: detailLine
                    required property int index
                    required property string fileName
                    required property real fraction
                    required property bool running

                    objectName: "downloadDetail-" + index
                    readonly property string name: detailLine.fileName
                    readonly property string progressLabel: downloadMark.progressLabelFor(
                                                                detailLine.fraction)
                    // A recorded download is in the same list; the mark
                    // reports what is running, so a settled row takes no room.
                    visible: detailLine.running
                    width: downloadDetail.width
                    height: detailLine.running ? lineName.implicitHeight : 0

                    Text {
                        id: lineProgress
                        anchors.right: parent.right
                        anchors.baseline: lineName.baseline
                        text: detailLine.progressLabel
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.ignored: true
                    }

                    Text {
                        id: lineName
                        anchors.left: parent.left
                        anchors.right: lineProgress.left
                        anchors.rightMargin: 8
                        anchors.top: parent.top
                        text: detailLine.name
                        elide: Text.ElideMiddle
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        Accessible.role: Accessible.StaticText
                        Accessible.name: detailLine.name + " · " + detailLine.progressLabel
                    }
                }
            }

            ChromeButton {
                objectName: "allDownloadsButton"
                width: parent.width
                height: 26
                label: qsTr("All downloads")
                accessibleName: qsTr("All downloads")
                accent: root.colors.accent
                onClicked: {
                    root.expanded = "";
                    root.downloadsRequested();
                }
            }
        }

        Column {
            id: releaseDetail
            objectName: "releaseDetail"
            width: parent.width
            visible: root.detail === "release"
            spacing: 4

            Text {
                objectName: "releaseDetailHeadline"
                width: parent.width
                text: root.releaseHeadline
                wrapMode: Text.Wrap
                color: root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Text {
                objectName: "releaseDetailInstruction"
                width: parent.width
                visible: root.releaseInstruction.length > 0
                text: root.releaseInstruction
                wrapMode: Text.Wrap
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            // Reading the notes is what tells the reader, so the mark goes
            // with it. Nothing is installed by the browser either way.
            ChromeButton {
                objectName: "releaseNotesButton"
                width: parent.width
                height: 26
                label: qsTr("Release notes")
                accessibleName: qsTr("Release notes")
                accent: root.colors.accent
                onClicked: {
                    const notes = root.releaseNotes;
                    root.expanded = "";
                    root.releaseNotesRequested(notes);
                    if (releaseMark.announcing)
                        root.releaseWatch.dismiss();
                }
            }
        }
    }

    Row {
        id: marks
        objectName: "statusIslandMarks"
        x: DevicePixels.snap((root.width - root.marksWidth) / 2, root.pixelRatio)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        height: root.markSize
        spacing: 2

        ProblemMark {
            id: problemMark
            objectName: "problemRecordingMark"
            width: root.single ? root.labelledWidth(recordingLabel) : root.markSize
            height: root.markSize
            leftAlign: root.single
            horizontalPadding: 4
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            recording: root.problemRecording
            labelled: root.single
            onReportRequested: root.problemReportRequested()

            MarkLabel {
                id: recordingLabel
                text: problemMark.label
                color: root.colors.urgent
            }
        }

        DownloadMark {
            id: downloadMark
            objectName: "downloadMark"
            width: root.single ? root.labelledWidth(downloadLabel) : root.markSize
            height: root.markSize
            leftAlign: root.single
            horizontalPadding: 4
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            model: root.downloads
            savedFileNoticeShowing: root.savedFileNoticeShowing
            labelled: root.single
            onClicked: root.toggle("downloads")

            MarkLabel {
                id: downloadLabel
                text: downloadMark.summary
            }
        }

        ReleaseMark {
            id: releaseMark
            objectName: "releaseMark"
            width: root.single ? root.labelledWidth(releaseLabel) : root.markSize
            height: root.markSize
            leftAlign: root.single
            horizontalPadding: 4
            colors: root.colors
            iconFontFamily: root.iconFontFamily
            watch: root.releaseWatch
            privateWindow: root.privateWindow
            onClicked: root.toggle("release")

            MarkLabel {
                id: releaseLabel
                text: releaseMark.label
            }
        }
    }

    // A mark's icon keeps the left of its slot and the label stands after it.
    readonly property int labelInset: 4 + Style.font.iconLarge + 6

    // Measured apart from the label, whose own width follows the mark's.
    function labelledWidth(label) {
        return root.labelInset + Math.min(label.naturalWidth, root.room - root.labelInset - 16) + 8;
    }

    component MarkLabel: Text {
        id: markLabel
        readonly property real naturalWidth: metrics.advanceWidth

        objectName: parent.objectName + "Label"
        anchors.left: parent.left
        anchors.leftMargin: root.labelInset
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        visible: root.single
        elide: Text.ElideRight
        color: root.colors.text
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        Accessible.ignored: true

        TextMetrics {
            id: metrics
            font: markLabel.font
            text: markLabel.text
        }
    }
}
