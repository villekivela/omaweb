import QtQuick
import Quickshell
import qs.Commons
import qs.Commons as Commons
import "BarModel.mjs" as BarModel

// The tab switcher's state and keys (#906), whatever draws it: every Space's
// tabs as a board, a cursor on one of them, and the reader's words. Moving
// the cursor moves nothing in the browser; taking a tab switches to it.
Item {
    id: core

    property BrowserLink link
    signal dismissed

    // A drawing whose Spaces run down rather than across swaps the arrows.
    readonly property bool transposed: !!variants[variant].transposed

    // PROTOTYPE: which drawing is on show, F1 to F3 or Ctrl+[ and Ctrl+].
    property int variant: Number(Quickshell.env("OMAWEB_SWITCHER_VARIANT") || 0)
    readonly property var variants: [{
            "name": "Space board",
            "source": "PrototypeSwitcherBoard.qml"
        }, {
            "name": "Drop from the bar",
            "source": "PrototypeSwitcherSheet.qml"
        }, {
            "name": "Favicon wall",
            "source": "PrototypeSwitcherWall.qml",
            "transposed": true
        }]
    function stepVariant(step) {
        variant = (variant + step + variants.length) % variants.length;
    }

    property string filter: ""
    // The tab the reader moved to, kept by its Space and id so a new state
    // from the browser leaves the cursor on it. Until they move, or once it
    // is gone, the board's own choice holds.
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
    readonly property var chosen: cursor.column >= 0 ? board.columns[cursor.column].tabs[cursor.row] :
                                                       null
    readonly property var chosenColumn: cursor.column >= 0 ? board.columns[cursor.column] : null
    readonly property bool searching: filter.trim() !== ""

    // The menu's tokens, as Omarchy's pickers use.
    readonly property color fg: Commons.Color.menu.text
    readonly property color ground: Commons.Color.menu.background
    readonly property color selectedGround: Commons.Color.menu.selectedBackground
    readonly property color selectedFg: Commons.Color.menu.selectedText
    readonly property string font: Style.font.menuFamily

    function reset() {
        filter = "";
        chosenSpace = "";
        chosenTab = "";
        forceActiveFocus();
    }

    function setFilter(text) {
        filter = text;
        chosenSpace = "";
        chosenTab = "";
    }

    function choose(column, row) {
        const entry = board.columns[column] && board.columns[column].tabs[row];
        if (!entry)
            return;
        chosenSpace = board.columns[column].space.id;
        chosenTab = entry.tab.id;
    }

    // Looks at another Space's tab on show, or its first.
    function view(column) {
        const each = board.columns[column];
        if (!each || each.tabs.length === 0)
            return;
        const shown = each.tabs.findIndex(entry => entry.tab.id === each.space.active);
        choose(column, Math.max(0, shown));
    }

    function move(dx, dy) {
        if (cursor.column < 0)
            return;
        const next = BarModel.moveOnBoard(board.columns, cursor.column, cursor.row, dx, dy);
        choose(next.column, next.row);
    }

    function take(column, row) {
        const each = board.columns[column];
        const entry = each && each.tabs[row];
        if (!entry)
            return;
        link.activate(entry.tab, each.space.id);
        dismissed();
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

    function soundWords(tab) {
        return !tab || tab.sound === "" ? "" : tab.sound === "muted" ? qsTr("Muted") : qsTr("Playing");
    }

    focus: true
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: function (event) {
        const control = event.modifiers & Qt.ControlModifier;
        const across = core.transposed ? [0, 1] : [1, 0];
        const down = core.transposed ? [1, 0] : [0, 1];
        if (event.key >= Qt.Key_F1 && event.key < Qt.Key_F1 + core.variants.length) {
            core.variant = event.key - Qt.Key_F1;
        } else if (control && event.key === Qt.Key_BracketLeft) {
            core.stepVariant(-1);
        } else if (control && event.key === Qt.Key_BracketRight) {
            core.stepVariant(1);
        } else if (event.key === Qt.Key_Escape) {
            if (core.filter)
                core.setFilter("");
            else
                core.dismissed();
        } else if (Util.editsFilter(event, core.filter)) {
            core.setFilter(Util.editedFilter(event, core.filter));
        } else if (event.key === Qt.Key_Up || (control && event.key === Qt.Key_P)) {
            core.move(-down[0], -down[1]);
        } else if (event.key === Qt.Key_Down || (control && event.key === Qt.Key_N)) {
            core.move(down[0], down[1]);
        } else if (event.key === Qt.Key_Left) {
            core.move(-across[0], -across[1]);
        } else if (event.key === Qt.Key_Right) {
            core.move(across[0], across[1]);
        } else if (event.key === Qt.Key_Backtab) {
            core.move(-1, 0);
        } else if (event.key === Qt.Key_Tab) {
            core.move(1, 0);
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            core.take(core.cursor.column, core.cursor.row);
        } else if (control && event.key === Qt.Key_M) {
            core.toggleSound(core.chosen && core.chosen.tab);
        } else if (control && event.key === Qt.Key_W) {
            core.close(core.chosen && core.chosen.tab);
        } else if (!control && event.text && event.text.length === 1 && event.text.charCodeAt(0) >= 32
                   && event.text.charCodeAt(0) !== 127) {
            core.setFilter(core.filter + event.text);
        } else {
            return;
        }
        event.accepted = true;
    }

    Loader {
        id: drawing
        anchors.fill: parent
        // The drawing gets the core as it is made, before its first binding.
        readonly property string drawn: core.variants[core.variant].source
        onDrawnChanged: setSource(drawn, {
                                      "core": core
                                  })
        Component.onCompleted: setSource(drawn, {
                                             "core": core
                                         })
    }
}
