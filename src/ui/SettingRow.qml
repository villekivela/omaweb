import QtQuick
import qs.Commons

// One setting: what it does on the left, what it costs underneath, whatever
// control the setting needs on the right. The dialog it replaces had no room
// for the second line, which is why it never said which settings reach the
// network.
//
// A setting that is simply on or off is the Omarchy kit's `Toggle` instead —
// it owns its own switch and its own click. This row stays Omaweb's because the
// kit has no row that carries arbitrary content: the surfaces left here hold a
// status line, a button, or nothing at all. It reads the kit's type scale and
// spacing tokens so it sits in the same pane as a `Toggle` without looking
// like a second design.
Item {
    id: root

    property var colors
    property string title: ""
    property string note: ""
    property bool separated: true

    default property alias control: holder.data

    readonly property int verticalPadding: Style.spacing.huge

    implicitHeight: verticalPadding + titleText.implicitHeight
        + (note.length > 0 ? Style.spacing.md + noteText.implicitHeight : 0)
        + verticalPadding
    height: implicitHeight

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Style.spacing.hairline
        visible: root.separated
        color: root.colors.border
        opacity: 0.5
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.right: holder.left
        anchors.rightMargin: Style.spacing.rowPaddingX * 2
        anchors.top: parent.top
        anchors.topMargin: root.verticalPadding
        text: root.title
        color: root.colors.text
        wrapMode: Text.WordWrap
        font.family: Style.font.family
        font.pixelSize: Style.font.title
    }

    Text {
        id: noteText
        anchors.left: parent.left
        anchors.right: holder.left
        anchors.rightMargin: Style.spacing.rowPaddingX * 2
        anchors.top: titleText.bottom
        anchors.topMargin: Style.spacing.md
        visible: root.note.length > 0
        text: root.note
        color: root.colors.mutedText
        wrapMode: Text.WordWrap
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
    }

    Item {
        id: holder
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: root.verticalPadding
        width: childrenRect.width
        height: childrenRect.height
    }
}
