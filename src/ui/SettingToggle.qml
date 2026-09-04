import QtQuick
import qs.Ui as Omarchy

// A setting that is simply on or off: what it does on the left, what it costs
// underneath, the switch on the right. Drawn by the Omarchy kit's `Toggle`
// (third_party/omarchy-shell), which owns both the switch and the click.
//
// This file is the adapter: call sites keep Omaweb's `title` / `note` /
// `accessibleName` vocabulary and its palette, and Omaweb keeps the
// accessibility annotations the kit does not carry.
//
// The role is `Switch`, not `CheckBox`: a switch says the browser changed when
// it was flipped, and `SettingCheckbox` is what an argument to a pending action
// takes instead (ADR 0031). Qt's QML `Accessible` attached type does expose
// `Switch`, which `tst_omarchy_kit.qml` pins by its QAccessible value.
//
// The kit is stateless about the value — it reports the click and leaves
// `checked` bound to the model — so a call site flips the setting itself
// rather than reading a control that already flipped. Its switch does not fill
// its track with the accent when checked; that is the kit's shape, kept.
Omarchy.Toggle {
    id: root

    property var colors
    property string title: ""
    property string note: ""
    property string accessibleName: title

    label: title
    description: note
    foreground: colors.text
    accent: colors.accent

    Accessible.role: Accessible.Switch
    Accessible.name: accessibleName
    Accessible.checked: checked
    Accessible.onPressAction: root.clicked()
    Accessible.onToggleAction: root.clicked()
}
