import QtQuick

// Form history's suggestions for the page field that has the keyboard. The
// engine reports the field and the keys the page gave up; the values stay
// here, read from the field's Space, until the reader accepts one, and only
// that one is written into the page. A Private window's store keeps nothing,
// so it has nothing to offer here.
//
// The list opens when a field with history is focused and lists its values,
// most recently used first and six at most. Typing narrows it to the values
// that start with what was typed. Escape, or accepting a value, closes it
// until the field is focused again.
Item {
    id: root

    // The engine whose field has the keyboard, or null.
    property var engine: null
    property var browser: null

    readonly property var field: engine ? engine.formField : null
    readonly property alias shown: list.shown
    readonly property int maxRows: 6

    property var entries: []
    // Which focus of which engine's field the list is for. A page counts its
    // focuses from the start again, so the engine is part of it.
    property string fieldKey: ""
    property string fieldValue: ""
    property int engineVersion: 0
    // Escape and accepting close the list for this focus of the field only.
    property bool dismissed: false

    readonly property bool open: field !== null && !dismissed
    readonly property var rows: root.offered(root.entries, root.fieldValue)

    // A row is built here rather than in the arrow function that maps to it:
    // qmlformat reads an object literal returned there as a block.
    function row(value, typed) {
        return {
            "value": value,
            "typed": typed
        };
    }

    function offered(values, typed) {
        const lowered = typed.toLowerCase();
        const starts = value => value !== typed && value.toLowerCase().startsWith(lowered);
        return values.filter(starts).slice(0, root.maxRows).map(value => root.row(value, typed));
    }

    function refresh() {
        if (!root.field || !root.browser) {
            root.entries = [];
            return;
        }
        root.entries = root.browser.formHistory(root.engine.spaceId, root.field.name);
    }

    // The page takes the list's keys only while it is drawn, and Enter and
    // Shift+Delete only while a row is highlighted, so a key the list has no
    // use for still reaches the page.
    function tellPage() {
        if (root.engine && root.field)
            root.engine.showFormSuggestions(list.shown, list.shown && list.highlighted >= 0);
    }

    function accept(index) {
        if (index < 0 || index >= root.rows.length || !root.field)
            return;
        const value = root.rows[index].value;
        root.dismissed = true;
        root.engine.fillFormField(value);
    }

    function forget(index) {
        if (index < 0 || index >= root.rows.length || !root.field)
            return;
        root.browser.forgetFormEntry(root.engine.spaceId, root.field.name, root.rows[index].value);
        root.refresh();
        list.highlighted = Math.min(index, root.rows.length - 1);
    }

    function follow() {
        const key = root.field ? root.engineVersion + "/" + root.field.serial : "";
        const value = root.field ? root.field.value : "";
        if (key !== root.fieldKey) {
            root.fieldKey = key;
            root.fieldValue = value;
            root.dismissed = false;
            list.highlighted = -1;
            root.refresh();
            root.tellPage();
        } else if (value !== root.fieldValue) {
            root.fieldValue = value;
            list.highlighted = -1;
        }
    }

    onEngineChanged: {
        root.engineVersion += 1;
        root.follow();
    }
    onFieldChanged: root.follow()

    Connections {
        target: root.engine
        ignoreUnknownSignals: true

        function onFormKeyPressed(key) {
            if (!list.shown)
                return;
            if (key === "down")
                list.highlighted = Math.min(list.count - 1, list.highlighted + 1);
            else if (key === "up")
                list.highlighted = Math.max(-1, list.highlighted - 1);
            else if (key === "escape")
                root.dismissed = true;
            else if (key === "accept")
                root.accept(list.highlighted);
            else if (key === "forget")
                root.forget(list.highlighted);
        }
    }

    SuggestionList {
        id: list
        objectName: "formSuggestions"
        rows: root.rows
        open: root.open
        anchorRect: root.field ? root.engine.mapToItem(root, root.field.x, root.field.y,
                                                       root.field.width, root.field.height) :
                                 Qt.rect(0, 0, 0, 0)
        onShownChanged: root.tellPage()
        onHighlightedChanged: root.tellPage()
        onAccepted: function (index) {
            root.accept(index);
        }
        onHovered: function (index) {
            list.highlighted = index;
        }
    }
}
