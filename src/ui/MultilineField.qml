import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Ui as Omarchy

// The multi-line counterpart to `SettingField`, for the user-rules editor.
//
// The Omarchy kit's `TextField` is single-line and the kit has no text area,
// so this is Omaweb's own control rather than an adapter. It is built from the
// kit's tokens — the same fill, border spec, corner radius, and type scale
// `TextField` reads — so it sits beside a kit field without looking like a
// second design, and nothing under third_party is forked to get there.
TextArea {
    id: root

    property var colors
    property string placeholder: ""
    property string accessibleName: ""

    readonly property color foreground: colors.text
    readonly property color accent: colors.accent
    readonly property bool hot: hovered
    readonly property var edge: Border.controlSpec(activeFocus ? "focus" : (hot ? "hover-cursor" :
                                                                                  "normal"),
                                                   foreground, accent)

    // A rules editor is worth nothing at one line high. Five is enough to see a
    // rule in the company of its neighbours, and it is derived rather than
    // pinned so it follows the theme's type scale like everything else here.
    implicitHeight: topPadding + bottomPadding + Math.round(Style.font.body * 1.4) * 5

    placeholderText: placeholder
    font.family: Style.font.family
    font.pixelSize: Style.font.body
    color: foreground
    selectionColor: Style.selectionFillFor(foreground, accent)
    selectedTextColor: foreground
    placeholderTextColor: Qt.darker(foreground, 1.6)
    wrapMode: TextArea.NoWrap

    leftPadding: Style.spacing.controlPaddingX + Border.left(edge)
    rightPadding: Style.spacing.controlPaddingX + Border.right(edge)
    topPadding: Style.spacing.inputPaddingY + Border.top(edge)
    bottomPadding: Style.spacing.inputPaddingY + Border.bottom(edge)

    Accessible.role: Accessible.EditableText
    Accessible.name: accessibleName

    background: Omarchy.BorderSurface {
        color: Style.controlFill(root.activeFocus, root.hot, root.foreground, root.accent)
        borderSpec: root.edge
        radius: Style.cornerRadius
    }
}
