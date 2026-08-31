import QtQuick
import qs.Ui as Omarchy

// One button shape for the whole shell, now drawn by the Omarchy kit's
// `Button` (third_party/omarchy-shell). This file is the adapter: call sites
// keep Tanto's vocabulary — a palette and a typography block passed down, a
// `primary` answer, a `destructive` one — and Tanto keeps the accessibility
// annotations the kit does not carry.
//
// Emphasis follows the kit rather than Tanto's old filled rectangle: the
// primary answer is bold, tinted, and bordered in the accent instead of being
// a solid accent block. Colors stay per-instance so ThemeController remains
// the single source of truth for the palette.
Omarchy.Button {
    id: root

    property var colors
    property var typography
    property string label: ""
    property bool primary: false
    property bool destructive: false
    property string accessibleName: label

    readonly property color edge: destructive ? colors.privateAccent : colors.accent

    text: label
    focusable: true
    bordered: true
    selected: primary
    foreground: colors.text
    accent: edge
    fontFamily: typography.family
    fontSize: typography.size
    opacity: enabled ? 1.0 : 0.4

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.onPressAction: root.clicked()
}
