import QtQuick
import qs.Commons

// What a Space at rest shows instead of a page. There is no page: nothing has
// been opened yet, or the last one has been closed, and an empty viewport
// teaches nothing about a browser driven from the keyboard. So the keymap
// itself stands in for the page — every browser command and the keys that run
// it, read from the same KeyMap the window dispatches through and the same
// command registry the command panel lists, so the sheet cannot promise a key
// the window does not answer or name a command that no longer exists.
//
// It takes the sidebar's fill and the sidebar's translucency rather than the
// opaque backing a webpage viewport needs. A page has to be shown against
// something solid; a resting Space does not, and letting the desktop through
// here makes the window read as one plate rather than a lit panel beside a
// dark hole.
Rectangle {
    id: root
    objectName: "startPage"

    property var colors
    property string iconFontFamily
    property var commands
    property var keymap
    property bool privateWindow: false
    property bool open: false

    // Standing in for a page that does not exist, or summoned over one that
    // does. Standing in, the sheet is the viewport and takes the sidebar's
    // translucency, with nothing to dismiss because there is nothing behind it.
    // Summoned, it is a sheet over a live page: opaque, because a webpage read
    // through a list of shortcuts is neither, and closeable, because the reader
    // came from somewhere.
    property bool overPage: false

    // The page to blur behind the sheet, when there is one. Must not be an
    // ancestor of this item.
    property Item pageSource: null

    signal closed()

    // The order the sheet reads in, and the only groups it shows: the command
    // registry also groups the page's own scrolling and link hints, and those
    // belong to a page rather than to the empty viewport standing in for one.
    readonly property var groups: ["navigation", "tabs", "spaces", "interface", "developer"]

    // Commands a Private window cannot run are left out rather than listed
    // dead: pinning, moving a tab and the Spaces themselves belong to the
    // Spaces a Private window is deliberately outside of.
    readonly property var privateExclusions: ["pin-tab", "move-tab", "next-space",
        "select-space", "new-space"]

    // Rebuilt whenever the keymap is: reading `browserBindings` here is what
    // makes an edited keyboard configuration reach the sheet, since the keys
    // themselves come from a function call QML cannot watch.
    readonly property var sections: {
        // Read for the dependency alone: the keys come from keysFor(), and a
        // function call is not something QML can watch for changes.
        void (root.keymap ? root.keymap.browserBindings : null)
        const descriptions = root.commands ? root.commands.descriptions : ({})
        const list = []
        for (let group = 0; group < root.groups.length; ++group) {
            const name = root.groups[group]
            const entries = []
            for (const command in descriptions) {
                if (descriptions[command].group !== name) continue
                if (root.privateWindow
                    && root.privateExclusions.indexOf(command) !== -1) continue
                // A command the engine or the window cannot carry out here has
                // no key worth promising. The command registry decides that,
                // so the sheet and the command panel cannot disagree.
                if (root.commands && !root.commands.available(command)) continue
                const keys = root.keymap ? root.keymap.keysFor(command) : ""
                // A command with no binding is reachable from the command
                // panel and has nothing to say on a sheet of keys.
                if (keys.length === 0) continue
                entries.push({"title": descriptions[command].title, "keys": keys})
            }
            if (entries.length > 0) list.push({"group": name, "entries": entries})
        }
        return list
    }

    visible: open
    // The fill is the backdrop's: over a page it goes on top of that page
    // blurred, and standing in for one it is all there is.
    color: "transparent"
    focus: open && overPage

    // Summoned over a page, the sheet has to hear Escape itself: the page it
    // covers is what otherwise holds the keyboard.
    onOpenChanged: if (open && overPage) forceActiveFocus()

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape) {
            root.closed()
            event.accepted = true
        }
    }

    PageBackdrop {
        objectName: "shortcutsBackdrop"
        anchors.fill: parent
        source: root.pageSource
        // Standing in for the page it is a plate like the sidebar and takes the
        // sidebar's translucency; over a page it is a sheet, and lets more of
        // what it covers through.
        tint: root.overPage ? root.colors.sheet : root.colors.sidebar
    }

    Flickable {
        id: sheetView
        anchors.fill: parent
        contentWidth: width
        contentHeight: sheet.y + sheet.implicitHeight + 56
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: sheet
            x: Math.max(40, (sheetView.width - width) / 2)
            y: 56
            width: Math.min(760, Math.max(240, sheetView.width - 80))
            spacing: 28

            Item {
                width: parent.width
                height: 34

                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Shortcuts"
                    color: root.colors.text
                    font.family: Style.font.family
                    font.pixelSize: 26
                    Accessible.role: Accessible.Heading
                    Accessible.name: "Keyboard shortcuts"
                }

                ChromeButton {
                    objectName: "closeShortcutsButton"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    visible: root.overPage
                    icon: "close"
                    accessibleName: "Close shortcuts"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.closed()
                }
            }

            Grid {
                id: columns
                width: parent.width
                // One column while the viewport is narrow: a two-column sheet
                // whose keys and titles collide reads worse than a long one.
                columns: width >= 560 ? 2 : 1
                columnSpacing: 40
                rowSpacing: 26

                Repeater {
                    model: root.sections

                    Column {
                        id: groupBlock

                        required property var modelData

                        width: (columns.width - columns.columnSpacing * (columns.columns - 1))
                            / columns.columns
                        spacing: 5

                        SectionLabel {
                            width: parent.width
                            colors: root.colors
                            text: groupBlock.modelData.group
                        }

                        Repeater {
                            model: groupBlock.modelData.entries

                            Item {
                                id: entryRow

                                required property var modelData

                                width: parent.width
                                height: 24
                                Accessible.role: Accessible.StaticText
                                Accessible.name: modelData.title + ": " + modelData.keys

                                Text {
                                    anchors.left: parent.left
                                    anchors.right: entryKeys.left
                                    anchors.rightMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: entryRow.modelData.title
                                    color: root.colors.text
                                    elide: Text.ElideRight
                                    font.family: Style.font.family
                                    font.pixelSize: Style.font.body
                                }

                                // The keys are set exactly as the command panel
                                // sets them, so the same command reads the same
                                // way in both places.
                                Text {
                                    id: entryKeys
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: entryRow.modelData.keys
                                    color: root.colors.mutedText
                                    opacity: 0.85
                                    font.family: Style.font.family
                                    font.pixelSize: Style.font.caption
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
