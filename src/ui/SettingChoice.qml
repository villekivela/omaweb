import QtQuick
import qs.Ui as Omarchy

// Settings-facing adapter for the Omarchy kit's pick-one-of-N row, as
// SettingDropdown is for its dropdown: a setting with a few short choices shows
// them all, and the one in force reads as selected.
Omarchy.ButtonGroup {
    id: root

    property var colors
    property string accessibleName: ""

    // The kit sizes each button to its label, which leaves a border between
    // two pixels, and at the pane's right edge past its clip. Every button
    // takes the widest one's width instead, rounded up to a whole pixel, so
    // the choices also read as one control of equal parts.
    readonly property real buttonWidth: {
        let widest = 0;
        for (let index = 0; index < root.children.length; ++index) {
            const child = root.children[index];
            if (child.hasOwnProperty("bordered"))
                widest = Math.max(widest, child.implicitWidth);
        }
        return Math.ceil(widest);
    }

    foreground: colors.text
    // On the pane's own ground, as the stepper's buttons are, so only the
    // choice in force is filled.
    background: "transparent"
    accent: colors.accent

    Accessible.role: Accessible.Grouping
    Accessible.name: accessibleName

    onChildrenChanged: {
        for (let index = 0; index < root.children.length; ++index) {
            const child = root.children[index];
            if (child.hasOwnProperty("bordered"))
                child.width = Qt.binding(function () {
                    return root.buttonWidth;
                });
        }
    }
}
