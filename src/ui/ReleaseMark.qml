import QtQuick

// The outline footer control for a newer Omaweb. It appears when a release the
// reader has not seen exists, and is gone the rest of the time: this is a
// status beside the Download mark and the Sync mark, not an interruption.
//
// Omaweb never installs anything. Selecting the mark opens the release notes
// and the notice is done: a reader who has read them has been told, and the
// next release tells them again.
ChromeButton {
    id: root

    property var colors
    property string iconFontFamily
    // The application's answer about its own age. One watch answers for every
    // window, so the Private rule is applied here, where a window knows whether
    // it is one.
    property var watch: null
    property bool privateWindow: false

    // Truthiness rather than a null check: a window built without a watch, as
    // the UI lab and the tests are, has `undefined` here rather than null.
    readonly property bool announcing: !root.privateWindow && !!root.watch && root.watch.announcing

    signal notesRequested(url notes)

    visible: root.announcing
    icon: "upgrade"
    foreground: root.colors.text
    accent: root.colors.accent
    fontFamily: root.iconFontFamily
    focusable: root.visible
    // The version and what to run about it, because a reader who is told a
    // release exists and not how to get it goes looking for instructions.
    accessibleName: root.announcing ? "Omaweb " + root.watch.release + " is out" : ""
    Accessible.description: root.announcing ? root.watch.instruction : ""

    onClicked: {
        if (!root.announcing)
            return;
        root.notesRequested(root.watch.notes);
        root.watch.dismiss();
    }
}
