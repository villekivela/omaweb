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

    signal closed

    // The order the sheet reads in, and the only groups it shows: the command
    // registry also groups the page's own scrolling and link hints, and those
    // belong to a page rather than to the empty viewport standing in for one.
    // The page group here is Omaweb's own page commands — find, zoom, print —
    // which the registry leaves out wherever there is no page to run them on.
    readonly property var groups: ["navigation", "page", "tabs", "spaces", "interface", "developer"]

    // Commands a Private window cannot run are left out rather than listed
    // dead: pinning, moving a tab and the Spaces themselves belong to the
    // Spaces a Private window is deliberately outside of.
    readonly property var privateExclusions: ["pin-tab", "move-tab", "next-space", "select-space",
        "new-space"]

    // Rebuilt whenever the keymap is: reading `browserBindings` here is what
    // makes an edited keyboard configuration reach the sheet, since the keys
    // themselves come from a function call QML cannot watch.
    readonly property var sections: {
        // Read for the dependency alone: the keys come from keysFor(), and a
        // function call is not something QML can watch for changes.
        void (root.keymap ? root.keymap.browserBindings : null);
        const descriptions = root.commands ? root.commands.descriptions : ({});
        const list = [];
        for (let group = 0; group < root.groups.length; ++group) {
            const name = root.groups[group];
            const entries = [];
            for (const command in descriptions) {
                if (descriptions[command].group !== name)
                    continue;
                if (root.privateWindow && root.privateExclusions.indexOf(command) !== -1)
                    continue;
                // A command the engine or the window cannot carry out here has
                // no key worth promising. The command registry decides that,
                // so the sheet and the command panel cannot disagree.
                if (root.commands && !root.commands.available(command))
                    continue;
                const keys = root.keymap ? root.keymap.keysFor(command) : "";
                // A command with no binding is reachable from the command
                // panel and has nothing to say on a sheet of keys.
                if (keys.length === 0)
                    continue;
                entries.push({
                                 "title": descriptions[command].title,
                                 "keys": keys
                             });
            }
            if (entries.length > 0)
                list.push({
                              "group": name,
                              "entries": entries
                          });
        }
        return list;
    }

    // ------------------------------------------------------------- geometry
    //
    // The sheet knows every command and every binding it will draw before it
    // lays anything out, so it measures them. A pixel count written here would
    // be right at one theme font size and wrong at every other, and the theme
    // owns the font size: `Style.spacing.*` and `Style.space()` already move
    // with it, and so must the column that holds the keys.

    FontMetrics {
        id: keyMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        font.bold: true
    }

    FontMetrics {
        id: titleMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    readonly property string headingText: "Keyboard commands"

    TextMetrics {
        id: headingMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.display
        text: root.headingText
    }

    // Every gap is the kit's own scale rather than a pixel count, so a theme
    // that makes the shell denser or roomier moves the sheet with everything
    // else. `Style.space()` is what keeps the proportions the sheet already had.
    readonly property int sideMargin: Style.space(40)
    readonly property int topInset: Style.space(56)
    readonly property int columnGap: Style.space(40)
    // The gap a group adds on top of the one its own label reserves.
    readonly property int groupGap: Style.space(10)
    readonly property int entryGap: Style.space(5)
    readonly property int closeSize: Style.space(30)
    readonly property int headingGap: Style.space(28)
    readonly property int keyGap: Style.spacing.xl

    // The widest string any row will draw in one of its two columns, measured
    // in the face that row is drawn in. Six and a half characters was a guess,
    // and a guess is too narrow for a long chord and too wide for a keymap
    // without one.
    function widestOf(metrics, field) {
        let widest = 0;
        for (let group = 0; group < root.sections.length; ++group) {
            const entries = root.sections[group].entries;
            for (let index = 0; index < entries.length; ++index)
                widest = Math.max(widest, metrics.advanceWidth(entries[index][field]));
        }
        return Math.ceil(widest);
    }

    readonly property int keyColumnWidth: {
        // The font is read here for the dependency alone: advanceWidth()
        // measures in C++ off a font the binding never otherwise touches, so a
        // theme that changes the type would leave the column on the last size
        // it was measured at.
        void (keyMetrics.font.pixelSize);
        void (keyMetrics.font.family);
        return root.widestOf(keyMetrics, "keys");
    }

    readonly property int titleColumnWidth: {
        void (titleMetrics.font.pixelSize);
        void (titleMetrics.font.family);
        return root.widestOf(titleMetrics, "title");
    }

    // A column is exactly as wide as the widest row it has to hold: a key, the
    // gap, and a title that fits on one line. That is also the cap the sheet
    // used to spend on 760 pixels — a column no wider than its own content
    // cannot run away on a wide display, and there is nothing left to guess.
    readonly property int minimumColumnWidth: root.keyColumnWidth + root.keyGap
                                              + root.titleColumnWidth
    readonly property int availableWidth: Math.max(0, root.width - root.sideMargin * 2)

    // How many of those fit, never more than there are groups to put in them.
    // There is no threshold constant: the answer is a function of the type, so
    // it stays right when the theme changes its size, and it reaches one column
    // in a narrow window by the same arithmetic rather than by a special case.
    readonly property int columnCount: {
        if (root.sections.length === 0)
            return 1;
        const fits = Math.floor((root.availableWidth + root.columnGap) / Math.max(1,
                                                                                  root.minimumColumnWidth
                                                                                  + root.columnGap));
        return Math.max(1, Math.min(root.sections.length, fits));
    }

    readonly property int columnWidth: Math.max(1, Math.min(root.minimumColumnWidth,
                                                            root.availableWidth))
    readonly property int gridWidth: root.columnWidth * root.columnCount + root.columnGap * (root.columnCount
                                                                                             - 1)

    // A row is the body line plus the room its rule needs under it, so the text
    // never grows into the hairline the way a fixed 24 pixels let it.
    readonly property int rowHeight: Math.ceil(titleMetrics.height) + Style.spacing.lg

    readonly property int headingWidth: Math.ceil(headingMetrics.advanceWidth) + (root.overPage
                                                                                  ? root.closeSize
                                                                                    + root.keyGap :
                                                                                    0)

    // Both roles take the same derivation, because both want the same thing:
    // every key in view at once. Standing in for a page the sheet has the whole
    // viewport, and summoned over one it is read by someone who needs a key
    // now — a column count that fits the window serves the impatient reader
    // better than a narrow sheet that scrolls.
    readonly property int contentWidth: Math.max(root.gridWidth, Math.min(root.availableWidth,
                                                                          root.headingWidth))
    readonly property int contentHeight: sheet.y + sheet.implicitHeight + root.topInset

    // The groups split into `columnCount` columns so that the tallest column is
    // as short as it can be. Group order is meaningful, so a column may only
    // cut the list, never reorder it; cutting it well is what stops a
    // four-entry group reserving a fifteen-entry row, which is what row-wise
    // flow through a Grid did.
    function packColumns(sections, count) {
        const total = sections.length;
        if (total === 0)
            return [];
        if (count <= 1)
            return [sections.slice()];
        const columns = Math.min(count, total);

        // A group costs its entries plus the label above them.
        const weights = [];
        for (let index = 0; index < total; ++index)
            weights.push(sections[index].entries.length + 1);

        // best[k][i] is the shortest possible tallest column when the first i
        // groups are split into k columns; cut[k][i] remembers where that
        // split's last column starts, which is what turns the number back into
        // an answer. Six groups and at most six columns, so the cost of being
        // exact here is nothing.
        const best = [];
        const cut = [];
        for (let k = 0; k <= columns; ++k) {
            const row = [];
            const cuts = [];
            for (let i = 0; i <= total; ++i) {
                row.push(Number.MAX_VALUE);
                cuts.push(0);
            }
            best.push(row);
            cut.push(cuts);
        }
        best[0][0] = 0;
        for (let k = 1; k <= columns; ++k) {
            for (let i = k; i <= total; ++i) {
                let tail = 0;
                for (let j = i; j >= k; --j) {
                    tail += weights[j - 1];
                    const height = Math.max(best[k - 1][j - 1], tail);
                    if (height < best[k][i]) {
                        best[k][i] = height;
                        cut[k][i] = j - 1;
                    }
                }
            }
        }

        const packed = [];
        let end = total;
        for (let k = columns; k >= 1; --k) {
            const start = cut[k][end];
            packed.unshift(sections.slice(start, end));
            end = start;
        }
        return packed;
    }

    readonly property var layoutColumns: root.packColumns(root.sections, root.columnCount)

    visible: open
    // The fill is the backdrop's: over a page it goes on top of that page
    // blurred, and standing in for one it is all there is.
    color: "transparent"
    focus: open && overPage

    // Summoned over a page, the sheet has to hear Escape itself: the page it
    // covers is what otherwise holds the keyboard.
    onOpenChanged: if (open && overPage)
                       forceActiveFocus()

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.closed();
            event.accepted = true;
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
        contentHeight: root.contentHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: sheet
            x: Math.max(root.sideMargin, (sheetView.width - width) / 2)
            y: root.topInset
            width: root.contentWidth
            spacing: root.headingGap

            Item {
                width: parent.width
                height: heading.height + eyebrow.height + Style.spacing.md

                // What the sheet is, above what it is called: the line names
                // the place and the two keys that reach it and leave it, which
                // is the one thing a sheet of keys cannot itself list.
                Text {
                    id: eyebrow
                    objectName: "shortcutsEyebrow"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    text: root.overPage ? "shortcuts · esc closes" : "shortcuts"
                    color: root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                    font.bold: true
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 1.6
                    Accessible.ignored: true
                }

                Text {
                    id: heading
                    anchors.left: parent.left
                    anchors.top: eyebrow.bottom
                    anchors.topMargin: Style.spacing.md
                    text: root.headingText
                    color: root.colors.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.display
                    Accessible.role: Accessible.Heading
                    Accessible.name: "Keyboard shortcuts"
                }

                ChromeButton {
                    objectName: "closeShortcutsButton"
                    anchors.right: parent.right
                    anchors.verticalCenter: heading.verticalCenter
                    width: root.closeSize
                    height: root.closeSize
                    visible: root.overPage
                    icon: "close"
                    accessibleName: "Close shortcuts"
                    fontFamily: root.iconFontFamily
                    foreground: root.colors.mutedText
                    accent: root.colors.accent
                    onClicked: root.closed()
                }
            }

            Row {
                id: columns
                spacing: root.columnGap

                Repeater {
                    model: root.layoutColumns

                    Column {
                        id: columnBlock

                        required property var modelData

                        width: root.columnWidth
                        spacing: root.groupGap

                        Repeater {
                            model: columnBlock.modelData

                            Column {
                                id: groupBlock

                                required property var modelData

                                width: root.columnWidth
                                spacing: root.entryGap

                                SectionLabel {
                                    width: parent.width
                                    colors: root.colors
                                    text: groupBlock.modelData.group
                                }

                                Repeater {
                                    model: groupBlock.modelData.entries

                                    // The key comes first and in the accent:
                                    // the reader is here to find which key runs
                                    // a command, and a column of keys is what
                                    // they can scan. The rule under each row is
                                    // what lets a key and its command be read
                                    // across a gap this wide.
                                    Item {
                                        id: entryRow

                                        required property var modelData

                                        width: parent.width
                                        height: root.rowHeight
                                        Accessible.role: Accessible.StaticText
                                        Accessible.name: modelData.title + ": " + modelData.keys

                                        // The keys are set exactly as the
                                        // command panel sets them, so the same
                                        // command reads the same way in both
                                        // places.
                                        Text {
                                            id: entryKeys
                                            anchors.left: parent.left
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: root.keyColumnWidth
                                            text: entryRow.modelData.keys
                                            color: root.colors.accent
                                            elide: Text.ElideRight
                                            font.family: Style.font.family
                                            font.pixelSize: Style.font.body
                                            font.bold: true
                                        }

                                        Text {
                                            anchors.left: entryKeys.right
                                            anchors.leftMargin: root.keyGap
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: entryRow.modelData.title
                                            color: root.colors.text
                                            elide: Text.ElideRight
                                            font.family: Style.font.family
                                            font.pixelSize: Style.font.body
                                        }

                                        Rectangle {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.bottom: parent.bottom
                                            height: Style.spacing.hairline
                                            color: root.colors.separator
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
