import QtQuick
import QtTest
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

    function initTestCase() {
        window = windowComponent.createObject(null)
        verify(window !== null)
        window.show()
        wait(50)
    }

    function cleanupTestCase() {
        window.destroy()
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

    function test_applicationWindowUsesPlatformAppropriateFrame() {
        verifyApplicationWindowFlags(window)
    }

    function test_newTabWaitsForCommittedDestination() {
        const previousTabId = browser.activeTabId
        const newTabButton = findChild(window.contentItem, "newTabButton")
        verify(newTabButton !== null)

        newTabButton.forceActiveFocus()
        keyClick(Qt.Key_Return)
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

    function test_layoutKeepsChromeOutOfThePagesWay() {
        const settledSidebar = findChild(window.contentItem, "sidebar")
        window.sidebarCollapsed = false
        tryVerify(function() { return settledSidebar.visible && settledSidebar.width > 200 })

        const spaceHeading = findChild(window.contentItem, "spaceHeading")
        const engineViewport = findChild(window.contentItem, "engineViewport")
        const navigationCluster = findChild(window.contentItem, "navigationCluster")
        verify(spaceHeading !== null)
        verify(engineViewport !== null)
        verify(navigationCluster !== null)
        compare(engineViewport.height, window.height)

        // Navigation floats over the page instead of taking a band above it.
        verify(navigationCluster.height < engineViewport.height / 4)
        verify(navigationCluster.y > engineViewport.height / 2)

        // The Space heading opens the outline; pinned rows sit above the tabs.
        browser.toggleActivePinned()
        const pinnedRow = findChild(window.contentItem, "pinned-" + browser.activeTabId)
        verify(pinnedRow !== null)
        tryVerify(function() { return pinnedRow.visible && pinnedRow.height > 0 })

        const sidebar = findChild(window.contentItem, "sidebar")
        const headingTop = spaceHeading.mapToItem(sidebar, 0, 0).y
        tryVerify(function() {
            return pinnedRow.mapToItem(sidebar, 0, 0).y > headingTop
        })

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
        window.sidebarCollapsed = true
        tryVerify(function() { return !sidebar.visible })
        tryVerify(function() { return engineViewport.width > expandedViewport })
        verify(navigationCluster.visible)

        window.sidebarCollapsed = false
        tryVerify(function() { return sidebar.visible })
    }

    function test_switchingTabsPreservesPageLocalState() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        verify(engineHost !== null)
        verify(engineHost.item !== null)

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
        const newTabButton = findChild(window.contentItem, "newTabButton")
        const addressButton = findChild(window.contentItem, "addressButton")
        const collapseButton = findChild(window.contentItem, "collapseButton")
        const reloadButton = findChild(window.contentItem, "reloadButton")
        const commandPanelButton = findChild(window.contentItem, "commandPanelButton")
        const manageSpacesButton = findChild(window.contentItem, "manageSpacesButton")
        const settingsButton = findChild(window.contentItem, "settingsButton")
        const materialSymbolsFont = findChild(window, "materialSymbolsFont")

        compare(newTabButton.accessibleName, "New tab")
        compare(settingsButton.accessibleName, "Browsing settings and downloads")
        compare(addressButton.accessibleName, "Search or enter address")
        compare(collapseButton.accessibleName, "Hide sidebar")
        compare(commandPanelButton.accessibleName, "Command panel")
        compare(manageSpacesButton.label, "more_horiz")
        verify(iconFontSource.toString().endsWith("/material-symbols-rounded.ttf"))
        verify(materialSymbolsFont !== null)
        tryCompare(materialSymbolsFont, "status", FontLoader.Ready)
        verify(newTabButton.activeFocusOnTab)
        verify(addressButton.activeFocusOnTab)
        verify(collapseButton.activeFocusOnTab)

        addressButton.forceActiveFocus()
        compare(window.activeFocusItem.objectName, "addressButton")

        let visitedTab = false
        for (let step = 0; step < 10 && window.activeFocusItem.objectName !== "newTabButton"; ++step) {
            keyClick(Qt.Key_Tab)
            if (window.activeFocusItem.objectName.indexOf("tab-") === 0)
                visitedTab = true
        }
        verify(visitedTab)
        compare(window.activeFocusItem.objectName, "newTabButton")
        keyClick(Qt.Key_Tab)
        compare(window.activeFocusItem.objectName, "settingsButton")
        keyClick(Qt.Key_Backtab)
        compare(window.activeFocusItem.objectName, "newTabButton")
    }

    function test_everyBrowserCommandIsBoundAndSearchable() {
        const bindings = keyboardNavigation.browserBindings
        verify(Object.keys(bindings).length > 0)

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

    function test_spaceSwitchRecreatesEngineView() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        verify(engineLoader.item !== null)
        const personalSpaceId = browser.activeSpaceId
        const previousEngineView = engineLoader.item
        const workSpaceId = browser.createSpace("Work")

        verify(browser.switchSpace(workSpaceId))
        tryVerify(function() {
            return engineLoader.item !== null
                && engineLoader.item !== previousEngineView
                && engineLoader.item.profilePath === browser.activeProfilePath
        })

        verify(browser.deleteSpace(workSpaceId, ""))
        compare(browser.activeSpaceId, personalSpaceId)
        tryVerify(function() {
            return engineLoader.item !== null
                && engineLoader.item.profilePath === browser.activeProfilePath
        })
    }

    function test_privateWindowUsesTemporaryIdentityAndDistinctChrome() {
        compare(windowManager.privateWindowCount, 0)
        windowManager.openPrivateWindow()
        tryCompare(windowManager, "privateWindowCount", 1)

        const privateBrowser = findChild(window, "privateBrowserWindow")
        verify(privateBrowser !== null)
        verify(privateBrowser.visible)
        verifyApplicationWindowFlags(privateBrowser)
        compare(privateBrowser.colors.accent, privateBrowser.colors.privateAccent)

        const spaceSwitcher = findChild(privateBrowser.contentItem, "spaceSwitcher")
        const pinnedList = findChild(privateBrowser.contentItem, "pinnedList")
        const manageSpacesButton = findChild(privateBrowser.contentItem, "manageSpacesButton")
        const privateEngine = findChild(privateBrowser.contentItem, "engineLoader")
        const privateIndicator = findChild(privateBrowser.contentItem, "privateIndicator")
        verify(spaceSwitcher !== null)
        verify(pinnedList !== null)
        verify(manageSpacesButton !== null)
        verify(privateEngine !== null)
        verify(privateIndicator !== null)
        verify(!spaceSwitcher.visible)
        verify(!pinnedList.visible)
        verify(!manageSpacesButton.visible)
        verify(privateIndicator.visible)
        compare(privateEngine.item.profilePath, windowManager.privateProfilePath)
        compare(privateEngine.item.browserProfile, window.privateProfileHost.profile)

        privateBrowser.windowBrowser.closeActiveTab()
        tryCompare(windowManager, "privateWindowCount", 0)
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
        keyboardNavigationEnabled.click()
        compare(keyboardNavigation.enabled, false)
        keyboardNavigationEnabled.click()
        compare(keyboardNavigation.enabled, true)
    }
}
