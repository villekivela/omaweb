import QtQuick
import QtQuick.Controls
import qs.Commons
import qs.Ui as Omarchy

// The browser theme, as Settings offers it (#710): following the desktop's
// theme, named for it, then every installed Omarchy theme by the name
// Omarchy's menu shows. Each row ends with the theme's background, foreground
// and accent, which the kit's searchable dropdown has no place for, so this is
// that control rebuilt from the kit's parts, as PermissionDropdown is: the
// trigger, the search field and the keys follow the kit's (Return, Space or
// Down opens with the search focused; Down walks into the list; Up, Down, j and
// k walk; Return picks; Escape closes).
//
// Walking the list previews nothing. A theme applies when it is chosen.
Omarchy.BorderSurface {
    id: root

    property var colors
    property var browserTheme: null
    property string iconFontFamily: ""
    property string accessibleName: ""
    readonly property string value: root.browserTheme ? root.browserTheme.choice : ""
    readonly property bool popupOpen: popup.opened
    // The open popup, which stands in the window's overlay rather than under
    // this item.
    readonly property Item popupItem: popup.contentItem

    readonly property var options: {
        const theme = root.browserTheme;
        if (!theme)
            return [];
        const rows = [
                  {
                      value: "",
                      label: qsTr("Follow Omarchy"),
                      detail: theme.desktopName,
                      colours: theme.desktopColours
                  }
              ];
        for (const entry of theme.themes)
            rows.push({
                          value: entry.folder,
                          label: entry.name,
                          detail: "",
                          colours: entry
                      });
        return rows;
    }
    property var filtered: options

    // A choice the reader made. The owner stores it, and the dropdown shows
    // what was stored.
    signal chosen(string value)

    function filter() {
        // The swatches arrive after the list does, which builds the options
        // again; the row the reader walked to stays the one highlighted.
        const highlighted = popup.opened && list.currentIndex >= 0 && list.currentIndex
              < root.filtered.length ? root.filtered[list.currentIndex].value : root.value;
        const query = search.text.toLowerCase();
        root.filtered = query === "" ? root.options : root.options.filter(function (option) {
            return option.label.toLowerCase().indexOf(query) >= 0;
        });
        const current = root.filtered.findIndex(function (option) {
            return option.value === highlighted;
        });
        list.currentIndex = current >= 0 ? current : (root.filtered.length > 0 ? 0 : -1);
    }

    function open() {
        popup.open();
    }

    function choose(index) {
        const option = root.filtered[index];
        popup.close();
        if (option && option.value !== root.value)
            root.chosen(option.value);
    }

    onOptionsChanged: filter()

    // The kit's width, or wider for a name that would not fit beside the
    // desktop theme's and the squares.
    implicitWidth: Math.max(Style.spacing.searchableDropdownWidth, Math.ceil(trigger.implicitWidth)
                            + root.borderLeft + root.borderRight + Style.spacing.controlPaddingX
                            + Style.spacing.md + arrow.implicitWidth + Style.spacing.xs)
    implicitHeight: Style.spacing.controlHeight
    radius: Style.cornerRadius
    opacity: root.enabled ? 1 : 0.5
    color: Style.controlFill(root.activeFocus, hover.hovered, root.colors.text, root.colors.accent)
    borderSpec: Border.controlSpec(root.activeFocus ? "focus" : (hover.hovered ? "hover-cursor" :
                                                                                 "normal"),
                                   root.colors.text, root.colors.accent)
    activeFocusOnTab: true
    Accessible.role: Accessible.ComboBox
    Accessible.name: root.accessibleName
    Accessible.description: trigger.label

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

    TapHandler {
        onTapped: {
            root.forceActiveFocus();
            popup.opened ? popup.close() : popup.open();
        }
    }

    ThemeEntry {
        id: trigger
        anchors.left: parent.left
        anchors.leftMargin: root.borderLeft + Style.spacing.controlPaddingX
        anchors.right: arrow.left
        anchors.rightMargin: Style.spacing.md
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        // A picked theme that is not installed is not in the list, and is
        // still the reader's choice.
        label: root.value === "" ? qsTr("Follow Omarchy") : root.browserTheme.choiceName
        detail: root.value === "" && root.browserTheme ? root.browserTheme.desktopName : ""
        swatches: root.options.length > 0 ? (root.options.find(function (option) {
            return option.value === root.value;
        }) || {
                                                 colours: {}
                                             }).colours : ({})
    }

    Text {
        id: arrow
        anchors.right: parent.right
        anchors.rightMargin: root.borderRight + Style.spacing.xs
        anchors.verticalCenter: parent.verticalCenter
        text: "expand_more"
        color: root.colors.mutedText
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.icon
        Accessible.ignored: true
    }

    Popup {
        id: popup
        objectName: "themePopup"
        y: root.height + Style.spacing.xxs
        width: root.width
        height: Math.min(search.height + Style.spacing.md * 2 + 1 + list.contentHeight,
                         Style.spacing.popupRowHeight * 9)
        padding: Style.spacing.hairline
        focus: true

        onOpened: {
            if (root.browserTheme)
                root.browserTheme.refreshThemes();
            search.text = "";
            list.currentIndex = -1;
            root.filter();
            search.forceActiveFocus();
        }
        onClosed: root.forceActiveFocus()

        background: Omarchy.BorderSurface {
            radius: Style.cornerRadius
            color: root.colors.overlayOpaque
            borderSpec: Border.controlSpec("normal", root.colors.text, root.colors.accent)
        }

        contentItem: Column {
            spacing: 0

            Omarchy.TextField {
                id: search
                x: Style.spacing.md
                width: parent.width - 2 * Style.spacing.md
                topPadding: Style.spacing.md
                placeholderText: qsTr("Search themes")
                foreground: root.colors.text
                accent: root.colors.accent
                onTextChanged: root.filter()

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Down && list.count > 0) {
                        list.currentIndex = 0;
                        list.forceActiveFocus();
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.choose(list.currentIndex);
                    } else if (event.key === Qt.Key_Escape) {
                        popup.close();
                    } else {
                        return;
                    }
                    event.accepted = true;
                }
            }

            Item {
                width: parent.width
                height: Style.spacing.md
            }

            Rectangle {
                width: parent.width
                height: 1
                color: root.colors.separator
            }

            ListView {
                id: list
                objectName: "themeList"
                width: parent.width
                height: popup.availableHeight - search.height - Style.spacing.md - 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.filtered
                keyNavigationEnabled: false

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Down || event.text === "j") {
                        list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1);
                    } else if (event.key === Qt.Key_Up || event.text === "k") {
                        if (list.currentIndex <= 0)
                            search.forceActiveFocus();
                        else
                            list.currentIndex -= 1;
                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.choose(list.currentIndex);
                    } else if (event.key === Qt.Key_Escape) {
                        popup.close();
                    } else {
                        return;
                    }
                    event.accepted = true;
                }

                delegate: Rectangle {
                    id: option

                    required property int index
                    required property var modelData

                    width: ListView.view.width
                    height: Style.spacing.popupRowHeight
                    color: option.index === list.currentIndex ? Style.hoverFillFor(root.colors.text,
                                                                                   root.colors.accent) :
                                                                "transparent"

                    ThemeEntry {
                        anchors.left: parent.left
                        anchors.leftMargin: Style.spacing.controlPaddingX
                        anchors.right: parent.right
                        anchors.rightMargin: Style.spacing.controlPaddingX
                        anchors.verticalCenter: parent.verticalCenter
                        colors: root.colors
                        label: option.modelData.label
                        detail: option.modelData.detail
                        swatches: option.modelData.colours
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onPositionChanged: list.currentIndex = option.index
                        onClicked: root.choose(option.index)
                    }
                }
            }
        }
    }

    // A theme's name, a muted word after it, and its three colours at the end.
    component ThemeEntry: Item {
        id: entry

        property var colors
        property string label: ""
        property string detail: ""
        property var swatches: ({})

        implicitHeight: Math.max(name.implicitHeight, 12)
        implicitWidth: words.implicitWidth + Style.spacing.md + squares.implicitWidth

        Row {
            id: words
            anchors.left: parent.left
            anchors.right: squares.left
            anchors.rightMargin: Style.spacing.md
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.spacing.sm
            clip: true

            Text {
                id: name
                objectName: "themeLabel"
                text: entry.label
                color: entry.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Text {
                visible: entry.detail !== ""
                text: "·"
                color: entry.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Text {
                objectName: "themeDetail"
                visible: entry.detail !== ""
                text: entry.detail
                color: entry.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
        }

        Row {
            id: squares
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.spacing.xxs

            Repeater {
                model: ["background", "foreground", "accent"]

                // A square the colour of the ground it sits on would vanish, so
                // each has a hairline round it.
                Rectangle {
                    required property string modelData

                    objectName: "swatch" + modelData.charAt(0).toUpperCase() + modelData.slice(1)
                    width: 12
                    height: 12
                    color: entry.swatches && entry.swatches[modelData] ? entry.swatches[modelData] :
                                                                         "transparent"
                    border.width: 1
                    border.color: entry.colors.separator
                }
            }
        }
    }
}
