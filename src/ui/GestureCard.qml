import QtQuick
import qs.Commons

// One touchpad swipe in Settings: its drawing, what it is, where the fingers
// go, and the action it runs (#933). The card shows the keyboard's focus on
// its picker, or the pointer over it, with its border in the accent, and the
// hand in the drawing moves then.
FocusScope {
    id: root

    property var colors
    // The name Settings and settings.json know the swipe by.
    property string gesture: ""
    property int fingers: 2
    property string title: ""
    property string note: ""
    property string action: ""
    property var options: []
    // The tallest card's height in the row, so cards side by side end level
    // with their pickers in line when one note wraps and the other does not.
    property real rowHeight: 0
    signal chosen(string action)

    objectName: "gestureCard-" + gesture
    implicitHeight: column.implicitHeight + Style.spacing.md + picker.height + Style.spacing.huge
                    * 2
    height: Math.max(implicitHeight, rowHeight)

    readonly property color fill: Qt.tint(root.colors.sheetOpaque, Qt.rgba(root.colors.text.r,
                                                                           root.colors.text.g,
                                                                           root.colors.text.b,
                                                                           0.04))

    HoverHandler {
        id: hover
    }

    Rectangle {
        anchors.fill: parent
        radius: Style.cornerRadius
        color: root.fill
        border.width: Style.spacing.hairline
        border.color: root.colors.separator
    }

    Rectangle {
        objectName: "gestureFocusRing"
        anchors.fill: parent
        visible: root.activeFocus
        radius: Style.cornerRadius
        color: "transparent"
        border.width: Style.spacing.hairline * 2
        border.color: root.colors.accent
    }

    Column {
        id: column
        x: Style.spacing.huge
        y: Style.spacing.huge
        width: root.width - Style.spacing.huge * 2
        spacing: Style.spacing.md

        GestureDrawing {
            objectName: "gestureDrawing"
            width: parent.width
            height: implicitHeight
            colors: root.colors
            fingers: root.fingers
            fill: root.fill
            active: hover.hovered || root.activeFocus
            Accessible.ignored: true
        }

        Text {
            objectName: "gestureTitle"
            width: parent.width
            text: root.title
            color: root.colors.text
            font.family: Style.font.family
            font.pixelSize: Style.font.body
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            text: root.note
            color: root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.bodySmall
            wrapMode: Text.WordWrap
        }
    }

    SettingDropdown {
        id: picker
        objectName: "gestureAction-" + root.gesture
        x: column.x
        y: root.height - Style.spacing.huge - height
        focus: true
        enabled: root.enabled
        width: column.width
        colors: root.colors
        options: root.options
        value: root.action
        accessibleName: root.title
        onChanged: function (action) {
            root.chosen(action);
        }
    }
}
