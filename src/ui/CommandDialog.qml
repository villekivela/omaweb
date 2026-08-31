import QtQuick

// Tanto already has one place where it asks the user something: the Omnibar.
// Every question about the browser itself arrives in the same shape — centred,
// scrimmed, one accent-bordered panel — so the answer is always typed or picked
// in a list, and there is only ever one grammar to learn.
Item {
    id: root

    property var colors
    property var typography
    property bool open: false
    property string label: ""
    property string message: ""
    property bool destructive: false
    property bool inputVisible: false
    property string placeholder: ""
    property string presetText: ""
    property bool selectPreset: false
    property string confirmHint: ""
    property string cancelHint: "esc cancel"
    // Rows turn the panel into a chooser: the same panel, answered with the
    // arrow keys instead of the keyboard.
    property var rows: []
    property int selected: 0

    readonly property alias text: field.text
    readonly property color edge: destructive ? colors.privateAccent : colors.accent

    signal accepted(string text)
    signal rowActivated(int index)
    signal dismissed()

    visible: open
    focus: open

    onOpenChanged: {
        if (!open) return
        selected = 0
        field.text = presetText
        Qt.callLater(function() {
            if (root.inputVisible) {
                field.focusInput()
                if (root.selectPreset) field.selectAllText()
            } else {
                root.forceActiveFocus()
            }
        })
    }

    function step(delta) {
        if (rows.length === 0) return
        selected = (selected + delta + rows.length) % rows.length
    }

    function accept() {
        if (rows.length > 0) {
            root.rowActivated(root.selected)
            return
        }
        root.accepted(field.text)
    }

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.dismissed()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.accept()
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            root.step(1)
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            root.step(-1)
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#99000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        id: panel
        objectName: "commandDialogPanel"
        width: Math.min(660, parent.width - 96)
        height: head.height + body.height + foot.height
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(80, parent.height * 0.14)
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.edge
        clip: true

        Item {
            id: head
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 38

            SectionLabel {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                typography: root.typography
                text: root.label
                color: root.destructive ? root.colors.privateAccent : root.colors.mutedText
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: root.cancelHint
                color: root.colors.mutedText
                font.family: root.typography.family
                font.pixelSize: root.typography.smallSize
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.colors.border
            }
        }

        Column {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: head.bottom
            height: implicitHeight
            spacing: 0

            Item {
                width: parent.width
                height: root.inputVisible ? 52 : 0
                visible: root.inputVisible

                Text {
                    id: caret
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    text: "›"
                    color: root.edge
                    font.family: root.typography.family
                    font.pixelSize: 20
                }

                // The input is the panel's own line rather than a boxed field:
                // the panel is the box.
                TextInput {
                    id: field
                    objectName: "commandDialogInput"
                    anchors.left: caret.right
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.colors.text
                    selectionColor: root.edge
                    selectedTextColor: root.colors.windowOpaque
                    font.family: root.typography.family
                    font.pixelSize: 17
                    Accessible.role: Accessible.EditableText
                    Accessible.name: root.label

                    function focusInput() { field.forceActiveFocus() }
                    function selectAllText() { field.selectAll() }

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Escape) {
                            root.dismissed()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                            root.accept()
                            event.accepted = true
                        }
                    }
                }

                Text {
                    anchors.left: caret.right
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    visible: field.text.length === 0
                    text: root.placeholder
                    color: root.colors.mutedText
                    font.family: root.typography.family
                    font.pixelSize: 17
                }
            }

            Text {
                width: parent.width - 32
                x: 16
                topPadding: root.inputVisible ? 0 : 14
                bottomPadding: 12
                visible: root.message.length > 0
                text: root.message
                color: root.colors.text
                wrapMode: Text.WordWrap
                font.family: root.typography.family
                font.pixelSize: root.typography.size
            }

            Repeater {
                model: root.rows

                Rectangle {
                    required property var modelData
                    required property int index

                    objectName: "commandDialogRow" + index
                    width: parent.width
                    height: 40
                    color: index === root.selected ? root.colors.surface : "transparent"
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.label

                    Rectangle {
                        width: 2
                        height: parent.height
                        color: index === root.selected ? root.edge : "transparent"
                    }

                    Text {
                        id: rowLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: root.colors.text
                        font.family: root.typography.family
                        font.pixelSize: root.typography.size
                    }

                    Text {
                        anchors.left: rowLabel.right
                        anchors.leftMargin: 12
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.note !== undefined ? modelData.note : ""
                        color: root.colors.mutedText
                        elide: Text.ElideRight
                        font.family: root.typography.family
                        font.pixelSize: root.typography.smallSize
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = index
                        onClicked: root.rowActivated(index)
                    }
                }
            }
        }

        Item {
            id: foot
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: body.bottom
            height: root.confirmHint.length > 0 ? 34 : 8

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                visible: root.confirmHint.length > 0
                color: root.colors.border
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                visible: root.confirmHint.length > 0
                text: root.confirmHint
                color: root.colors.mutedText
                font.family: root.typography.family
                font.pixelSize: root.typography.smallSize
            }
        }
    }
}
