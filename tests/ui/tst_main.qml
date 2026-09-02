import QtQuick
import QtTest
import Tanto
import "../../src/ui" as Tanto

TestCase {
    id: testCase
    name: "BrowserChrome"
    when: true

    property var window: null

    Component {
        id: windowComponent
        Tanto.Main {}
    }

    // The harness loads Tanto's own default keymap, which this build knows
    // every command in, so nothing is ever ignored in it. These stand the two
    // surfaces up against a keymap that did report something.
    Component {
        id: reportingSettingsComponent

        Tanto.SettingsPage {
            id: reportingSettings

            property string report: ""

            colors: testCase.window.colors
            iconFontFamily: ""
            keyboard: QtObject { property string errorMessage: reportingSettings.report }
            open: true
            section: 1
            width: 900
            height: 700
        }
    }

    Component {
        id: attentionOutlineComponent

        Tanto.SpaceOutline {
            colors: testCase.window.colors
            iconFontFamily: ""
            settingsAttention: true
            width: 300
            height: 600
        }
    }

    function initTestCase() {
        window = windowComponent.createObject(null)
        verify(window !== null)
        window.show()
        wait(50)
    }

    function cleanupTestCase() {
        window.destroy()
    }

    // A Space at rest shows the start page: no engine is spent on the blank
    // tab standing in for a page, and the outline lists no ordinary tab row.
    // A test about a page, an engine or a tab row opens a page first.
    function openPage(url) {
        const engineHost = findChild(window.contentItem, "engineLoader")
        verify(engineHost !== null)
        browser.openInput(url, false)
        tryVerify(function() {
            return engineHost.item !== null
                && engineHost.item.currentUrl.toString() === url
        })
        return engineHost.item
    }

    function verifyApplicationWindowFlags(applicationWindow) {
        verify(Boolean(applicationWindow.flags & Qt.Window))
        if (Qt.platform.os === "osx") {
            verify(!Boolean(applicationWindow.flags & Qt.FramelessWindowHint))
            verify(Boolean(applicationWindow.flags & Qt.ExpandedClientAreaHint))
            verify(Boolean(applicationWindow.flags & Qt.NoTitleBarBackgroundHint))
            compare(applicationWindow.topPadding, 0)
        } else {
            verify(Boolean(applicationWindow.flags & Qt.FramelessWindowHint))
        }
    }

    // The address of the page on show goes to the clipboard on its own: no
    // title, no markup, and nothing at all from a tab that has no address.
    function test_copyAddressPutsOnlyTheAddressOnTheClipboard() {
        SystemClipboard.copyText("something the reader already had")
        browser.openInput("about:blank", true)
        tryVerify(function() { return browser.activeTabBlank })
        verify(!window.commands.available("copy-address"))
        window.commands.run("copy-address", -1)
        compare(SystemClipboard.text(), "something the reader already had")
        browser.closeActiveTab()

        openPage("https://copy-me.example/path?q=1")
        verify(window.commands.available("copy-address"))
        window.commands.run("copy-address", -1)
        compare(SystemClipboard.text(), "https://copy-me.example/path?q=1")
        compare(window.commands.keymap.keysFor("copy-address"),
            Qt.platform.os === "osx" ? "⌘⇧C" : "Ctrl+Shift+C")
    }

    // An engine that supplies no inspector leaves the command listed and
    // unavailable, and takes an open dock with it.
    function test_developerToolsAreUnavailableWithoutAnInspector() {
        const dock = findChild(window.contentItem, "developerToolsDock")
        const engine = openPage("https://no-inspector.example")
        window.commands.run("developer-tools", -1)
        tryVerify(function() { return dock.visible })

        engine.inspectorAvailable = false
        tryVerify(function() { return !window.developerToolsAvailable })
        browser.closeDeveloperTools()
        tryVerify(function() { return !dock.visible })

        // Neither command can be run, and neither promises a key.
        window.commands.run("developer-tools", -1)
        window.commands.run("inspect-element", -1)
        compare(browser.developerToolsTabId, "")
        compare(engine.inspectedElementCount, 0)
        verify(!dock.visible)
        const startPage = findChild(window.contentItem, "startPage")
        compare(startPage.sections.filter(function(section) {
            return section.group === "developer"
        }).length, 0)

        engine.inspectorAvailable = true
        tryVerify(function() { return window.developerToolsAvailable })
    }

    // A Space at rest is running no engine at all, so there is nothing to
    // inspect there either.
    function test_developerToolsAreUnavailableWithoutAnEngine() {
        const dock = findChild(window.contentItem, "developerToolsDock")
        const engineHost = findChild(window.contentItem, "engineLoader")
        // A blank tab is given no engine, so it is the one tab in any Space
        // that has nothing to inspect.
        browser.openInput("about:blank", true)
        tryVerify(function() { return engineHost.item === null })
        verify(window.pagelessViewport)
        verify(!window.developerToolsAvailable)

        const listed = window.commands.actions().filter(function(action) {
            return action.command === "developer-tools"
                || action.command === "inspect-element"
        })
        compare(listed.length, 2)
        for (let index = 0; index < listed.length; ++index) {
            verify(!listed[index].enabled)
            window.commands.invoke(listed[index])
        }
        compare(browser.developerToolsTabId, "")
        verify(!dock.visible)

        // The sheet of keys promises nothing it cannot carry out either, so the
        // registry is the only place that decides.
        const startPage = findChild(window.contentItem, "startPage")
        verify(startPage !== null)
        const developerSections = startPage.sections.filter(function(section) {
            return section.group === "developer"
        })
        compare(developerSections.length, 0)

        browser.closeActiveTab()
    }

    // Developer tools take a column of their own beside the tab they inspect:
    // the page gives up that width rather than being covered by it, and the
    // view in the dock is the one the engine handed over.
    function test_developerToolsDockBesideTheTabTheyInspect() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        const engine = openPage("https://inspected.example")
        const dock = findChild(window.contentItem, "developerToolsDock")
        verify(dock !== null)
        verify(!dock.visible)
        verify(window.developerToolsAvailable)

        window.commands.run("developer-tools", -1)
        tryVerify(function() { return dock.visible })
        verify(engine.developerToolsAttached)
        compare(browser.developerToolsTabId, browser.activeTabId)
        tryCompare(dock, "width", window.developerToolsWidth)
        // The page stops where the dock starts rather than running under it,
        // and the two of them together are the whole viewport. Measured against
        // the viewport rather than against a remembered width, because the
        // sidebar beside it has a width animation of its own.
        const viewport = findChild(window.contentItem, "engineViewport")
        verify(viewport !== null)
        tryVerify(function() {
            return Math.round(engineHost.width + dock.width) === Math.round(viewport.width)
        })
        compare(Math.round(engineHost.x + engineHost.width), Math.round(dock.x))

        // The dock arrives beside the page rather than in front of it, so the
        // keyboard stays where the reader left it.
        verify(engine.activeFocus)

        // The engine's own view is what the dock shows, at the dock's shape.
        const view = engine.developerToolsView
        verify(view !== null)
        compare(view.parent, dock)
        tryCompare(view, "width", dock.width)
        // Drawn in the window's colours rather than the engine's own, and
        // actually painted there: the probe is taken off the dock itself
        // rather than off the property that was set on it.
        compare(String(view.color), String(window.colors.windowOpaque))
        const painted = grabImage(dock)
        verify(Qt.colorEqual(
            painted.pixel(Math.round(dock.width / 2), Math.round(dock.height / 2)),
            window.colors.windowOpaque))

        window.commands.run("developer-tools", -1)
        tryVerify(function() { return !dock.visible })
        verify(!engine.developerToolsAttached)
        compare(browser.developerToolsTabId, "")
        verify(engine.activeFocus)
        // The page takes the whole viewport back.
        tryVerify(function() {
            return Math.round(engineHost.width) === Math.round(viewport.width)
        })
    }

    // One inspector, attached to one tab. It hides behind another tab and comes
    // back with its own, and a Space switch does not move or lose it.
    function test_developerToolsFollowTheTabTheyAreAttachedTo() {
        const dock = findChild(window.contentItem, "developerToolsDock")
        const inspected = openPage("https://follows.example")
        const inspectedTabId = browser.activeTabId
        window.commands.run("developer-tools", -1)
        tryVerify(function() { return dock.visible })
        const view = inspected.developerToolsView

        browser.openInput("https://other.example", true)
        const otherTabId = browser.activeTabId
        tryVerify(function() { return !dock.visible })
        compare(browser.developerToolsTabId, inspectedTabId)

        browser.activateTab(inspectedTabId)
        tryVerify(function() { return dock.visible })
        compare(inspected.developerToolsView, view)

        const otherSpaceId = browser.createSpace("Inspecting")
        verify(browser.switchSpace(otherSpaceId))
        tryVerify(function() { return !dock.visible })
        compare(browser.developerToolsTabId, inspectedTabId)

        verify(browser.switchSpace(browser.spaces.data(
            browser.spaces.index(0, 0), Qt.UserRole + 1)))
        tryVerify(function() { return dock.visible })
        // The same page, still inspected by the same inspector.
        compare(browser.developerToolsTabId, inspectedTabId)
        compare(findChild(window.contentItem, "engineLoader").developerToolsView, view)

        // Asking for them on the other tab moves them rather than opening a
        // second inspector.
        browser.activateTab(otherTabId)
        window.commands.run("developer-tools", -1)
        tryVerify(function() { return dock.visible })
        compare(browser.developerToolsTabId, otherTabId)
        verify(!inspected.developerToolsAttached)

        browser.closeDeveloperTools()
        tryVerify(function() { return !dock.visible })
    }

    // Inspect element opens the dock and asks the page for the target the
    // reader pointed at, which is more than opening the inspector.
    function test_inspectElementOpensTheDockAndAsksForTheTarget() {
        const dock = findChild(window.contentItem, "developerToolsDock")
        const engine = openPage("https://inspect-element.example")
        compare(engine.inspectedElementCount, 0)

        window.commands.run("inspect-element", -1)
        tryVerify(function() { return dock.visible })
        compare(engine.inspectedElementCount, 1)
        compare(browser.developerToolsTabId, browser.activeTabId)

        browser.closeDeveloperTools()
        tryVerify(function() { return !dock.visible })
    }

    // The inspector's own close button, and the tab going away underneath it.
    function test_developerToolsDetachWithTheirTabOrOnRequest() {
        const dock = findChild(window.contentItem, "developerToolsDock")
        const engine = openPage("https://detaches.example")
        window.commands.run("developer-tools", -1)
        tryVerify(function() { return dock.visible })

        engine.simulateDeveloperToolsClose()
        tryVerify(function() { return !dock.visible })
        compare(browser.developerToolsTabId, "")

        browser.openInput("https://kept.example", true)
        const inspectedTabId = browser.activeTabId
        const second = findChild(window.contentItem, "engineLoader").item
        window.commands.run("developer-tools", -1)
        tryVerify(function() { return dock.visible })
        browser.closeTab(inspectedTabId)
        tryVerify(function() { return !dock.visible })
        compare(browser.developerToolsTabId, "")
    }

    // The seam on the right answers the same way the sidebar's does, and the
    // key that points away from the window's edge widens the panel.
    function test_developerToolsWidthAnswersToBothPointerAndKeyboard() {
        window.settingsOpen = false
        window.requestActivate()
        tryVerify(function() { return window.active })
        openPage("https://resized.example")
        window.commands.run("developer-tools", -1)
        const dock = findChild(window.contentItem, "developerToolsDock")
        tryVerify(function() { return dock.visible })
        const resizer = findChild(window.contentItem, "developerToolsResizer")
        verify(resizer !== null)
        window.setDeveloperToolsWidth(window.developerToolsDefaultWidth)
        tryCompare(dock, "width", window.developerToolsDefaultWidth)

        // The handle rides the seam it moves.
        compare(Math.round(resizer.x + resizer.width / 2), Math.round(dock.x))

        resizer.forceActiveFocus()
        compare(window.activeFocusItem.objectName, "developerToolsResizer")
        keyClick(Qt.Key_Left)
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth + 16)
        keyClick(Qt.Key_Right)
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth)
        keyClick(Qt.Key_Left, Qt.ShiftModifier)
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth + 48)
        keyClick(Qt.Key_Space)
        compare(window.developerToolsWidth, window.developerToolsDefaultWidth)

        // A request past either end stops at the end.
        window.setDeveloperToolsWidth(window.developerToolsMaximumWidth + 400)
        compare(window.developerToolsWidth, window.developerToolsMaximumWidth)
        window.setDeveloperToolsWidth(0)
        compare(window.developerToolsWidth, window.developerToolsMinimumWidth)
        window.setDeveloperToolsWidth(window.developerToolsDefaultWidth)

        // The handle reads as part of the dock, so it leaves like the rest of
        // the chrome does.
        keyClick(Qt.Key_Escape)
        const engineHost = findChild(window.contentItem, "engineLoader")
        tryVerify(function() { return engineHost.item.activeFocus })

        browser.closeDeveloperTools()
        tryVerify(function() { return !dock.visible })
    }

    function test_applicationWindowUsesPlatformAppropriateFrame() {
        verifyApplicationWindowFlags(window)
    }

    function test_sidebarHasNoNewTabButton() {
        const newTabButton = findChild(window.contentItem, "newTabButton")
        verify(newTabButton === null)
    }

    function test_newTabRequestWaitsForCommittedDestination() {
        const previousTabId = browser.activeTabId
        window.openOmnibar(true)
        tryCompare(window, "omnibarOpen", true)
        compare(browser.activeTabId, previousTabId)

        const omnibarInput = findChild(window.contentItem, "omnibarInput")
        verify(omnibarInput !== null)
        omnibarInput.text = "https://example.com"
        omnibarInput.forceActiveFocus()
        keyClick(Qt.Key_Return)

        tryVerify(function() { return browser.activeTabId !== previousTabId })
        compare(browser.activeUrl.toString(), "https://example.com")
    }

    function test_regularTabShowsItsCloseButtonOnHover() {
        const activeTabId = browser.activeTabId
        browser.openInputInBackground("https://close-me.example")
        const tabIndex = browser.tabs.index(browser.tabs.rowCount() - 1, 0)
        const tabId = browser.tabs.data(tabIndex, Qt.UserRole + 1)
        const tabRow = findChild(window.contentItem, "tab-" + tabId)
        const closeButton = findChild(window.contentItem, "close-" + tabId)
        const tabPointer = findChild(window.contentItem, "tabPointer-" + tabId)
        verify(tabRow !== null)
        verify(closeButton !== null)
        verify(tabPointer !== null)
        compare(closeButton.parent, tabPointer)
        verify(closeButton.visible)
        compare(closeButton.foreground.a, 0)

        mouseMove(tabRow, tabRow.width / 2, tabRow.height / 2)
        tryVerify(function() { return closeButton.foreground.a > 0.5 })
        mouseClick(tabRow, tabRow.width - closeButton.width / 2 - 4, tabRow.height / 2)

        tryVerify(function() { return findChild(window.contentItem, "tab-" + tabId) === null })
        compare(browser.activeTabId, activeTabId)
    }

    // A tab that starts making sound turns its site chip into a speaker, and
    // the speaker is where the sound is given back. It takes the chip's own
    // box rather than a box of its own in front of it: a row that widened for
    // it would shove its own title sideways every time a page started and
    // stopped playing.
    function test_soundingTabTurnsItsChipIntoASpeaker() {
        const engine = openPage("https://sounding.example")
        engine.simulateAudible(false)
        const tabId = browser.activeTabId
        const tabRow = findChild(window.contentItem, "tab-" + tabId)
        const speaker = findChild(window.contentItem, "audio-" + tabId)
        const tile = findChild(window.contentItem, "siteTile-" + tabId)
        verify(tabRow !== null)
        verify(speaker !== null)
        verify(tile !== null)

        // A silent tab says nothing about sound and shows its chip.
        verify(!speaker.visible)
        verify(tile.visible)
        const chipX = tile.x
        const chipWidth = tile.width

        engine.simulateAudible(true)
        tryVerify(function() { return speaker.visible })
        verify(!tile.visible)
        // The speaker stands in the chip's box, so nothing after it moves.
        compare(speaker.x, chipX)
        compare(speaker.width, chipWidth)
        compare(tile.x, chipX)

        mouseClick(tabRow, speaker.x + speaker.width / 2, speaker.y + speaker.height / 2)
        tryVerify(function() { return engine.audioMuted })
        // Muting the tab is not the page falling silent: the page stops
        // reporting sound, and the speaker stays because it is the only way
        // back.
        engine.simulateAudible(false)
        verify(speaker.visible)
        verify(!tile.visible)

        mouseClick(tabRow, speaker.x + speaker.width / 2, speaker.y + speaker.height / 2)
        tryVerify(function() { return !engine.audioMuted })
        tryVerify(function() { return !speaker.visible })
        verify(tile.visible)
    }

    // Pinning the first tab in a Space is where the section appears, and the
    // outline has to make room for it: the pins belong under the address, not
    // over the controls at the top.
    function test_pinningTheFirstTabPutsTheSectionUnderTheAddress() {
        const section = findChild(window.contentItem, "pinnedList")
        const address = findChild(window.contentItem, "addressButton")
        verify(section !== null)
        verify(address !== null)

        openPage("https://pinned-place.example")
        const tabId = browser.activeTabId
        browser.toggleActivePinned()
        tryVerify(function() { return section.visible })

        const addressBottom = address.mapToItem(window.contentItem, 0, address.height).y
        const sectionTop = section.mapToItem(window.contentItem, 0, 0).y
        verify(sectionTop >= addressBottom)
        verify(section.height > 0)

        browser.toggleActivePinned()
        tryVerify(function() { return !section.visible })
    }

    // A pin is a square holding one chip, with nothing in front of anything to
    // put a speaker before, so it wears the speaker in its top right corner.
    function test_soundingPinWearsItsSpeakerInTheCorner() {
        const engine = openPage("https://sounding-pin.example")
        const tabId = browser.activeTabId
        browser.toggleActivePinned()
        tryVerify(function() {
            return findChild(window.contentItem, "pinned-" + tabId) !== null
        })
        const pinRow = findChild(window.contentItem, "pinned-" + tabId)
        const audioButton = findChild(window.contentItem, "audio-" + tabId)
        verify(audioButton !== null)
        verify(!audioButton.visible)

        engine.simulateAudible(true)
        tryVerify(function() { return audioButton.visible })
        verify(audioButton.x + audioButton.width > pinRow.width / 2)
        verify(audioButton.y + audioButton.height < pinRow.height / 2)

        mouseClick(pinRow, audioButton.x + audioButton.width / 2,
            audioButton.y + audioButton.height / 2)
        tryVerify(function() { return engine.audioMuted })
        // The pin was not activated by the click that muted it.
        compare(browser.activeTabId, tabId)

        browser.toggleTabMuted(tabId)
        tryVerify(function() { return !engine.audioMuted })
        engine.simulateAudible(false)
        browser.toggleActivePinned()
    }

    function test_layoutKeepsChromeOutOfThePagesWay() {
        const settledSidebar = findChild(window.contentItem, "sidebar")
        window.sidebarCollapsed = false
        tryVerify(function() { return settledSidebar.visible && settledSidebar.width > 200 })

        const spaceHeading = findChild(window.contentItem, "spaceHeading")
        const sidebarNavigation = findChild(window.contentItem, "sidebarNavigation")
        const engineViewport = findChild(window.contentItem, "engineViewport")
        const navigationCluster = findChild(window.contentItem, "navigationCluster")
        verify(spaceHeading !== null)
        verify(sidebarNavigation !== null)
        verify(engineViewport !== null)
        verify(navigationCluster !== null)
        compare(engineViewport.height, window.height)

        // Navigation floats over the page instead of taking a band above it.
        verify(navigationCluster.height < engineViewport.height / 4)
        verify(navigationCluster.y > engineViewport.height / 2)

        // Navigation opens the outline; pinned rows sit above the tabs.
        browser.toggleActivePinned()
        const pinnedRow = findChild(window.contentItem, "pinned-" + browser.activeTabId)
        verify(pinnedRow !== null)
        tryVerify(function() { return pinnedRow.visible && pinnedRow.height > 0 })

        const sidebar = findChild(window.contentItem, "sidebar")
        const navigationTop = sidebarNavigation.mapToItem(sidebar, 0, 0).y
        tryVerify(function() {
            return pinnedRow.mapToItem(sidebar, 0, 0).y > navigationTop
        })

        // The browsing identity closes the outline, below every tab row.
        verify(spaceHeading.mapToItem(sidebar, 0, 0).y
            > pinnedRow.mapToItem(sidebar, 0, 0).y)
        const pinnedList = findChild(window.contentItem, "pinnedList")
        verify(pinnedList.capacity >= 3)
        verify(pinnedList.capacity <= 5)
        compare(pinnedList.columns, Math.min(pinnedList.capacity, browser.pinnedTabs.rowCount()))
        compare(pinnedRow.x, 0)
        compare(pinnedRow.width, pinnedList.width)
        compare(pinnedRow.height, 44)

        const tabRow = findChild(window.contentItem, "tab-" + browser.activeTabId)
        if (tabRow !== null && tabRow.visible) {
            verify(pinnedRow.mapToItem(sidebar, 0, 0).y < tabRow.mapToItem(sidebar, 0, 0).y)
        }
        browser.toggleActivePinned()
    }

    function test_collapsingTheSidebarLeavesOnlyTheFloatingControls() {
        const sidebar = findChild(window.contentItem, "sidebar")
        const engineViewport = findChild(window.contentItem, "engineViewport")
        const navigationCluster = findChild(window.contentItem, "navigationCluster")
        verify(sidebar !== null)
        verify(sidebar.visible)

        const expandedViewport = engineViewport.width
        // While the outline is open it carries the controls itself.
        verify(!navigationCluster.visible)
        window.sidebarCollapsed = true
        tryVerify(function() { return !sidebar.visible })
        tryVerify(function() { return engineViewport.width > expandedViewport })
        verify(navigationCluster.visible)

        window.sidebarCollapsed = false
        tryVerify(function() { return sidebar.visible })
    }

    function test_sidebarWidthAnswersToBothPointerAndKeyboard() {
        window.settingsOpen = false
        window.requestActivate()
        tryVerify(function() { return window.active })
        window.sidebarCollapsed = false
        window.setSidebarWidth(window.sidebarDefaultWidth)
        const sidebar = findChild(window.contentItem, "sidebar")
        const resizer = findChild(window.contentItem, "sidebarResizer")
        verify(resizer !== null)
        tryCompare(sidebar, "width", window.sidebarDefaultWidth)

        // The handle rides the seam it moves.
        compare(Math.round(resizer.x + resizer.width / 2), Math.round(sidebar.width))

        // Everything the pointer can do here, the keyboard can do too.
        resizer.forceActiveFocus()
        compare(window.activeFocusItem.objectName, "sidebarResizer")
        keyClick(Qt.Key_Right)
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 16)
        keyClick(Qt.Key_Left)
        compare(window.sidebarWidth, window.sidebarDefaultWidth)
        keyClick(Qt.Key_Right, Qt.ShiftModifier)
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 48)
        keyClick(Qt.Key_Home)
        compare(window.sidebarWidth, window.sidebarMinimumWidth)
        keyClick(Qt.Key_End)
        compare(window.sidebarWidth, window.sidebarMaximumWidth)
        keyClick(Qt.Key_Space)
        compare(window.sidebarWidth, window.sidebarDefaultWidth)
        tryCompare(sidebar, "width", window.sidebarDefaultWidth)

        // A request past either end stops at the end.
        window.setSidebarWidth(window.sidebarMaximumWidth + 400)
        compare(window.sidebarWidth, window.sidebarMaximumWidth)
        window.setSidebarWidth(0)
        compare(window.sidebarWidth, window.sidebarMinimumWidth)

        window.setSidebarWidth(window.sidebarDefaultWidth)
        window.commands.run("widen-sidebar", -1)
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 24)
        window.commands.run("narrow-sidebar", -1)
        compare(window.sidebarWidth, window.sidebarDefaultWidth)
        window.commands.run("reset-sidebar", -1)
        compare(window.sidebarWidth, window.sidebarDefaultWidth)

        // Asking a hidden sidebar for more of itself brings it back.
        window.sidebarCollapsed = true
        window.commands.run("narrow-sidebar", -1)
        compare(window.sidebarCollapsed, true)
        window.commands.run("widen-sidebar", -1)
        compare(window.sidebarCollapsed, false)
        window.setSidebarWidth(window.sidebarDefaultWidth)
        tryCompare(sidebar, "width", window.sidebarDefaultWidth)

        // Dragging the seam moves it by the same distance the pointer travelled.
        mousePress(resizer, resizer.width / 2, 300)
        mouseMove(resizer, resizer.width / 2 + 60, 300)
        compare(window.sidebarWidth, window.sidebarDefaultWidth + 60)
        mouseRelease(resizer, resizer.width / 2, 300)
        compare(resizer.dragging, false)
        tryCompare(sidebar, "width", window.sidebarDefaultWidth + 60)

        // The width the reader settled on outlives the session that set it.
        window.setSidebarWidth(344)
        tryVerify(function() { return browser.preference("sidebar-width", "") === "344" })
        window.setSidebarWidth(window.sidebarDefaultWidth)
        window.restoreSidebarWidth()
        compare(window.sidebarWidth, 344)

        window.setSidebarWidth(window.sidebarDefaultWidth)
    }

    function test_focusMovesBetweenTheOutlineAndThePage() {
        window.settingsOpen = false
        window.requestActivate()
        tryVerify(function() { return window.active })
        window.sidebarCollapsed = false
        const engineHost = findChild(window.contentItem, "engineLoader")
        openPage("https://focus.example")

        // Focusing the outline lands on the row the reader is already reading.
        window.commands.run("focus-sidebar", -1)
        const landed = window.activeFocusItem.objectName
        verify(landed === "tab-" + browser.activeTabId
            || landed === "pinned-" + browser.activeTabId
            || landed === "addressButton")

        // Escape is the way back out of the outline.
        keyClick(Qt.Key_Escape)
        tryVerify(function() { return engineHost.item.activeFocus })

        window.commands.run("focus-sidebar", -1)
        verify(!engineHost.item.activeFocus)
        window.commands.run("focus-page", -1)
        tryVerify(function() { return engineHost.item.activeFocus })

        // The resize handle reads as part of the sidebar, so it leaves like
        // the rest of it. Every control in there answers to the same key.
        const controls = ["addressButton", "settingsButton",
            "manageSpacesButton", "sidebarResizer", "tab-" + browser.activeTabId]
        for (let index = 0; index < controls.length; ++index) {
            const control = findChild(window.contentItem, controls[index])
            verify(control !== null)
            control.forceActiveFocus()
            compare(window.activeFocusItem.objectName, controls[index])
            keyClick(Qt.Key_Escape)
            tryVerify(function() { return engineHost.item.activeFocus })
        }

        // Asking a hidden outline for the keyboard shows it first.
        window.sidebarCollapsed = true
        window.commands.run("focus-sidebar", -1)
        compare(window.sidebarCollapsed, false)
        window.commands.run("focus-page", -1)
    }

    function test_pageHintsReceiveTheActiveThemeAndFont() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        verify(engineHost !== null)
        tryVerify(function() { return engineHost.item !== null })

        const hintTheme = engineHost.item.keyboardNavigationConfiguration.hintTheme
        compare(String(hintTheme.surface), String(window.colors.surface))
        compare(String(hintTheme.text), String(window.colors.text))
        compare(String(hintTheme.accent), String(window.colors.accent))
        compare(hintTheme.font.family, window.colors.font.family)
        compare(hintTheme.font.size, window.colors.font.size)
    }

    function test_pageReceivesGgDespiteBrowserSequences() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        verify(engineHost !== null)
        tryVerify(function() { return engineHost.item !== null })
        window.commands.run("focus-page", -1)
        tryVerify(function() { return engineHost.item.activeFocus })
        engineHost.item.keyboardInput = ""

        keyClick(Qt.Key_G)
        keyClick(Qt.Key_G)

        compare(engineHost.item.keyboardInput, "gg")
    }

    function test_switchingTabsPreservesPageLocalState() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        verify(engineHost !== null)
        openPage("https://first.example")

        const firstTabId = browser.activeTabId
        engineHost.item.pageLocalState = "edited form value"

        browser.openInput("https://second.example", true)
        tryVerify(function() {
            return engineHost.item !== null
                && engineHost.item.currentUrl.toString() === "https://second.example"
        })

        browser.activateTab(firstTabId)
        tryCompare(engineHost.item, "pageLocalState", "edited form value")
    }

    function test_primaryChromeIsAccessibleFromKeyboard() {
        window.requestActivate()
        tryVerify(function() { return window.active })
        const addressButton = findChild(window.contentItem, "addressButton")
        const collapseButton = findChild(window.contentItem, "collapseButton")
        const reloadButton = findChild(window.contentItem, "reloadButton")
        const commandPanelButton = findChild(window.contentItem, "commandPanelButton")
        const manageSpacesButton = findChild(window.contentItem, "manageSpacesButton")
        const settingsButton = findChild(window.contentItem, "settingsButton")
        const materialSymbolsFont = findChild(window, "materialSymbolsFont")

        compare(settingsButton.accessibleName, "Browsing settings and downloads")
        compare(addressButton.accessibleName, "Search or enter address")
        compare(collapseButton.accessibleName, "Hide sidebar")
        compare(commandPanelButton.accessibleName, "Command panel")
        compare(manageSpacesButton.icon, "more_horiz")
        verify(iconFontSource.toString().endsWith("/material-symbols-rounded.ttf"))
        verify(materialSymbolsFont !== null)
        tryCompare(materialSymbolsFont, "status", FontLoader.Ready)
        verify(addressButton.activeFocusOnTab)
        verify(collapseButton.activeFocusOnTab)

        addressButton.forceActiveFocus()
        compare(window.activeFocusItem.objectName, "addressButton")

        let visitedTab = false
        for (let step = 0; step < 20 && window.activeFocusItem.objectName !== "settingsButton"; ++step) {
            keyClick(Qt.Key_Tab)
            if (window.activeFocusItem.objectName.indexOf("tab-") === 0)
                visitedTab = true
        }
        verify(visitedTab)
        compare(window.activeFocusItem.objectName, "settingsButton")
        keyClick(Qt.Key_Backtab)
        compare(window.activeFocusItem.objectName, "manageSpacesButton")
    }

    function test_everyBrowserCommandIsBoundAndSearchable() {
        const bindings = keyboardNavigation.browserBindings
        verify(Object.keys(bindings).length > 0)
        compare(bindings.J, "next-tab")
        compare(bindings.K, "previous-tab")
        compare(bindings.X, "reopen-tab")
        compare(bindings.u, undefined)
        compare(keyboardNavigation.bindings.d, "scroll-half-page-down")
        compare(keyboardNavigation.bindings.u, "scroll-half-page-up")

        // The panel is the keymap: every action it lists carries its keys, and
        // every configured command is reachable from it.
        const actions = window.commands.actions()
        const listed = {}
        for (let index = 0; index < actions.length; ++index) {
            listed[actions[index].command] = actions[index]
        }
        for (const binding in bindings) {
            const command = bindings[binding]
            verify(listed[command] !== undefined)
            verify(listed[command].keys.length > 0)
        }

        const matches = window.commands.search("reopen")
        verify(matches.length > 0)
        compare(matches[0].command, "reopen-tab")
    }

    function test_tabCommandsWalkTheModelInOrder() {
        const start = browser.activeTabId
        const count = browser.tabs.rowCount()
        verify(count > 1)

        window.stepTab(1)
        verify(browser.activeTabId !== start)
        window.stepTab(-1)
        tryCompare(browser, "activeTabId", start)

        for (let step = 0; step < count; ++step) {
            window.stepTab(1)
        }
        tryCompare(browser, "activeTabId", start)
    }

    function test_spaceSwitchKeepsEachSpacesPagesLoaded() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        openPage("https://personal-space.example")
        const personalSpaceId = browser.activeSpaceId
        const personalEngineView = engineLoader.item
        const workSpaceId = browser.createSpace("Work")

        verify(browser.switchSpace(workSpaceId))
        openPage("https://work-space.example")
        tryVerify(function() {
            return engineLoader.item !== null
                && engineLoader.item !== personalEngineView
                && engineLoader.item.profilePath === browser.activeProfilePath
        })
        const workEngineView = engineLoader.item

        // Coming back finds the very page that was left, not a reload of it.
        verify(browser.switchSpace(personalSpaceId))
        tryCompare(engineLoader, "item", personalEngineView)

        // A deleted Space takes its pages with it. It has a page open now, so
        // deleting it takes its name as confirmation.
        verify(browser.switchSpace(workSpaceId))
        tryCompare(engineLoader, "item", workEngineView)
        verify(browser.deleteSpace(workSpaceId, "Work"))
        compare(browser.activeSpaceId, personalSpaceId)
        tryCompare(engineLoader, "item", personalEngineView)
    }

    // Site artwork is page state, not session state, so the Space switch that
    // reloads a Space's tabs from its store drops it. The page itself is kept
    // alive across the switch and will never report its icon again, so coming
    // back has to find the icon the reader already watched load.
    function test_spaceSwitchKeepsTheIconsItsPagesAlreadyReported() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        openPage("https://one.example/page")

        const personalSpaceId = browser.activeSpaceId
        const tabId = browser.activeTabId
        const iconUrl = "https://one.example/favicon.ico"
        const personalEngineView = engineLoader.item
        const tabShowsIcon = function() {
            const row = findChild(window.contentItem, "tab-" + tabId)
            return row !== null && String(row.tabIconUrl) === iconUrl
        }

        personalEngineView.pageIconUrl = iconUrl
        tryVerify(tabShowsIcon)

        const workSpaceId = browser.createSpace("Work")
        verify(browser.switchSpace(workSpaceId))
        openPage("https://work-icons.example")
        tryVerify(function() {
            return engineLoader.item !== null && engineLoader.item !== personalEngineView
        })

        verify(browser.switchSpace(personalSpaceId))
        tryCompare(engineLoader, "item", personalEngineView)
        tryVerify(tabShowsIcon)
    }

    // A page kept playing while its Space was away, and kept the muting it was
    // given. The Space's tabs come back from a store that records neither, so
    // the row reads both off the page that outlived the switch.
    function test_spaceSwitchKeepsWhatItsPagesAreStillPlaying() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        const personalEngineView = openPage("https://one.example/sound")

        const personalSpaceId = browser.activeSpaceId
        const tabId = browser.activeTabId
        personalEngineView.simulateAudible(true)
        const speaker = findChild(window.contentItem, "audio-" + tabId)
        verify(speaker !== null)
        tryVerify(function() { return speaker.visible })
        browser.toggleTabMuted(tabId)
        tryVerify(function() { return personalEngineView.audioMuted })

        const workSpaceId = browser.createSpace("Sound")
        verify(browser.switchSpace(workSpaceId))
        openPage("https://work-sound.example")
        tryVerify(function() {
            return engineLoader.item !== null && engineLoader.item !== personalEngineView
        })

        verify(browser.switchSpace(personalSpaceId))
        tryCompare(engineLoader, "item", personalEngineView)
        // The page is still muted, so the row still says so and still offers
        // the sound back.
        verify(personalEngineView.audioMuted)
        const restored = findChild(window.contentItem, "audio-" + tabId)
        verify(restored !== null)
        tryVerify(function() { return restored.visible })

        browser.toggleTabMuted(tabId)
        tryVerify(function() { return !personalEngineView.audioMuted })
        personalEngineView.simulateAudible(false)
    }

    // Moving a tab to another Space is not the same as switching to one. A
    // Space switch puts its pages aside; a move takes the tab away from the
    // Space that held its engine, so the page is discarded and the moved tab
    // arrives without one. It therefore shows its lettered tile until its page
    // is loaded again and reports its own artwork — which is the contract
    // `BrowserController::setTabIcon` states, rather than a second instance of
    // the Space-switch bug. What has to survive the move is the wiring: the
    // tab's new engine still has to reach the tab it belongs to.
    //
    // Losing the page is the intended cost of a move, not a gap left to close:
    // a moved tab gives up its scroll position, form state and history along
    // with its artwork, and arrives in its new Space as a tab to be loaded.
    // Preserving any of that across a move is deliberately not a goal, so this
    // test asserts the discard rather than tolerating it.
    function test_movingATabToAnotherSpaceLeavesItsPageAndIconBehind() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        openPage("https://moved.example/page")

        const sourceSpaceId = browser.activeSpaceId
        const tabId = browser.activeTabId
        const iconUrl = "https://moved.example/favicon.ico"
        const tabIcon = function() {
            const row = findChild(window.contentItem, "tab-" + tabId)
            return row === null ? null : String(row.tabIconUrl)
        }

        engineLoader.item.pageIconUrl = iconUrl
        tryVerify(function() { return tabIcon() === iconUrl })

        const workSpaceId = browser.createSpace("Work")
        verify(browser.requestTabMoveToSpace(tabId, workSpaceId, false))
        verify(browser.switchSpace(workSpaceId))
        compare(browser.activeTabId, tabId)

        // The page did not come with the tab, so neither did its artwork.
        verify(engineLoader.engines[tabId] === undefined)
        tryVerify(function() { return tabIcon() === "" })

        // The tab is served by a new engine, and that engine still reports to
        // the right tab.
        tryVerify(function() { return engineLoader.item !== null })
        const reloadedIconUrl = "https://moved.example/reloaded.ico"
        engineLoader.item.pageIconUrl = reloadedIconUrl
        tryVerify(function() { return tabIcon() === reloadedIconUrl })

        // Leave the suite in the Space it started in.
        verify(browser.switchSpace(sourceSpaceId))
    }

    function test_privateWindowUsesTemporaryIdentityAndDistinctChrome() {
        compare(windowManager.privateWindowCount, 0)
        windowManager.openPrivateWindow()
        tryCompare(windowManager, "privateWindowCount", 1)

        const privateBrowser = window.privateWindows[0]
        verify(privateBrowser !== undefined && privateBrowser !== null)
        compare(privateBrowser.objectName, "privateBrowserWindow")
        verify(privateBrowser.visible)
        verifyApplicationWindowFlags(privateBrowser)
        // A window of its own: no transient parent for a tiling compositor to
        // read as "float this next to its opener".
        verify(!privateBrowser.transientParent)
        compare(privateBrowser.colors.accent, privateBrowser.colors.privateAccent)

        const spaceSwitcher = findChild(privateBrowser.contentItem, "spaceSwitcher")
        const pinnedList = findChild(privateBrowser.contentItem, "pinnedList")
        const manageSpacesButton = findChild(privateBrowser.contentItem, "manageSpacesButton")
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader")
        const privateBadge = findChild(privateBrowser.contentItem, "privateBadge")
        verify(spaceSwitcher !== null)
        verify(pinnedList !== null)
        verify(manageSpacesButton !== null)
        verify(privateEngine !== null)
        verify(!spaceSwitcher.visible)
        verify(!pinnedList.visible)
        verify(!manageSpacesButton.visible)
        // The window names itself with its palette and the mask in the footer.
        // Nothing is drawn over the page to say it.
        verify(findChild(privateBrowser.contentItem, "privateIndicator") === null)
        verify(privateBadge !== null)
        verify(privateBadge.visible)
        compare(privateBadge.text, "domino_mask")
        compare(String(privateBadge.color), String(privateBrowser.colors.privateAccent))
        privateBrowser.windowBrowser.openInput("https://private.example", false)
        tryVerify(function() { return privateEngine.item !== null })
        compare(privateEngine.item.profilePath, windowManager.privateProfilePath)
        compare(privateEngine.item.browserProfile, window.privateProfileHost.profile)

        privateBrowser.windowBrowser.closeActiveTab()
        tryCompare(windowManager, "privateWindowCount", 0)
        tryVerify(function() { return window.privateWindows.length === 0 })
    }

    function test_newWindowRequestsRouteToTabsOrAuxiliaryWindows() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        const previousTabCount = browser.tabs.rowCount()
        const previousActiveTabId = browser.activeTabId

        engineLoader.item.simulateBackgroundTabRequest("https://example.com/background")
        tryVerify(function() { return browser.tabs.rowCount() === previousTabCount + 1 })
        compare(browser.activeTabId, previousActiveTabId)

        engineLoader.item.simulateNewWindowRequest("https://example.com/tab", false)
        tryVerify(function() { return browser.tabs.rowCount() === previousTabCount + 2 })
        compare(browser.activeUrl.toString(), "https://example.com/tab")

        engineLoader.item.simulateNewWindowRequest("https://example.com/dialog", true)
        const auxiliary = findChild(window, "auxiliaryWindow")
        tryVerify(function() { return auxiliary !== null && auxiliary.visible })
        verify(!Boolean(auxiliary.flags & Qt.FramelessWindowHint))

        const auxiliaryEngine = findChild(auxiliary.contentItem, "auxiliaryEngineLoader")
        verify(auxiliaryEngine !== null)
        tryVerify(function() { return auxiliaryEngine.item !== null })
        compare(auxiliaryEngine.item.sharedProfile, engineLoader.item.browserProfile)
        compare(auxiliaryEngine.item.currentUrl.toString(), "https://example.com/dialog")
        auxiliaryEngine.item.simulateWindowCloseRequest()
        tryVerify(function() { return !auxiliary.visible })
        window.requestActivate()
        tryVerify(function() { return window.active })
    }

    // A page names no address at all while a navigation is in flight, and a
    // page adopted from a new-window request passes through that gap on its way
    // to the address the link asked for. The tab has not lost its page there,
    // and must not be blanked for it: a blank tab is given no engine, so
    // believing the gap would take the renderer down mid-load and leave the
    // Start page standing where the opened page belongs.
    function test_aPageBetweenAddressesKeepsItsTabAndItsEngine() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const startPage = findChild(window.contentItem, "startPage")
        verify(engineLoader !== null)
        verify(startPage !== null)

        openPage("https://opener.example")
        engineLoader.item.simulateNewWindowRequest("https://opened.example/page", false)
        tryVerify(function() {
            return browser.activeUrl.toString() === "https://opened.example/page"
        })
        const openedTabId = browser.activeTabId
        const openedEngine = engineLoader.item
        verify(openedEngine !== null)

        openedEngine.currentUrl = ""
        compare(browser.activeUrl.toString(), "https://opened.example/page")
        compare(engineLoader.engines[openedTabId], openedEngine)
        compare(engineLoader.item, openedEngine)
        verify(!startPage.visible)

        // The address the navigation commits to is the one the tab takes.
        openedEngine.currentUrl = "https://opened.example/next"
        tryVerify(function() {
            return browser.activeUrl.toString() === "https://opened.example/next"
        })
        verify(!startPage.visible)
    }

    // Every engine the host holds answers for a tab that exists, and the tab
    // showing is the only one drawing. An engine keyed to no tab would be a
    // page nothing can show, hide or take away: it would sit over the page the
    // reader came back to, which is what the tab they left looks like from
    // behind it.
    function test_everyEngineAnswersForATabThatExists() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)

        const openerEngine = openPage("https://opener.example")
        const openerTabId = browser.activeTabId
        engineLoader.item.simulateNewWindowRequest("https://opened.example/page", false)
        tryVerify(function() {
            return browser.activeUrl.toString() === "https://opened.example/page"
        })
        const openedTabId = browser.activeTabId

        // Both tabs hold a page, so both are listed: an engine keyed to
        // anything else answers for no tab in the Space.
        for (const tabId in engineLoader.engines) {
            verify(tabId.length > 0)
            verify(findChild(window.contentItem, "tab-" + tabId) !== null)
            compare(engineLoader.engines[tabId].visible, tabId === openedTabId)
        }

        // The tab the link was clicked from still draws the page it left.
        browser.activateTab(openerTabId)
        tryVerify(function() { return engineLoader.item === openerEngine })
        compare(engineLoader.engines[openerTabId], openerEngine)
        verify(openerEngine.visible)
        verify(!engineLoader.engines[openedTabId].visible)
    }

    function test_omnibarShowsOnlyActiveSpaceHistory() {
        const personalSpaceId = browser.activeSpaceId
        browser.recordVisit("https://personal.example/docs", "Personal docs")
        window.openOmnibar(true)
        const input = findChild(window.contentItem, "omnibarInput")
        const suggestions = findChild(window.contentItem, "historySuggestionList")
        verify(input !== null)
        verify(suggestions !== null)
        input.text = "docs"
        tryCompare(suggestions, "count", 1)

        // The typed text is the selection until the user steps into the list,
        // so the first Down lands on the first suggestion and Up leaves again.
        const panel = findChild(window.contentItem, "commandPanel")
        verify(panel !== null)
        compare(panel.selected, -1)
        panel.step(1)
        compare(panel.selected, 0)
        panel.step(1)
        compare(panel.selected, -1)
        panel.step(-1)
        compare(panel.selected, 0)

        window.closeOmnibar()
        const workSpaceId = browser.createSpace("History Work")
        verify(browser.switchSpace(workSpaceId))
        window.openOmnibar(true)
        input.text = "docs"
        tryCompare(suggestions, "count", 0)
        window.closeOmnibar()
        verify(browser.switchSpace(personalSpaceId))
        verify(browser.deleteSpace(workSpaceId, ""))
    }

    function test_theSidebarOpensAndClosesSettings() {
        const settingsButton = findChild(window.contentItem, "settingsButton")
        const settingsSurface = findChild(window.contentItem, "settingsSurface")
        verify(settingsButton !== null)
        verify(settingsSurface !== null)
        verify(!settingsSurface.visible)
        // A page to cover, so the backdrop below has something to blur.
        openPage("https://under-settings.example")

        settingsButton.clicked()
        tryVerify(function() { return settingsSurface.visible })

        // Settings is a place over the page, not instead of it: the same
        // translucency the sidebar has, over the page it covers, blurred.
        const settingsBackdrop = findChild(window.contentItem, "settingsBackdrop")
        verify(settingsBackdrop !== null)
        compare(String(settingsBackdrop.tint), String(window.colors.sheet))
        tryVerify(function() { return settingsBackdrop.sampling })

        const closeButton = findChild(window.contentItem, "closeSettingsButton")
        verify(closeButton !== null)
        closeButton.clicked()
        tryVerify(function() { return !settingsSurface.visible })
    }

    function test_theKeymapOpensSettings() {
        const settingsSurface = findChild(window.contentItem, "settingsSurface")
        verify(settingsSurface !== null)
        verify(!settingsSurface.visible)
        compare(keyboardNavigation.browserBindings["Primary+,"], "settings")

        window.requestActivate()
        tryVerify(function() { return window.active })
        keyClick(Qt.Key_Comma, Qt.ControlModifier)
        tryVerify(function() { return settingsSurface.visible })

        findChild(window.contentItem, "closeSettingsButton").clicked()
        tryVerify(function() { return !settingsSurface.visible })
    }

    function test_settingsExposeNetworkAndDownloadPolicy() {
        const settingsButton = findChild(window.contentItem, "settingsButton")
        const remoteSuggestionsStatus = findChild(window.contentItem, "remoteSuggestionsStatus")
        const automaticRequestsStatus = findChild(window.contentItem, "automaticRequestsStatus")
        const keyboardNavigationEnabled = findChild(
            window.contentItem, "keyboardNavigationEnabled")
        verify(settingsButton !== null)
        verify(remoteSuggestionsStatus !== null)
        verify(automaticRequestsStatus !== null)
        verify(keyboardNavigationEnabled !== null)
        compare(remoteSuggestionsStatus.text, "Remote search suggestions: Off")
        verify(automaticRequestsStatus.text.indexOf("automatic network requests") >= 0)
        // A keyboard-driven browser ships with its keymap live.
        compare(keyboardNavigationEnabled.checked, true)
        // The kit's Toggle is stateless about the value: it reports the click
        // and the settings page flips the setting, which flows back through
        // the `checked` binding. Drive it from the keyboard, since that is how
        // this browser is meant to be reached, and on both of the kit's
        // activation keys.
        window.requestActivate()
        tryVerify(function() { return window.active })
        keyboardNavigationEnabled.forceActiveFocus()
        verify(keyboardNavigationEnabled.activeFocus)
        keyClick(Qt.Key_Space)
        compare(keyboardNavigation.enabled, false)
        compare(keyboardNavigationEnabled.checked, false)
        keyClick(Qt.Key_Return)
        compare(keyboardNavigation.enabled, true)
        compare(keyboardNavigationEnabled.checked, true)
    }

    // A Space with nothing open in it has no page to show and no ordinary tab
    // to list. What stands in for the page is the keymap itself, drawn from the
    // same command registry and the same bindings the window dispatches
    // through, and no renderer is spent on the blank tab behind it.
    function test_restingSpaceShowsTheShortcutSheetInsteadOfAPage() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const startPage = findChild(window.contentItem, "startPage")
        const engineBacking = findChild(window.contentItem, "engineBacking")
        verify(engineLoader !== null)
        verify(startPage !== null)
        verify(engineBacking !== null)

        const homeSpaceId = browser.activeSpaceId
        const restingSpaceId = browser.createSpace("Resting")
        verify(browser.switchSpace(restingSpaceId))
        tryVerify(function() { return browser.atRest })

        // No row stands for a page nobody opened.
        const restingTabId = browser.activeTabId
        tryVerify(function() {
            return findChild(window.contentItem, "tab-" + restingTabId) === null
        })
        tryVerify(function() { return startPage.visible })

        // The resting page area is translucent like the sidebar rather than
        // sealed with the opaque backing a webpage needs, and with no page
        // there is nothing to blur behind it.
        verify(!engineBacking.visible)
        const restingBackdrop = findChild(window.contentItem, "shortcutsBackdrop")
        verify(restingBackdrop !== null)
        compare(String(restingBackdrop.tint), String(window.colors.sidebar))
        verify(!restingBackdrop.sampling)

        // A blank tab is not worth a renderer process.
        compare(engineLoader.engines[restingTabId], undefined)
        compare(engineLoader.item, null)

        // Every command the sheet lists carries the keys the window answers to,
        // and it names them exactly as the command panel does.
        verify(startPage.sections.length > 0)
        let listed = 0
        let openAddressKeys = ""
        for (let group = 0; group < startPage.sections.length; ++group) {
            const entries = startPage.sections[group].entries
            for (let index = 0; index < entries.length; ++index) {
                verify(entries[index].keys.length > 0)
                if (entries[index].title === "Open address")
                    openAddressKeys = entries[index].keys
                ++listed
            }
        }
        verify(listed > 8)
        compare(openAddressKeys, window.commands.keymap.keysFor("open-address"))
        verify(openAddressKeys.length > 0)

        // Committing an address ends the rest: the page arrives, and its row
        // arrives with it.
        openPage("https://resting.example")
        tryVerify(function() { return !startPage.visible })
        verify(engineBacking.visible)
        tryVerify(function() {
            return findChild(window.contentItem, "tab-" + restingTabId) !== null
        })

        verify(browser.switchSpace(homeSpaceId))
        verify(browser.deleteSpace(restingSpaceId, "Resting"))
    }

    // A blank address is not the same thing as a resting Space, and it must not
    // be an empty viewport either: there is no page, so the sheet stands in.
    // The tab itself stays listed, because the reader put it there and has to
    // be able to close it.
    function test_blankAddressShowsTheSheetRatherThanAnEmptyViewport() {
        const startPage = findChild(window.contentItem, "startPage")
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(startPage !== null)
        openPage("https://not-blank.example")
        browser.openInputInBackground("https://beside.example")
        verify(browser.tabs.rowCount() > 1)
        verify(!startPage.visible)

        const tabId = browser.activeTabId
        browser.openInput("about:blank", false)
        tryVerify(function() { return startPage.visible })

        // The Space is not at rest, so the tab keeps its row and its close
        // button, and the sheet stands in without pretending otherwise.
        verify(!browser.atRest)
        verify(findChild(window.contentItem, "tab-" + tabId) !== null)
        verify(!startPage.overPage)
        tryVerify(function() { return engineLoader.engines[tabId] === undefined })

        openPage("https://not-blank.example/again")
        tryVerify(function() { return !startPage.visible })
    }

    // Leaving a blank tab for a page and coming back has to bring the sheet
    // back with it. The host names one active engine, and a tab that has none
    // has to clear that name rather than leave the last tab's page standing in
    // as the answer to "is a page up?".
    function test_returningToABlankTabBringsTheSheetBack() {
        const startPage = findChild(window.contentItem, "startPage")
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(startPage !== null)

        openPage("https://neighbour.example")
        const blankTabId = browser.activeTabId
        browser.openInput("about:blank", false)
        tryVerify(function() { return startPage.visible })

        browser.openInput("https://elsewhere.example", true)
        const pageTabId = browser.activeTabId
        tryVerify(function() {
            return !startPage.visible && engineLoader.item !== null
        })

        browser.activateTab(blankTabId)
        tryVerify(function() { return startPage.visible })
        // Nothing is drawing a page, and the window is told so.
        compare(engineLoader.item, null)

        browser.activateTab(pageTabId)
        tryVerify(function() { return !startPage.visible })
        verify(engineLoader.item !== null)
    }

    // The sheet is a command like any other, so it answers on demand over a
    // live page. There it cannot be translucent — a webpage read through a list
    // of shortcuts is neither — and the reader came from somewhere, so it
    // closes.
    function test_shortcutSheetAnswersOnDemandOverAPage() {
        const startPage = findChild(window.contentItem, "startPage")
        verify(startPage !== null)
        openPage("https://busy.example")
        verify(!startPage.visible)

        window.commands.run("shortcuts", -1)
        tryVerify(function() { return startPage.visible })
        verify(startPage.overPage)

        // Over a page the sheet keeps the sidebar's translucency and blurs that
        // page behind itself, so what the reader left is still legible as a
        // place without being readable as a page.
        const sheetBackdrop = findChild(window.contentItem, "shortcutsBackdrop")
        verify(sheetBackdrop !== null)
        // A sheet over a page lets more through than the sidebar does: at the
        // sidebar's own value a dark page shows as nothing.
        compare(String(sheetBackdrop.tint), String(window.colors.sheet))
        verify(Qt.color(window.colors.sheet).a < Qt.color(window.colors.sidebar).a)
        tryVerify(function() { return sheetBackdrop.sampling })
        compare(sheetBackdrop.source, findChild(window.contentItem, "engineLoader"))

        const closeButton = findChild(window.contentItem, "closeShortcutsButton")
        verify(closeButton !== null)
        verify(closeButton.visible)

        // The same command takes it away again.
        window.commands.run("shortcuts", -1)
        tryVerify(function() { return !startPage.visible })

        // So does Escape, and so does the close button.
        window.commands.run("shortcuts", -1)
        tryVerify(function() { return startPage.visible })
        window.requestActivate()
        tryVerify(function() { return window.active })
        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !startPage.visible })

        window.commands.run("shortcuts", -1)
        tryVerify(function() { return startPage.visible })
        closeButton.clicked()
        tryVerify(function() { return !startPage.visible })

        // Being in the registry is what makes it searchable and bindable.
        const matches = window.commands.search("Keyboard shortcuts")
        verify(matches.length > 0)
        compare(matches[0].command, "shortcuts")
        verify(matches[0].keys.length > 0)
    }

    // A binding this build cannot honour is dropped rather than taking the
    // keymap with it, so a notice is the only thing that says a configured key
    // is missing. It waits in the settings section it belongs to, and the
    // sidebar's settings button carries a mark so it is findable from outside.
    function test_settingsMarksItselfWhenTheKeymapReportsIgnoredBindings() {
        const settings = findChild(window.contentItem, "settingsSurface")
        const notice = findChild(window.contentItem, "keyboardBindingNotice")
        const dot = findChild(window.contentItem, "settingsAttentionDot")
        verify(settings !== null)
        verify(notice !== null)
        verify(dot !== null)

        // This build knows every command in its own default file, so nothing is
        // waiting and neither the notice nor the mark is drawn.
        compare(keyboardNavigation.errorMessage, "")
        verify(!settings.needsAttention)
        verify(!notice.visible)
        verify(!dot.visible)

        // The notice and the mark are drawn in the one colour the theme names
        // for something being wrong, so the two read as one thing.
        compare(String(dot.color), String(window.colors.urgent))
        compare(String(notice.urgent), String(window.colors.urgent))
        verify(String(window.colors.urgent) !== String(window.colors.privateAccent))

        // The notice states what the keymap reported, wherever that came from.
        compare(notice.detail, settings.keyboardReport)

        // A keymap that did report something: the notice states it, and the
        // section it belongs to says the reader is wanted.
        const reported = "Ignored bindings this build does not know: "
            + "Primary+Shift+D (debug-current-tab)"
        const reporting = reportingSettingsComponent.createObject(window.contentItem,
            {"report": reported})
        verify(reporting !== null)
        compare(reporting.keyboardReport, reported)
        verify(reporting.needsAttention)
        const reportingNotice = findChild(reporting, "keyboardBindingNotice")
        verify(reportingNotice !== null)
        tryVerify(function() { return reportingNotice.visible })
        compare(reportingNotice.detail, reported)
        verify(reportingNotice.height > 0)
        // It is not on the section it does not belong to.
        reporting.section = 0
        tryVerify(function() { return !reportingNotice.visible })
        reporting.destroy()

        // And the mark is drawn once the outline is told.
        const marked = attentionOutlineComponent.createObject(window.contentItem)
        verify(marked !== null)
        const markedDot = findChild(marked, "settingsAttentionDot")
        verify(markedDot !== null)
        tryVerify(function() { return markedDot.visible })
        compare(String(markedDot.color), String(window.colors.urgent))
        marked.destroy()
    }

    // A tab's chip stands in for artwork that is not being drawn, so it takes
    // the site's own colour: the hue of the favicon, at the theme's saturation
    // and lightness. An icon with no hue to give leaves the chip neutral
    // instead of falling back to a colour the site never chose.
    function test_chipTakesItsColourFromTheFaviconWhenArtworkIsOff() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        openPage("https://chip.example")

        const tabId = browser.activeTabId
        const tile = findChild(window.contentItem, "siteTile-" + tabId)
        verify(tile !== null)

        const hostTint = tile.hostTint
        engineLoader.item.pageIconUrl = colouredFaviconUrl
        tryCompare(tile, "iconUrl", colouredFaviconUrl)

        // Artwork on: the chip's colour is still the host's, because the
        // artwork itself is what carries the site's colour.
        compare(window.useFavicons, true)
        compare(String(tile.chipTint), String(hostTint))

        const useFavicons = findChild(window.contentItem, "useFavicons")
        verify(useFavicons !== null)
        useFavicons.clicked()
        compare(window.useFavicons, false)

        const blue = Qt.color("#2f5ce6")
        tryVerify(function() { return String(tile.chipTint) !== String(hostTint) })
        fuzzyCompare(tile.chipTint.hsvHue, blue.hsvHue, 0.03)
        // The theme still owns how strong a chip may be.
        fuzzyCompare(tile.chipTint.hslSaturation, tile.tintSaturation, 0.02)
        fuzzyCompare(tile.chipTint.hslLightness, tile.tintLightness, 0.02)

        // A white icon names no colour, so the chip goes neutral rather than
        // borrowing one from the host's name.
        engineLoader.item.pageIconUrl = colourlessFaviconUrl
        tryCompare(tile, "iconUrl", colourlessFaviconUrl)
        tryVerify(function() {
            return String(tile.chipTint) === String(Qt.color(window.colors.mutedText))
        })

        useFavicons.clicked()
        compare(window.useFavicons, true)
        engineLoader.item.pageIconUrl = ""
    }

    function test_tabArtworkSettingsAreLiveAndSaved() {
        const useFavicons = findChild(window.contentItem, "useFavicons")
        const tintFavicons = findChild(window.contentItem, "tintFavicons")
        verify(useFavicons !== null)
        verify(tintFavicons !== null)
        compare(window.useFavicons, true)
        compare(window.tintFavicons, true)

        useFavicons.clicked()
        compare(window.useFavicons, false)
        compare(useFavicons.checked, false)
        compare(tintFavicons.enabled, false)
        compare(browser.preference("use-favicons", "true"), "false")

        useFavicons.clicked()
        tintFavicons.clicked()
        compare(window.useFavicons, true)
        compare(window.tintFavicons, false)
        compare(browser.preference("tint-favicons", "true"), "false")

        tintFavicons.clicked()
        compare(window.tintFavicons, true)
    }
}
