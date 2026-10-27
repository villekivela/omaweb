import QtQuick
import qs.Ui as Omarchy

// One button shape for the whole shell, drawn by the Omarchy kit's `Button`
// (third_party/omarchy-shell). This file is the adapter: call sites keep
// Omaweb's vocabulary — a palette passed down, a `primary` answer, a
// `destructive` one — and Omaweb keeps the accessibility annotations the kit
// does not carry.
//
// Emphasis follows the kit rather than Omaweb's old filled rectangle: the
// primary answer is bold, tinted, and bordered in the accent instead of being
// a solid accent block. Colors stay per-instance so ThemeController remains
// the single source of truth for the palette; type comes from the kit's
// `Style.font` scale, which every kit component already defaults to.
Omarchy.Button {
    id: root

    property var colors
    property string label: ""
    property bool primary: false
    property bool destructive: false
    property string accessibleName: label

    readonly property color edge: destructive ? colors.privateAccent : colors.accent

    // The kit sizes a button by its text, to a fraction of a pixel. A button on
    // a fractional position is snapped when it is drawn, which can push its
    // border past a clip at its edge, as at the right of a Settings row.
    //
    // The width is the text's own size, measured apart from the button. The
    // kit fits its label to the button's width, so `implicitWidth` shrinks
    // with a width taken from it, and a row of buttons laid out meanwhile
    // keeps the shrunk size and draws them over one another (#762). A button
    // here has no icon, so the kit's room for one is left out.
    width: Math.ceil(labelMetrics.advanceWidth + horizontalPadding * 2 + _reservedBorderLeft
                     + _reservedBorderRight)
    text: label
    focusable: true
    bordered: true
    selected: primary
    foreground: colors.text
    accent: edge
    opacity: enabled ? 1.0 : 0.4

    TextMetrics {
        id: labelMetrics

        text: root.label
        font.family: root.fontFamily
        font.pixelSize: root.fontSize
        font.bold: root.selected
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.onPressAction: root.clicked()
}
