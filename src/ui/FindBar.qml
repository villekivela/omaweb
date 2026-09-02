import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// Find belongs to one tab, and so does this bar: what it shows is the query and
// the match position the tab's own engine is holding. Hiding it leaves both
// standing, so coming back to the tab finds the search where it was left.
//
// It docks at the top of the page rather than covering the middle of it,
// because what the reader is looking for is in the page behind it.
Item {
    id: root

    property var colors
    property string iconFontFamily
    property bool open: false
    // The tab's own search, read off its engine. `query` is what the engine was
    // last asked for; the field is what the reader is typing, and the two are
    // the same except while a tab that was never searched is being adopted.
    property string query: ""
    property int matchCount: 0
    property int activeMatch: 0

    readonly property alias text: field.text
    readonly property bool searching: field.text.length > 0

    signal searchRequested(string text, bool forward)
    signal closed()

    // Writing the tab's own query into the field is not the reader typing, so
    // it must not run a search: the matches were cleared with the last page and
    // are the reader's to ask for again.
    property bool adopting: false

    function adopt(value) {
        if (field.text === value) return
        root.adopting = true
        field.text = value
        root.adopting = false
    }

    function focusField() {
        field.focusInput()
        field.selectAllText()
    }

    onQueryChanged: root.adopt(root.query)
    onOpenChanged: if (open) root.adopt(root.query)

    visible: open
    implicitHeight: surface.implicitHeight
    height: open ? implicitHeight : 0
    width: 420

    Omarchy.BorderSurface {
        id: surface
        objectName: "findBar"
        anchors.fill: parent
        radius: Style.cornerRadius
        padding: Style.spacing.md
        color: root.colors.overlay
        borderSpec: Border.controlSpec("normal", root.colors.accent, root.colors.accent)
        implicitHeight: contentTopInset + Style.spacing.controlHeight + contentBottomInset

        Text {
            id: mark
            anchors.left: parent.left
            anchors.leftMargin: surface.contentLeftInset + Style.spacing.md
            anchors.verticalCenter: parent.verticalCenter
            text: "search"
            color: root.colors.mutedText
            font.family: root.iconFontFamily
            font.pixelSize: Style.font.icon
            Accessible.ignored: true
        }

        SettingField {
            id: field
            objectName: "findInput"
            anchors.left: mark.right
            anchors.leftMargin: Style.spacing.lg
            anchors.right: tally.left
            anchors.rightMargin: Style.spacing.lg
            anchors.verticalCenter: parent.verticalCenter
            colors: root.colors
            placeholder: "find in page"
            accessibleName: "Find in page"

            onTextChanged: if (!root.adopting) root.searchRequested(field.text, true)

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Escape) {
                    root.closed()
                    event.accepted = true
                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    root.searchRequested(field.text,
                        (event.modifiers & Qt.ShiftModifier) === 0)
                    event.accepted = true
                }
            }
        }

        // Where the search has reached, in the shape every browser writes it.
        // A query with nothing to match says so rather than showing 0 of 0.
        Text {
            id: tally
            anchors.right: previousMatch.left
            anchors.rightMargin: Style.spacing.md
            anchors.verticalCenter: parent.verticalCenter
            text: {
                if (!root.searching) return ""
                if (root.matchCount === 0) return "no matches"
                return root.activeMatch + "/" + root.matchCount
            }
            color: root.matchCount === 0 && root.searching
                ? root.colors.urgent : root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        ChromeButton {
            id: previousMatch
            objectName: "findPreviousButton"
            anchors.right: nextMatch.left
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 26
            icon: "keyboard_arrow_up"
            accessibleName: "Previous match"
            fontFamily: root.iconFontFamily
            enabled: root.matchCount > 0
            foreground: root.colors.text
            accent: root.colors.accent
            onClicked: root.searchRequested(field.text, false)
        }

        ChromeButton {
            id: nextMatch
            objectName: "findNextButton"
            anchors.right: closeButton.left
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 26
            icon: "keyboard_arrow_down"
            accessibleName: "Next match"
            fontFamily: root.iconFontFamily
            enabled: root.matchCount > 0
            foreground: root.colors.text
            accent: root.colors.accent
            onClicked: root.searchRequested(field.text, true)
        }

        ChromeButton {
            id: closeButton
            objectName: "findCloseButton"
            anchors.right: parent.right
            anchors.rightMargin: surface.contentRightInset
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 26
            icon: "close"
            accessibleName: "Hide find"
            fontFamily: root.iconFontFamily
            foreground: root.colors.text
            accent: root.colors.accent
            onClicked: root.closed()
        }
    }
}
