import QtQuick
import QtTest
import qs.Commons
import "../../src/ui" as Omaweb

// The Shortcut sheet's geometry, which is derived rather than written down. Every
// number that decides how wide a column is, how many columns there are and how
// tall a row is comes from the type the theme is set in and from the bindings
// the keymap actually holds. So it is asserted at more than one width *and*
// more than one type size: the type size is the axis a pixel count silently
// breaks, and the two axes the kit exposes — the type tokens and the spacing
// scale — are checked one at a time, so neither can stand in for the other.
TestCase {
    id: testCase
    name: "ShortcutSheetLayout"
    when: windowShown

    // A maximised window beside the sidebar on a 2560px display, which is where
    // the sheet was found scrolling with roughly a thousand pixels empty.
    readonly property int wideViewport: 1745
    readonly property int narrowViewport: 320

    readonly property var colorsFixture: ({
                                              text: "#f3f1fa",
                                              mutedText: "#8d88a3",
                                              accent: "#9b87ff",
                                              separator: "#4a4658",
                                              border: "#4a4658",
                                              surface: "#26232f",
                                              sidebar: "#26232fcc",
                                              sheet: "#26232f99",
                                              windowOpaque: "#16151d"
                                          })

    // The shape of the real registry: six groups of very different sizes, so a
    // layout that reserves a fifteen-entry row for a two-entry group shows up
    // as a measurable waste rather than as an opinion. The Spaces commands are
    // named as the registry names them, because those are the ones a Private
    // window drops.
    readonly property var groupSizes: ({
                                           "navigation": 8,
                                           "page": 9,
                                           "tabs": 15,
                                           "spaces": 4,
                                           "interface": 13,
                                           "developer": 2
                                       })

    readonly property var privateExclusions: ["pin-tab", "move-tab", "next-space", "select-space",
        "new-space"]

    readonly property string longestTitle: "Keep this Pinned tab active"
    readonly property string longestKeys: "Ctrl+Shift+Alt+Backspace"

    // The command a group's nth entry stands for. The first entries of `spaces`
    // and `tabs` take the names a Private window refuses, so the fixture loses
    // content there exactly as the real registry does.
    function commandName(group, index) {
        if (group === "spaces" && index < 3)
            return testCase.privateExclusions[index + 2];
        if (group === "tabs" && index < 2)
            return testCase.privateExclusions[index];
        return group + "-" + index;
    }

    function buildDescriptions() {
        const descriptions = ({});
        for (const group in testCase.groupSizes) {
            const count = testCase.groupSizes[group];
            for (let index = 0; index < count; ++index) {
                const title = (index === 0) ? testCase.longestTitle : group + " command " + index;
                descriptions[testCase.commandName(group, index)] = {
                    "group": group,
                    "title": title
                };
            }
        }
        return descriptions;
    }

    function buildBindings() {
        const bindings = ({});
        let first = true;
        for (const group in testCase.groupSizes) {
            const count = testCase.groupSizes[group];
            for (let index = 0; index < count; ++index) {
                const keys = first ? testCase.longestKeys : "Ctrl+" + group.charAt(0) + index;
                first = false;
                bindings[keys] = testCase.commandName(group, index);
            }
        }
        return bindings;
    }

    Component {
        id: sheetComponent

        Omaweb.ShortcutSheet {
            id: sheet

            property var bindings: testCase.buildBindings()

            colors: testCase.colorsFixture
            iconFontFamily: ""
            open: true
            width: testCase.wideViewport
            height: 1200

            commands: QtObject {
                readonly property var descriptions: testCase.buildDescriptions()
                function available(command) {
                    return true;
                }
            }

            keymap: QtObject {
                readonly property var browserBindings: sheet.bindings
                function keysFor(command) {
                    for (const binding in sheet.bindings) {
                        if (sheet.bindings[binding] === command)
                            return binding;
                    }
                    return "";
                }
            }
        }
    }

    // What the longest binding actually paints: its keys as a row of caps, as
    // the sheet draws them. A column reserved from the advance alone can still
    // cut off the one chord it was measured from, and that is the failure the
    // column exists to avoid.
    Component {
        id: keyProbeComponent

        Omaweb.KeyCaps {
            keys: testCase.longestKeys
            colors: testCase.colorsFixture
        }
    }

    // The width the longest binding paints, drawn fresh: a probe kept from
    // before a change of type keeps the size it was laid out at.
    function longestKeysWidth() {
        const probe = keyProbeComponent.createObject(testCase);
        const width = Math.ceil(probe.implicitWidth);
        probe.destroy();
        return width;
    }

    // Both axes the theme can move the sheet along, shared with the other
    // pages that ask the same question of their own layouts.
    ThemeAxis {
        id: theme
    }

    property var liveSheet: null

    function initTestCase() {
        theme.remember();
    }

    // Every test starts from a theme it states rather than the desktop's.
    function init() {
        theme.useStatedTheme();
    }

    // A failing verify() throws, so nothing after it in the test body runs. The
    // sheet and the singleton it moved are put back here instead, where one
    // test's failure cannot leave the next one reading a shell it did not set.
    function cleanup() {
        theme.restore();
        if (liveSheet !== null) {
            liveSheet.destroy();
            liveSheet = null;
        }
    }

    function makeSheet() {
        liveSheet = sheetComponent.createObject(testCase);
        verify(liveSheet !== null);
        return liveSheet;
    }

    // Every item under `item` named `name`.
    function descendants(item, name) {
        let found = [];
        const below = item.contentItem ? [item.contentItem].concat(Array.from(item.children)) :
                                         Array.from(item.children);
        for (const child of below) {
            if (child.objectName === name)
                found.push(child);
            found = found.concat(descendants(child, name));
        }
        return found;
    }

    Component {
        id: keyCapComponent

        Omaweb.KeyCap {
            colors: testCase.colorsFixture
        }
    }

    // A special key is drawn as a symbol, not a word: a Material Symbols icon
    // where the font carries a matching glyph, otherwise the Unicode symbol in
    // the interface face, and without the icon font the Unicode symbol for
    // all. Escape, Ctrl, Alt and the navigation keys without a clear symbol
    // stay words, and every cap keeps the key's spoken name.
    function test_specialKeysAreSymbolsAndTheRestWords() {
        const cap = keyCapComponent.createObject(testCase, {
                                                     "iconFontFamily": "Material Symbols Rounded"
                                                 });
        const icons = {
            "Return": "keyboard_return",
            "Enter": "keyboard_return",
            "Up": "arrow_upward",
            "Down": "arrow_downward",
            "Left": "arrow_back",
            "Right": "arrow_forward",
            "Tab": "keyboard_tab",
            "Backspace": "backspace",
            "Shift": "shift",
            "Space": "space_bar"
        };
        for (const key in icons) {
            compare(cap.faceFor(key).text, icons[key], key);
            verify(cap.faceFor(key).icon, key);
        }
        compare(cap.faceFor("Delete").text, "\u2326");
        verify(!cap.faceFor("Delete").icon);
        for (const key of ["Ctrl", "Alt", "Home", "End", "PageUp", "PageDown", "R", "[", "F10"]) {
            compare(cap.faceFor(key).text, key);
            verify(!cap.faceFor(key).icon, key);
        }
        compare(cap.faceFor("Esc").text, "Esc");
        compare(cap.faceFor("Escape").text, "Esc");
        compare(cap.spokenName("Return"), "Return");
        compare(cap.spokenName("Esc"), "Escape");
        compare(cap.spokenName("Up"), "Up");
        cap.iconFontFamily = "";
        const glyphs = {
            "Return": "\u21b5",
            "Up": "\u2191",
            "Down": "\u2193",
            "Left": "\u2190",
            "Right": "\u2192",
            "Tab": "\u21e5",
            "Backspace": "\u232b",
            "Shift": "\u21e7",
            "Space": "\u2423"
        };
        for (const key in glyphs) {
            compare(cap.faceFor(key).text, glyphs[key], key);
            verify(!cap.faceFor(key).icon, key);
        }
        cap.destroy();
    }

    // A command with two bindings, which the key map joins with a dot, is
    // two runs of caps with the dot between, not one cap holding both.
    function test_alternativeBindingsAreSeparateRunsOfCaps() {
        const probe = keyProbeComponent.createObject(testCase, {
                                                         "keys": "Ctrl+[  \u00b7  H"
                                                     });
        compare(probe.parts.join(), "Ctrl,[,H");
        compare(probe.entries.filter(function (entry) {
            return entry.separator;
        }).length, 1);
        compare(probe.entries[2].key, "\u00b7");
        compare(Math.ceil(probe.implicitWidth), Math.ceil(probe.widthOf("Ctrl+[  \u00b7  H")));
        probe.destroy();
    }

    // Each binding is a row of the website's key caps, one cap to a key, so a
    // chord of four keys is four caps and the column holds the widest row.
    function test_everyBindingIsARowOfKeyCaps() {
        const sheet = makeSheet();
        const rows = descendants(sheet, "keycaps");
        verify(rows.length > 0);
        let longest = 0;
        for (const row of rows) {
            // The rulers that measure the rows hold no key.
            if (row.keys.length === 0)
                continue;
            const caps = descendants(row, "keycap").filter(function (cap) {
                return cap.text.length > 0;
            });
            compare(caps.length, row.parts.length);
            for (const cap of caps) {
                compare(cap.height, 22 * Style.font.body / 12);
                verify(cap.width >= 22 * Style.font.body / 12);
                compare(String(findChild(cap, "keycapLabel").color), String(
                            testCase.colorsFixture.accent));
            }
            longest = Math.max(longest, row.parts.length);
        }
        compare(longest, testCase.longestKeys.split("+").length);
    }

    // The key column is the widest binding the keymap actually sets, measured
    // in the face it is drawn in — and wide enough for what that binding
    // paints, not merely for what it advances by.
    function test_keyColumnIsMeasuredFromTheWidestBinding() {
        const sheet = makeSheet();
        verify(sheet.keyColumnWidth >= testCase.longestKeysWidth());
        // And no wider than that binding needs: the rest of the row is title.
        // One body glyph of slack is the most a rounded measurement can add.
        verify(sheet.keyColumnWidth <= testCase.longestKeysWidth() + Style.font.body);
    }

    // Everything on the row grows with the type, so the column that holds it
    // has to grow too. The old sheet grew the keys and kept the column, which
    // is what elided the titles at a large theme font.
    function test_columnWidthGrowsWithTheTypeSize() {
        const sheet = makeSheet();
        const smallKeys = sheet.keyColumnWidth;
        const smallColumn = sheet.minimumColumnWidth;
        const smallRow = sheet.rowHeight;

        theme.useTypeTokens(2);
        verify(sheet.keyColumnWidth > smallKeys);
        verify(sheet.minimumColumnWidth > smallColumn);
        verify(sheet.rowHeight > smallRow);
        // The row still has room for the line it draws and the rule under it.
        verify(sheet.rowHeight > Style.font.body);
        verify(sheet.keyColumnWidth >= testCase.longestKeysWidth());
    }

    // A column count is a question about the type, not about pixels: the same
    // viewport holds fewer columns when the theme sets a larger font.
    function test_columnCountFollowsTheViewportAndTheTypeSize() {
        const sheet = makeSheet();
        const wide = sheet.columnCount;
        verify(wide > 1);

        sheet.width = 420;
        compare(sheet.columnCount, 1);

        sheet.width = testCase.wideViewport;
        compare(sheet.columnCount, wide);
        theme.useTypeTokens(2);
        verify(sheet.columnCount < wide);
        verify(sheet.columnCount >= 1);
    }

    // A wide window is filled rather than left two thirds empty beside a capped
    // column, and the sheet that fits stops scrolling.
    function test_wideViewportIsFilledInsteadOfCapped() {
        const sheet = makeSheet();
        verify(sheet.contentWidth > 760);
        verify(sheet.contentWidth <= sheet.width);
        // Nothing below the fold on a window this size.
        verify(sheet.contentHeight <= sheet.height);
    }

    // No column may hold more than its share: the packing is what stops a
    // four-entry group reserving a fifteen-entry row.
    function test_groupsArePackedWithoutReservingBlankRows() {
        const sheet = makeSheet();
        const columns = sheet.layoutColumns;
        compare(columns.length, sheet.columnCount);
        verify(sheet.columnCount > 1);

        // Group order is meaningful, so the packing may only cut it, never
        // reorder it.
        const flattened = [];
        const heights = [];
        for (let column = 0; column < columns.length; ++column) {
            let height = 0;
            for (let index = 0; index < columns[column].length; ++index) {
                flattened.push(columns[column][index].group);
                height += columns[column][index].entries.length + 1;
            }
            verify(columns[column].length > 0);
            heights.push(height);
        }
        compare(flattened.length, sheet.sections.length);
        for (let section = 0; section < sheet.sections.length; ++section)
            compare(flattened[section], sheet.sections[section].group);

        // Row-wise flow through a Grid pairs the largest group with the
        // smallest and pays for the difference twice: each row costs its
        // tallest group. The packing has to beat that outright, or a
        // four-entry group is still reserving a fifteen-entry row.
        let tallest = 0;
        for (let column = 0; column < heights.length; ++column)
            tallest = Math.max(tallest, heights[column]);

        let rowWise = 0;
        for (let index = 0; index < sheet.sections.length; index += sheet.columnCount) {
            let row = 0;
            for (let offset = 0; offset < sheet.columnCount && index + offset
                 < sheet.sections.length; ++offset) {
                row = Math.max(row, sheet.sections[index + offset].entries.length + 1);
            }
            rowWise += row;
        }
        verify(tallest < rowWise);
    }

    // The narrow window is the same derivation reaching its floor, not a second
    // special case.
    function test_narrowViewportReachesOneColumnWithoutASpecialCase() {
        const sheet = makeSheet();
        sheet.width = testCase.narrowViewport;
        compare(sheet.columnCount, 1);
        compare(sheet.layoutColumns.length, 1);
        compare(sheet.layoutColumns[0].length, sheet.sections.length);
        verify(sheet.contentWidth <= sheet.width);
    }

    // The sheet's own margins are the kit's rhythm, so a theme that makes the
    // shell denser or roomier moves them without touching the type.
    function test_marginsFollowTheThemeSpacingScale() {
        const sheet = makeSheet();
        const margin = sheet.sideMargin;
        const inset = sheet.topInset;
        const gap = sheet.columnGap;
        verify(margin > 0);

        theme.useSpacingScale(2);
        verify(sheet.sideMargin > margin);
        verify(sheet.topInset > inset);
        verify(sheet.columnGap > gap);
    }

    // A Private window drops the commands it cannot run, so the content the
    // layout derives from is not the same in every window: the columns are
    // measured from what this window shows, not from the registry.
    function test_layoutIsDerivedFromWhatThisWindowShows() {
        const sheet = makeSheet();

        function listed() {
            let count = 0;
            for (let group = 0; group < sheet.sections.length; ++group)
                count += sheet.sections[group].entries.length;
            return count;
        }

        const ordinary = listed();
        const ordinaryColumn = sheet.minimumColumnWidth;
        verify(ordinary > 0);

        sheet.privateWindow = true;
        compare(listed(), ordinary - testCase.privateExclusions.length);
        // The column is re-measured from the shorter list rather than kept at
        // the ordinary window's width.
        verify(sheet.minimumColumnWidth <= ordinaryColumn);
        compare(sheet.layoutColumns.length, sheet.columnCount);
    }

    TextMetrics {
        id: headingMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.display
        text: "Keyboard commands"
    }

    // The sheet always closes back to what it covered, so the heading leaves
    // room for its close affordance.
    function test_theHeadingLeavesRoomForItsCloseAffordance() {
        const sheet = makeSheet();
        sheet.width = testCase.narrowViewport;

        compare(sheet.headingWidth, Math.ceil(headingMetrics.advanceWidth) + sheet.closeSize
                + sheet.keyGap);
        // The same derivation still serves: one column at this width.
        compare(sheet.columnCount, 1);
    }

    // The keymap is read for its own sake, not sampled once: an edited keyboard
    // configuration that drops the longest chord has to give the key column
    // back the width that chord was holding.
    function test_theKeyColumnIsReDerivedWhenTheKeymapChanges() {
        const sheet = makeSheet();
        const wide = sheet.keyColumnWidth;

        const shortened = ({});
        const bindings = sheet.bindings;
        for (const binding in bindings) {
            if (binding === testCase.longestKeys)
                continue;
            shortened[binding] = bindings[binding];
        }
        sheet.bindings = shortened;

        verify(sheet.keyColumnWidth < wide);
        verify(sheet.keyColumnWidth > 0);
    }

    // The keys of each drawn row of caps. A Flickable's content item
    // is also among its children, so `descendants` meets each row twice.
    function drawnKeys(sheet) {
        return descendants(sheet, "keycaps").filter(function (row, index, rows) {
            return row.keys.length > 0 && rows.indexOf(row) === index;
        }).map(function (row) {
            return row.keys;
        });
    }

    // A closed sheet lays nothing out: the sidebar sliding or a Space switch
    // changes what it would show, and the chrome moving at that moment must
    // not wait on it (#594). Opening it still shows the current keymap at the
    // current width.
    function test_aClosedSheetLaysOutOnlyWhenItOpens() {
        const sheet = makeSheet();
        const columns = sheet.layoutColumns;
        const rows = descendants(sheet, "keycaps");
        verify(sheet.columnCount > 1);
        verify(drawnKeys(sheet).indexOf(testCase.longestKeys) !== -1);
        sheet.open = false;

        const shortened = ({});
        for (const binding in sheet.bindings) {
            if (binding !== testCase.longestKeys)
                shortened[binding] = sheet.bindings[binding];
        }
        sheet.bindings = shortened;
        sheet.width = testCase.narrowViewport;
        verify(sheet.layoutColumns === columns, "the closed sheet packed its columns again");
        const kept = descendants(sheet, "keycaps");
        compare(kept.length, rows.length);
        for (let index = 0; index < rows.length; ++index)
            verify(kept[index] === rows[index], "the closed sheet built its rows again");

        sheet.open = true;
        compare(sheet.columnCount, 1);
        compare(sheet.layoutColumns.length, 1);
        verify(sheet.contentWidth <= sheet.width);
        const keys = drawnKeys(sheet);
        compare(keys.length, Object.keys(shortened).length);
        verify(keys.indexOf(testCase.longestKeys) === -1);
    }
}
