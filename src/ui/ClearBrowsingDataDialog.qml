import QtQuick
import qs.Commons

// Clearing browsing data is an action, so it composes its arguments in a
// dialog rather than on the settings page: the categories to take, how far the
// action reaches, and how far back it goes (ADR 0031). Opening it deliberately
// is the confirmation — as it is in Chrome, Firefox, Edge and Safari — so
// confirming acts immediately and nothing asks a second time.
//
// The one exception is scope. No mainstream browser clears every profile at
// once, so there is no tested pattern to borrow for the widest thing this
// dialog can do; every Space asks for the typed guard the core already
// enforces, and is the one value the dialog never remembers.
DialogPanel {
    id: root

    // The Space the action falls on when its scope is not widened.
    property string spaceName: ""

    // The remembered selection, owned by the call site: the dialog reports a
    // tick and leaves the array bound to the model, as the kit's controls do.
    property var categories: []
    // How far back the action reaches, as the dropdown's own value: a duration
    // in milliseconds, or "0" for all time. It is a string because that is what
    // a dropdown option and a preference both hold.
    property string range: "86400000"

    // Scope is dialog-local and starts over every time the dialog opens.
    // A scope inherited from a config file written weeks ago is a default
    // rather than a choice, and this is the choice that cannot be undone.
    property bool everySpace: false

    readonly property var categoryOptions: [
        {
            value: "cookies",
            label: "Cookies",
            note: "Signs out of sites that kept you signed in."
        },
        {
            value: "storage",
            label: "Site storage",
            note: "Local storage, databases and service workers."
        },
        {
            value: "cache",
            label: "Cache",
            note: "Files kept to load pages faster."
        },
        {
            value: "permissions",
            label: "Site permissions",
            note: "Decisions sites are asked for again."
        },
        {
            value: "history",
            label: "History",
            note: "Pages visited, and what the address bar suggests."
        }
    ]

    readonly property var rangeOptions: [
        {
            value: "3600000",
            label: "the last hour"
        },
        {
            value: "86400000",
            label: "the last day"
        },
        {
            value: "604800000",
            label: "the last week"
        },
        {
            value: "0",
            label: "all time"
        }
    ]

    function holds(value) {
        return root.categories.indexOf(value) >= 0
    }

    // Tracked by hand rather than bound: a binding that reads both dropdowns'
    // popup state is re-evaluated far more often than the two moments it
    // changes, and it is read on every key the window sees.
    property bool anyListOpen: false

    function trackOpenLists() {
        root.anyListOpen = scope.popupOpen || timeRange.popupOpen
    }
    readonly property bool confirmable: root.categories.length > 0 && (!root.everySpace
                                                                       || confirmation.text
                                                                       === "CLEAR ALL")

    signal categoryToggled(string value)
    signal rangeChosen(string value)
    signal confirmed(var categories, real since, bool everySpace, string confirmation)

    panelObjectName: "clearBrowsingDataPanel"
    label: "clear browsing data"
    destructive: true
    confirmHint: "⏎ clear · tab moves · space ticks"

    onOpenChanged: {
        if (!open)
            return
        root.everySpace = false
        confirmation.text = ""
        // The first argument, so Tab walks the form from its top. Set now
        // rather than deferred: `forceActiveFocus` claims the panel's own scope
        // on the way up, so there is no moment where the dialog is open and the
        // keyboard is somewhere behind it.
        const first = categoryList.itemAt(0)
        if (first)
            first.forceActiveFocus()
    }

    function confirm() {
        if (!root.confirmable)
            return
        const duration = Number(root.range)
        const since = duration === 0 ? 0 : Date.now() - duration
        root.confirmed(root.categories, since, root.everySpace, confirmation.text)
    }

    Keys.onPressed: function (event) {
        // A dropdown that is open answers Escape itself, and having answered it
        // keeps it: the first Escape closes the list, the second closes this.
        if (event.key === Qt.Key_Escape) {
            root.dismissed()
            event.accepted = true
        }
    }

    // Enter has to reach past whichever control holds focus — the kit's
    // dropdown trigger takes Return for itself, and a form where confirming
    // depends on where the reader happens to be standing is not a form. An open
    // list is the one place Return still belongs to the control.
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: root.open && !root.anyListOpen
        onActivated: root.confirm()
    }

    Item {
        width: parent.width
        height: form.implicitHeight + 2 * 16

        Column {
            id: form
            x: 16
            y: 16
            width: parent.width - 32
            spacing: 14

            Text {
                width: parent.width
                text: root.everySpace
                      ? "Every Space loses what is ticked below. This cannot be undone." :
                        "Clears the ticked data from " + (root.spaceName.length > 0
                                                          ? root.spaceName : "this Space")
                        + ". This cannot be undone."
                color: root.colors.text
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Repeater {
                id: categoryList
                model: root.categoryOptions

                SettingCheckbox {
                    required property var modelData
                    required property int index

                    objectName: "clearCategory-" + modelData.value
                    width: form.width
                    colors: root.colors
                    title: modelData.label
                    note: modelData.note
                    checked: root.holds(modelData.value)
                    onClicked: root.categoryToggled(modelData.value)
                }
            }

            Row {
                width: parent.width
                spacing: 16

                SettingDropdown {
                    id: scope
                    objectName: "clearScope"
                    width: (parent.width - parent.spacing) / 2
                    colors: root.colors
                    showLabel: true
                    label: "Clear from"
                    options: [
                        {
                            value: "space",
                            label: root.spaceName.length > 0 ? root.spaceName : "this Space"
                        },
                        {
                            value: "every",
                            label: "every Space"
                        }
                    ]
                    value: root.everySpace ? "every" : "space"
                    accessibleName: "Which Spaces to clear"
                    onPopupOpenChanged: root.trackOpenLists()
                    onChanged: function (value) {
                        root.everySpace = value === "every"
                    }
                }

                SettingDropdown {
                    id: timeRange
                    objectName: "clearTimeRange"
                    width: (parent.width - parent.spacing) / 2
                    colors: root.colors
                    showLabel: true
                    label: "Over"
                    options: root.rangeOptions
                    value: root.range
                    accessibleName: "Browsing data time range"
                    onPopupOpenChanged: root.trackOpenLists()
                    onChanged: function (value) {
                        root.rangeChosen(value)
                    }
                }
            }

            // The core refuses an unconfirmed every-Space clear either way; the
            // field is here so the reader is told that before pressing, rather
            // than by a button that quietly did nothing.
            SettingField {
                id: confirmation
                objectName: "clearEverySpaceConfirmation"
                width: parent.width
                visible: root.everySpace
                colors: root.colors
                destructive: true
                placeholder: "type CLEAR ALL"
                accessibleName: "Confirm clearing every Space"
                // The kit's field does not take Tab by itself, and a field the
                // confirm button depends on that Tab walks past is worse than
                // no field at all.
                activeFocusOnTab: true
            }

            Item {
                width: parent.width
                height: confirmButton.implicitHeight

                ActionButton {
                    id: confirmButton
                    objectName: "clearBrowsingDataConfirm"
                    anchors.right: parent.right
                    colors: root.colors
                    destructive: true
                    label: root.everySpace ? "Clear every Space" : "Clear browsing data"
                    enabled: root.confirmable
                    onClicked: root.confirm()
                }
            }
        }
    }
}
