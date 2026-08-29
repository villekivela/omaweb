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
    }

    function test_primaryChromeIsAccessibleFromKeyboard() {
        const newTabButton = findChild(window.contentItem, "newTabButton")
        const addressButton = findChild(window.contentItem, "addressButton")
        const collapseButton = findChild(window.contentItem, "collapseButton")

        compare(newTabButton.accessibleName, "New tab")
        compare(addressButton.accessibleName, "Search or enter address")
        compare(collapseButton.accessibleName, "Collapse sidebar")
        verify(newTabButton.activeFocusOnTab)
        verify(addressButton.activeFocusOnTab)
        verify(collapseButton.activeFocusOnTab)
    }
}
