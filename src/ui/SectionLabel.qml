import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// The small label that introduces a section — "pinned", "tabs", "add a list" —
// drawn by the Omarchy kit's `PanelSectionHeader` (third_party/omarchy-shell).
// This file is the adapter: it carries Omaweb's palette in and the
// accessibility annotation the kit does not have.
//
// The kit's header is bold and set in the caption size, and it derives its own
// muting from `foreground` rather than reading a separate muted role. Omaweb
// sets it larger and in capitals: a section label is what the reader navigates
// by — which of a handful of sections holds the row they came for — and at the
// caption size, in the same case as the rows under it, it reads as one more row
// rather than as the thing that divides them. `text` keeps the written form, so
// the capitals are drawn rather than spoken.
Omarchy.PanelSectionHeader {
    id: root

    property var colors

    foreground: colors.text
    fontSize: Style.font.subtitle
    font.capitalization: Font.AllUppercase
    Accessible.role: Accessible.StaticText
    Accessible.name: text
}
