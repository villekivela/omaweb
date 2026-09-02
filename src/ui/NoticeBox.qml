import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// Something the reader has to know about, stated where they can act on it. The
// Omarchy kit has no notice of its own — `urgent` is a colour role its buttons
// and its confirm dialog reach for, not a surface — so this is Omaweb's, built
// out of the kit's `BorderSurface` and its border specs rather than a variant
// forked from a component that does something else.
//
// It is a statement, not a question: a question over the page is
// `PageQuestionBar`, which has actions and takes the keyboard. This one waits
// in the settings it is about for as long as it is true.
Omarchy.BorderSurface {
    id: root

    property var colors
    property string iconFontFamily
    property string glyph: "warning"
    property string title: ""
    property string detail: ""

    readonly property color urgent: colors.urgent

    implicitHeight: contentTopInset + body.implicitHeight + contentBottomInset
    height: implicitHeight
    radius: Style.cornerRadius
    padding: Style.spacing.huge
    // The wash carries the colour across the whole surface; the border is what
    // makes it read as a notice rather than a tinted panel.
    color: Qt.rgba(urgent.r, urgent.g, urgent.b, 0.12)
    // "normal" rather than "selected": the kit gives a selected surface no
    // border at all by default, and the border is the whole point here. The
    // theme's own border alpha governs how hard it reads.
    borderSpec: Border.controlSpec("normal", root.urgent, root.urgent)
    Accessible.role: Accessible.AlertMessage
    Accessible.name: title + ". " + detail

    Text {
        id: mark
        anchors.left: parent.left
        anchors.leftMargin: root.contentLeftInset
        anchors.top: parent.top
        anchors.topMargin: root.contentTopInset
        text: root.glyph
        color: root.urgent
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.iconLarge
        Accessible.ignored: true
    }

    Column {
        id: body
        anchors.left: mark.right
        anchors.leftMargin: Style.spacing.lg
        anchors.right: parent.right
        anchors.rightMargin: root.contentRightInset
        anchors.top: parent.top
        anchors.topMargin: root.contentTopInset
        spacing: Style.spacing.md

        Text {
            width: parent.width
            text: root.title
            color: root.colors.text
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.title
        }

        Text {
            width: parent.width
            visible: root.detail.length > 0
            text: root.detail
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }
    }
}
