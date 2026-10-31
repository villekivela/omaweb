import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// PROTOTYPE (#906), variant C: a row of big favicons for each Space, and the
// one under the cursor named large below them. Up and down change Space,
// left and right move along it.
Item {
    id: variant
    property var core

    readonly property real tile: Style.space(40)
    readonly property real tileGap: Style.space(8)

    BorderSurface {
        id: card

        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(variant.height * 0.22)
        width: Math.min(variant.width - Style.gapsOut * 4, Math.max(Style.space(520), contentLeftInset
                                                                    + contentRightInset + rows.widest))
        height: contentTopInset + contentBottomInset + search.height + rows.height + named.height
                + Style.space(22) * 2
        radius: Style.cornerRadius
        color: core.ground
        borderSpec: Border.surfaceSpec("menu", "border", Commons.Color.menu.border, Math.max(1,
                                                                                            Style.space(2)))
        padding: Style.space(22)

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors.fill: parent
            anchors.topMargin: card.contentTopInset
            anchors.rightMargin: card.contentRightInset
            anchors.bottomMargin: card.contentBottomInset
            anchors.leftMargin: card.contentLeftInset
            spacing: Style.space(22)

            Text {
                id: search
                visible: core.searching
                height: visible ? implicitHeight : -Style.space(22)
                width: parent.width
                text: core.filter
                textFormat: Text.PlainText
                elide: Text.ElideLeft
                color: core.fg
                font.family: core.font
                font.pixelSize: Style.font.heading
            }

            Column {
                id: rows
                width: parent.width
                spacing: Style.space(10)
                readonly property real nameWidth: Style.space(110)
                readonly property real widest: {
                    let most = 0;
                    for (const each of core.board.columns)
                        most = Math.max(most, each.tabs.length * (variant.tile + variant.tileGap)
                                        + (each.tabs.some(entry => entry.pinned) ? Style.space(14) : 0));
                    return nameWidth + most;
                }

                Repeater {
                    model: core.board.columns

                    Row {
                        id: line
                        required property var modelData
                        required property int index
                        readonly property bool here: core.cursor.column === index
                        spacing: 0
                        height: variant.tile

                        Text {
                            width: rows.nameWidth
                            anchors.verticalCenter: parent.verticalCenter
                            text: line.modelData.space.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: line.modelData.space.colour
                            opacity: line.here ? 1 : 0.6
                            font.family: core.font
                            font.pixelSize: Style.font.body
                            font.bold: line.here
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: core.view(line.index)
                            }
                        }

                        Text {
                            visible: line.modelData.tabs.length === 0
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Nothing open")
                            textFormat: Text.PlainText
                            color: core.fg
                            opacity: 0.4
                            font.family: core.font
                            font.pixelSize: Style.font.body
                        }

                        Repeater {
                            model: line.modelData.tabs

                            Item {
                                id: cell
                                required property var modelData
                                required property int index
                                readonly property var tab: modelData.tab
                                readonly property bool isSelected: line.here && core.cursor.row === index
                                // A gap after the pins.
                                readonly property real lead: !modelData.pinned && index > 0
                                                             && line.modelData.tabs[index - 1].pinned ?
                                                                 Style.space(14) : 0
                                width: variant.tile + variant.tileGap + lead
                                height: variant.tile

                                Rectangle {
                                    x: cell.lead
                                    width: variant.tile
                                    height: variant.tile
                                    radius: Style.cornerRadius
                                    color: cell.isSelected ? core.selectedGround : Qt.alpha(core.fg, 0.04)
                                    border.width: cell.isSelected ? 2 : 0
                                    border.color: core.selectedFg

                                    TabIcon {
                                        anchors.centerIn: parent
                                        size: 22
                                        tab: cell.tab
                                        fg: core.fg
                                        font: core.font
                                    }
                                    // The tab its Space shows, in the Space's
                                    // colour.
                                    Rectangle {
                                        visible: cell.tab.id === line.modelData.space.active
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 3
                                        width: Style.space(12)
                                        height: 2
                                        radius: 1
                                        color: line.modelData.space.colour
                                    }
                                    // The sound, at the corner.
                                    Text {
                                        visible: cell.tab.sound !== ""
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 2
                                        text: cell.tab.sound === "muted" ? "󰖁" : "󰕾"
                                        color: core.fg
                                        opacity: cell.tab.sound === "muted" ? 0.5 : 0.9
                                        font.family: core.font
                                        font.pixelSize: Style.font.bodySmall
                                    }
                                    Rectangle {
                                        visible: !!cell.tab.agent
                                        x: 3
                                        y: 3
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: Commons.Color.accent
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                                        onContainsMouseChanged: if (containsMouse)
                                                                    core.choose(line.index, cell.index)
                                        onClicked: function (mouse) {
                                            if (mouse.button === Qt.MiddleButton)
                                                core.close(cell.tab);
                                            else
                                                core.take(line.index, cell.index);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: core.board.columns.length === 0 && !!core.state
                    text: qsTr("No tab matches “%1”.").arg(core.filter.trim())
                    textFormat: Text.PlainText
                    color: core.fg
                    opacity: 0.6
                    font.family: core.font
                    font.pixelSize: Style.font.body
                }
            }

            // The tab under the cursor, named in full.
            Column {
                id: named
                width: parent.width
                spacing: Style.space(4)
                readonly property var tab: core.chosen ? core.chosen.tab : null
                Text {
                    width: parent.width
                    text: named.tab ? named.tab.title : core.searching ? "" : qsTr("Type to search every Space")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: core.fg
                    opacity: named.tab ? 1 : 0.45
                    font.family: core.font
                    font.pixelSize: Math.round(Style.font.heading * 1.25)
                    font.bold: !!named.tab
                }
                Text {
                    width: parent.width
                    text: !named.tab ? " " : [String(named.tab.host || "").replace(/^www\./, ""), core.chosenColumn ?
                                                                                                    core.chosenColumn.space.name :
                                                                                                    "",
                                              named.tab.agent ? qsTr("An Agent is using it") : "", core.soundWords(
                                                  named.tab)].filter(Boolean).join("    ")
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: core.fg
                    opacity: 0.5
                    font.family: core.font
                    font.pixelSize: Style.font.body
                }
            }
        }
    }
}
