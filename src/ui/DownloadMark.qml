import QtQuick
import qs.Commons

// The outline footer's mark for downloads in flight: one aggregate for all of
// them, not one control per download. A footer has room for a control, not for
// a list, and the list already exists behind settings — what the footer owes
// the reader is that something is being fetched and roughly how far it has got.
//
// The mark is not permanent furniture. It appears when a download starts and
// leaves once the last one has ended, so the footer says nothing about
// downloads on the ordinary days when there are none.
//
// The footer is the whole of where it lives. `Primary+B` hides the sidebar and
// the floating cluster stands in for the top of it, not for the footer: the
// strip carries the commands a page needs under the pointer, and a download is
// not one of those. In that state the notices a download raises are what
// remain, and the mark is back as soon as the outline is.
//
// It is also the way in: it opens the list it is the summary of. That makes it
// an action, so the same action is in the command panel — chrome the pointer
// can reach and the keyboard cannot is a defect (ADR 0011).
//
// The whole of what it says comes off one aggregate the window derives from
// the downloads it is running. This file is where that aggregate is read, so
// the outline passes it along without unpacking it and the panel beside the
// mark reads the names back off here rather than off a second copy.
ChromeButton {
    id: root

    // The theme's colours and the icon font, passed down per instance as every
    // other piece of chrome takes them.
    property var colors
    property string iconFontFamily
    property var activity: null
    // How long the finished state stands. The window passes the span the
    // notice naming the saved file stands for, so the two stop together.
    property int dwellMilliseconds: 4200

    // How many downloads are in flight, how far through the whole of them the
    // bytes have got, and how many have finished since the mark last had
    // nothing to show. A fraction below zero is a download the server sent no
    // length for: the track is drawn empty rather than a percentage being
    // invented from the bytes that happen to have arrived.
    readonly property int running: root.activity ? root.activity.running : 0
    readonly property real fraction: root.activity ? root.activity.fraction : -1
    readonly property int finished: root.activity ? root.activity.finished : 0
    // Each download in flight as a name and a fraction of its own, which the
    // window builds only while the reader is asking for them.
    readonly property var downloads: root.activity ? root.activity.downloads : []

    readonly property bool measured: root.fraction >= 0
    // Nothing is running and the mark is standing anyway, on the downloads it
    // is still reporting as finished.
    readonly property bool holding: root.running === 0 && dwell.running
    // The reader asking which file is which: the pointer resting on the mark,
    // or the keyboard arriving at it, which is the same question asked without
    // a pointer. The panel itself belongs to the outline — this file only says
    // whether it is wanted, because the outline is what clips.
    readonly property bool detailRequested: root.visible && (root.hot || root.activeFocus)

    // One sentence for a reader who cannot see the track, saying the same thing
    // the track draws. A download with no total is named as one whose size is
    // unknown, because a percentage is exactly what there is not.
    readonly property string summary: root.holding
        ? (root.finished === 1 ? "1 download finished"
            : String(root.finished) + " downloads finished")
        : (root.running === 1 ? "1 download · " + root.progressLabel
            : String(root.running) + " downloads · " + root.progressLabel)
    readonly property string progressLabel: root.progressLabelFor(root.fraction)

    // How far one download has got, as the mark says it. The panel beside the
    // mark says it about each file with this same function: "size unknown" has
    // to mean one thing in both places, and two copies of the sentence are two
    // things that can drift apart.
    function progressLabelFor(fraction) {
        return fraction >= 0 ? Math.round(fraction * 100) + "%" : "size unknown"
    }

    // Which files, for a reader who cannot see the panel the pointer opens. The
    // names are only built while the detail is wanted, which is exactly when
    // this is worth reading out.
    function fileSummary() {
        const names = []
        for (let index = 0; index < root.downloads.length; ++index) {
            names.push(root.downloads[index].name + " · "
                + root.progressLabelFor(root.downloads[index].fraction))
        }
        return names.join(", ")
    }

    visible: root.running > 0 || root.holding
    icon: root.holding ? "download_done" : "download"
    // A download in flight is not the quiet grey of a control at rest: it is
    // the one thing in the footer that is currently happening.
    foreground: root.colors.text
    accent: root.colors.accent
    fontFamily: root.iconFontFamily
    focusable: root.visible
    accessibleName: root.summary
    Accessible.description: root.detailRequested ? root.fileSummary() : ""

    // The last download ending starts the dwell; a new one starting cuts it
    // short, because what is running now is the more useful of the two things
    // the mark could be saying. The counts arrive as two readings of one
    // aggregate, in whichever order the engine evaluates them, so the dwell is
    // started from either side: the last download's end is both of them moving.
    onRunningChanged: {
        if (root.running > 0) dwell.stop()
        else if (root.finished > 0) dwell.restart()
    }

    onFinishedChanged: {
        if (root.running === 0 && root.finished > 0) dwell.restart()
    }

    Timer {
        id: dwell
        interval: root.dwellMilliseconds
    }

    // The count says there is more than one download behind the one mark. One
    // download needs no numeral — the glyph already says that much.
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

    // The track is as wide as the mark and sits under the glyph, so how far
    // along a download is reads as a length rather than as a number to parse.
    // Both track and fill clear the WCAG AA non-text threshold on their own
    // account: the track is the same border colour the outline separates its
    // surfaces with, and the fill is the theme's accent.
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
            // A finished download is the whole track rather than the last
            // percentage the bytes happened to report before the end.
            width: root.holding ? track.width
                : (root.measured ? Math.round(track.width * root.fraction) : 0)
            color: root.colors.accent
        }
    }
}
