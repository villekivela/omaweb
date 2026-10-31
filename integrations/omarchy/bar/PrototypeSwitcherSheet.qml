import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// PROTOTYPE (#906), variant B: a sheet hung from the bar under its center
// widget, as if the Space pill had opened. One Space at a time; its name and
// the others' run along the top edge.
Item {
    id: variant
    property var core

    readonly property var column: core.chosenColumn || (core.board.columns.length ?
                                                            core.board.columns[0] : null)
    readonly property real rowHeight: Style.space(30)

    // A notch pointing up at the center widget.
    Rectangle {
        anchors.horizontalCenter: sheet.horizontalCenter
        y: sheet.y - height / 2 + 1
        width: Style.space(12)
        height: Style.space(12)
        rotation: 45
        color: core.ground
        border.color: Commons.Color.menu.border
        border.width: Math.max(1, Style.space(2))
    }

    BorderSurface {
        id: sheet

        anchors.horizontalCenter: parent.horizontalCenter
        y: Style.bar.sizeHorizontal + Style.space(10)
        width: Style.space(440)
        height: contentTopInset + contentBottomInset + spaces.height + list.height + footer.height
                + Style.space(12) * 2
        radius: Style.cornerRadius
        color: core.ground
        borderSpec: Border.surfaceSpec("menu", "border", Commons.Color.menu.border, Math.max(1,
                                                                                            Style.space(2)))
        padding: Style.space(12)

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors.fill: parent
            anchors.topMargin: sheet.contentTopInset
            anchors.rightMargin: sheet.contentRightInset
            anchors.bottomMargin: sheet.contentBottomInset
            anchors.leftMargin: sheet.contentLeftInset
            spacing: Style.space(12)

            // The Spaces as words, the one in view underlined in its
            // colour; typing puts the words here instead.
            Item {
                id: spaces
                width: parent.width
                height: Style.space(26)

                Row {
                    visible: !core.searching
                    x: Style.space(6)
                    height: parent.height
                    spacing: Style.space(16)
                    Repeater {
                        model: core.board.columns
                        Item {
                            required property var modelData
                            required property int index
                            readonly property bool inView: variant.column
                                                           && modelData.space.id === variant.column.space.id
                            width: word.implicitWidth
                            height: parent.height
                            Text {
                                id: word
                                anchors.verticalCenter: parent.verticalCenter
                                text: parent.modelData.space.name
                                textFormat: Text.PlainText
                                color: parent.inView ? parent.modelData.space.colour : core.fg
                                opacity: parent.inView ? 1 : 0.5
                                font.family: core.font
                                font.pixelSize: Style.font.body
                                font.bold: parent.inView
                            }
                            Rectangle {
                                visible: parent.inView
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 2
                                radius: 1
                                color: parent.modelData.space.colour
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: core.view(parent.index)
                            }
                        }
                    }
                }

                Text {
                    visible: core.searching
                    x: Style.space(6)
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - x
                    text: core.filter
                    textFormat: Text.PlainText
                    elide: Text.ElideLeft
                    color: core.fg
                    font.family: core.font
                    font.pixelSize: Style.font.heading
                }
            }

            ListView {
                id: list
                width: parent.width
                // While searching every match is listed under its Space's
                // name; otherwise the Space in view only.
                readonly property var rows: {
                    const out = [];
                    const columns = core.searching ? core.board.columns : variant.column ? [variant.column] :
                                                                                           [];
                    for (const each of columns) {
                        const at = core.board.columns.indexOf(each);
                        if (core.searching)
                            out.push({
                                         "heading": each.space
                                     });
                        each.tabs.forEach((entry, row) => out.push({
                                                                       "entry": entry,
                                                                       "space": each.space,
                                                                       "column": at,
                                                                       "row": row
                                                                   }));
                    }
                    return out;
                }
                height: rows.length === 0 ? Style.space(60) : Math.min(contentHeight, variant.height * 0.6)
                clip: true
                interactive: false
                model: rows
                currentIndex: rows.findIndex(each => each.entry && each.column === core.cursor.column
                                             && each.row === core.cursor.row)
                highlightMoveDuration: 0
                onCurrentIndexChanged: if (currentIndex >= 0)
                                           positionViewAtIndex(currentIndex, ListView.Contain)

                delegate: Item {
                    id: row
                    required property var modelData
                    readonly property var tab: modelData.entry ? modelData.entry.tab : null
                    readonly property bool isSelected: !!tab && modelData.column === core.cursor.column
                                                       && modelData.row === core.cursor.row
                    width: list.width
                    height: tab ? variant.rowHeight : Style.space(26)

                    Text {
                        visible: !row.tab
                        x: Style.space(6)
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Style.space(4)
                        text: row.modelData.heading ? row.modelData.heading.name : ""
                        textFormat: Text.PlainText
                        color: row.modelData.heading ? row.modelData.heading.colour : core.fg
                        font.family: core.font
                        font.pixelSize: Style.font.bodySmall
                        font.bold: true
                    }

                    Rectangle {
                        visible: !!row.tab
                        anchors.fill: parent
                        radius: Style.cornerRadius
                        color: row.isSelected ? core.selectedGround : "transparent"
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: !!row.tab
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                        onContainsMouseChanged: if (containsMouse)
                                                    core.choose(row.modelData.column, row.modelData.row)
                        onClicked: function (mouse) {
                            if (mouse.button === Qt.MiddleButton)
                                core.close(row.tab);
                            else
                                core.take(row.modelData.column, row.modelData.row);
                        }
                    }

                    TabIcon {
                        visible: !!row.tab
                        x: Style.space(8)
                        anchors.verticalCenter: parent.verticalCenter
                        tab: row.tab || ({})
                        fg: core.fg
                        font: core.font
                    }
                    Text {
                        visible: !!row.tab
                        x: Style.space(34)
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - x - doing.width - Style.space(12)
                        text: row.tab ? row.tab.title : ""
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: row.isSelected ? core.selectedFg : core.fg
                        font.family: core.font
                        font.pixelSize: Style.font.body
                    }
                    // What the tab is doing, in words; the sound is a button.
                    Text {
                        id: doing
                        visible: !!row.tab
                        anchors.right: parent.right
                        anchors.rightMargin: Style.space(8)
                        anchors.verticalCenter: parent.verticalCenter
                        text: !row.tab ? "" : [row.tab.agent ? qsTr("Agent") : "", core.soundWords(row.tab),
                                               row.tab.id === row.modelData.space.active ? qsTr(
                                                                                               "On show") :
                                                                                           ""].filter(
                                                 Boolean).join("  ")
                        textFormat: Text.PlainText
                        color: core.fg
                        opacity: 0.5
                        font.family: core.font
                        font.pixelSize: Style.font.bodySmall
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -Style.space(6)
                            enabled: !!row.tab && row.tab.sound !== ""
                            cursorShape: Qt.PointingHandCursor
                            onClicked: core.toggleSound(row.tab)
                        }
                    }
                }

                Text {
                    visible: list.rows.length === 0 && !!core.state
                    anchors.centerIn: parent
                    text: core.searching ? qsTr("No tab matches “%1”.").arg(core.filter.trim()) :
                                           qsTr("Nothing open")
                    textFormat: Text.PlainText
                    color: core.fg
                    opacity: 0.5
                    font.family: core.font
                    font.pixelSize: Style.font.body
                }
            }

            Text {
                id: footer
                x: Style.space(6)
                width: parent.width - x
                text: core.searching ? qsTr("Enter goes there. Esc clears.") : qsTr(
                                           "Type to search every Space. Enter goes there.")
                textFormat: Text.PlainText
                elide: Text.ElideRight
                color: core.fg
                opacity: 0.4
                font.family: core.font
                font.pixelSize: Style.font.bodySmall
            }
        }
    }
}
