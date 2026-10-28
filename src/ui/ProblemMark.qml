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
    // Whether the Status island draws `islandLabel` beside the icon, which
    // keeps the minutes left up to date while it does.
    property bool labelled: false

    signal reportRequested

    // Truthiness rather than a null check, as for the Release mark: a window
    // built without a recording has `undefined` here.
    readonly property bool running: !!root.recording && root.recording.recording
    // How long is left, for the tooltip while the reader points at the mark
    // or reaches it with the keyboard.
    readonly property string summary: root.running && root.read ? qsTr(
                                                                      "Recording a problem, %1 min left").arg(
                                                                      root.minutesLeft) : ""
    readonly property bool read: root.hot || root.activeFocus

    // The minutes left, while the island's label or the tooltip says them. A
    // minute is the finest either states, so the clock is read again as one
    // of them starts saying it and twice a minute after, and no oftener.
    property real now: Date.now()
    readonly property int minutesLeft: root.running ? Math.max(1, Math.ceil((
                                                                                root.recording.endsAt.getTime(
                                                                                    ) - root.now)
                                                                            / 60000)) : 0
    readonly property string islandLabel: root.running ? qsTr("Recording · %1 min left").arg(
                                                             root.minutesLeft) : ""

    Timer {
        interval: 30000
        repeat: true
        running: root.running && (root.labelled || root.read)
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
