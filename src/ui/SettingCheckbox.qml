import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// A selection: an argument to an action that has not happened yet. Ticking one
// changes nothing until the button that reads it is pressed, which is the whole
// reason it is not a `SettingToggle` — a switch promises the browser changed
// when it was flipped (ADR 0031).
//
// The kit ships no standalone checkbox, so this lifts the one
// `qs/Ui/MultiSelect.qml` draws as a popup-list delegate: a 16px `BorderSurface`
// filled with `Style.selectedFillFor`, bordered by `Border.controlSpec` and
// ticked with a check glyph. The shape is the kit's; the row around it, the
// palette and the accessibility annotations are Omaweb's.
//
// Stateless about the value, as the kit's `Toggle` is: it reports the click and
// leaves `checked` bound to the model. `Space` toggles and `Enter` does not,
// because a form of these sits in a dialog whose `Enter` confirms it.
Item {
    id: root

    property var colors
    property string title: ""
    property string note: ""
    property bool checked: false
    property string accessibleName: title

    readonly property color foreground: colors.text
    readonly property color accent: colors.accent

    signal clicked

    activeFocusOnTab: true
    Keys.onSpacePressed: root.clicked()

    implicitWidth: box.width + row.spacing + labels.implicitWidth
    implicitHeight: Math.max(box.height, labels.implicitHeight)

    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName
    Accessible.checked: checked
    Accessible.onPressAction: root.clicked()
    Accessible.onToggleAction: root.clicked()

    Row {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Style.spacing.rowGap

        Omarchy.BorderSurface {
            id: box
            objectName: "settingCheckboxBox"
            width: Style.space(16)
            height: Style.space(16)
            radius: Math.max(2, Style.cornerRadius / 2)
            anchors.verticalCenter: parent.verticalCenter
            color: root.checked ? Style.selectedFillFor(root.foreground, root.accent) :
                                  "transparent"
            // Focus is the box's own border rather than a ring around the row:
            // the row is a line of text, and a ring around text reads as a
            // second control.
            borderSpec: Border.controlSpec(root.activeFocus ? "focus" : (root.checked ? "selected" :
                                                                                        "normal"),
                                           root.foreground, root.accent)

            Text {
                objectName: "settingCheckboxTick"
                anchors.centerIn: parent
                visible: root.checked
                text: "✓"
                color: Style.selectedStateColor(root.foreground, root.accent)
                font.family: Style.font.family
                font.pixelSize: Math.round(box.height * 0.85)
                font.bold: true
            }
        }

        Column {
            id: labels
            width: parent.width - box.width - parent.spacing
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.spacing.xs

            Text {
                textFormat: Text.PlainText
                width: parent.width
                text: root.title
                color: root.foreground
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Text {
                textFormat: Text.PlainText
                width: parent.width
                visible: root.note.length > 0
                text: root.note
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.clicked()
        }
    }
}
