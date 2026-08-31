import QtQuick

// The themed text box the settings page and the dialogs share, so every place
// Tanto takes typed input looks like the address field rather than like the
// platform's own.
Rectangle {
    id: root

    property var colors
    property var typography
    property string placeholder: ""
    property alias text: input.text
    property bool multiline: false
    property bool destructive: false
    property string accessibleName: ""

    signal accepted()

    function focusInput() { input.forceActiveFocus() }
    function selectAllText() { input.selectAll() }

    implicitHeight: multiline ? 96 : 32
    radius: 2
    color: colors.surface
    border.width: 1
    border.color: input.activeFocus
        ? (destructive ? colors.privateAccent : colors.accent)
        : colors.border

    TextEdit {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 9
        anchors.rightMargin: 9
        anchors.topMargin: root.multiline ? 8 : 0
        anchors.bottomMargin: root.multiline ? 8 : 0
        verticalAlignment: root.multiline ? Text.AlignTop : Text.AlignVCenter
        color: root.colors.text
        selectionColor: root.colors.accent
        selectedTextColor: root.colors.windowOpaque
        font.family: root.typography.family
        font.pixelSize: root.typography.size
        wrapMode: root.multiline ? TextEdit.NoWrap : TextEdit.NoWrap
        clip: true
        Accessible.role: Accessible.EditableText
        Accessible.name: root.accessibleName

        Keys.onPressed: function(event) {
            if (root.multiline) return
            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                root.accepted()
                event.accepted = true
            }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 9
        anchors.top: parent.top
        anchors.topMargin: root.multiline ? 8 : 0
        anchors.bottom: root.multiline ? undefined : parent.bottom
        verticalAlignment: root.multiline ? Text.AlignTop : Text.AlignVCenter
        visible: input.text.length === 0
        text: root.placeholder
        color: root.colors.mutedText
        font.family: root.typography.family
        font.pixelSize: root.typography.size
    }
}
