import QtQuick
import QtTest
import qs.Commons
import "../../src/ui" as Omaweb

// The clear-browsing-data dialog is Omaweb's first dialog with a real tab
// order, and a tab order is the most environment-dependent thing a QML test can
// assert: `activeFocus` is false for every item in a window the platform does
// not call active, and in a test case where one window is shared by a hundred
// tests, which window is active is decided by whichever of them opened one
// last. Asserting it there made the suite pass on a developer's machine and
// fail on CI — the same defect at a different speed, twice.
//
// So the keyboard contract is asserted here instead, against a dialog standing
// on its own in this test case's own window, where nothing else can take the
// keyboard off it. What `tst_main.qml` keeps is the part that needs the browser
// around it: that confirming clears and opening does not.
TestCase {
    id: testCase
    name: "ClearBrowsingDataDialog"
    when: windowShown
    visible: true
    width: 900
    height: 700

    property var colorsFixture: ({
                                     text: "#f3f1fa",
                                     mutedText: "#8d88a3",
                                     accent: "#9b87ff",
                                     privateAccent: "#dc6bce",
                                     border: "#4a4658",
                                     separator: "#332f3f",
                                     surface: "#26232f",
                                     surfaceHover: "#3d394e",
                                     overlay: "#1c1a24",
                                     windowOpaque: "#16151d"
                                 })

    readonly property var everyCategory: ["cookies", "storage", "cache", "permissions", "history"]

    Component {
        id: dialogComponent

        Omaweb.ClearBrowsingDataDialog {
            id: dialog

            // What the settings page does with the tick, done here so the
            // dialog is exercised against a call site that owns the value the
            // way the real one does.
            property int confirmCount: 0
            property var lastConfirm: null

            anchors.fill: parent
            colors: testCase.colorsFixture
            spaceName: "Personal"
            categories: testCase.everyCategory
            range: "86400000"

            onCategoryToggled: function (value) {
                const next = dialog.categories.slice()
                const at = next.indexOf(value)
                if (at >= 0)
                    next.splice(at, 1)
                else
                    next.push(value)
                dialog.categories = next
            }
            onRangeChosen: function (value) {
                dialog.range = value
            }
            onConfirmed: function (categories, since, everySpace, confirmation) {
                dialog.confirmCount += 1
                dialog.lastConfirm = {
                    "categories": categories,
                    "since": since,
                    "everySpace": everySpace,
                    "confirmation": confirmation
                }
            }
        }
    }

    // Standing the dialog up is the same three lines every time, and every one
    // of these presses a key, so the window has to be the active one first.
    function openDialog(properties) {
        const dialog = createTemporaryObject(dialogComponent, testCase, properties)
        verify(dialog !== null)
        dialog.open = true
        tryVerify(function () {
            return dialog.visible
        })
        return dialog
    }

    function category(dialog, value) {
        return findChild(dialog, "clearCategory-" + value)
    }

    function test_theDialogOpensOnItsFirstArgument() {
        const dialog = openDialog({})
        tryVerify(function () {
            return category(dialog, "cookies").activeFocus
        })
    }

    // The order the form reads in, walked both ways.
    function test_tabWalksTheFormAndShiftTabWalksItBack() {
        const dialog = openDialog({})
        tryVerify(function () {
            return category(dialog, "cookies").activeFocus
        })

        const order = testCase.everyCategory
        for (let step = 1; step < order.length; ++step) {
            keyClick(Qt.Key_Tab)
            verify(category(dialog, order[step]).activeFocus)
        }
        for (let back = order.length - 2; back >= 0; --back) {
            keyClick(Qt.Key_Backtab)
            verify(category(dialog, order[back]).activeFocus)
        }
    }

    // A selection is an argument, so ticking one changes the argument and
    // nothing else: the keyboard does not move and nothing is cleared.
    function test_spaceTicksTheControlUnderTheKeyboardAndClearsNothing() {
        const dialog = openDialog({})
        const cookies = category(dialog, "cookies")
        tryVerify(function () {
            return cookies.activeFocus
        })

        compare(cookies.checked, true)
        keyClick(Qt.Key_Space)
        compare(cookies.checked, false)
        compare(dialog.categories.indexOf("cookies"), -1)
        verify(cookies.activeFocus)
        compare(dialog.confirmCount, 0)

        keyClick(Qt.Key_Space)
        compare(cookies.checked, true)
    }

    // The kit's dropdown trigger takes Return for itself, so a form that let it
    // would confirm from four of its controls and not from the other two.
    function test_enterConfirmsFromWhereverTheKeyboardIs() {
        const dialog = openDialog({})
        tryVerify(function () {
            return category(dialog, "cookies").activeFocus
        })

        keyClick(Qt.Key_Return)
        compare(dialog.confirmCount, 1)
        compare(dialog.lastConfirm.categories.length, 5)
        compare(dialog.lastConfirm.everySpace, false)

        findChild(dialog, "clearTimeRange").forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(dialog.confirmCount, 2)

        findChild(dialog, "clearBrowsingDataConfirm").forceActiveFocus()
        keyClick(Qt.Key_Return)
        compare(dialog.confirmCount, 3)
    }

    // The part that is always wrong: two controls want Escape, and the one the
    // reader is standing in has to answer first.
    function test_escapeClosesAnOpenListBeforeItClosesTheDialog() {
        const dialog = openDialog({})
        const range = findChild(dialog, "clearTimeRange")
        tryVerify(function () {
            return category(dialog, "cookies").activeFocus
        })

        let dismissals = 0
        dialog.dismissed.connect(function () {
            dismissals += 1
        })

        range.open()
        tryVerify(function () {
            return range.popupOpen
        })
        keyClick(Qt.Key_Escape)
        tryVerify(function () {
            return !range.popupOpen
        })
        compare(dismissals, 0)

        keyClick(Qt.Key_Escape)
        compare(dismissals, 1)
        // Return belongs to the list while the list is open, so a dialog that
        // confirmed on the way past would have acted on the first Escape.
        compare(dialog.confirmCount, 0)
    }

    // An empty selection used to be a press that silently did nothing, because
    // the core refuses an empty `dataTypes` and the call site ignored the
    // answer. The button says so before it is pressed instead.
    function test_confirmIsRefusedWhileNothingIsTicked() {
        const dialog = openDialog({})
        const confirm = findChild(dialog, "clearBrowsingDataConfirm")
        compare(confirm.enabled, true)

        const order = testCase.everyCategory
        for (let index = 0; index < order.length; ++index)
            category(dialog, order[index]).clicked()
        compare(dialog.categories.length, 0)
        compare(confirm.enabled, false);

        // And a refused confirm is refused by the key as well as by the button.
        keyClick(Qt.Key_Return)
        compare(dialog.confirmCount, 0)
    }

    // Every Space has no prior art to borrow, so it asks for the typed guard
    // the core enforces — and asks in the dialog, where the reader is told
    // before pressing rather than by a button that quietly did nothing.
    function test_everySpaceAsksForTheTypedGuardAndIsNeverInherited() {
        const dialog = openDialog({})
        const confirmation = findChild(dialog, "clearEverySpaceConfirmation")
        const confirm = findChild(dialog, "clearBrowsingDataConfirm")
        verify(!confirmation.visible)

        dialog.everySpace = true
        verify(confirmation.visible)
        compare(confirm.enabled, false)
        // The kit's field does not take Tab by itself, and the confirm button
        // now depends on what is typed in this one.
        compare(confirmation.activeFocusOnTab, true)

        confirmation.text = "CLEAR ALL"
        compare(confirm.enabled, true)
        keyClick(Qt.Key_Return)
        compare(dialog.confirmCount, 1)
        compare(dialog.lastConfirm.everySpace, true)
        compare(dialog.lastConfirm.confirmation, "CLEAR ALL");

        // Closed and opened again, the scope is back to the one Space and the
        // guard is empty: it is the one argument the dialog never inherits.
        dialog.open = false
        dialog.open = true
        tryVerify(function () {
            return dialog.visible
        })
        compare(dialog.everySpace, false)
        compare(confirmation.text, "")
    }

    // All time is the one range that is not a duration back from now.
    function test_theRangeIsTheDurationTheDropdownNames() {
        const dialog = openDialog({})
        const before = Date.now()
        keyClick(Qt.Key_Return)
        verify(dialog.lastConfirm.since <= before - 86400000 + 1000)
        verify(dialog.lastConfirm.since > 0)

        findChild(dialog, "clearTimeRange").changed("0")
        compare(dialog.range, "0")
        keyClick(Qt.Key_Return)
        compare(dialog.lastConfirm.since, 0)
    }
}
