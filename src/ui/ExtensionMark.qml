import QtQuick
import qs.Commons

// The mark a Known extension is reached from. It is here because a
// keyboard-driven browser still has to be usable with a pointer, and because a
// panel that grows out of the thing it belongs to needs that thing to be
// somewhere: the popup lifts from this mark.
//
// One mark, however many extensions are hosted. A reader who has enabled two
// gets a menu from it rather than a row of marks, because the sidebar's bottom
// row is the browser's own and an extension does not get to grow in it.
//
// Absent rather than dead when nothing is hosted: an engine that cannot host an
// extension, a reader who has enabled none, and a Private window all leave
// nothing to open, and none of them is a state worth drawing.
ChromeButton {
    id: root

    property var colors
    property string iconFontFamily
    // What this window's Space is hosting, as `{ key, name, id, popupUrl }`.
    property var hosted: []

    readonly property bool single: root.hosted.length === 1
    readonly property string summary: root.hosted.length === 0 ? "" : (root.single
                                                                       ? root.hosted[0].name :
                                                                         root.hosted.length
                                                                         + " extensions")

    visible: root.hosted.length > 0
    icon: "extension"
    foreground: root.colors.text
    accent: root.colors.accent
    fontFamily: root.iconFontFamily
    focusable: root.visible
    accessibleName: root.summary
}
