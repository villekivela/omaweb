import QtQuick
import QtTest
import qs.Commons
import "../../src/ui" as Omaweb

// The settings page's layout, which is derived rather than written down. Every
// margin, gap and measure comes from the kit's spacing scale or from the face
// the text is actually drawn in, so it is asserted on both axes the theme can
// move — the type tokens and the spacing scale — one at a time, so neither can
// stand in for the other. The type size is the axis a pixel count breaks
// silently: a count that is right at one theme font size clips or crowds at
// the next.
TestCase {
    id: testCase
    name: "SettingsPageLayout"
    when: windowShown
    // A TestCase is invisible by default, and this case asks what the page
    // draws: an item under an invisible ancestor reports `visible` false
    // whatever it says itself, so a walk over the tree would skip the section
    // it came to look at.
    visible: true
    width: 1200
    height: 900

    readonly property var colorsFixture: ({
                                              text: "#f3f1fa",
                                              mutedText: "#8d88a3",
                                              accent: "#9b87ff",
                                              privateAccent: "#ffb37a",
                                              urgent: "#ff6f7d",
                                              separator: "#4a4658",
                                              border: "#4a4658",
                                              surface: "#26232f",
                                              sidebar: "#26232fcc",
                                              sheet: "#26232f99",
                                              overlay: "#1f1d27f2",
                                              windowOpaque: "#16151d"
                                          })

    // Two of everything the page lists, so a gap between one row and the next
    // is a thing this test can measure rather than a thing it has to imagine.
    readonly property var retainedTabsFixture: [
        {
            tabId: "kept-one",
            title: "First kept tab",
            url: "https://one.example",
            spaceName: "Personal",
            inspected: false,
            running: true,
            residentBytes: 120 * 1024 * 1024
        },
        {
            tabId: "kept-two",
            title: "Second kept tab",
            url: "https://two.example",
            spaceName: "Work",
            inspected: true,
            running: false,
            residentBytes: 0
        }
    ]

    readonly property var downloadsFixture: [
        {
            path: "/home/reader/Downloads/first.zip",
            state: "completed",
            error: ""
        },
        {
            path: "/home/reader/Downloads/second.zip",
            state: "interrupted",
            error: "the connection was lost"
        }
    ]

    Component {
        id: pageComponent

        Omaweb.SettingsPage {
            colors: testCase.colorsFixture
            iconFontFamily: ""
            open: true
            retainedTabs: testCase.retainedTabsFixture
            downloads: testCase.downloadsFixture
            width: 1200
            height: 900
        }
    }

    // Both axes the theme can move the page along, shared with the other pages
    // that ask the same question of their own layouts.
    ThemeAxis {
        id: theme
    }

    property var livePage: null

    function initTestCase() {
        theme.remember()
    }

    // A failing verify() throws, so nothing after it in the test body runs. The
    // page and the singleton it moved are put back here instead, where one
    // test's failure cannot leave the next one reading a shell it did not set.
    function cleanup() {
        theme.restore()
        if (livePage !== null) {
            livePage.destroy()
            livePage = null
        }
    }

    function makePage() {
        livePage = pageComponent.createObject(testCase)
        verify(livePage !== null)
        return livePage
    }

    // The room between where one item stops being drawn and the next starts,
    // in the page's own coordinates, so a gap is read off what is drawn rather
    // than off the container that spaced it.
    function gapBetween(page, above, below) {
        return Math.round(below.mapToItem(page, 0, 0).y - (above.mapToItem(page, 0, 0).y
                                                           + above.height))
    }

    // The page's own frame is the kit's rhythm rather than a pixel count, so a
    // theme that makes the shell denser or roomier moves it with everything
    // else.
    function test_theFrameFollowsTheThemeSpacingScale() {
        const page = makePage()
        const frame = ["sideMargin", "topInset", "headerGap", "bottomInset", "railGap", "closeSize"]
        const before = ({})
        for (let index = 0; index < frame.length; ++index) {
            verify(page[frame[index]] > 0)
            before[frame[index]] = page[frame[index]]
        }

        theme.useSpacingScale(2)
        for (let index = 0; index < frame.length; ++index)
            verify(page[frame[index]] > before[frame[index]])
    }

    // And the frame is what the page is actually drawn on, rather than a set of
    // properties beside a layout that still carries its own numbers.
    function test_theHeaderAndTheBodyAreDrawnOnThatFrame() {
        const page = makePage()
        const eyebrow = findChild(page, "settingsEyebrow")
        const close = findChild(page, "closeSettingsButton")
        verify(eyebrow !== null)
        verify(close !== null)

        compare(eyebrow.mapToItem(page, 0, 0).x, page.sideMargin)
        compare(eyebrow.mapToItem(page, 0, 0).y, page.topInset)
        compare(close.width, page.closeSize)
        compare(close.height, page.closeSize)
        compare(Math.round(close.mapToItem(page, close.width, 0).x), page.width - page.sideMargin)

        theme.useSpacingScale(2)
        compare(eyebrow.mapToItem(page, 0, 0).x, page.sideMargin)
        compare(eyebrow.mapToItem(page, 0, 0).y, page.topInset)
        compare(close.width, page.closeSize)
    }

    // The rail's rhythm is the theme's too: the room each name takes around
    // itself and the gap between one name and the next both move with the
    // spacing scale, measured from where the names are drawn rather than read
    // off the container that spaced them.
    function test_theRailRhythmFollowsTheThemeSpacing() {
        const page = makePage()
        const first = findChild(page, "settingsSection0")
        const second = findChild(page, "settingsSection1")
        verify(first !== null)
        verify(second !== null);

        // A name leans on nothing: it belongs to the rail, not to what follows.
        compare(first.topPadding, first.bottomPadding)
        verify(first.topPadding > 0)

        function railGapBetweenNames() {
            return testCase.gapBetween(page, first, second)
        }

        const padding = first.topPadding
        tryVerify(function () {
            return railGapBetweenNames() > 0
        })
        const gap = railGapBetweenNames()

        theme.useSpacingScale(3)
        verify(first.topPadding > padding)
        // A Column repositions on the next polish, so the gap is read back
        // rather than sampled the instant the singleton moved.
        tryVerify(function () {
            return railGapBetweenNames() > gap
        })
    }

    // The gap between the blocks a section is made of belongs to the pane, so
    // adding a row to one block cannot change the rhythm of the block under it.
    // Inside a block the rows abut on purpose: each draws the rule that divides
    // it from the row above, and a gap there would break the list into cards.
    function test_thePaneOwnsTheGapBetweenTheBlocksOfASection() {
        const page = makePage()
        const pane = findChild(page, "settingsPane")
        verify(pane !== null)
        verify(pane.spacing > 0);

        // Two cards, which want the pane's gap between them.
        const favicons = findChild(page, "useFavicons")
        const tint = findChild(page, "tintFavicons");
        // Two rows of a ruled list, which do not.
        const firstKept = findChild(page, "retainedTab-kept-one")
        const secondKept = findChild(page, "retainedTab-kept-two")
        verify(favicons !== null)
        verify(tint !== null)
        verify(firstKept !== null)
        verify(secondKept !== null)

        function cardGap() {
            return testCase.gapBetween(page, favicons, tint)
        }
        function ruledGap() {
            return testCase.gapBetween(page, firstKept, secondKept)
        }

        tryVerify(function () {
            return cardGap() === pane.spacing
        })
        tryVerify(function () {
            return ruledGap() === 0
        })

        const spacing = pane.spacing
        theme.useSpacingScale(3)
        verify(pane.spacing > spacing)
        tryVerify(function () {
            return cardGap() === pane.spacing
        })
        tryVerify(function () {
            return ruledGap() === 0
        })
    }

    // And a ruled list is a ruled list wherever it is, not a rule the tabs
    // section happens to keep: the downloads a Space recorded abut the row that
    // names the directory they went to.
    function test_aRuledListAbutsInEverySectionThatHasOne() {
        const page = makePage()
        page.section = 4
        const rows = []
        for (let index = 0; index < testCase.downloadsFixture.length; ++index)
            rows.push(findChild(page, "recordedDownload-" + index))
        for (let index = 0; index < rows.length; ++index)
            verify(rows[index] !== null)

        tryVerify(function () {
            return testCase.gapBetween(page, rows[0], rows[1]) === 0
        })
    }

    // A section label leans toward the list it heads rather than sitting evenly
    // between two, because it belongs to what follows it. The exception is a
    // label that opens its section: leaning away from nothing is dead space, so
    // it takes only the sliver a tall glyph paints above its own box.
    function test_aSectionLabelLeansTowardWhatItHeadsUnlessItOpensTheSection() {
        const page = makePage()
        const keptActive = findLabel(page, "kept active")
        verify(keptActive !== null)
        verify(keptActive.topPadding > keptActive.bottomPadding)
        verify(keptActive.topPadding > keptActive.overshoot)

        page.section = 6
        const privacy = findLabel(page, "privacy")
        verify(privacy !== null)
        compare(privacy.topPadding, privacy.overshoot)
    }

    // The kit's section header carries no objectName of its own, so it is found
    // by the one thing that identifies it: the words it draws.
    function findLabel(item, text) {
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index]
            if (child.text === text && child.overshoot !== undefined)
                return child
            const deeper = findLabel(child, text)
            if (deeper !== null)
                return deeper
        }
        return null
    }

    // Two form fields share a row while a pair still fits the words they ask
    // for. A pane too narrow for that stacks them, rather than shaving each one
    // past its own placeholder — which is what a fixed two columns did, out of
    // sight of any width the page reports.
    function test_pairedFieldsStackRatherThanNarrowPastTheirWords() {
        const page = makePage()
        page.section = 2
        const fields = findChild(page, "subscriptionFields")
        verify(fields !== null)
        verify(page.fieldFloor > 0)

        tryVerify(function () {
            return fields.columns === 2
        })
        verify(fields.fieldWidth >= page.fieldFloor)

        const roomy = page.width
        page.width = page.sideMargin * 2 + page.railWidth + page.railGap + page.fieldFloor * 2
                - Style.spacing.huge
        tryVerify(function () {
            return fields.columns === 1
        })
        verify(fields.fieldWidth >= page.fieldFloor);

        // And back, by the same arithmetic rather than by a remembered state.
        page.width = roomy
        tryVerify(function () {
            return fields.columns === 2
        })
    }

    // A line of explanation beside a row is a measure, not a pixel count: the
    // box grows with the face it is drawn in, so it still wraps to about as
    // many lines when the theme doubles the type. A fixed 260 pixels wrapped
    // the same words into twice the lines and pushed the row it explains off
    // the pane.
    function test_theExplanationIsMeasuredInTheFaceItIsDrawnIn() {
        const page = makePage();
        // Network, which is where the longest of these explanations sits.
        page.section = 3
        const status = findChild(page, "automaticRequestsStatus")
        verify(status !== null)
        verify(page.noteMeasure > 0)

        tryVerify(function () {
            return status.lineCount > 1
        })
        const measure = page.noteMeasure
        const width = status.width
        const lines = status.lineCount

        theme.useTypeTokens(2)
        verify(page.noteMeasure > measure)
        tryVerify(function () {
            return status.width > width
        })
        // The words did not gain a paragraph's worth of lines on the way.
        tryVerify(function () {
            return status.lineCount <= lines + 1
        })
    }

    // The first item under the pane that runs past its right edge, or that had
    // to elide its own text to fit — crowded counts as well as clipped — or
    // null when there is none.
    function overflowingItem(pane, item) {
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index]
            if (child.visible === false)
                continue
            if (child.width > 0 && child.mapToItem(pane, child.width, 0).x > pane.width + 1)
                return child
            // A row that had to elide its own text is crowded even though it
            // fits, which is the other half of what a fixed count costs.
            if (child.truncated === true)
                return child
            const deeper = overflowingItem(pane, child)
            if (deeper !== null)
                return deeper
        }
        return null
    }

    // The whole-page form of the same question the properties above ask one at
    // a time, and it is asked of all eight sections: a pixel count left in any
    // one of them shows up here and nowhere else.
    function test_noSectionOverflowsThePaneAtALargerType() {
        const page = makePage()
        const pane = findChild(page, "settingsPane")
        verify(pane !== null)
        compare(page.sections.length, 8)

        theme.useTypeTokens(2)
        for (let section = 0; section < page.sections.length; ++section) {
            page.section = section
            tryVerify(function () {
                return testCase.overflowingItem(pane, pane) === null
            }, 5000, "section " + page.sections[section] + " overflows the pane")
            // And the section put something in the pane to begin with, so a
            // section that drew nothing cannot pass by being empty.
            tryVerify(function () {
                return pane.height > 0
            })
        }
    }
}
