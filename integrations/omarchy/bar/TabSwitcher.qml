import QtQuick
import qs.Commons
import qs.Commons as Commons
import qs.Ui
import "BarModel.mjs" as BarModel

// The tab switcher's card (#906): every Space side by side, a column of its
// tabs under its name, and as the reader types only the tabs that match.
// Moving around the board moves only the card; the browser switches when a
// tab is taken. A row is drawn as the sidebar draws it.
BorderSurface {
    id: card

    property BrowserLink link
    signal dismissed
    // A tab was taken, and `window` shows it, or is null where it is not
    // known: the card's owner raises it once the card is gone.
    signal taken(var window)

    property string filter: ""
    // The card opens for moving, h, j, k and l as the arrows; / starts a
    // search, where letters are the search's until Esc.
    property bool searching: false
    // The tab the reader moved to, kept by its Space and id so a new state
    // from the browser leaves the cursor on it, or the Space alone for its
    // heading. Until they move, or once it is gone, the board's own choice
    // holds.
    property string chosenSpace: ""
    property string chosenTab: ""

    readonly property var state: link && link.live ? link.browser : null
    readonly property var board: state ? BarModel.board(state, filter) : ({
                                                                              "columns": [],
                                                                              "column": -1,
                                                                              "row": -1
                                                                          })
    readonly property var cursor: {
        const column = board.columns.findIndex(each => each.space.id === chosenSpace);
        if (column >= 0 && chosenTab === "")
            return {
                "column": column,
                "row": -1
            };
        const row = column >= 0 ? board.columns[column].tabs.findIndex(entry => entry.tab.id
                                                                                === chosenTab) : -1;
        return row >= 0 ? {
                              "column": column,
                              "row": row
                          } : {
            "column": board.column,
            "row": board.row
        };
    }
    readonly property var chosen: cursor.column >= 0 && cursor.row >= 0
                                  ? board.columns[cursor.column].tabs[cursor.row] : null
    // The Space whose heading the cursor is on.
    readonly property var chosenHeading: cursor.column >= 0 && cursor.row < 0
                                         ? board.columns[cursor.column].space : null

    // The card's own surface tokens are the menu's, as Omarchy's pickers use.
    readonly property color fg: Commons.Color.menu.text
    readonly property color selectedGround: Commons.Color.menu.selectedBackground
    readonly property color selectedFg: Commons.Color.menu.selectedText
    readonly property string font: Style.font.menuFamily

    readonly property real columnWidth: Style.space(220)
    readonly property real gutter: Style.space(20)
    readonly property real rowHeight: Style.space(32)
    // An Agent's last act is a second line under the title.
    readonly property real agentRowHeight: Style.space(44)
    // The pins are square tiles of their favicons, as in the sidebar, as
    // many to a line as the column holds.
    readonly property real pinTile: Style.space(36)
    readonly property real pinSpacing: Style.space(6)
    readonly property int pinsPerLine: Math.max(1, Math.floor((columnWidth + pinSpacing) / (pinTile
                                                                                            + pinSpacing)))
    readonly property real widest: parent ? parent.width - Style.gapsOut * 4 : Style.space(1200)
    readonly property real tallest: parent ? parent.height * 0.6 : Style.space(520)

    width: Math.min(widest, Math.max(Style.space(440), contentLeftInset + contentRightInset
                                     + board.columns.length * (columnWidth + gutter) - gutter))
    height: contentTopInset + contentBottomInset + search.height + boardArea.height + detail.height
            + Style.spacing.lg * 2
    radius: Style.cornerRadius
    color: Commons.Color.menu.background
    borderSpec: Border.surfaceSpec("menu", "border", Commons.Color.menu.border, Math.max(1, Style.space(
                                                                                             2)))

    padding: Style.spacing.panelPadding

    function reset() {
        searching = false;
        filter = "";
        chosenSpace = "";
        chosenTab = "";
        keys.forceActiveFocus();
    }

    function setFilter(text) {
        filter = text;
        chosenSpace = "";
        chosenTab = "";
    }

    function choose(column, row) {
        if (board.columns[column] && row < 0) {
            chosenSpace = board.columns[column].space.id;
            chosenTab = "";
            return;
        }
        const entry = board.columns[column] && board.columns[column].tabs[row];
        if (!entry)
            return;
        chosenSpace = board.columns[column].space.id;
        chosenTab = entry.tab.id;
    }

    function move(dx, dy) {
        if (cursor.column < 0)
            return;
        const next = BarModel.moveOnBoard(board.columns, cursor.column, cursor.row, dx, dy,
                                          pinsPerLine);
        choose(next.column, next.row);
    }

    // A tab, or on a heading its Space, which the browser switches to.
    function take(column, row) {
        const each = board.columns[column];
        if (each && row < 0) {
            const window = link.windowOf(null);
            link.send({
                          "do": "space",
                          "space": each.space.id
                      });
            taken(window);
            return;
        }
        const entry = each && each.tabs[row];
        if (!entry)
            return;
        const window = link.windowOf(entry.tab);
        link.activate(entry.tab, each.space.id);
        taken(window);
    }

    function close(tab) {
        if (tab)
            link.send({
                          "do": "close",
                          "tab": tab.id
                      });
    }

    function toggleSound(tab) {
        if (tab && tab.sound !== "")
            link.send({
                          "do": tab.sound === "muted" ? "unmute" : "mute",
                          "tab": tab.id
                      });
    }

    function pinsHeight(count) {
        return count === 0 ? 0 : Math.ceil(count / pinsPerLine) * (pinTile + pinSpacing)
                             + Style.space(4);
    }

    // A position worked out by halving, on the display's pixels, as
    // src/ui/DevicePixels.mjs puts the window's (#571), which a plugin cannot
    // import.
    function snap(value) {
        return Math.round(value * Screen.devicePixelRatio) / Screen.devicePixelRatio;
    }

    function rowHeightOf(tab) {
        return tab.agent && tab.agentAct ? agentRowHeight : rowHeight;
    }

    MouseArea {
        anchors.fill: parent
    }

    Item {
        id: keys
        anchors.fill: parent
        focus: true

        Keys.priority: Keys.BeforeItem
        Keys.onPressed: function (event) {
            const control = event.modifiers & Qt.ControlModifier;
            const letter = !control && event.text && event.text.length === 1 && event.text.charCodeAt(
                      0) >= 32 && event.text.charCodeAt(0) !== 127 ? event.text : "";
            const vim = card.searching ? "" : letter;
            if (event.key === Qt.Key_Escape) {
                // Out of the search keeps its matches to move among; then
                // all the tabs again; then the card goes.
                if (card.searching)
                    card.searching = false;
                else if (card.filter)
                    card.setFilter("");
                else
                    card.dismissed();
            } else if (card.searching && event.key === Qt.Key_Backspace && !card.filter) {
                card.searching = false;
            } else if (card.searching && Util.editsFilter(event, card.filter)) {
                card.setFilter(Util.editedFilter(event, card.filter));
            } else if (event.key === Qt.Key_Up || vim === "k" || (control && event.key
                                                                  === Qt.Key_P)) {
                card.move(0, -1);
            } else if (event.key === Qt.Key_Down || vim === "j" || (control && event.key
                                                                    === Qt.Key_N)) {
                card.move(0, 1);
            } else if (event.key === Qt.Key_Left || vim === "h" || event.key === Qt.Key_Backtab) {
                card.move(-1, 0);
            } else if (event.key === Qt.Key_Right || vim === "l" || event.key === Qt.Key_Tab) {
                card.move(1, 0);
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                card.take(card.cursor.column, card.cursor.row);
            } else if (control && event.key === Qt.Key_M) {
                card.toggleSound(card.chosen && card.chosen.tab);
            } else if (control && event.key === Qt.Key_W) {
                card.close(card.chosen && card.chosen.tab);
            } else if (vim === "/") {
                card.searching = true;
            } else if (card.searching && letter) {
                card.setFilter(card.filter + letter);
            } else {
                return;
            }
            event.accepted = true;
        }
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
            text: card.searching ? "/" + card.filter : card.filter || qsTr("/ searches every Space")
            textFormat: Text.PlainText
            elide: Text.ElideLeft
            color: card.fg
            opacity: card.searching || card.filter ? 1 : 0.45
            font.family: card.font
            font.pixelSize: Style.font.heading
        }

        // The Spaces side by side, scrolled to keep the cursor's in sight
        // when there are more than fit.
        Flickable {
            id: boardArea
            width: parent.width
            height: card.board.columns.length === 0 ? Style.space(96) : Math.min(card.tallest,
                                                                                 columns.tallestColumn)
            contentWidth: columns.width
            contentX: {
                const left = Math.max(0, card.cursor.column) * (card.columnWidth + card.gutter);
                return Math.max(0, Math.min(left, contentWidth - width));
            }
            interactive: false
            clip: true

            Row {
                id: columns
                spacing: card.gutter
                readonly property real tallestColumn: {
                    let most = card.rowHeight;
                    for (const column of card.board.columns) {
                        const pins = column.tabs.filter(entry => entry.pinned);
                        const height = card.pinsHeight(pins.length) + column.tabs.filter(entry =>
                        !entry.pinned).reduce((sum, entry) => sum + card.rowHeightOf(entry.tab), 0);
                        most = Math.max(most, height);
                    }
                    return Style.space(36) + most;
                }

                Repeater {
                    model: card.board.columns

                    Item {
                        id: column
                        required property var modelData
                        required property int index
                        readonly property var space: modelData.space
                        readonly property bool onShow: space.id === (card.state
                                                                     && card.state.activeSpace)

                        // The pins come first on the board, so a row below
                        // them is this many places on.
                        readonly property int pinCount: modelData.tabs.filter(entry
                                                                              => entry.pinned).length
                        width: card.columnWidth
                        height: boardArea.height

                        // The heading, a place for the cursor that takes the
                        // browser to the Space.
                        Rectangle {
                            width: parent.width
                            height: name.height + Style.space(6)
                            radius: Style.cornerRadius
                            color: card.cursor.column === column.index && card.cursor.row < 0
                                   ? card.selectedGround : "transparent"
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onContainsMouseChanged: if (containsMouse)
                                                            card.choose(column.index, -1)
                                onClicked: card.take(column.index, -1)
                            }
                        }
                        // The Space's name in its colour, ruled under; the
                        // rule is heavier under the Space on show, in room
                        // every rule keeps, so nothing under it moves when
                        // another Space comes on show.
                        Text {
                            id: name
                            x: Style.space(8)
                            y: Style.space(3)
                            width: parent.width - x
                            text: column.space.name
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            color: column.space.colour
                            font.family: card.font
                            font.pixelSize: Style.font.body
                            font.bold: true
                        }
                        Item {
                            id: rule
                            anchors.top: name.bottom
                            anchors.topMargin: Style.space(6)
                            width: parent.width
                            height: 3
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: column.onShow ? 3 : 1
                                radius: height / 2
                                color: column.space.colour
                                opacity: column.onShow ? 1 : 0.45
                            }
                        }

                        Text {
                            visible: column.modelData.tabs.length === 0
                            anchors.top: rule.bottom
                            anchors.topMargin: Style.space(10)
                            x: Style.space(8)
                            text: qsTr("Nothing open")
                            textFormat: Text.PlainText
                            color: card.fg
                            opacity: 0.4
                            font.family: card.font
                            font.pixelSize: Style.font.body
                        }

                        Flow {
                            id: pins
                            anchors.top: rule.bottom
                            anchors.topMargin: column.pinCount > 0 ? Style.space(8) : 0
                            width: parent.width
                            height: column.pinCount > 0 ? card.pinsHeight(column.pinCount) - Style.space(4) :
                                                          0

                            spacing: card.pinSpacing

                            Repeater {
                                model: column.modelData.tabs.slice(0, column.pinCount)

                                Rectangle {
                                    id: pin
                                    required property var modelData
                                    required property int index
                                    readonly property var tab: modelData.tab
                                    readonly property bool isSelected: card.cursor.column
                                                                       === column.index
                                                                       && card.cursor.row === index
                                    width: card.pinTile
                                    height: card.pinTile
                                    radius: Style.cornerRadius
                                    color: isSelected ? card.selectedGround : Qt.alpha(card.fg,
                                                                                       0.05)
                                    border.width: isSelected ? Math.max(1, Style.space(2)) : 0
                                    border.color: card.selectedFg

                                    TabIcon {
                                        anchors.centerIn: parent
                                        size: 18
                                        tab: pin.tab
                                        fg: card.fg
                                        font: card.font
                                    }
                                    // The tab its Space shows, in its colour.
                                    Rectangle {
                                        visible: pin.tab.id === column.space.active
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 2
                                        width: Style.space(12)
                                        height: 2
                                        radius: 1
                                        color: column.space.colour
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                                        onContainsMouseChanged: if (containsMouse)
                                                                    card.choose(column.index,
                                                                                pin.index)
                                        onClicked: function (mouse) {
                                            if (mouse.button === Qt.MiddleButton)
                                                card.close(pin.tab);
                                            else
                                                card.take(column.index, pin.index);
                                        }
                                    }
                                    // A pin has only its favicon to say which
                                    // site it is, so its speaker goes in the
                                    // corner over it, as in the sidebar.
                                    Rectangle {
                                        visible: pin.tab.sound !== ""
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: -Style.space(4)
                                        width: Style.space(16)
                                        height: Style.space(16)
                                        radius: width / 2
                                        color: Commons.Color.menu.background
                                        Text {
                                            anchors.centerIn: parent
                                            text: pin.tab.sound === "muted" ? "󰖁" : "󰕾"
                                            color: card.fg
                                            opacity: pin.tab.sound === "muted" ? 0.5 : 1
                                            font.family: card.font
                                            font.pixelSize: Style.font.bodySmall
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: card.toggleSound(pin.tab)
                                        }
                                    }
                                }
                            }
                        }

                        ListView {
                            id: tabs
                            anchors.top: pins.bottom
                            anchors.topMargin: Style.space(6)
                            anchors.bottom: parent.bottom
                            width: parent.width
                            clip: true
                            interactive: false
                            model: column.modelData.tabs.slice(column.pinCount)
                            currentIndex: card.cursor.column === column.index ? card.cursor.row
                                                                                - column.pinCount :
                                                                                -1
                            highlightMoveDuration: 0
                            onCurrentIndexChanged: if (currentIndex >= 0)
                                                       positionViewAtIndex(currentIndex,
                                                                           ListView.Contain)

                            delegate: Item {
                                id: row
                                required property var modelData
                                required property int index
                                readonly property var tab: modelData.tab
                                // Its place on the board, after the pins.
                                readonly property int place: index + column.pinCount
                                readonly property bool isSelected: card.cursor.column
                                                                   === column.index
                                                                   && card.cursor.row === place
                                readonly property bool shown: tab.id === column.space.active
                                readonly property bool sounding: tab.sound !== ""
                                readonly property bool acting: !!tab.agent && !!tab.agentAct
                                width: tabs.width
                                height: card.rowHeightOf(tab)

                                Rectangle {
                                    id: ground
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: card.rowHeightOf(row.tab)
                                    radius: Style.cornerRadius
                                    color: row.isSelected ? card.selectedGround : "transparent"
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
                                    id: pointer
                                    anchors.fill: ground
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                                    onContainsMouseChanged: if (containsMouse)
                                                                card.choose(column.index, row.place)
                                    onClicked: function (mouse) {
                                        if (mouse.button === Qt.MiddleButton)
                                            card.close(row.tab);
                                        else
                                            card.take(column.index, row.place);
                                    }
                                }

                                // The speaker stands in the favicon's place,
                                // as in the sidebar, so the title never moves
                                // when a page starts or stops playing.
                                TabIcon {
                                    id: icon
                                    visible: !row.sounding
                                    x: Style.space(10)
                                    y: card.snap(ground.y + (card.rowHeight - height) / 2)
                                    tab: row.tab
                                    fg: card.fg
                                    font: card.font
                                }
                                Rectangle {
                                    visible: row.sounding
                                    anchors.centerIn: icon
                                    width: Style.space(24)
                                    height: Style.space(24)
                                    radius: Style.cornerRadius
                                    color: Qt.alpha(card.fg, soundPointer.containsMouse ? 0.14 : 0)
                                    Text {
                                        anchors.centerIn: parent
                                        text: row.tab.sound === "muted" ? "󰖁" : "󰕾"
                                        color: row.isSelected ? card.selectedFg : card.fg
                                        opacity: row.tab.sound === "muted" ? 0.5 : 1
                                        font.family: card.font
                                        font.pixelSize: Style.font.heading
                                    }
                                    MouseArea {
                                        id: soundPointer
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onContainsMouseChanged: if (containsMouse)
                                                                    card.choose(column.index,
                                                                                row.place)
                                        onClicked: card.toggleSound(row.tab)
                                    }
                                }

                                Text {
                                    id: title
                                    anchors.left: icon.right
                                    anchors.leftMargin: Style.space(10)
                                    anchors.right: end.left
                                    anchors.rightMargin: Style.space(6)
                                    y: card.snap(icon.y + (icon.height - height) / 2)
                                    text: row.tab.title || row.tab.host
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: row.isSelected ? card.selectedFg : card.fg
                                    opacity: row.isSelected || row.shown ? 1 : 0.7
                                    font.family: card.font
                                    font.pixelSize: Style.font.body
                                }
                                // What the Agent last did, under the title.
                                Text {
                                    visible: row.acting
                                    anchors.left: title.left
                                    anchors.right: title.right
                                    anchors.top: title.bottom
                                    text: row.tab.agentAct || ""
                                    textFormat: Text.PlainText
                                    elide: Text.ElideRight
                                    color: Commons.Color.accent
                                    font.family: card.font
                                    font.pixelSize: Style.font.bodySmall
                                }

                                // The row's end: close under the pointer,
                                // otherwise a tab in a window of its own says
                                // so.
                                Item {
                                    id: end
                                    anchors.right: parent.right
                                    anchors.rightMargin: Style.space(4)
                                    y: card.snap(icon.y + (icon.height - height) / 2)
                                    width: Style.space(22)
                                    height: Style.space(22)
                                    Text {
                                        visible: !pointer.containsMouse &&
                                                 !closePointer.containsMouse && !!row.tab.poppedOut
                                        anchors.centerIn: parent
                                        text: "󰁜"
                                        color: card.fg
                                        opacity: 0.6
                                        font.family: card.font
                                        font.pixelSize: Style.font.body
                                    }
                                    Rectangle {
                                        visible: pointer.containsMouse || closePointer.containsMouse
                                        anchors.fill: parent
                                        radius: Style.cornerRadius
                                        color: Qt.alpha(card.fg, closePointer.containsMouse ? 0.14 :
                                                                                              0)
                                        Text {
                                            anchors.centerIn: parent
                                            text: "󰅖"
                                            color: card.fg
                                            opacity: 0.7
                                            font.family: card.font
                                            font.pixelSize: Style.font.body
                                        }
                                    }
                                    MouseArea {
                                        id: closePointer
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: card.close(row.tab)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                visible: !!card.state && card.board.columns.length === 0
                anchors.centerIn: parent
                width: parent.width - Style.space(40)
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: qsTr("No tab matches “%1”.").arg(card.filter.trim())
                textFormat: Text.PlainText
                color: card.fg
                opacity: 0.6
                font.family: card.font
                font.pixelSize: Style.font.body
            }
        }

        // The tab under the cursor in full, which a column cuts short, and
        // what the keys do to it.
        Item {
            id: detail
            width: parent.width
            height: Style.space(36)
            readonly property var tab: card.chosen ? card.chosen.tab : null
            readonly property var space: card.chosenHeading

            Column {
                anchors.left: parent.left
                anchors.right: keysHint.left
                anchors.rightMargin: Style.space(16)
                anchors.verticalCenter: parent.verticalCenter
                spacing: Style.space(2)
                Text {
                    width: parent.width
                    text: detail.tab ? detail.tab.title : detail.space ? detail.space.name : ""
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: card.fg
                    font.family: card.font
                    font.pixelSize: Style.font.body
                }
                Text {
                    width: parent.width
                    text: detail.tab ? String(detail.tab.host || "").replace(/^www\./, "") : ""
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: card.fg
                    opacity: 0.5
                    font.family: card.font
                    font.pixelSize: Style.font.bodySmall
                }
            }

            Text {
                id: keysHint
                visible: !!detail.tab || !!detail.space
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                horizontalAlignment: Text.AlignRight
                text: {
                    if (detail.space)
                        return qsTr("hjkl moves, Enter switches to this Space");
                    if (!detail.tab)
                        return "";
                    const moving = card.searching ? qsTr("Enter goes there, Esc stops searching") :
                                                    qsTr("hjkl moves, Enter goes there");
                    if (detail.tab.sound === "muted")
                        return moving + "\n" + qsTr("Ctrl+W closes, Ctrl+M unmutes");
                    if (detail.tab.sound !== "")
                        return moving + "\n" + qsTr("Ctrl+W closes, Ctrl+M mutes");
                    return moving + "\n" + qsTr("Ctrl+W closes it");
                }
                textFormat: Text.PlainText
                lineHeight: 1.15
                color: card.fg
                opacity: 0.4
                font.family: card.font
                font.pixelSize: Style.font.bodySmall
            }
        }
    }
}
