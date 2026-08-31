import QtQuick

// One setting: what it does on the left, what it costs underneath, the control
// on the right. The dialog it replaces had no room for the second line, which
// is why it never said which settings reach the network.
Item {
    id: root

    property var colors
    property var typography
    property string title: ""
    property string note: ""
    property bool separated: true

    default property alias control: holder.data

    implicitHeight: 18 + titleText.implicitHeight
        + (note.length > 0 ? 6 + noteText.implicitHeight : 0) + 18
    height: implicitHeight

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        visible: root.separated
        color: root.colors.border
        opacity: 0.5
    }

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.right: holder.left
        anchors.rightMargin: 24
        anchors.top: parent.top
        anchors.topMargin: 18
        text: root.title
        color: root.colors.text
        wrapMode: Text.WordWrap
        font.family: root.typography.family
        font.pixelSize: 14
    }

    Text {
        id: noteText
        anchors.left: parent.left
        anchors.right: holder.left
        anchors.rightMargin: 24
        anchors.top: titleText.bottom
        anchors.topMargin: 6
        visible: root.note.length > 0
        text: root.note
        color: root.colors.mutedText
        wrapMode: Text.WordWrap
        font.family: root.typography.family
        font.pixelSize: root.typography.smallSize
    }

    Item {
        id: holder
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 18
        width: childrenRect.width
        height: childrenRect.height
    }
}
