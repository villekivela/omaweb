import QtQuick
import qs.Commons
import qs.Ui as Omarchy

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
    //
    // Said in the tooltip the kit draws for any button that has one, as well as
    // to a screen reader. Omaweb's chrome draws no tooltip unless it is asked
    // to, so a mark carrying its instruction in the accessibility layer alone
    // is a mark that answers everyone but the reader looking straight at it.
    //
    // The instruction is not there for the moment between the release being
    // known and pacman saying who owns the binary, so the tooltip is the
    // version until it arrives rather than a version with a dangling separator.
    readonly property string summary: !root.announcing ? "" : (root.watch.instruction.length > 0
                                                               ? "Omaweb " + root.watch.release
                                                                 + " is out\n"
                                                                 + root.watch.instruction :
                                                                 "Omaweb " + root.watch.release
                                                                 + " is out")

    accessibleName: root.announcing ? "Omaweb " + root.watch.release + " is out" : ""
    Accessible.description: root.announcing ? root.watch.instruction : ""

    onClicked: {
        if (!root.announcing)
            return;
        root.notesRequested(root.watch.notes);
        root.watch.dismiss();
    }

    // Declared here rather than set through the kit button's own `tooltipText`.
    // That one draws in the button's `fontFamily`, which for a mark is the icon
    // face, and a sentence set in Material Symbols is not a sentence. The kit's
    // tooltip is still the one used, with the face the rest of the chrome reads
    // in.
    Omarchy.PanelToolTip {
        objectName: "releaseMarkToolTip"
        visible: root.announcing && (root.hot || root.activeFocus)
        text: root.summary
        fontFamily: Style.font.family
    }
}
