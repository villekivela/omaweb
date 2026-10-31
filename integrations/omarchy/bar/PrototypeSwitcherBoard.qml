import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui

// PROTOTYPE (#906), variant A: every Space side by side as a column of its
// tabs, the full title of the one under the cursor below.
Item {
    id: variant
    property var core

    BorderSurface {
        id: card

        readonly property real columnWidth: Style.space(208)
        readonly property real gutter: Style.space(20)
        readonly property real rowHeight: Style.space(32)
        readonly property real widest: variant.width - Style.gapsOut * 4
        readonly property real tallest: variant.height * 0.6

        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(variant.height * 0.2)
        width: Math.min(widest, Math.max(Style.space(440), contentLeftInset + contentRightInset
                                         + core.board.columns.length * (columnWidth + gutter) - gutter))
        height: contentTopInset + contentBottomInset + search.height + boardArea.height
                + detail.height + Style.spacing.lg * 2
        radius: Style.cornerRadius
        color: core.ground
        borderSpec: Border.surfaceSpec("menu", "border", Commons.Color.menu.border, Math.max(1,
                                                                                            Style.space(2)))
        padding: Style.spacing.panelPadding

        MouseArea {
            anchors.fill: parent
        }

        Column {
            anchors.fill: parent
            anchors.topMargin: card.contentTopInset
            anchors.rightMargin: card.contentRightInset
            anchors.bottomMargin: card.contentBottomInset
            anchors.leftMargin: card.contentLeftInset
            spacing: Style.spacing.lg

            Text {
                id: search
                width: parent.width
                height: implicitHeight + Style.space(10)
                text: core.filter || qsTr("Type to search every Space")
                textFormat: Text.PlainText
                elide: Text.ElideLeft
                color: core.fg
                opacity: core.filter ? 1 : 0.45
                font.family: core.font
                font.pixelSize: Style.font.heading
            }

            // The Spaces side by side, scrolled to keep the cursor's in sight
            // when there are more than fit.
            Flickable {
                id: boardArea
                width: parent.width
                height: core.board.columns.length === 0 ? Style.space(96) : Math.min(card.tallest,
                                                                                     columns.tallestColumn)
                contentWidth: columns.width
                contentX: {
                    const left = Math.max(0, core.cursor.column) * (card.columnWidth + card.gutter);
                    return Math.max(0, Math.min(left, contentWidth - width));
                }
                interactive: false
                clip: true

                Row {
                    id: columns
                    spacing: card.gutter
                    readonly property real tallestColumn: {
                        let most = 0;
                        for (const column of core.board.columns)
                            most = Math.max(most, Style.space(36) + column.tabs.length * card.rowHeight
                                            + (column.tabs.some(entry => entry.pinned) ? Style.space(8) :
                                                                                         0));
                        return Math.max(most, Style.space(36) + card.rowHeight);
                    }

                    Repeater {
                        model: core.board.columns

                        Item {
                            id: column
                            required property var modelData
                            required property int index
                            readonly property var space: modelData.space
                            readonly property bool onShow: space.id === (core.state
                                                                         && core.state.activeSpace)
                            width: card.columnWidth
                            height: boardArea.height

                            // The Space's name in its colour, ruled under; the
                            // rule is heavier under the Space on show.
                            Text {
                                id: name
                                x: Style.space(8)
                                width: parent.width - x
                                text: column.space.name
                                textFormat: Text.PlainText
                                elide: Text.ElideRight
                                color: column.space.colour
                                font.family: core.font
                                font.pixelSize: Style.font.body
                                font.bold: true
                            }
                            Rectangle {
                                id: rule
                                anchors.top: name.bottom
                                anchors.topMargin: Style.space(6)
                                width: parent.width
                                height: column.onShow ? 3 : 1
                                radius: height / 2
                                color: column.space.colour
                                opacity: column.onShow ? 1 : 0.45
                            }

                            Text {
                                visible: column.modelData.tabs.length === 0
                                anchors.top: rule.bottom
                                anchors.topMargin: Style.space(10)
                                x: Style.space(8)
                                text: qsTr("Nothing open")
                                textFormat: Text.PlainText
                                color: core.fg
                                opacity: 0.4
                                font.family: core.font
                                font.pixelSize: Style.font.body
                            }

                            ListView {
                                id: tabs
                                anchors.top: rule.bottom
                                anchors.topMargin: Style.space(6)
                                anchors.bottom: parent.bottom
                                width: parent.width
                                clip: true
                                interactive: false
                                model: column.modelData.tabs
                                currentIndex: core.cursor.column === column.index ? core.cursor.row : -1
                                highlightMoveDuration: 0
                                onCurrentIndexChanged: if (currentIndex >= 0)
                                                           positionViewAtIndex(currentIndex, ListView.Contain)

                                delegate: Item {
                                    id: row
                                    required property var modelData
                                    required property int index
                                    readonly property var tab: modelData.tab
                                    readonly property bool isSelected: core.cursor.column === column.index
                                                                       && core.cursor.row === index
                                    readonly property bool shown: tab.id === column.space.active
                                    // A gap rather than a mark sets the pins apart.
                                    readonly property real gap: !modelData.pinned && index > 0
                                                                && column.modelData.tabs[index - 1].pinned ?
                                                                    Style.space(8) : 0
                                    width: tabs.width
                                    height: card.rowHeight + gap

                                    Rectangle {
                                        id: ground
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: card.rowHeight
                                        radius: Style.cornerRadius
                                        color: row.isSelected ? core.selectedGround : "transparent"
                                    }
                                    // The tab each Space shows, in its colour.
                                    Rectangle {
                                        visible: row.shown
                                        anchors.verticalCenter: ground.verticalCenter
                                        width: 3
                                        height: Style.space(16)
                                        radius: 1.5
                                        color: column.space.colour
                                    }

                                    MouseArea {
                                        anchors.fill: ground
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                                        onContainsMouseChanged: if (containsMouse)
                                                                    core.choose(column.index, row.index)
                                        onClicked: function (mouse) {
                                            if (mouse.button === Qt.MiddleButton)
                                                core.close(row.tab);
                                            else
                                                core.take(column.index, row.index);
                                        }
                                    }

                                    TabIcon {
                                        id: icon
                                        x: Style.space(10)
                                        anchors.verticalCenter: ground.verticalCenter
                                        tab: row.tab
                                        fg: core.fg
                                        font: core.font
                                    }

                                    Text {
                                        anchors.left: icon.right
                                        anchors.leftMargin: Style.space(10)
                                        anchors.right: sound.visible ? sound.left : parent.right
                                        anchors.rightMargin: Style.space(6)
                                        anchors.verticalCenter: ground.verticalCenter
                                        text: row.tab.title
                                        textFormat: Text.PlainText
                                        elide: Text.ElideRight
                                        color: row.isSelected ? core.selectedFg : core.fg
                                        font.family: core.font
                                        font.pixelSize: Style.font.body
                                    }

                                    // The sound, as a button; the Agent is named
                                    // under the board.
                                    Rectangle {
                                        id: sound
                                        visible: row.tab.sound !== ""
                                        anchors.right: parent.right
                                        anchors.verticalCenter: ground.verticalCenter
                                        width: Style.space(26)
                                        height: Style.space(26)
                                        radius: Style.cornerRadius
                                        color: Qt.alpha(core.fg, soundMouse.containsMouse ? 0.14 : 0)
                                        Text {
                                            anchors.centerIn: parent
                                            text: row.tab.sound === "muted" ? "󰖁" : "󰕾"
                                            color: row.isSelected ? core.selectedFg : core.fg
                                            opacity: row.tab.sound === "muted" ? 0.5 : 1
                                            font.family: core.font
                                            font.pixelSize: Style.font.body
                                        }
                                        MouseArea {
                                            id: soundMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: core.toggleSound(row.tab)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    visible: !!core.state && core.board.columns.length === 0
                    anchors.centerIn: parent
                    width: parent.width - Style.space(40)
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    text: qsTr("No tab matches “%1”.").arg(core.filter.trim())
                    textFormat: Text.PlainText
                    color: core.fg
                    opacity: 0.6
                    font.family: core.font
                    font.pixelSize: Style.font.body
                }
            }

            // The tab under the cursor in full, which a column cuts short, and
            // what the keys do to it.
            Item {
                id: detail
                width: parent.width
                height: Style.space(36)
                readonly property var tab: core.chosen ? core.chosen.tab : null

                Column {
                    anchors.left: parent.left
                    anchors.right: keysHint.left
                    anchors.rightMargin: Style.space(16)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Style.space(2)
                    Text {
                        width: parent.width
                        text: detail.tab ? detail.tab.title : ""
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: core.fg
                        font.family: core.font
                        font.pixelSize: Style.font.body
                    }
                    Text {
                        width: parent.width
                        text: !detail.tab ? "" : [String(detail.tab.host || "").replace(/^www\./, ""),
                                                  detail.tab.agent ? qsTr("An Agent is using it.") : "",
                                                  detail.tab.sound === "muted" ? qsTr("Muted.") :
                                                                                 detail.tab.sound !== "" ?
                                                                                     qsTr("Playing.") : ""].filter(
                                                      Boolean).join("   ")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: core.fg
                        opacity: 0.5
                        font.family: core.font
                        font.pixelSize: Style.font.bodySmall
                    }
                }

                Text {
                    id: keysHint
                    visible: !!detail.tab
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignRight
                    text: !detail.tab ? "" : detail.tab.sound !== "" ? qsTr(
                                                                           "Enter goes there\nCtrl+W closes, Ctrl+M mutes") :
                                                                       qsTr("Enter goes there\nCtrl+W closes it")
                    textFormat: Text.PlainText
                    lineHeight: 1.15
                    color: core.fg
                    opacity: 0.4
                    font.family: core.font
                    font.pixelSize: Style.font.bodySmall
                }
            }
        }
    }
}
