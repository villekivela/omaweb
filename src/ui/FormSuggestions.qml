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
//
// A field the page marks as a card's, other than its security code, is
// offered the reader's saved cards while it is empty (ADR 0053): each by its
// nickname, or its brand, and its last four digits, with the name on the card
// and the expiry under them. Picking one is the confirmation: the page fills
// the number, the name and the expiry of the field's form, in the field's
// frame. Only a page whose certificate the engine accepted without an
// exception is offered one, and the list says so to any other.
Item {
    id: root

    // The engine whose field has the keyboard, or null.
    property var engine: null
    property var browser: null
    // Whether what the field's page reports may be an Agent's typing: an
    // Agent's step is running in its tab, or the tab is an Agent's. Nothing
    // is offered then, since the field is not the reader's to fill.
    property bool agentTyping: false

    readonly property var field: engine ? engine.formField : null
    readonly property alias shown: list.shown
    readonly property int maxRows: 6

    property var entries: []
    property var addresses: []
    property var cards: []
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
    readonly property bool open: field !== null && !dismissed && fieldOnShow && !agentTyping
    // Whether the page is one a card may be offered on, as the window judges
    // it: a certificate the engine accepted without an exception. A payment
    // frame of an origin whose check was waived is no more secure than a page
    // of one.
    property bool pageSecure: false
    readonly property bool secure: pageSecure && !(!!field && !!field.origin && !!browser
                                                   && browser.certificateExceptionInEffect(
                                                       field.origin))
    readonly property var rows: root.offeredRows(root.addresses, root.entries, root.field,
                                                 root.fieldValue).concat(root.offeredCards(
                                                                             root.cards, root.field,
                                                                             root.secure))

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

    function cardRow(card) {
        const expiry = card.expiryMonth > 0 ? String(card.expiryMonth).padStart(2, "0") + "/" + String(
                                                  card.expiryYear % 100).padStart(2, "0") : "";
        const named = card.nickname || card.brand || qsTr("Card", "a payment card with no name");
        return {
            "value": qsTr("%1 •••• %2",
                          "a saved card: its nickname or brand, its last four digits").arg(
                         named).arg(card.last4),
            "detail": card.name && expiry ? qsTr("%1 · %2",
                                                 "a saved card: the name on it · its expiry").arg(
                                                card.name).arg(expiry) : card.name || expiry,
            "card": card,
            "group": "cards"
        };
    }

    function secureNoteRow() {
        return {
            "value": qsTr("Saved cards are offered only on secure pages"),
            "note": true,
            "group": "cards"
        };
    }

    // The saved cards under an empty card field, or on a page that is not
    // secure, a row that says why there are none.
    function offeredCards(cards, field, secure) {
        if (!field || !field.card || !field.empty || cards.length === 0)
            return [];
        if (!secure)
            return [root.secureNoteRow()];
        return cards.slice(0, root.maxRows).map(card => root.cardRow(card));
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
            root.cards = [];
            return;
        }
        root.refreshCards();
        root.entries = root.field.name ? root.browser.formHistory(root.engine.spaceId,
                                                                  root.field.name) : [];
        root.addresses = root.field.address ? root.browser.addresses() : [];
    }

    // The keyring is read only for a field a card could go into: an empty
    // card field, on a secure page, that an Agent is not typing into. On any
    // other page the cards already read say whether there is anything to
    // say no to.
    function refreshCards() {
        const field = root.field;
        if (!field || !field.card || !field.empty || !root.browser || root.agentTyping) {
            root.cards = [];
            return;
        }
        root.cards = root.secure || root.browser.paymentCardsState === "ready"
                ? root.browser.paymentCards() : [];
    }

    onAgentTypingChanged: root.refreshCards()

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
        if (row.note)
            return;
        root.dismissed = true;
        if (row.card)
            root.engine.fillPaymentCard(root.browser.paymentCardForFill(row.card.id),
                                        root.field.serial);
        else if (row.address)
            root.engine.fillAddress(row.address, root.field.serial);
        else
            root.engine.fillFormField(row.value);
    }

    // Shift+Delete forgets a value form history kept. An address is the
    // reader's to remove in Settings.
    function forget(index) {
        if (index < 0 || index >= root.rows.length || !root.field || root.rows[index].address
                || root.rows[index].card || root.rows[index].note)
            return;
        root.browser.forgetFormEntry(root.engine.spaceId, root.field.name, root.rows[index].value);
        root.refresh();
        list.highlighted = Math.min(index, root.rows.length - 1);
    }

    function follow() {
        // Each frame counts its focuses from the start, so the frame is part
        // of which focus the list is for.
        const key = root.field ? root.engineVersion + "/" + (root.field.frame || "") + "/"
                                 + root.field.serial : "";
        const value = root.field ? root.field.value : "";
        if (key !== root.fieldKey) {
            root.fieldKey = key;
            root.fieldValue = value;
            root.dismissed = false;
            list.highlighted = -1;
            root.refresh();
            root.tellPage();
        } else {
            if (value !== root.fieldValue) {
                root.fieldValue = value;
                list.highlighted = -1;
            }
            // A card field emptied again offers the cards again.
            root.refreshCards();
        }
    }

    onEngineChanged: {
        root.engineVersion += 1;
        root.follow();
    }
    onFieldChanged: root.follow()

    // The keyring is read the first time a card field asks, and answers later.
    Connections {
        target: root.browser
        ignoreUnknownSignals: true

        function onPaymentCardsChanged() {
            root.refreshCards();
        }
    }

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
