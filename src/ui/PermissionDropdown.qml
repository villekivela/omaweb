import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Ui as Omarchy

// Allow, Ask or Block for one of a site's permissions, as Site information
// offers it. The kit's dropdown draws its arrow from a Nerd Font glyph that a
// machine without one shows as a box, so this is the same control with the
// icon font's own arrow: the trigger, the popup and the keys follow the kit's
// (Return, Space or Down opens; Up, Down, j and k walk; Return picks; Escape
// closes).
Omarchy.BorderSurface {
    id: root

    property var colors
    property string iconFontFamily: ""
    property string value: "ask"
    property string accessibleName: ""
    readonly property var options: [
        {
            "value": "allow",
            "label": qsTr("Allow", "a site permission")
        },
        {
            "value": "ask",
            "label": qsTr("Ask", "a site permission")
        },
        {
            "value": "block",
            "label": qsTr("Block", "a site permission")
        }
    ]
    readonly property bool popupOpen: popup.opened
    // The open popup's rows, which stand in the window's overlay rather than
    // under this item.
    readonly property Item optionItems: optionList
    readonly property bool hot: hover.hovered || root.activeFocus

    signal changed(string value)

    function labelOf(value) {
        for (const option of root.options) {
            if (option.value === value)
                return option.label;
        }
        return value;
    }

    function choose(index) {
        const option = root.options[index];
        popup.close();
        if (!option || option.value === root.value)
            return;
        root.value = option.value;
        root.changed(option.value);
    }

    implicitWidth: 108
    implicitHeight: 26
    radius: Style.cornerRadius
    color: Style.controlFill(root.activeFocus, hover.hovered, root.colors.text, root.colors.accent)
    borderSpec: Border.controlSpec(root.activeFocus ? "focus" : (hover.hovered ? "hover-cursor" :
                                                                                 "normal"),
                                   root.colors.text, root.colors.accent)
    activeFocusOnTab: true
    Accessible.role: Accessible.ComboBox
    Accessible.name: root.accessibleName
    Accessible.description: root.labelOf(root.value)

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space
                || event.key === Qt.Key_Down) {
            popup.open();
            event.accepted = true;
        }
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    Text {
        objectName: "permissionDropdownValue"
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: arrow.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.labelOf(root.value)
        color: root.colors.text
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    Text {
        id: arrow
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        text: "expand_more"
        color: root.colors.mutedText
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.icon
        Accessible.ignored: true
    }

    TapHandler {
        onTapped: {
            root.forceActiveFocus();
            popup.opened ? popup.close() : popup.open();
        }
    }

    Popup {
        id: popup
        objectName: "permissionDropdownPopup"
        y: root.height + 2
        width: root.width
        padding: 2
        focus: true

        onOpened: {
            optionList.currentIndex = Math.max(0, root.options.findIndex(function (option) {
                return option.value === root.value;
            }));
            optionList.forceActiveFocus();
        }
        onClosed: root.forceActiveFocus()

        background: Omarchy.BorderSurface {
            radius: Style.cornerRadius
            color: root.colors.overlayOpaque
            borderSpec: Border.controlSpec("normal", root.colors.text, root.colors.accent)
        }

        contentItem: Column {
            id: optionList

            property int currentIndex: 0

            focus: true
            Keys.onPressed: function (event) {
                if (event.key === Qt.Key_Escape) {
                    popup.close();
                } else if (event.key === Qt.Key_Down || event.text === "j") {
                    optionList.currentIndex = Math.min(root.options.length - 1,
                                                       optionList.currentIndex + 1);
                } else if (event.key === Qt.Key_Up || event.text === "k") {
                    optionList.currentIndex = Math.max(0, optionList.currentIndex - 1);
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.choose(optionList.currentIndex);
                } else {
                    return;
                }
                event.accepted = true;
            }

            Repeater {
                model: root.options

                Rectangle {
                    id: option

                    required property int index
                    required property var modelData

                    objectName: "permissionOption_" + modelData.value
                    width: popup.availableWidth
                    height: 26
                    radius: Style.cornerRadius
                    color: option.index === optionList.currentIndex ? Style.hoverFillFor(
                                                                          root.colors.text,
                                                                          root.colors.accent) :
                                                                      "transparent"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: option.modelData.label
                        color: option.index === optionList.currentIndex ? Style.hoverStateColor(
                                                                              root.colors.text,
                                                                              root.colors.accent) :
                                                                          root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPositionChanged: optionList.currentIndex = option.index
                        onClicked: root.choose(option.index)
                    }
                }
            }
        }
    }
}
