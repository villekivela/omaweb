import QtQuick
import QtTest
import qs.Commons
import "../../src/ui" as Omaweb

// The Start page's geometry, which is derived rather than written down. Every
// number that decides how wide a column is, how many columns there are and how
// tall a row is comes from the type the theme is set in and from the bindings
// the keymap actually holds. So it is asserted at more than one width *and*
// more than one type size: the type size is the axis a pixel count silently
// breaks, and the two axes the kit exposes — the type tokens and the spacing
// scale — are checked one at a time, so neither can stand in for the other.
TestCase {
    id: testCase
    name: "StartPageLayout"
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
        "navigation": 8, "page": 9, "tabs": 15,
        "spaces": 4, "interface": 13, "developer": 2
    })

    readonly property var privateExclusions: ["pin-tab", "move-tab", "next-space",
        "select-space", "new-space"]

    readonly property string longestTitle: "Keep this Pinned tab active"
    readonly property string longestKeys: "Ctrl+Shift+Alt+Backspace"

    // The command a group's nth entry stands for. The first entries of `spaces`
    // and `tabs` take the names a Private window refuses, so the fixture loses
    // content there exactly as the real registry does.
    function commandName(group, index) {
        if (group === "spaces" && index < 3)
            return testCase.privateExclusions[index + 2]
        if (group === "tabs" && index < 2)
            return testCase.privateExclusions[index]
        return group + "-" + index
    }

    function buildDescriptions() {
        const descriptions = ({})
        for (const group in testCase.groupSizes) {
            const count = testCase.groupSizes[group]
            for (let index = 0; index < count; ++index) {
                const title = (index === 0)
                    ? testCase.longestTitle
                    : group + " command " + index
                descriptions[testCase.commandName(group, index)]
                    = {"group": group, "title": title}
            }
        }
        return descriptions
    }

    function buildBindings() {
        const bindings = ({})
        let first = true
        for (const group in testCase.groupSizes) {
            const count = testCase.groupSizes[group]
            for (let index = 0; index < count; ++index) {
                const keys = first ? testCase.longestKeys : "Ctrl+" + group.charAt(0) + index
                first = false
                bindings[keys] = testCase.commandName(group, index)
            }
        }
        return bindings
    }

    Component {
        id: sheetComponent

        Omaweb.StartPage {
            id: sheet

            property var bindings: testCase.buildBindings()

            colors: testCase.colorsFixture
            iconFontFamily: ""
            open: true
            width: testCase.wideViewport
            height: 1200

            commands: QtObject {
                readonly property var descriptions: testCase.buildDescriptions()
                function available(command) { return true }
            }

            keymap: QtObject {
                readonly property var browserBindings: sheet.bindings
                function keysFor(command) {
                    for (const binding in sheet.bindings) {
                        if (sheet.bindings[binding] === command) return binding
                    }
                    return ""
                }
            }
        }
    }

    FontMetrics {
        id: keyMetrics
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        font.bold: true
    }

    // What the longest binding actually paints, rather than what it advances
    // by. A column reserved from the advance alone can still elide the one
    // chord it was measured from, and eliding the longest key is the failure
    // the measurement exists to prevent.
    Text {
        id: keyProbe
        visible: false
        text: testCase.longestKeys
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        font.bold: true
    }

    property var savedFontOverrides: ({})
    property real savedSpacingScale: 1
    property var liveSheet: null

    function initTestCase() {
        savedFontOverrides = Style.fontOverrides
        savedSpacingScale = Style.spacingScale
    }

    // A failing verify() throws, so nothing after it in the test body runs. The
    // sheet and the singleton it moved are put back here instead, where one
    // test's failure cannot leave the next one reading a shell it did not set.
    function cleanup() {
        Style.fontOverrides = savedFontOverrides
        Style.spacingScale = savedSpacingScale
        if (liveSheet !== null) {
            liveSheet.destroy()
            liveSheet = null
        }
    }

    // The type the theme sets. shell.toml pins these tokens directly, and
    // Omaweb's own theme sets the base size the kit derives them from; either
    // way the sheet has to re-measure. `fontBaseSize` itself cannot be driven
    // from here, because Omaweb's theme owns it and writes it straight back.
    function useTypeTokens(scale) {
        Style.fontOverrides = ({
            "caption": Math.round(10 * scale),
            "body-small": Math.round(11 * scale),
            "body": Math.round(12 * scale),
            "subtitle": Math.round(13 * scale),
            "title": Math.round(14 * scale),
            "heading": Math.round(16 * scale),
            "display": Math.round(24 * scale),
            "display-large": Math.round(28 * scale)
        })
    }

    // The rhythm the theme sets, which is the other half of the same question:
    // `[spacing] scale` makes the whole shell denser or roomier without
    // touching the type.
    function useSpacingScale(scale) {
        Style.spacingScale = scale
    }

    function makeSheet() {
        liveSheet = sheetComponent.createObject(testCase)
        verify(liveSheet !== null)
        return liveSheet
    }

    // The key column is the widest binding the keymap actually sets, measured
    // in the face it is drawn in — and wide enough for what that binding
    // paints, not merely for what it advances by.
    function test_keyColumnIsMeasuredFromTheWidestBinding() {
        const sheet = makeSheet()
        verify(sheet.keyColumnWidth >= Math.ceil(keyProbe.implicitWidth))
        // And no wider than that binding needs: the rest of the row is title.
        // One body glyph of slack is the most a rounded measurement can add.
        verify(sheet.keyColumnWidth
            <= Math.ceil(keyMetrics.advanceWidth(testCase.longestKeys)) + Style.font.body)
    }

    // Everything on the row grows with the type, so the column that holds it
    // has to grow too. The old sheet grew the keys and kept the column, which
    // is what elided the titles at a large theme font.
    function test_columnWidthGrowsWithTheTypeSize() {
        const sheet = makeSheet()
        const smallKeys = sheet.keyColumnWidth
        const smallColumn = sheet.minimumColumnWidth
        const smallRow = sheet.rowHeight

        useTypeTokens(2)
        verify(sheet.keyColumnWidth > smallKeys)
        verify(sheet.minimumColumnWidth > smallColumn)
        verify(sheet.rowHeight > smallRow)
        // The row still has room for the line it draws and the rule under it.
        verify(sheet.rowHeight > Style.font.body)
        verify(sheet.keyColumnWidth >= Math.ceil(keyProbe.implicitWidth))
    }

    // A column count is a question about the type, not about pixels: the same
    // viewport holds fewer columns when the theme sets a larger font.
    function test_columnCountFollowsTheViewportAndTheTypeSize() {
        const sheet = makeSheet()
        const wide = sheet.columnCount
        verify(wide > 1)

        sheet.width = 420
        compare(sheet.columnCount, 1)

        sheet.width = testCase.wideViewport
        compare(sheet.columnCount, wide)
        useTypeTokens(2)
        verify(sheet.columnCount < wide)
        verify(sheet.columnCount >= 1)
    }

    // A wide window is filled rather than left two thirds empty beside a capped
    // column, and the sheet that fits stops scrolling.
    function test_wideViewportIsFilledInsteadOfCapped() {
        const sheet = makeSheet()
        verify(sheet.contentWidth > 760)
        verify(sheet.contentWidth <= sheet.width)
        // Nothing below the fold on a window this size.
        verify(sheet.contentHeight <= sheet.height)
    }

    // No column may hold more than its share: the packing is what stops a
    // four-entry group reserving a fifteen-entry row.
    function test_groupsArePackedWithoutReservingBlankRows() {
        const sheet = makeSheet()
        const columns = sheet.layoutColumns
        compare(columns.length, sheet.columnCount)
        verify(sheet.columnCount > 1)

        // Group order is meaningful, so the packing may only cut it, never
        // reorder it.
        const flattened = []
        const heights = []
        for (let column = 0; column < columns.length; ++column) {
            let height = 0
            for (let index = 0; index < columns[column].length; ++index) {
                flattened.push(columns[column][index].group)
                height += columns[column][index].entries.length + 1
            }
            verify(columns[column].length > 0)
            heights.push(height)
        }
        compare(flattened.length, sheet.sections.length)
        for (let section = 0; section < sheet.sections.length; ++section)
            compare(flattened[section], sheet.sections[section].group)

        // Row-wise flow through a Grid pairs the largest group with the
        // smallest and pays for the difference twice: each row costs its
        // tallest group. The packing has to beat that outright, or a
        // four-entry group is still reserving a fifteen-entry row.
        let tallest = 0
        for (let column = 0; column < heights.length; ++column)
            tallest = Math.max(tallest, heights[column])

        let rowWise = 0
        for (let index = 0; index < sheet.sections.length; index += sheet.columnCount) {
            let row = 0
            for (let offset = 0; offset < sheet.columnCount
                && index + offset < sheet.sections.length; ++offset) {
                row = Math.max(row, sheet.sections[index + offset].entries.length + 1)
            }
            rowWise += row
        }
        verify(tallest < rowWise)
    }

    // The narrow window is the same derivation reaching its floor, not a second
    // special case.
    function test_narrowViewportReachesOneColumnWithoutASpecialCase() {
        const sheet = makeSheet()
        sheet.width = testCase.narrowViewport
        compare(sheet.columnCount, 1)
        compare(sheet.layoutColumns.length, 1)
        compare(sheet.layoutColumns[0].length, sheet.sections.length)
        verify(sheet.contentWidth <= sheet.width)
    }

    // The sheet's own margins are the kit's rhythm, so a theme that makes the
    // shell denser or roomier moves them without touching the type.
    function test_marginsFollowTheThemeSpacingScale() {
        const sheet = makeSheet()
        const margin = sheet.sideMargin
        const inset = sheet.topInset
        const gap = sheet.columnGap
        verify(margin > 0)

        useSpacingScale(2)
        verify(sheet.sideMargin > margin)
        verify(sheet.topInset > inset)
        verify(sheet.columnGap > gap)
    }

    // A Private window drops the commands it cannot run, so the content the
    // layout derives from is not the same in every window: the columns are
    // measured from what this window shows, not from the registry.
    function test_layoutIsDerivedFromWhatThisWindowShows() {
        const sheet = makeSheet()

        function listed() {
            let count = 0
            for (let group = 0; group < sheet.sections.length; ++group)
                count += sheet.sections[group].entries.length
            return count
        }

        const ordinary = listed()
        const ordinaryColumn = sheet.minimumColumnWidth
        verify(ordinary > 0)

        sheet.privateWindow = true
        compare(listed(), ordinary - testCase.privateExclusions.length)
        // The column is re-measured from the shorter list rather than kept at
        // the ordinary window's width.
        verify(sheet.minimumColumnWidth <= ordinaryColumn)
        compare(sheet.layoutColumns.length, sheet.columnCount)
    }

    // Summoned over a page the sheet carries a close affordance the resting one
    // does not, and the heading has to leave room for it.
    function test_theSummonedRoleLeavesRoomForItsCloseAffordance() {
        const sheet = makeSheet()
        sheet.width = testCase.narrowViewport
        const resting = sheet.headingWidth

        sheet.overPage = true
        verify(sheet.headingWidth > resting)
        compare(sheet.headingWidth - resting, sheet.closeSize + sheet.keyGap)
        // The same derivation still serves: one column at this width.
        compare(sheet.columnCount, 1)
    }

    // The keymap is read for its own sake, not sampled once: an edited keyboard
    // configuration that drops the longest chord has to give the key column
    // back the width that chord was holding.
    function test_theKeyColumnIsReDerivedWhenTheKeymapChanges() {
        const sheet = makeSheet()
        const wide = sheet.keyColumnWidth

        const shortened = ({})
        const bindings = sheet.bindings
        for (const binding in bindings) {
            if (binding === testCase.longestKeys) continue
            shortened[binding] = bindings[binding]
        }
        sheet.bindings = shortened

        verify(sheet.keyColumnWidth < wide)
        verify(sheet.keyColumnWidth > 0)
    }
}
