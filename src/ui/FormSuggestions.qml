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
//
// A field the page marks with an address token is offered the reader's saved
// addresses too, above its history and counted in the same six: by name, with
// the street and city under it. Accepting one fills every address field of
// the form, which the page does. They are read from the window's browser, and
// a Private window's has none.
Item {
    id: root

    // The engine whose field has the keyboard, or null.
    property var engine: null
    property var browser: null

    readonly property var field: engine ? engine.formField : null
    readonly property alias shown: list.shown
    readonly property int maxRows: 6

    property var entries: []
    property var addresses: []
    // Which focus of which engine's field the list is for. A page counts its
    // focuses from the start again, so the engine is part of it.
    property string fieldKey: ""
    property string fieldValue: ""
    property int engineVersion: 0
    // Escape and accepting close the list for this focus of the field only.
    property bool dismissed: false

    // The page the field is in, and whether the field is on show in it: a
    // field scrolled out of the page has no list, which would stand over the
    // chrome.
    readonly property rect pageRect: engine ? engine.mapToItem(root, 0, 0, engine.width,
                                                               engine.height) : Qt.rect(0, 0, 0, 0)
    readonly property rect fieldRect: field ? engine.mapToItem(root, field.x, field.y, field.width,
                                                               field.height) : Qt.rect(0, 0, 0, 0)
    readonly property bool fieldOnShow: fieldRect.y + fieldRect.height > pageRect.y && fieldRect.y
                                        < pageRect.y + pageRect.height
    readonly property bool open: field !== null && !dismissed && fieldOnShow
    readonly property var rows: root.offeredRows(root.addresses, root.entries, root.field,
                                                 root.fieldValue)

    // A row is built here rather than in the arrow function that maps to it:
    // qmlformat reads an object literal returned there as a block.
    function row(value, typed) {
        return {
            "value": value,
            "typed": typed,
            "group": "history"
        };
    }

    function addressRow(address) {
        return {
            "value": address.name,
            "detail": [address.street, address.city].filter(part => part.length > 0).join(", "),
            "address": address,
            "group": "addresses"
        };
    }

    // What an address puts in a field with this token, as the page fills it.
    function addressValue(address, token) {
        const name = address.name.trim();
        const split = name.lastIndexOf(" ");
        switch (token) {
        case "name":
            return name;
        case "given-name":
            return split < 0 ? name : name.slice(0, split);
        case "family-name":
            return split < 0 ? "" : name.slice(split + 1);
        case "street-address":
        case "address-line1":
            return address.street;
        case "postal-code":
            return address.postalCode;
        case "address-level2":
            return address.city;
        case "country":
        case "country-name":
            return address.country;
        case "tel":
        case "tel-national":
            return address.phone;
        case "email":
            return address.email;
        }
        return "";
    }

    function offered(values, typed) {
        const lowered = typed.toLowerCase();
        const starts = value => value !== typed && value.toLowerCase().startsWith(lowered);
        return values.filter(starts).slice(0, root.maxRows).map(value => root.row(value, typed));
    }

    // The addresses whose value for the field starts with what was typed.
    function offeredAddresses(addresses, token, typed) {
        if (!token)
            return [];
        const lowered = typed.toLowerCase();
        const starts = address => (root.addressValue(address, token) || "").toLowerCase().startsWith(
                                                                                lowered);
        return addresses.filter(starts).map(address => root.addressRow(address));
    }

    // Addresses first, then form history, six in all.
    function offeredRows(addresses, entries, field, typed) {
        const token = field ? field.address : "";
        return root.offeredAddresses(addresses, token, typed).concat(root.offered(entries, typed)).slice(
                    0, root.maxRows);
    }

    function refresh() {
        if (!root.field || !root.browser || !root.engine) {
            root.entries = [];
            root.addresses = [];
            return;
        }
        root.entries = root.field.name ? root.browser.formHistory(root.engine.spaceId,
                                                                  root.field.name) : [];
        root.addresses = root.field.address ? root.browser.addresses() : [];
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
        const row = root.rows[index];
        root.dismissed = true;
        if (row.address)
            root.engine.fillAddress(row.address);
        else
            root.engine.fillFormField(row.value);
    }

    // Shift+Delete forgets a value form history kept. An address is the
    // reader's to remove in Settings.
    function forget(index) {
        if (index < 0 || index >= root.rows.length || !root.field || root.rows[index].address)
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
        anchorRect: root.fieldRect
        boundsRect: root.pageRect
        onShownChanged: root.tellPage()
        onHighlightedChanged: root.tellPage()
        onAccepted: function (index) {
            root.accept(index);
        }
    }
}
