import QtQuick

// A setting is on or off, and the control says which at a glance from across
// the page. It carries the checked/click contract Qt Quick Controls uses, so a
// caller flips the model in onClicked exactly as it would with a CheckBox.
Item {
    id: root

    property var colors
    property bool checked: false
    property string accessibleName: ""

    signal clicked()

    function click() {
        root.checked = !root.checked
        root.clicked()
    }

    implicitWidth: 38
    implicitHeight: 20
    activeFocusOnTab: true
    Accessible.role: Accessible.CheckBox
    Accessible.name: accessibleName
    Accessible.checked: checked
    Accessible.onPressAction: root.click()
    Accessible.onToggleAction: root.click()

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.click()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? root.colors.accent : root.colors.surface
        border.width: 1
        border.color: root.checked
            ? root.colors.accent
            : (root.activeFocus ? root.colors.accent : root.colors.border)

        Rectangle {
            width: parent.height - 6
            height: width
            radius: width / 2
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: root.checked ? root.colors.windowOpaque : root.colors.mutedText

            Behavior on x {
                NumberAnimation { duration: 90; easing.type: Easing.OutCubic }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus()
            root.click()
        }
    }
}
