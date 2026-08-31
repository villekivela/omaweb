import QtQuick
import qs.Ui as Omarchy

// The small chrome control — the arrows in the navigation strip, the Space
// letters in the sidebar, the settings and command-panel buttons — drawn by
// the Omarchy kit's `Button` (third_party/omarchy-shell). This file is the
// adapter: call sites keep Tanto's vocabulary — a `label` for a word, an
// `icon` for a glyph — and Tanto keeps the accessibility annotations the kit
// does not carry.
//
// The two slots are separate because the kit sizes them separately: a glyph
// takes `Style.font.icon` and a word `Style.font.body`, where Tanto's own
// button gave both one hard-coded 15px.
//
// The kit paints hover, focus and pressed fills itself from `foreground` and
// `accent`, so Tanto's `hoverBackground` is gone: chrome is borderless and
// transparent at rest and tints under the cursor like the rest of the kit.
Omarchy.Button {
    id: root

    property string label: ""
    property string icon: ""
    property string accessibleName: ""

    text: label
    iconText: icon
    // A disabled control is not a keyboard stop, which is what Tanto's own
    // `activeFocusOnTab: enabled` said before the kit took the painting over.
    focusable: enabled
    bordered: false
    opacity: enabled ? 1.0 : 0.35

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName
    Accessible.onPressAction: root.clicked()
}
