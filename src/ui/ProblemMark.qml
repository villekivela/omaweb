import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// The Status island's mark of a running Problem recording (ADR 0068). It is
// there while the recording runs and gone the rest of the time, in the
// theme's urgent colour and held still: a recording is a state the reader
// chose, not news, and a pulse would draw frames for ten minutes.
//
// Selecting it opens the report, which ends the recording and takes it in.
ChromeButton {
    id: root

    property var colors
    property string iconFontFamily
    property var recording: null
    // Whether the status island draws `label` beside the icon, which keeps
    // the minutes left up to date while it does.
    property bool labelled: false

    signal reportRequested

    // Truthiness rather than a null check, as for the Release mark: a window
    // built without a recording has `undefined` here.
    readonly property bool running: !!root.recording && root.recording.recording
    // How long is left, worked out again whenever the reader points at the
    // mark or reaches it with the keyboard, which is when the tooltip shows it.
    readonly property string summary: {
        if (!root.running || !(root.hot || root.activeFocus))
            return "";
        const left = Math.max(1, Math.ceil((root.recording.endsAt.getTime() - Date.now()) / 60000));
        return qsTr("Recording a problem, %1 min left").arg(left);
    }

    // The minutes left, as the island's label says them. A minute is the
    // finest it states, so it is asked again twice a minute and no oftener.
    property real now: Date.now()
    readonly property int minutesLeft: root.running ? Math.max(1, Math.ceil((
                                                                                root.recording.endsAt.getTime(
                                                                                    ) - root.now)
                                                                            / 60000)) : 0
    readonly property string label: root.running ? qsTr("Recording · %1 min left").arg(
                                                       root.minutesLeft) : ""

    Timer {
        interval: 30000
        repeat: true
        running: root.running && root.labelled
        triggeredOnStart: true
        onTriggered: root.now = Date.now()
    }

    visible: root.running
    icon: "radio_button_checked"
    foreground: root.colors.urgent
    accent: root.colors.urgent
    fontFamily: root.iconFontFamily
    focusable: root.visible
    accessibleName: root.running ? qsTr("Recording a problem") : ""
    Accessible.description: root.running ? qsTr(
                                               "Select to end the recording and report the problem") :
                                           ""

    onClicked: {
        if (root.running)
            root.reportRequested();
    }

    // In the face the rest of the chrome reads in, not the icon face the
    // button draws its glyph in, as for the Release mark.
    Omarchy.PanelToolTip {
        objectName: "problemRecordingMarkToolTip"
        visible: root.running && (root.hot || root.activeFocus)
        text: root.summary
        fontFamily: Style.font.family
    }
}
