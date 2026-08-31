import QtQuick
import qs.Ui as Omarchy

// The small label that introduces a section — "pinned", "tabs", "add a list" —
// drawn by the Omarchy kit's `PanelSectionHeader` (third_party/omarchy-shell).
// This file is the adapter: it carries Tanto's palette in and the
// accessibility annotation the kit does not have.
//
// The kit's header is bold and set in the caption size, and it derives its own
// muting from `foreground` rather than reading a separate muted role. It does
// not letter-space or upper-case, so Tanto's labels now read as they are
// written instead of being shouted.
Omarchy.PanelSectionHeader {
    id: root

    property var colors

    foreground: colors.text
    Accessible.role: Accessible.StaticText
    Accessible.name: text
}
