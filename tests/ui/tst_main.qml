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

    function test_layoutKeepsChromeInSidebar() {
        const pinnedGrid = findChild(window.contentItem, "pinnedGrid")
        const spaceHeading = findChild(window.contentItem, "spaceHeading")
        const engineViewport = findChild(window.contentItem, "engineViewport")
        verify(pinnedGrid !== null)
        verify(spaceHeading !== null)
        verify(engineViewport !== null)
        verify(pinnedGrid.y < spaceHeading.y)
        compare(engineViewport.height, window.height)

        browser.toggleActivePinned()
        const pinnedDelegate = findChild(
            window.contentItem, "pinned-tab-" + browser.activeTabId)
        verify(pinnedDelegate !== null)
        tryVerify(function() { return pinnedDelegate.visible })
        verify(pinnedDelegate.width < 80)
        verify(pinnedGrid.y < spaceHeading.y)
        browser.toggleActivePinned()
    }

    function test_primaryChromeIsAccessibleFromKeyboard() {
        const newTabButton = findChild(window.contentItem, "newTabButton")
        const addressButton = findChild(window.contentItem, "addressButton")
        const collapseButton = findChild(window.contentItem, "collapseButton")
        const reloadButton = findChild(window.contentItem, "reloadButton")
        const pinButton = findChild(window.contentItem, "pinButton")
        const moveTabButton = findChild(window.contentItem, "moveTabButton")
        const manageSpacesButton = findChild(window.contentItem, "manageSpacesButton")
        const materialSymbolsFont = findChild(window, "materialSymbolsFont")

        compare(newTabButton.accessibleName, "New tab")
        compare(addressButton.accessibleName, "Search or enter address")
        compare(collapseButton.accessibleName, "Collapse sidebar")
        compare(moveTabButton.label, "drive_file_move")
        compare(manageSpacesButton.label, "more_horiz")
        verify(iconFontSource.toString().endsWith("/material-symbols-rounded.ttf"))
        verify(materialSymbolsFont !== null)
        tryCompare(materialSymbolsFont, "status", FontLoader.Ready)
        verify(newTabButton.activeFocusOnTab)
        verify(addressButton.activeFocusOnTab)
        verify(collapseButton.activeFocusOnTab)

        reloadButton.forceActiveFocus()
        compare(window.activeFocusItem.objectName, "reloadButton")
        keyClick(Qt.Key_Tab)
        compare(window.activeFocusItem.objectName, "pinButton")
        keyClick(Qt.Key_Tab)
        compare(window.activeFocusItem.objectName, "moveTabButton")
        keyClick(Qt.Key_Tab)
        compare(window.activeFocusItem.objectName, "addressButton")

        let visitedTab = false
        for (let step = 0; step < 8 && window.activeFocusItem.objectName !== "newTabButton"; ++step) {
            keyClick(Qt.Key_Tab)
            if (window.activeFocusItem.objectName.indexOf("tab-") === 0)
                visitedTab = true
        }
        verify(visitedTab)
        compare(window.activeFocusItem.objectName, "newTabButton")
        keyClick(Qt.Key_Tab)
        compare(window.activeFocusItem.objectName, "collapseButton")
        keyClick(Qt.Key_Backtab)
        compare(window.activeFocusItem.objectName, "newTabButton")
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
}
