import QtQuick
import qs.Ui as Omarchy

// The themed text box the settings page and the dialogs share, drawn by the
// Omarchy kit's `TextField` (third_party/omarchy-shell). This file is the
// adapter: it keeps Omaweb's `placeholder` and `destructive` vocabulary, the
// `focusInput` / `selectAllText` calls the dialogs make, and the
// accessibility annotation the kit does not carry.
//
// The kit's field is single-line. A destructive field takes the private
// accent, which the kit uses for its focus fill and border the same way the
// ordinary accent is used elsewhere. `MultilineField` covers the one surface
// that needs more than a line.
Omarchy.TextField {
    id: root

    property var colors
    property string placeholder: ""
    property bool destructive: false
    property string accessibleName: ""

    function focusInput() { root.forceActiveFocus() }
    function selectAllText() { root.selectAll() }

    placeholderText: placeholder
    foreground: colors.text
    accent: destructive ? colors.privateAccent : colors.accent

    Accessible.role: Accessible.EditableText
    Accessible.name: accessibleName
}
