import QtQuick
import QtTest
import Omaweb
import "../../src/ui" as Omaweb

TestCase {
    id: testCase
    name: "BrowserChrome"
    when: true

    property var window: null

    Component {
        id: windowComponent
        Omaweb.Main {}
    }

    // The harness loads Omaweb's own default keymap, which this build knows
    // every command in, so nothing is ever ignored in it. These stand the two
    // surfaces up against a keymap that did report something.
    Component {
        id: reportingSettingsComponent

        Omaweb.SettingsPage {
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

        Omaweb.SpaceOutline {
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

    // A button in a row the positioner has not laid out yet sits on top of its
    // neighbours, so a press meant for one lands on another. The first answer
    // in a row is at the left edge; every one after it has been moved.
    function settleActions(action) {
        if (!action) return
        tryVerify(function() { return action.width > 0 })
        wait(50)
    }

    // A row whose place in the list has stopped moving. The outline fills in
    // behind the model, so a row read too early is read where it is not going
    // to be.
    function settleRow(row) {
        let previous = -1
        let steady = 0
        tryVerify(function() {
            const at = row.mapToItem(window.contentItem, 0, 0).y
            steady = at === previous ? steady + 1 : 0
            previous = at
            return steady >= 3
        })
    }

    // A hand moving, rather than one jump. The events are sent in the window's
    // own coordinates because the row is about to leave the list and follow the
    // pointer: measured against the row itself, every step would be measured
    // from somewhere the row has already moved to.
    function dragRowBy(from, distance) {
        const steps = 6
        for (let step = 1; step <= steps; ++step) {
            mouseMove(window.contentItem, from.x, from.y + distance * step / steps)
            wait(1)
        }
    }

    // The speaker press as the sidebar reports it, so the test exercises the
    // shell's decision about what a press means rather than reaching past it.
    function sidebar_tabMuteToggled(tabId) {
        const outline = findChild(window.contentItem, "sidebar")
        verify(outline !== null)
        outline.tabMuteToggled(tabId)
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

    // Omaweb draws the page's context menu, so it offers what Omaweb can do with
    // what was under the pointer — and the engine's own menu never appears.
    function test_pageContextMenuOffersWhatWasUnderThePointer() {
        const engine = openPage("https://context.example/page")
        const menu = findChild(window.contentItem, "pageMenu")
        verify(menu !== null)
        verify(!menu.visible)

        function labels() {
            return window.pageMenuActions.map(function(row) {
                return row.separator === true ? "—" : row.label
            })
        }

        // Bare page: navigation, the address, and the inspector.
        engine.simulateContextMenu({})
        tryVerify(function() { return menu.visible })
        compare(labels(), ["Back", "Forward", "Reload", "Retry over insecure HTTP",
            "—", "Copy address",
            "—", "Inspect element"])
        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !menu.visible })

        // A link offers what you do with a link, first.
        engine.simulateContextMenu({"linkUrl": "https://linked.example/target"})
        tryVerify(function() { return menu.visible })
        compare(labels().slice(0, 5), ["Open link in new tab",
            "Open link in background", "Copy link address", "Save link as", "—"])

        // And running a row does that thing to that link.
        SystemClipboard.copyText("stale")
        window.runPageMenu(2)
        compare(SystemClipboard.text(), "https://linked.example/target")
        tryVerify(function() { return !menu.visible })

        // A selection offers copying it; an image offers its own address.
        engine.simulateContextMenu({"selectedText": "chosen words",
            "mediaUrl": "https://linked.example/cat.png", "mediaType": "image"})
        tryVerify(function() { return menu.visible })
        compare(labels().slice(0, 5), ["Open image in new tab", "Copy image address",
            "Copy image", "Save image as", "—"])
        window.runPageMenu(5)
        compare(SystemClipboard.text(), "chosen words")

        // Opening a link in a background tab leaves the reader where they were.
        const before = browser.activeTabId
        engine.simulateContextMenu({"linkUrl": "https://background.example/"})
        tryVerify(function() { return menu.visible })
        window.runPageMenu(1)
        compare(browser.activeTabId, before)
        tryVerify(function() {
            for (let row = 0; row < browser.tabs.rowCount(); ++row) {
                const index = browser.tabs.index(row, 0)
                if (String(browser.tabs.data(index, Qt.UserRole + 3))
                    === "https://background.example/") return true
            }
            return false
        })
    }

    // A row that cannot be run is listed and passed over rather than hidden:
    // the reader learns the command exists, and the keyboard never lands on it.
    function test_pageContextMenuSkipsRowsItCannotRun() {
        const engine = openPage("https://skips.example/")
        const menu = findChild(window.contentItem, "pageMenu")
        window.requestActivate()
        tryVerify(function() { return window.active })

        engine.simulateContextMenu({})
        tryVerify(function() { return menu.visible })
        // Back and Forward have nowhere to go on a tab with one page, so the
        // first row the keyboard can reach is Reload.
        compare(window.pageMenuActions[0].enabled, false)
        compare(window.pageMenuActions[1].enabled, false)
        compare(menu.selected, 2)

        // Stepping never stops on a separator.
        menu.step(1)
        compare(window.pageMenuActions[menu.selected].separator, undefined)
        menu.step(1)
        compare(window.pageMenuActions[menu.selected].separator, undefined)

        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !menu.visible })
    }

    function test_pageContextMenuOpensFromKeyboardAndCommandPanel() {
        const engine = openPage("https://keyboard-context.example/")
        const menu = findChild(window.contentItem, "pageMenu")
        window.requestActivate()
        tryVerify(function() { return window.active })

        keyClick(Qt.Key_F10, Qt.ShiftModifier)
        tryVerify(function() { return menu.visible })
        compare(engine.contextMenuRequestCount, 1)
        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !menu.visible })

        verify(window.commands.available("open-page-context-menu"))
        verify(window.commands.run("open-page-context-menu", -1))
        tryVerify(function() { return menu.visible })
        compare(engine.contextMenuRequestCount, 2)
    }

    function test_pageContextMenuRejectsAStaleTarget() {
        const engine = openPage("https://stale-target.example/before")
        SystemClipboard.copyText("keep this")
        engine.simulateContextMenu({"linkUrl": "https://stale-target.example/link"})
        tryVerify(function() { return window.pageMenuOpen })

        engine.currentUrl = "https://stale-target.example/after"
        tryVerify(function() {
            return browser.activeUrl.toString() === "https://stale-target.example/after"
        })
        window.runPageMenu(2)
        compare(SystemClipboard.text(), "keep this")
        verify(!window.pageMenuOpen)
    }

    function test_saveDialogRejectsATargetThatBecameStaleWhileOpen() {
        const engine = openPage("https://stale-save.example/before")
        window.pendingSaveEngine = engine
        window.pendingSaveAction = "save-link"
        window.pendingSaveTabId = browser.activeTabId
        window.pendingSaveGeneration = Number(engine.pageGeneration)
        engine.currentUrl = "https://stale-save.example/after"
        window.completeTargetSave("file:///tmp/archive.zip")
        compare(engine.lastContextAction, "")
    }

    function test_javascriptPromptsAreTabModalCancelableAndStoppable() {
        const engine = openPage("https://prompts.example/page")
        const bar = findChild(window.contentItem, "browserPromptBar")
        verify(bar !== null)

        engine.simulateJavaScriptPrompt("prompt", "https://prompts.example",
            "What should this page use?", "suggested")
        tryVerify(function() { return bar.visible })
        compare(window.pendingBrowserPrompt.kind, "javascript-prompt")
        compare(window.pendingBrowserPrompt.origin, "https://prompts.example")
        compare(window.pendingBrowserPrompt.defaultText, "suggested")

        window.respondToBrowserPrompt(false, "", "", "", true, false)
        compare(engine.lastPromptAccepted, false)
        verify(engine.javaScriptDialogsBlocked)
        verify(!bar.visible)

        engine.simulateJavaScriptPrompt("confirm", "https://prompts.example",
            "This must not open", "")
        wait(20)
        verify(!bar.visible)
    }

    function test_pagePromptDoesNotFollowTheReaderToAnotherTab() {
        const engine = openPage("https://prompt-tab.example/")
        const promptTabId = browser.activeTabId
        engine.simulateJavaScriptPrompt("confirm", "https://prompt-tab.example",
            "Stay on this tab?", "")
        tryVerify(function() { return window.browserPromptOpen })

        browser.openInput("https://another-tab.example/", true)
        tryVerify(function() { return browser.activeUrl.toString()
            === "https://another-tab.example/" })
        verify(!window.browserPromptOpen)
        verify(!engine.lastPromptAccepted)

        browser.activateTab(promptTabId)
        tryVerify(function() { return window.browserPromptOpen })
        window.respondToBrowserPrompt(false, "", "", "", false, false)
        verify(!window.browserPromptOpen)
    }

    function test_httpAuthenticationCredentialsStayInTheLiveEngine() {
        const engine = openPage("https://auth.example/private")
        engine.simulateHttpAuthentication("https://auth.example", "Members")
        tryVerify(function() { return window.browserPromptOpen })
        compare(window.pendingBrowserPrompt.kind, "http-authentication")
        compare(window.pendingBrowserPrompt.detail, "Members")

        window.respondToBrowserPrompt(true, "", "reader", "secret", false, false)
        compare(engine.lastPromptResponse.user, "reader")
        compare(engine.lastPromptResponse.password, "secret")
        verify(!window.browserPromptOpen)
        compare(browser.preference("http-authentication", "missing"), "missing")
    }

    function test_externalProtocolConfirmationNamesDestinationAndCanBeRemembered() {
        const engine = openPage("https://calendar.example/event")
        const destination = "webcal://calendar.example/team?id=42"
        engine.simulateExternalProtocol("Calendar", destination)
        tryVerify(function() { return window.browserPromptOpen })
        compare(window.pendingBrowserPrompt.kind, "external-protocol")
        compare(window.pendingBrowserPrompt.application, "Calendar")
        compare(window.pendingBrowserPrompt.scheme, "webcal")
        compare(window.pendingBrowserPrompt.origin, "https://calendar.example")
        compare(window.pendingBrowserPrompt.destination, destination)

        window.respondToBrowserPrompt(true, "", "", "", false, true)
        compare(engine.externalOpenCount, 1)
        verify(browser.externalProtocolAllowed(
            "https://calendar.example/elsewhere", "webcal"))

        engine.simulateExternalProtocol("Calendar", "webcal://calendar.example/next")
        compare(engine.externalOpenCount, 2)
        verify(!window.browserPromptOpen)
    }

    function test_targetActionsUseNativeSaveAndFileSelectionBoundaries() {
        const engine = openPage("https://files.example/form")
        const nativeOpen = findChild(window, "openFileDialog")
        const nativeSelection = findChild(window, "pageFileDialog")
        const nativeSave = findChild(window, "saveTargetDialog")
        verify(nativeOpen !== null)
        verify(nativeSelection !== null)
        verify(nativeSave !== null)
        verify(window.commands.available("open-file"))

        engine.simulateContextMenu({
            "linkUrl": "https://files.example/archive.zip",
            "mediaUrl": "https://files.example/photo.png",
            "mediaType": "image"
        })
        tryVerify(function() { return window.pageMenuOpen })
        const labels = window.pageMenuActions.map(function(row) { return row.label || "" })
        verify(labels.indexOf("Save link as") >= 0)
        verify(labels.indexOf("Copy image") >= 0)
        verify(labels.indexOf("Save image as") >= 0)

        engine.simulateFileSelection("open-multiple", ["image/png"])
        compare(window.pendingFileSelection.mode, "open-multiple")
        window.respondToFileSelection(["/tmp/one.png", "/tmp/two.png"])
        compare(engine.lastSelectedFiles.length, 2)
        engine.simulateFileSelection("open", ["text/plain"])
        window.respondToFileSelection([])
        verify(engine.fileSelectionCancelled)
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
        // The speaker is about muting here, so the origin is dealt with first:
        // a site the reader has not touched is held silent, and the speaker
        // answers for that instead.
        engine.simulateUserActivation()
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

    // The lock is the engine's report, not a reading of the address bar. A
    // certificate failure is drawn as one and stays drawn while the exception
    // the reader granted is in effect.
    // An Auxiliary window is where a sign-in or a payment finishes, so it is
    // exactly where a certificate failure must not be waved through. It asks
    // the same question of the same rule as an ordinary tab, in the window that
    // opened it.
    function test_auxiliaryWindowsAskTheSameCertificateQuestion() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        openPage("https://auxiliary-opener.example/start")
        engineLoader.item.simulateNewWindowRequest("https://localhost:9443/callback", true)
        const auxiliary = findChild(window, "auxiliaryWindow")
        tryVerify(function() { return auxiliary !== null && auxiliary.visible })
        const auxiliaryEngine = findChild(auxiliary.contentItem, "auxiliaryEngineLoader")
        tryVerify(function() { return auxiliaryEngine.item !== null })

        const bar = findChild(window.contentItem, "certificateQuestionBar")
        verify(bar !== null)
        verify(!bar.open)

        // A public site inside an Auxiliary window is refused, with no question
        // asked of the reader.
        const refused = auxiliaryEngine.item.simulateCertificateError({
            "url": "https://bank.example/callback"
        })
        verify(!bar.open)
        compare(auxiliaryEngine.item.certificateDecisions[refused], false)

        // The Local-development main frame is the one case that is offered, and
        // the answer reaches the Auxiliary window's own engine.
        const offered = auxiliaryEngine.item.simulateCertificateError({})
        tryVerify(function() { return bar.open })
        const action = findChild(bar, "questionAction0")
        settleActions(action)
        mouseClick(action, action.width / 2, action.height / 2)
        tryVerify(function() { return !bar.open })
        compare(auxiliaryEngine.item.certificateDecisions[offered], true)

        auxiliaryEngine.item.simulateWindowCloseRequest()
        tryVerify(function() { return !auxiliary.visible })
        window.requestActivate()
        tryVerify(function() { return window.active })
    }

    function test_addressTriggerReportsOnlyWhatTheEngineKnows() {
        const engine = openPage("https://reported-secure.example/page")
        const security = findChild(window.contentItem, "securityIndicator")
        verify(security !== null)
        tryCompare(security, "text", "lock")

        // The engine says the connection is in error; the trigger follows it
        // rather than the https it can see in the address.
        const requestId = engine.simulateCertificateError({})
        tryCompare(security, "text", "warning")
        compare(engine.connectionState, "certificate-error")

        // Granting the exception does not turn the warning into a lock: the
        // check was waived, not passed.
        engine.respondToCertificateError(requestId, true)
        tryCompare(security, "text", "warning")

        openPage("http://reported-insecure.example/page")
        tryCompare(security, "text", "lock_open")
        openPage("https://reported-secure-again.example/page")
        tryCompare(security, "text", "lock")
    }

    // An engine stops reporting a certificate failure once it has been told to
    // accept the certificate, and starts calling the connection secure. Coming
    // back to a site whose check the reader waived must still say so.
    function test_aWaivedCertificateCheckStaysVisibleForTheSession() {
        const engine = openPage("https://localhost:7443/waived")
        const security = findChild(window.contentItem, "securityIndicator")
        const bar = findChild(window.contentItem, "certificateQuestionBar")

        const requestId = engine.simulateCertificateError({})
        tryVerify(function() { return bar.open })
        const action = findChild(bar, "questionAction0")
        settleActions(action)
        mouseClick(action, action.width / 2, action.height / 2)
        tryVerify(function() { return !bar.open })
        verify(browser.certificateExceptionInEffect("https://localhost:7443/waived"))

        // The engine has forgotten the failure — this is exactly what it does
        // after accepting — and the trigger still warns, because Omaweb has not.
        engine.certificateErrorOrigin = ""
        compare(engine.connectionState, "secure")
        tryCompare(security, "text", "warning")

        // Reading elsewhere and coming back to the same origin says it again.
        openPage("https://elsewhere-after-waiver.example/page")
        tryCompare(security, "text", "lock")
        openPage("https://localhost:7443/another-page")
        tryCompare(security, "text", "warning")

        mouseClick(security, security.width / 2, security.height / 2)
        const panel = findChild(window.contentItem, "siteInformationPanel")
        tryVerify(function() { return panel.visible })
        const connection = findChild(window.contentItem, "siteInformationConnection")
        compare(connection.text,
            "· certificate could not be verified · waived for this session")
        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !panel.visible })
    }

    // Nothing in the panel may reach past its own border. Every answer it
    // offers is a button the reader has to be able to hit, and a button drawn
    // outside the panel is drawn over the page behind it.
    function test_siteInformationKeepsEveryAnswerInsideItsBorder() {
        const engine = openPage("https://panel-geometry.example/page")
        const sidebar = findChild(window.contentItem, "sidebar")
        const panel = findChild(window.contentItem, "siteInformationPanel")
        const width = window.sidebarWidth
        // The narrowest the sidebar goes, which is where an overflowing row
        // shows up first. The width eases, so it is waited for.
        window.sidebarWidth = window.sidebarMinimumWidth
        tryVerify(function() { return sidebar.width === window.sidebarMinimumWidth })
        engine.persistentProfilesAvailable = true
        sidebar.statusOpen = true
        tryVerify(function() { return panel.visible })

        // The longest content the panel ever carries: a refused third party
        // with an origin no sidebar is wide enough for, and an allowance.
        panel.refusedThirdParties = [
            "https://private-user-images.githubusercontent.com",
            "https://avatars.githubusercontent.com"
        ]
        browser.allowThirdPartyCookies(
            "https://collector-pxxxxxx.eu-north-1.example", "payment")
        panel.refreshSiteInformation()
        panel.retainedDataBytes = 322.7 * 1024 * 1024

        const answers = ["clearSiteStorage", "clearSiteData", "resetSitePermissions",
            "manageThirdParties"]
        const lines = ["siteInformationOrigin", "siteInformationConnection",
            "siteInformationSiteData", "siteInformationRetainedData",
            "siteInformationCookies", "refusedThirdParty0", "cookieAllowance0",
            "refusedThirdPartyOverflow", "siteInformationNoPermissions"]
        for (const name of answers) settleActions(findChild(window.contentItem, name))

        // Measured first, asserted last: restoring the window before the
        // verify keeps a failure here from reaching the next test.
        const problems = []
        const left = panel.mapToItem(window.contentItem, 0, 0).x
        const right = left + panel.width
        if (right > sidebar.width) {
            problems.push("the panel ends at " + right + ", past the sidebar's " + sidebar.width)
        }
        for (const name of answers.concat(lines)) {
            const item = findChild(window.contentItem, name)
            if (item === null) {
                if (answers.indexOf(name) !== -1) problems.push(name + " is missing")
                continue
            }
            if (!item.visible) continue
            const at = item.mapToItem(window.contentItem, 0, 0)
            if (at.x < left) problems.push(name + " starts at " + at.x + ", left of " + left)
            if (at.x + item.width > right) {
                problems.push(name + " ends at " + (at.x + item.width) + ", past " + right)
            }
        }

        browser.revokeThirdPartyCookieAllowance(
            "https://collector-pxxxxxx.eu-north-1.example")
        panel.refusedThirdParties = []
        engine.persistentProfilesAvailable = false
        sidebar.statusOpen = false
        window.sidebarWidth = width
        tryVerify(function() { return !panel.visible })

        compare(problems.join("; "), "")
    }

    // Every question the panel leads to is asked in the window's own centred
    // dialog, which has room to name the scope. The panel goes away when the
    // dialog opens, so one surface holds the question.
    // `prepare` runs once the panel is open, for the state the lab has no
    // engine to supply — the panel reads that when it opens, so naming it
    // earlier would be overwritten.
    function openSiteAction(name, prepare) {
        const sidebar = findChild(window.contentItem, "sidebar")
        const panel = findChild(window.contentItem, "siteInformationPanel")
        sidebar.statusOpen = true
        tryVerify(function() { return panel.visible })
        if (prepare !== undefined) prepare(panel)
        const trigger = findChild(window.contentItem, name)
        verify(trigger !== null, name + " is missing")
        verify(trigger.enabled, name + " is not enabled")
        settleActions(trigger)
        mouseClick(trigger, trigger.width / 2, trigger.height / 2)
        const dialog = findChild(window.contentItem, "spaceDialog")
        tryVerify(function() { return dialog.open })
        // One surface holds the question: the panel goes away behind it.
        verify(!panel.visible)
        return dialog
    }

    // The only clearing that is about the site the panel is headed by. The
    // engine exposes no per-origin removal, so the page is asked to empty its
    // own storage and reports what it managed to take.
    function test_siteInformationEmptiesOneSitesStorageThroughItsPage() {
        const engine = openPage("https://site-storage.example/app")
        const dialog = openSiteAction("clearSiteStorage")

        // The dialog names the site, the scope, and what it cannot take.
        verify(window.dialogMode === "site-storage")
        verify(dialog.message.indexOf("site-storage.example") !== -1)
        verify(dialog.message.indexOf("local storage, databases") !== -1)
        verify(dialog.message.indexOf("cookies are not included") !== -1)
        compare(engine.pageSiteDataClearCount, 0)

        keyClick(Qt.Key_Return)
        tryVerify(function() { return engine.pageSiteDataClearCount === 1 })
        const notice = findChild(window.contentItem, "pageNotice")
        tryVerify(function() { return notice.showing })
        compare(notice.message, "Emptied site-storage.example's storage")
        verify(notice.detail.indexOf("cookies are cleared for the whole Space") !== -1)

        // A page holding nothing says so rather than reporting a success the
        // reader would read as having taken something.
        engine.pageSiteData = []
        openSiteAction("clearSiteStorage")
        keyClick(Qt.Key_Return)
        tryVerify(function() {
            return notice.message === "site-storage.example had nothing stored"
        })

        // And a page that cannot answer is not reported as one that did.
        engine.pageSiteDataRefusal = "databases could not be emptied"
        openSiteAction("clearSiteStorage")
        keyClick(Qt.Key_Return)
        tryVerify(function() {
            return notice.message === "Could not empty site-storage.example's storage"
        })
        compare(notice.detail, "databases could not be emptied")

        engine.pageSiteDataRefusal = ""
        engine.pageSiteData = ["local storage", "databases"]
    }

    // Clearing cookies and cache is the Space's, because the engine can only
    // take those for every site at once. The dialog says so before it happens.
    function test_siteInformationClearsTheSpacesDataOnceConfirmed() {
        const engine = openPage("https://space-data.example/page")
        const sidebar = findChild(window.contentItem, "sidebar")
        // An engine that keeps a profile on disk and names what it keeps
        // there, which the lab otherwise does not.
        engine.persistentProfilesAvailable = true
        sidebar.siteDataEntries = ["Cookies", "cache"]
        sidebar.retainedDataEntries = ["Local Storage", "IndexedDB"]
        window.spaceProfileHost.untouchedCategories = ["storage"]

        const panel = findChild(window.contentItem, "siteInformationPanel")
        sidebar.statusOpen = true
        tryVerify(function() { return panel.visible })
        const siteData = findChild(window.contentItem, "siteInformationSiteData")
        tryVerify(function() {
            return siteData.text.indexOf("of cookies and cache in this Space") !== -1
        })
        // What the engine holds and cannot take is a line of its own, never a
        // byte counted as clearable.
        const retained = findChild(window.contentItem, "siteInformationRetainedData")
        verify(retained !== null)
        verify(!retained.visible)
        panel.retainedDataBytes = 900 * 1024 * 1024
        tryVerify(function() { return retained.visible })
        compare(retained.text, "· 900 MB of storage and databases")
        sidebar.statusOpen = false

        const dialog = openSiteAction("clearSiteData")
        verify(dialog.message.indexOf("Every site in") !== -1)
        verify(dialog.message.indexOf("Storage and databases stay") !== -1)
        const cleared = window.spaceProfileHost.browsingDataClearCount

        keyClick(Qt.Key_Return)
        tryVerify(function() {
            return window.spaceProfileHost.browsingDataClearCount === cleared + 1
        })
        // The notice says what was taken, and the engine is what says which
        // categories it could not take.
        const notice = findChild(window.contentItem, "pageNotice")
        tryVerify(function() { return notice.showing })
        compare(notice.message, "Cleared this Space's cookies and cache")
        compare(notice.detail, "storage stayed: this engine has no way to remove them")

        engine.persistentProfilesAvailable = false
        sidebar.siteDataEntries = []
        sidebar.retainedDataEntries = []
        window.spaceProfileHost.untouchedCategories = []
    }

    // A blocked third party is named in the panel and answered in the dialog,
    // where there is room to say what allowing one is for. A reader looking at
    // an embedded asset host cannot judge it from its name alone.
    function test_thirdPartyAllowanceIsChosenInTheDialog() {
        openPage("https://allowance.example/checkout")
        const sidebar = findChild(window.contentItem, "sidebar")
        const panel = findChild(window.contentItem, "siteInformationPanel")
        sidebar.statusOpen = true
        tryVerify(function() { return panel.visible })

        // With nothing refused and nothing allowed there is nothing to answer.
        const trigger = findChild(window.contentItem, "manageThirdParties")
        verify(trigger !== null)
        verify(!trigger.enabled)

        // The lab has no third-party filter, so the origins one would have
        // refused are named here.
        const refused = ["https://pay.example", "https://cdn.example",
            "https://images.example", "https://fonts.example"]
        const name = function(surface) { surface.refusedThirdParties = refused }
        panel.refusedThirdParties = refused
        tryVerify(function() { return trigger.enabled })
        // Only the first few are listed; the rest are the dialog's to show.
        verify(findChild(window.contentItem, "refusedThirdParty2") !== null)
        compare(findChild(window.contentItem, "refusedThirdParty3"), null)
        const overflow = findChild(window.contentItem, "refusedThirdPartyOverflow")
        tryVerify(function() { return overflow.visible })
        compare(overflow.text, "· and 1 more, listed under third parties")

        sidebar.statusOpen = false

        const dialog = openSiteAction("manageThirdParties", name)
        verify(dialog.message.indexOf("not working") !== -1)
        verify(dialog.message.indexOf("does not need it") !== -1)
        compare(window.thirdPartyRows.length, 8)
        compare(findChild(window.contentItem, "commandDialogRow0").objectName,
            "commandDialogRow0")

        // The second row is the payment answer for the first origin.
        keyClick(Qt.Key_Down)
        keyClick(Qt.Key_Return)
        tryVerify(function() { return browser.thirdPartyCookieAllowances().length === 1 })
        compare(browser.thirdPartyCookieAllowances()[0].origin, "https://pay.example")
        compare(browser.thirdPartyCookieAllowances()[0].purpose, "payment")
        verify(browser.thirdPartyCookiesAllowed(browser.activeSpaceId, "https://pay.example"))
        const notice = findChild(window.contentItem, "pageNotice")
        tryVerify(function() { return notice.showing })
        compare(notice.message, "Allowing https://pay.example")
        verify(notice.detail.indexOf("for a payment") !== -1)

        // The panel now names it as allowed, beside the ones still refused.
        sidebar.statusOpen = true
        tryVerify(function() { return panel.visible })
        panel.refusedThirdParties = refused
        const allowed = findChild(window.contentItem, "cookieAllowance0")
        verify(allowed !== null)
        compare(allowed.text, "· https://pay.example — allowed for payment")
        sidebar.statusOpen = false

        // An allowance is taken back the same way, from the top of the list.
        openSiteAction("manageThirdParties", name)
        compare(window.thirdPartyRows[0].purpose, "")
        keyClick(Qt.Key_Return)
        tryVerify(function() { return browser.thirdPartyCookieAllowances().length === 0 })
        verify(!browser.thirdPartyCookiesAllowed(browser.activeSpaceId, "https://pay.example"))
        tryVerify(function() { return notice.message === "Stopped allowing https://pay.example" })

        panel.refusedThirdParties = []
    }

    function test_siteStatusStaysWithAddressAndDismisses() {
        openPage("https://status-position.example")
        const sidebar = findChild(window.contentItem, "sidebar")
        const address = findChild(window.contentItem, "addressButton")
        const security = findChild(window.contentItem, "securityIndicator")
        const panel = findChild(window.contentItem, "siteInformationPanel")
        verify(sidebar !== null)
        verify(address !== null)
        verify(security !== null)
        verify(panel !== null)

        mouseClick(security, security.width / 2, security.height / 2)
        tryVerify(function() { return panel.visible })
        const addressBottom = address.mapToItem(window.contentItem, 0, address.height).y
        const panelTop = panel.mapToItem(window.contentItem, 0, 0).y
        verify(panelTop >= addressBottom + 6)
        verify(panelTop <= addressBottom + 10)

        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !panel.visible })

        mouseClick(security, security.width / 2, security.height / 2)
        tryVerify(function() { return panel.visible })
        mouseClick(window.contentItem, sidebar.width + 80, window.height / 2)
        tryVerify(function() { return !panel.visible })
    }

    // A pin is a square holding one chip, with nothing in front of anything to
    // put a speaker before, so it wears the speaker in its top right corner.
    function test_soundingPinWearsItsSpeakerInTheCorner() {
        const engine = openPage("https://sounding-pin.example")
        // As above: the corner speaker is about muting, so the origin is dealt
        // with before the press is expected to mean it.
        engine.simulateUserActivation()
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

        // Navigation floats over the page instead of taking a band above it,
        // and it floats where the outline's own controls are: at the top, so
        // hiding the sidebar does not move the commands to the floor.
        verify(navigationCluster.height < engineViewport.height / 4)
        verify(navigationCluster.y < engineViewport.height / 2)
        verify(navigationCluster.mapToItem(engineViewport, 0, 0).y
            < sidebarNavigation.mapToItem(engineViewport, 0, 0).y
                + sidebarNavigation.height)

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

        // The browsing identity closes the outline, below every tab row, and a
        // rule separates it from the list rather than letting it read as one
        // more row.
        verify(spaceHeading.mapToItem(sidebar, 0, 0).y
            > pinnedRow.mapToItem(sidebar, 0, 0).y)
        const footerRule = findChild(window.contentItem, "outlineFooterRule")
        verify(footerRule !== null)
        verify(footerRule.visible)
        compare(footerRule.height, 1)
        verify(footerRule.mapToItem(sidebar, 0, 0).y
            > pinnedRow.mapToItem(sidebar, 0, 0).y)
        verify(footerRule.mapToItem(sidebar, 0, 0).y
            < spaceHeading.mapToItem(sidebar, 0, 0).y)
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

    // Only the Space on show keeps live pages. Putting one away takes its
    // renderers with it: coming back reloads its tabs from their addresses
    // rather than finding the very pages that were left. That is the memory
    // policy the browser is built on, not a shortcoming of the switch.
    function test_spaceSuspensionTakesThePagesItPutsAway() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        openPage("https://personal-space.example")
        const personalSpaceId = browser.activeSpaceId
        const personalTabId = browser.activeTabId
        const personalEngineView = engineLoader.item
        const workSpaceId = browser.createSpace("Work")

        verify(browser.switchSpace(workSpaceId))
        // The page is gone, not hidden: nothing is being kept for a Space the
        // reader is not looking at.
        tryVerify(function() { return engineLoader.engines[personalTabId] === undefined })
        verify(!engineLoader.keepsEngineFor(personalTabId))
        openPage("https://work-space.example")
        const workEngineView = engineLoader.item
        const workTabId = browser.activeTabId

        verify(browser.switchSpace(personalSpaceId))
        tryVerify(function() {
            return engineLoader.item !== null
                && engineLoader.item !== personalEngineView
                && engineLoader.item !== workEngineView
                && String(engineLoader.item.currentUrl) === "https://personal-space.example"
                && engineLoader.item.profilePath === browser.activeProfilePath
        })

        verify(browser.deleteSpace(workSpaceId, "Work"))
        compare(browser.activeSpaceId, personalSpaceId)
        verify(engineLoader.engines[workTabId] === undefined)
    }

    // The two exceptions to that policy, and nothing else: a Pinned tab the
    // reader marked Keep active, and the tab an inspector is attached to. Both
    // keep their page while their Space is away, both are named in the list of
    // what is being retained, and both are reported with what they cost.
    function test_suspensionKeepsOnlyTheTabsTheCoreRetains() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        const personalSpaceId = browser.activeSpaceId

        openPage("https://kept.example/room")
        const keptTabId = browser.activeTabId
        browser.toggleActivePinned()
        verify(browser.setTabKeepActive(keptTabId, true))
        const keptEngineView = engineLoader.engines[keptTabId]
        verify(keptEngineView !== undefined)

        browser.openInput("https://inspected.example", true)
        tryVerify(function() { return engineLoader.item !== null })
        const inspectedTabId = browser.activeTabId
        browser.openDeveloperTools()

        browser.openInput("https://ordinary.example", true)
        tryVerify(function() { return engineLoader.item !== null })
        const ordinaryTabId = browser.activeTabId

        const workSpaceId = browser.createSpace("Retained")
        verify(browser.switchSpace(workSpaceId))

        // The ordinary page is gone; the retained ones are still running.
        tryVerify(function() { return engineLoader.engines[ordinaryTabId] === undefined })
        verify(engineLoader.keepsEngineFor(keptTabId))
        verify(engineLoader.keepsEngineFor(inspectedTabId))
        verify(!engineLoader.keepsEngineFor(ordinaryTabId))
        compare(engineLoader.engines[keptTabId], keptEngineView)

        // The reader can see the whole list and what each one holds.
        tryVerify(function() { return engineLoader.retainedTabReport().length === 2 })
        const report = engineLoader.retainedTabReport()
        let keptReport = null
        for (let index = 0; index < report.length; ++index) {
            if (report[index].tabId === keptTabId) keptReport = report[index]
        }
        verify(keptReport !== null)
        compare(keptReport.spaceName, "Personal")
        verify(keptReport.running)

        // Coming back finds the retained page where it was left, and reloads
        // the ordinary one.
        verify(browser.switchSpace(personalSpaceId))
        tryVerify(function() { return engineLoader.engines[keptTabId] === keptEngineView })
        tryVerify(function() { return !engineLoader.keepsEngineFor(keptTabId) })

        browser.closeDeveloperTools()
        browser.setTabKeepActive(keptTabId, false)
        browser.activateTab(keptTabId)
        browser.toggleActivePinned()
        browser.closeTab(keptTabId)
        browser.closeTab(inspectedTabId)
        browser.closeTab(ordinaryTabId)
        verify(browser.deleteSpace(workSpaceId, "Retained"))
    }

    // A retained page kept playing and kept its artwork while its Space was
    // away. The Space's tabs come back from a store that records neither, so
    // the row reads both off the page that outlived the switch.
    function test_retainedTabBringsBackItsIconAndItsSound() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        verify(engineLoader !== null)
        const personalSpaceId = browser.activeSpaceId
        const keptEngineView = openPage("https://kept-sound.example")
        const keptTabId = browser.activeTabId
        browser.toggleActivePinned()
        verify(browser.setTabKeepActive(keptTabId, true))

        const iconUrl = "https://kept-sound.example/favicon.ico"
        keptEngineView.pageIconUrl = iconUrl
        keptEngineView.simulateAudible(true)
        const showsIcon = function() {
            const row = findChild(window.contentItem, "pinned-" + keptTabId)
            return row !== null && String(row.tabIconUrl) === iconUrl
        }
        tryVerify(showsIcon)

        const workSpaceId = browser.createSpace("Sound")
        verify(browser.switchSpace(workSpaceId))
        verify(browser.switchSpace(personalSpaceId))

        tryVerify(function() { return engineLoader.engines[keptTabId] === keptEngineView })
        tryVerify(showsIcon)
        const speaker = findChild(window.contentItem, "audio-" + keptTabId)
        verify(speaker !== null)
        tryVerify(function() { return speaker.visible })

        keptEngineView.simulateAudible(false)
        browser.setTabKeepActive(keptTabId, false)
        browser.activateTab(keptTabId)
        browser.toggleActivePinned()
        browser.closeTab(keptTabId)
        verify(browser.deleteSpace(workSpaceId, "Sound"))
    }

    // Muting is the session's, not the page's: a tab whose renderer was thrown
    // away with its Space comes back muted from the store, and the engine that
    // reloads it is told so.
    function test_mutingComesBackFromTheSessionAfterSuspension() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const personalSpaceId = browser.activeSpaceId
        const engineView = openPage("https://muted.example")
        const tabId = browser.activeTabId
        browser.toggleTabMuted(tabId)
        tryVerify(function() { return engineView.audioMuted })

        const workSpaceId = browser.createSpace("Muting")
        verify(browser.switchSpace(workSpaceId))
        tryVerify(function() { return engineLoader.engines[tabId] === undefined })
        verify(browser.switchSpace(personalSpaceId))

        // A different engine, drawing the same tab, muted because the tab is.
        tryVerify(function() {
            const reloaded = engineLoader.engines[tabId]
            return reloaded !== undefined && reloaded !== engineView && reloaded.audioMuted
        })
        const row = findChild(window.contentItem, "tab-" + tabId)
        verify(row !== null)
        verify(row.tabMuted)

        browser.toggleTabMuted(tabId)
        verify(browser.deleteSpace(workSpaceId, "Muting"))
    }

    // A page may start playing on its own; what waits for the reader is the
    // sound. Until they have dealt with the origin the tab is held silent, and
    // once they have, every tab on that origin is heard — including the one
    // they never touched.
    function test_soundWaitsForTheOriginToBeDealtWith() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const engineView = openPage("https://autoplay.example/clip")
        const tabId = browser.activeTabId

        verify(browser.tabSoundSuppressed(tabId))
        tryVerify(function() { return engineView.autoplayAllowed && engineView.audioMuted })
        // The page starts, and is not heard.
        verify(engineView.simulateAutoplay())
        verify(!engineView.pageAudible)

        // The row says so where it says muting, because that is what it is from
        // where the reader sits, and offers the sound back in the same place.
        const row = findChild(window.contentItem, "tab-" + tabId)
        verify(row !== null)
        verify(row.tabSoundSuppressed)
        verify(row.silenced)
        verify(!row.tabMuted)

        // A second tab on the same site is held silent too.
        browser.openInput("https://autoplay.example/other", true)
        tryVerify(function() { return engineLoader.item !== null })
        const secondTabId = browser.activeTabId
        const secondEngineView = engineLoader.engines[secondTabId]
        verify(secondEngineView !== undefined)
        tryVerify(function() { return secondEngineView.audioMuted })

        // A gesture on one page answers for the origin, so both are heard.
        engineView.simulateUserActivation()
        tryVerify(function() {
            return !engineView.audioMuted && !secondEngineView.audioMuted
        })
        verify(!browser.tabSoundSuppressed(tabId))
        verify(secondEngineView.simulateAutoplay())
        verify(secondEngineView.pageAudible)

        // The reader's own muting is still theirs, and still says so.
        browser.toggleTabMuted(secondTabId)
        tryVerify(function() { return secondEngineView.audioMuted })
        verify(!browser.tabSoundSuppressed(secondTabId))

        secondEngineView.simulateAudible(false)
        browser.closeTab(secondTabId)
        browser.closeTab(tabId)
    }

    // Asking a silenced row for its sound is the reader dealing with the
    // origin, not a muting decision of their own: the tab is heard, and the
    // next tab on that site is too.
    function test_theRowGivesBackASilencedTabsSound() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const engineView = openPage("https://granted.example/page")
        const tabId = browser.activeTabId
        verify(browser.tabSoundSuppressed(tabId))

        const speaker = findChild(window.contentItem, "audio-" + tabId)
        verify(speaker !== null)
        // The row only shows the speaker once there is sound to give back.
        engineView.simulateAudible(true)
        tryVerify(function() { return speaker.visible })
        compare(speaker.Accessible.name.indexOf("Allow sound from"), 0)

        sidebar_tabMuteToggled(tabId)
        tryVerify(function() { return !browser.tabSoundSuppressed(tabId) })
        tryVerify(function() { return !engineView.audioMuted })
        // Not a muting: pressing it again now mutes, as it always did.
        sidebar_tabMuteToggled(tabId)
        tryVerify(function() { return engineView.audioMuted })
        const row = findChild(window.contentItem, "tab-" + tabId)
        verify(row.tabMuted)

        browser.toggleTabMuted(tabId)
        engineView.simulateAudible(false)
        browser.closeTab(tabId)
    }

    // A notification names the origin and the Space before it says anything the
    // page wrote, and answering it goes to the tab that sent it.
    function test_notificationNamesOriginAndSpaceAndAnswersToItsTab() {
        const personalSpaceId = browser.activeSpaceId
        openPage("https://chat.example/room")
        const chatTabId = browser.activeTabId
        const host = window.profiles.hosts[personalSpaceId]
        verify(host !== undefined)

        const notificationId = host.simulateNotification(
            "https://chat.example", "New message", "Someone said something")
        const key = personalSpaceId + ":" + notificationId
        tryVerify(function() { return window.notifications.pending[key] !== undefined })
        const raised = window.notifications.pending[key]
        compare(raised.heading, "https://chat.example · Personal")
        compare(raised.detail, "New message — Someone said something")
        compare(raised.tabId, chatTabId)

        // Answering it takes the reader to the page that sent it, and the page
        // hears the click.
        browser.openInput("https://elsewhere.example", true)
        const elsewhereTabId = browser.activeTabId
        window.notifications.answer(key, true)
        compare(browser.activeTabId, chatTabId)
        verify(host.activatedNotifications.indexOf(notificationId) >= 0)
        verify(window.notifications.pending[key] === undefined)

        // A page whose Space has been put away, and which nothing is keeping
        // running, is refused rather than reaching the desktop.
        const workSpaceId = browser.createSpace("Notifying")
        verify(browser.switchSpace(workSpaceId))
        const refusedId = host.simulateNotification(
            "https://chat.example", "Ignored", "Nobody should see this")
        tryVerify(function() { return host.dismissedNotifications.indexOf(refusedId) >= 0 })
        verify(window.notifications.pending[personalSpaceId + ":" + refusedId] === undefined)

        verify(browser.switchSpace(personalSpaceId))
        browser.closeTab(elsewhereTabId)
        browser.closeTab(chatTabId)
        verify(browser.deleteSpace(workSpaceId, "Notifying"))
    }

    // A row's menu is about that row. The ordinary rows are offered the two
    // sweeping closes and their own close; a pin is offered neither, and gets
    // Keep active instead.
    function test_tabMenuOffersClosesToOrdinaryRowsAndKeepActiveToPins() {
        openPage("https://menu.example/one")
        const ordinaryTabId = browser.activeTabId
        const ordinaryLabels = window.tabMenuActionsFor(ordinaryTabId).map(
            function(action) { return action.label })
        verify(ordinaryLabels.indexOf("Close other tabs") >= 0)
        verify(ordinaryLabels.indexOf("Close tabs below") >= 0)
        verify(ordinaryLabels.indexOf("Duplicate tab") >= 0)
        verify(ordinaryLabels.indexOf("Keep active") === -1)

        browser.toggleActivePinned()
        const pinnedLabels = window.tabMenuActionsFor(ordinaryTabId).map(
            function(action) { return action.label })
        verify(pinnedLabels.indexOf("Keep active") >= 0)
        verify(pinnedLabels.indexOf("Close tab") === -1)
        verify(pinnedLabels.indexOf("Close other tabs") === -1)
        verify(pinnedLabels.indexOf("Close tabs below") === -1)

        // The row opens its own menu, by pointer and by keyboard, and the menu
        // runs against the row it was opened on.
        const row = findChild(window.contentItem, "pinned-" + ordinaryTabId)
        verify(row !== null)
        mouseClick(row, row.width / 2, row.height / 2, Qt.RightButton)
        tryVerify(function() { return window.tabMenuOpen })
        compare(window.tabMenuTabId, ordinaryTabId)
        window.tabMenuOpen = false

        // The keyboard reaches it through the command rather than a key of the
        // row's own: the page's context menu already owns Shift+F10.
        browser.activateTab(ordinaryTabId)
        verify(window.commands.run("tab-menu"))
        tryVerify(function() { return window.tabMenuOpen })
        compare(window.tabMenuTabId, ordinaryTabId)
        window.tabMenuOpen = false

        browser.activateTab(ordinaryTabId)
        browser.toggleActivePinned()
        browser.closeTab(ordinaryTabId)
    }

    // Order is the reader's, within one section. A drag down the ordinary list
    // moves a row past its neighbour and no further than the section's end,
    // and the keyboard does the same a step at a time.
    function test_tabsReorderByPointerAndKeyboardWithinTheirSection() {
        openPage("https://order.example/one")
        const firstTabId = browser.activeTabId
        browser.openInput("https://order.example/two", true)
        const secondTabId = browser.activeTabId
        browser.openInput("https://order.example/three", true)
        const thirdTabId = browser.activeTabId
        // A pin above them, which no ordinary row may be dragged into.
        browser.activateTab(firstTabId)
        browser.toggleActivePinned()

        // The window is shared with every other test, so the places are read
        // relative to where these two rows start rather than from the top.
        const secondPlace = browser.tabSectionIndex(secondTabId)
        compare(browser.tabSectionIndex(thirdTabId), secondPlace + 1)

        // The pointer asks the core for the same step the keyboard does.
        verify(browser.moveTab(secondTabId, secondPlace + 1))
        compare(browser.tabSectionIndex(secondTabId), secondPlace + 1)
        compare(browser.tabSectionIndex(thirdTabId), secondPlace)

        // The keyboard steps it back, and stops at the section's edge rather
        // than carrying it into the pins.
        browser.activateTab(secondTabId)
        verify(window.commands.run("move-tab-up"))
        compare(browser.tabSectionIndex(secondTabId), secondPlace)
        for (let step = 0; step < secondPlace + 2; ++step)
            window.commands.run("move-tab-up")
        compare(browser.tabSectionIndex(secondTabId), 0)
        verify(browser.tabPinned(firstTabId))

        browser.activateTab(firstTabId)
        browser.toggleActivePinned()
        browser.closeTab(thirdTabId)
        browser.closeTab(secondTabId)
        browser.closeTab(firstTabId)
    }

    // A dragged row leaves the list and follows the hand, the rows it passes
    // open the place it would drop into, and one drag can carry it the whole
    // length of its section. Nothing is reordered until it is let go.
    function test_draggingARowCarriesItToAnyPlaceInItsSection() {
        openPage("https://carried.example/one")
        const firstTabId = browser.activeTabId
        browser.openInput("https://carried.example/two", true)
        const secondTabId = browser.activeTabId
        browser.openInput("https://carried.example/three", true)
        const thirdTabId = browser.activeTabId
        const outline = findChild(window.contentItem, "sidebar")
        verify(outline !== null)

        // The list is still filling in behind the tabs just opened, and a row
        // that is about to be placed somewhere else cannot be dragged from
        // where it currently looks to be. The arrangement replaces the rows
        // showing it, so a row is asked for again after every settle.
        settleRow(findChild(window.contentItem, "tab-" + thirdTabId))
        const lastRow = findChild(window.contentItem, "tab-" + thirdTabId)
        verify(lastRow !== null)

        const place = browser.tabSectionIndex(thirdTabId)
        const rowHeight = lastRow.height
        verify(place >= 2)
        compare(browser.tabSectionIndex(secondTabId), place - 1)
        const grabY = rowHeight / 2
        // Two whole places in one gesture, which is what the step-at-a-time
        // reorder this replaced could not do.
        const travel = rowHeight * 2
        const grabbed = lastRow.mapToItem(window.contentItem, lastRow.width / 2, grabY)

        // A press alone is not a drag: a row is not lifted by the tremor in a
        // click, and nothing is asked of the list.
        mousePress(lastRow, lastRow.width / 2, grabY)
        mouseMove(window.contentItem, grabbed.x, grabbed.y + 2)
        wait(1)
        verify(!lastRow.lifted)

        // Carried up past both of its neighbours in one gesture. The row goes
        // with the hand rather than staying where the list put it, and the
        // rows it passes open the place it would drop into.
        dragRowBy(grabbed, -travel)
        verify(lastRow.lifted)
        verify(lastRow.carry.y < -rowHeight)
        compare(outline.dropDestination, place - 2)
        const passedRow = findChild(window.contentItem, "tab-" + secondTabId)
        verify(passedRow !== null)
        // The rows it passed settle into the places the arrangement would give
        // them rather than jumping, so the gap opens over a frame or two.
        tryVerify(function() { return passedRow.carry.y > 0 })
        // And nothing has actually moved yet.
        compare(browser.tabSectionIndex(thirdTabId), place)

        mouseRelease(window.contentItem, grabbed.x, grabbed.y - travel)
        // Two places up, and the rows it passed have each moved down one.
        compare(browser.tabSectionIndex(thirdTabId), place - 2)
        compare(browser.tabSectionIndex(secondTabId), place)

        // The arrangement replaces the rows that were showing it, so the row is
        // asked for again rather than remembered.
        const carried = findChild(window.contentItem, "tab-" + thirdTabId)
        verify(carried !== null)
        verify(!carried.lifted)
        compare(carried.carry.y, 0)

        // And the whole length of the section, from wherever it now sits to
        // the very first place.
        settleRow(carried)
        const regrabbed = carried.mapToItem(window.contentItem, carried.width / 2, grabY)
        // Well past the first row rather than exactly onto it: what is being
        // asked is that a drag off the top of the section lands at the top of
        // it, not that a particular pixel does.
        const toTheTop = rowHeight * (browser.tabSectionIndex(thirdTabId) + 4)
        mousePress(carried, carried.width / 2, grabY)
        dragRowBy(regrabbed, -toTheTop)
        compare(outline.dropDestination, 0)
        mouseRelease(window.contentItem, regrabbed.x, regrabbed.y - toTheTop)
        compare(browser.tabSectionIndex(thirdTabId), 0)

        browser.closeTab(thirdTabId)
        browser.closeTab(secondTabId)
        browser.closeTab(firstTabId)
    }

    // Pins are laid out across the section as well as down it, so a pin is
    // carried in both directions and the place it would take is read off where
    // the list put the other pins rather than from a row height.
    function test_draggingAPinCarriesItAcrossThePinnedSection() {
        openPage("https://pin-order.example/one")
        const firstTabId = browser.activeTabId
        browser.toggleActivePinned()
        browser.openInput("https://pin-order.example/two", true)
        const secondTabId = browser.activeTabId
        browser.toggleActivePinned()
        const outline = findChild(window.contentItem, "sidebar")

        settleRow(findChild(window.contentItem, "pinned-" + secondTabId))
        const secondPin = findChild(window.contentItem, "pinned-" + secondTabId)
        verify(secondPin !== null)
        compare(browser.tabSectionIndex(secondTabId), 1)

        const grabbed = secondPin.mapToItem(window.contentItem,
            secondPin.width / 2, secondPin.height / 2)
        mousePress(secondPin, secondPin.width / 2, secondPin.height / 2)
        const steps = 6
        for (let step = 1; step <= steps; ++step) {
            mouseMove(window.contentItem,
                grabbed.x - secondPin.width * step / steps, grabbed.y)
            wait(1)
        }
        verify(secondPin.lifted)
        // Carried sideways, which is the axis its section runs in.
        verify(secondPin.carry.x < 0)
        compare(outline.dropDestination, 0)
        mouseRelease(window.contentItem, grabbed.x - secondPin.width, grabbed.y)
        compare(browser.tabSectionIndex(secondTabId), 0)
        compare(browser.tabSectionIndex(firstTabId), 1)

        browser.activateTab(secondTabId)
        browser.toggleActivePinned()
        browser.closeTab(secondTabId)
        browser.activateTab(firstTabId)
        browser.toggleActivePinned()
        browser.closeTab(firstTabId)
    }

    // The right button opens the menu; it never carries the row.
    function test_theRightButtonNeverCarriesARow() {
        openPage("https://menu-only.example")
        const tabId = browser.activeTabId
        const row = findChild(window.contentItem, "tab-" + tabId)
        verify(row !== null)

        mousePress(row, row.width / 2, row.height / 2, Qt.RightButton)
        mouseMove(row, row.width / 2, row.height / 2 + row.height * 2)
        wait(1)
        verify(!row.lifted)
        compare(row.carry.y, 0)
        mouseRelease(row, row.width / 2, row.height / 2 + row.height * 2, Qt.RightButton)
        window.tabMenuOpen = false

        browser.closeTab(tabId)
    }

    // Duplicate opens the address again in a new ordinary tab, with its own
    // engine: no history, no form state, and no share of the page it came from.
    function test_duplicateOpensTheAddressInItsOwnNewTab() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const engineView = openPage("https://duplicated.example/page")
        const sourceTabId = browser.activeTabId
        engineView.pageLocalState = "typed into a form"

        verify(window.commands.run("duplicate-tab"))
        const duplicateTabId = browser.activeTabId
        verify(duplicateTabId !== sourceTabId)
        tryVerify(function() {
            const copy = engineLoader.engines[duplicateTabId]
            return copy !== undefined && copy !== engineView
                && String(copy.currentUrl) === "https://duplicated.example/page"
                && copy.pageLocalState === ""
        })
        verify(!browser.tabPinned(duplicateTabId))

        browser.closeTab(duplicateTabId)
        browser.closeTab(sourceTabId)
    }

    // Settings names every retained tab, which Space it belongs to, why it is
    // running and what it holds — and lets the reader stop one from there.
    function test_settingsListEveryRetainedTabAndItsCost() {
        const engineLoader = findChild(window.contentItem, "engineLoader")
        const personalSpaceId = browser.activeSpaceId
        openPage("https://listed.example")
        const keptTabId = browser.activeTabId
        browser.toggleActivePinned()
        verify(browser.setTabKeepActive(keptTabId, true))

        const workSpaceId = browser.createSpace("Listed")
        verify(browser.switchSpace(workSpaceId))
        window.settingsOpen = true
        window.refreshRetainedTabs()

        const listed = findChild(window.contentItem, "retainedTab-" + keptTabId)
        verify(listed !== null)
        verify(listed.note.indexOf("Personal") >= 0)
        verify(listed.note.indexOf("Keep active") >= 0)

        // Stopping it from the list is the same decision as the row's own, made
        // about a tab in a Space that is not on show.
        window.releaseRetainedTab(keptTabId)
        tryVerify(function() { return browser.retainedTabs.length === 0 })
        tryVerify(function() { return engineLoader.engines[keptTabId] === undefined })
        window.settingsOpen = false

        verify(browser.switchSpace(personalSpaceId))
        browser.activateTab(keptTabId)
        browser.toggleActivePinned()
        browser.closeTab(keptTabId)
        verify(browser.deleteSpace(workSpaceId, "Listed"))
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
        compare(privateBrowser.colors.overlay, theme.palette.privateOverlay)
        compare(privateBrowser.colors.mutedText, theme.palette.privateMutedText)
        compare(privateBrowser.colors.border, theme.palette.privateBorder)

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
        verify(browser.deleteSpace(workSpaceId, "History Work"))
    }

    function test_historyIsAFilteredBrowserOwnedSheet() {
        browser.recordVisit("https://history-sheet.example/first", "History sheet first")
        browser.recordVisit("https://other-sheet.example/second", "Other sheet")
        window.requestHistory()

        const surface = findChild(window.contentItem, "historySurface")
        const search = findChild(window.contentItem, "historySearch")
        const list = findChild(window.contentItem, "historyList")
        verify(surface !== null)
        verify(search !== null)
        verify(list !== null)
        tryVerify(function() { return surface.visible })
        search.text = "history-sheet.example"
        tryCompare(list, "count", 1)
        compare(String(list.model[0].url), "https://history-sheet.example/first")

        const personalSpaceId = browser.activeSpaceId
        const workSpaceId = browser.createSpace("History sheet work")
        verify(browser.switchSpace(workSpaceId))
        tryCompare(list, "count", 0)
        verify(browser.switchSpace(personalSpaceId))
        tryCompare(list, "count", 1)
        verify(browser.deleteSpace(workSpaceId, "History sheet work"))

        findChild(window.contentItem, "closeHistoryButton").clicked()
        tryVerify(function() { return !surface.visible })
    }

    function test_settingsAboutNamesTheVersion() {
        window.requestSettings()
        const aboutSection = findChild(window.contentItem, "settingsSection7")
        verify(aboutSection !== null)
        compare(aboutSection.text.toLowerCase(), "about")

        aboutSection.Accessible.pressAction()
        const name = findChild(window.contentItem, "aboutName")
        const version = findChild(window.contentItem, "aboutVersion")
        const links = findChild(window.contentItem, "aboutLinks")
        verify(name !== null)
        verify(version !== null)
        verify(links !== null)
        compare(name.text, "Omaweb")

        // The build carries a version, and the page shows that one rather than
        // a hardcoded string that would rot at the next release.
        verify(Qt.application.version.length > 0)
        compare(version.text, "Version " + Qt.application.version)
        verify(links.text.indexOf("omaweb.app") >= 0)

        // Test functions share one window and run in name order, so hand the
        // page back on the section the later settings tests open it expecting.
        findChild(window.contentItem, "settingsSection0").Accessible.pressAction()
        findChild(window.contentItem, "closeSettingsButton").clicked()
    }

    function test_settingsOwnSearchAndBrowsingDataControls() {
        window.requestSettings()
        const searchSection = findChild(window.contentItem, "settingsSection5")
        const dataSection = findChild(window.contentItem, "settingsSection6")
        verify(searchSection !== null)
        verify(dataSection !== null)
        compare(searchSection.text.toLowerCase(), "search")
        compare(dataSection.text.toLowerCase(), "privacy")

        searchSection.Accessible.pressAction()
        const engines = findChild(window.contentItem, "searchEngineList")
        verify(engines !== null)
        compare(engines.count, 1)
        verify(String(engines.model[0].name).indexOf("DuckDuckGo") >= 0)
        const providerPicker = findChild(window.contentItem, "searchProviderPreset")
        const addProvider = findChild(window.contentItem, "addSearchProviderButton")
        verify(providerPicker !== null)
        verify(addProvider !== null)
        verify(providerPicker.count >= 5)

        dataSection.Accessible.pressAction()
        verify(findChild(window.contentItem, "clearCookies") !== null)
        verify(findChild(window.contentItem, "clearStorage") !== null)
        verify(findChild(window.contentItem, "clearCache") !== null)
        verify(findChild(window.contentItem, "clearPermissions") !== null)
        verify(findChild(window.contentItem, "clearHistory") !== null)
        const everySpace = findChild(window.contentItem, "clearEverySpace")
        const confirmation = findChild(window.contentItem, "clearEverySpaceConfirmation")
        verify(everySpace !== null)
        verify(confirmation !== null)
        compare(confirmation.visible, false)
        everySpace.clicked()
        compare(confirmation.visible, true)

        findChild(window.contentItem, "closeSettingsButton").clicked()
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

    // The rail is as wide as the longest section name it draws, measured in the
    // bold face the current section takes. A pixel count was right at one theme
    // font size and clipped "Content Blocking" at the next, and a rail that
    // widened when the selection moved would shift the pane beside it.
    function test_settingsRailIsAsWideAsTheNamesItDraws() {
        const settingsSurface = findChild(window.contentItem, "settingsSurface")
        verify(settingsSurface !== null)
        window.requestSettings()
        tryVerify(function() { return settingsSurface.visible })

        let longest = 0
        for (let index = 0; index < settingsSurface.sections.length; ++index) {
            const entry = findChild(window.contentItem, "settingsSection" + index)
            verify(entry !== null)
            verify(entry.implicitWidth <= settingsSurface.railWidth)
            if (entry.implicitWidth > longest) {
                longest = entry.implicitWidth
                settingsSurface.section = index
            }
        }
        verify(longest > 0)

        // The longest name is now the current one, so it is drawn bold. The
        // rail was measured in that face, so it still fits — in a proportional
        // family bold is wider than the regular the loop above measured, and in
        // a monospace one it is the same. Either way the pane does not shift.
        const current = findChild(window.contentItem,
            "settingsSection" + settingsSurface.section)
        verify(current.font.bold)
        verify(current.implicitWidth >= longest)
        verify(current.implicitWidth <= settingsSurface.railWidth)

        settingsSurface.section = 0
        window.settingsOpen = false
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

    // A pin is a square with no title, so Omaweb paints it in the site's own
    // colour — which is artwork colour, and therefore the tint setting's to
    // give. Switched off, the pin is chrome: no wash, no site-coloured mark.
    function test_pinnedSiteColourFollowsTheTintSetting() {
        const tintFavicons = findChild(window.contentItem, "tintFavicons")
        verify(tintFavicons !== null)
        compare(window.tintFavicons, true)

        browser.toggleActivePinned()
        const pinnedRow = findChild(window.contentItem, "pinned-" + browser.activeTabId)
        verify(pinnedRow !== null)
        tryVerify(function() { return pinnedRow.visible })
        compare(pinnedRow.siteColored, true)

        tintFavicons.clicked()
        compare(window.tintFavicons, false)
        tryCompare(pinnedRow, "tintFavicons", false)
        compare(pinnedRow.siteColored, false)
        const tile = findChild(window.contentItem, "siteTile-" + browser.activeTabId)
        verify(tile !== null)
        compare(tile.siteColoredMark, false)

        tintFavicons.clicked()
        compare(window.tintFavicons, true)
        tryCompare(pinnedRow, "siteColored", true)
        browser.toggleActivePinned()
    }

    // ---- Everyday page commands ----------------------------------------

    function openPageInNewTab(url) {
        const engineHost = findChild(window.contentItem, "engineLoader")
        browser.openInput(url, true)
        tryVerify(function() {
            return engineHost.item !== null
                && engineHost.item.currentUrl.toString() === url
        })
        return engineHost.item
    }

    function commandEnabled(command) {
        const actions = window.commands.actions()
        for (let index = 0; index < actions.length; ++index) {
            if (actions[index].command === command) return actions[index].enabled
        }
        return undefined
    }

    // Find belongs to one tab: hiding it keeps the query and the match the tab
    // had reached, another tab has a search of its own, and a navigation takes
    // the matches without taking the query.
    function test_findBelongsToOneTabAndKeepsItsQueryWhileHidden() {
        const first = openPage("https://find-one.example/page")
        first.pageText = "alpha beta alpha gamma alpha"
        const firstTabId = browser.activeTabId
        const bar = findChild(window.contentItem, "findBar")
        verify(bar !== null)
        verify(!bar.open)

        window.commands.run("find", -1)
        tryVerify(function() { return bar.open })
        const input = findChild(bar, "findInput")
        verify(input !== null)
        // Asking to find puts the keyboard in the field: a bar that opens and
        // leaves the reader typing into the page has not answered the command.
        tryVerify(function() { return input.activeFocus })
        // One row, and everything in it has room: the field, the tally, the
        // two match steps and the close.
        const closeButton = findChild(bar, "findCloseButton")
        verify(closeButton !== null)
        verify(input.width > 100)
        verify(input.height <= bar.height)
        verify(closeButton.x + closeButton.width <= bar.width)
        verify(input.x + input.width <= closeButton.x)

        input.text = "alpha"
        tryCompare(bar, "matchCount", 3)
        compare(bar.activeMatch, 1)
        compare(first.findQuery, "alpha")

        window.commands.run("find-next", -1)
        compare(bar.activeMatch, 2)
        window.commands.run("find-previous", -1)
        compare(bar.activeMatch, 1)

        // Hidden, not forgotten — and the keyboard goes back to the page.
        window.closeFind()
        compare(bar.open, false)
        tryVerify(function() { return first.pageHasFocus })
        compare(first.findQuery, "alpha")
        compare(first.findActiveMatch, 1)

        window.commands.run("find", -1)
        tryVerify(function() { return bar.open })
        compare(bar.text, "alpha")

        // The tab beside it is searching for nothing, and says so by not
        // offering the bar at all.
        const second = openPageInNewTab("https://find-two.example/page")
        const secondTabId = browser.activeTabId
        compare(bar.open, false)
        compare(second.findQuery, "")

        // A search of its own, which goes away with the tab rather than
        // outliving it in the window's map.
        window.commands.run("find", -1)
        tryVerify(function() { return bar.open })
        verify(window.tabsShowingFind[secondTabId] === true)

        browser.activateTab(firstTabId)
        tryVerify(function() { return bar.open })
        compare(bar.text, "alpha")

        // A navigation invalidates where the search had reached. What the
        // reader was looking for is still theirs.
        browser.openInput("https://find-one.example/other", false)
        tryCompare(first, "findMatchCount", 0)
        compare(first.findQuery, "alpha")

        browser.closeTab(secondTabId)
        window.refreshFindOpen()
        compare(window.tabsShowingFind[secondTabId], undefined)

        window.closeFind()
    }

    // Zoom belongs to one tab, reaches that tab's engine, and leaves every
    // other tab at the size it was.
    function test_zoomBelongsToOneTabAndReachesItsEngine() {
        const first = openPage("https://zoom-one.example")
        const firstTabId = browser.activeTabId
        compare(browser.activeTabZoom, 1.0)
        compare(first.zoomFactor, 1.0)

        window.commands.run("zoom-in", -1)
        compare(browser.activeTabZoom, 1.1)
        tryCompare(first, "zoomFactor", 1.1)

        const notice = findChild(window.contentItem, "pageNotice")
        verify(notice !== null)
        compare(notice.message, "Page zoom 110%")

        const second = openPageInNewTab("https://zoom-two.example")
        const secondTabId = browser.activeTabId
        compare(browser.activeTabZoom, 1.0)
        compare(second.zoomFactor, 1.0)

        browser.activateTab(firstTabId)
        compare(browser.activeTabZoom, 1.1)
        window.commands.run("zoom-out", -1)
        compare(browser.activeTabZoom, 1.0)
        window.commands.run("zoom-in", -1)
        window.commands.run("zoom-reset", -1)
        compare(browser.activeTabZoom, 1.0)
        tryCompare(first, "zoomFactor", 1.0)

        browser.closeTab(secondTabId)
    }

    // Three asks that look alike from outside are three operations inside:
    // read the page again, read it again from the network, stop reading it.
    function test_reloadStopAndBypassingCacheAreSeparateOperations() {
        const engine = openPage("https://reload.example/page")
        const bypassedBefore = engine.bypassedCacheCount
        const stoppedBefore = engine.stoppedLoadCount

        window.commands.run("reload", -1)
        compare(engine.bypassedCacheCount, bypassedBefore)
        compare(engine.stoppedLoadCount, stoppedBefore)
        verify(engine.loading)

        window.commands.run("reload-bypassing-cache", -1)
        compare(engine.bypassedCacheCount, bypassedBefore + 1)

        window.commands.run("stop-loading", -1)
        compare(engine.stoppedLoadCount, stoppedBefore + 1)
        compare(engine.loading, false)

        // And the page is still the page: stopping a load clears nothing.
        compare(engine.currentUrl.toString(), "https://reload.example/page")
    }

    // A site holding the screen is not the reader holding it. The notice names
    // the origin, Escape hands the screen back, and the reader's own fullscreen
    // is untouched throughout.
    function test_siteFullscreenIsDistinctFromBrowserFullscreenAndLeavesWithEscape() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        const engine = openPage("https://cinema.example/watch")
        const notice = findChild(window.contentItem, "pageNotice")
        compare(window.browserFullscreen, false)
        compare(window.sidebarCollapsed, false)

        const floatingControls = findChild(window.contentItem, "navigationCluster")
        verify(floatingControls !== null)

        engine.simulateSiteFullscreen("cinema.example")
        tryVerify(function() { return engineHost.siteFullscreenActive })
        compare(window.browserFullscreen, false)
        compare(window.sidebarCollapsed, true)
        verify(notice.message.indexOf("cinema.example") !== -1)
        // The page has the whole window: standing the outline aside must not
        // put the floating strip over the page in its place.
        verify(!floatingControls.visible)

        keyClick(Qt.Key_Escape)
        tryVerify(function() { return !engineHost.siteFullscreenActive })
        compare(window.sidebarCollapsed, false)
        compare(window.browserFullscreen, false)

        window.commands.run("fullscreen", -1)
        compare(window.browserFullscreen, true)
        compare(engineHost.siteFullscreenActive, false)
        window.commands.run("fullscreen", -1)
        compare(window.browserFullscreen, false)
    }

    // The window is the desktop's to move too — a menu command or a keyboard
    // shortcut of the platform's own takes it in and out of fullscreen without
    // asking Omaweb. What Omaweb believes has to follow the window, or the next
    // fullscreen command toggles the wrong way and appears to do nothing.
    function test_fullscreenFollowsTheWindowWhateverMovedIt() {
        const engineHost = findChild(window.contentItem, "engineLoader")
        const engine = openPage("https://desktop.example/page")
        compare(window.browserFullscreen, false)

        window.visibility = Window.FullScreen
        tryCompare(window, "browserFullscreen", true)
        // A window filling the screen has no corners to round.
        compare(window.cornerRadius, 0)

        window.visibility = Window.Windowed
        tryCompare(window, "browserFullscreen", false)
        compare(window.cornerRadius, window.shellCornerRadius)

        // And the next command still works from there.
        window.commands.run("fullscreen", -1)
        compare(window.browserFullscreen, true)
        window.commands.run("fullscreen", -1)
        compare(window.browserFullscreen, false)

        // A site holding the screen is told when the screen is taken back by a
        // route it knows nothing about, and the outline comes back with it.
        engine.simulateSiteFullscreen("desktop.example")
        tryVerify(function() { return engineHost.siteFullscreenActive })
        compare(window.sidebarCollapsed, true)

        window.visibility = Window.Windowed
        tryVerify(function() { return !engineHost.siteFullscreenActive })
        compare(window.sidebarCollapsed, false)
        compare(window.browserFullscreen, false)
    }

    // A command this engine cannot carry out is listed, unavailable, and says
    // so when it is run. Doing nothing at all would leave the reader to guess
    // whether the key reached the browser.
    function test_everyPageOperationReportsAnEngineThatCannotDoIt() {
        const engine = openPage("https://limited.example/page")
        const notice = findChild(window.contentItem, "pageNotice")
        const bar = findChild(window.contentItem, "findBar")

        compare(testCase.commandEnabled("find"), true)
        compare(testCase.commandEnabled("zoom-in"), true)

        engine.findAvailable = false
        engine.zoomAvailable = false
        compare(window.findAvailable, false)
        compare(window.zoomAvailable, false)
        compare(testCase.commandEnabled("find"), false)
        compare(testCase.commandEnabled("find-next"), false)
        compare(testCase.commandEnabled("zoom-out"), false)

        window.commands.run("find", -1)
        compare(bar.open, false)
        tryCompare(notice, "message", "Find is not available")

        window.commands.run("zoom-in", -1)
        compare(browser.activeTabZoom, 1.0)
        tryCompare(notice, "message", "Zoom is not available")

        // Printing needs an engine that can render the page and a desktop with
        // a dialog to answer. The test session has no dialog, so the command is
        // unavailable and names that half.
        compare(window.printingAvailable, false)
        compare(testCase.commandEnabled("print"), false)
        window.commands.run("print", -1)
        tryCompare(notice, "message", "Print is not available")
        compare(notice.detail, "This desktop has no print dialog to answer")

        engine.findAvailable = true
        engine.zoomAvailable = true

        // A tab with no page at all is not an engine that lacks something, and
        // the notice does not say it is.
        browser.openInput("about:blank", true)
        const blankTabId = browser.activeTabId
        tryVerify(function() { return window.pagelessViewport })
        compare(testCase.commandEnabled("stop-loading"), false)
        window.commands.run("stop-loading", -1)
        tryCompare(notice, "message", "Stop loading is not available")
        compare(notice.detail, "There is no page here")

        browser.closeTab(blankTabId)
        notice.dismiss()
    }

    // A render that produced nothing is a failure the reader hears about,
    // rather than a print that quietly never happened.
    function test_printReportsARenderThatProducedNothing() {
        const engine = openPage("https://print.example/invoice")
        const notice = findChild(window.contentItem, "pageNotice")
        engine.printPage("")
        tryCompare(notice, "message", "Printing failed")
        notice.dismiss()
    }

    // Where the engine has no sandboxed PDF viewer the document is downloaded
    // instead, and the missing capability is reported rather than left to be
    // inferred from a page that never appeared.
    function test_pdfWithoutASandboxedViewerIsDownloadedAndReported() {
        const engine = openPage("https://docs.example/start")
        const notice = findChild(window.contentItem, "pageNotice")
        compare(window.inlinePdfViewingAvailable, true)

        browser.openInput("https://docs.example/inline.pdf", false)
        tryVerify(function() {
            return String(browser.activeUrl) === "https://docs.example/inline.pdf"
        })
        compare(notice.showing, false)

        engine.inlinePdfViewingAvailable = false
        compare(window.inlinePdfViewingAvailable, false)
        browser.openInput("https://docs.example/manual.pdf", false)
        tryCompare(notice, "message", "This engine cannot show PDFs")
        compare(notice.detail, "The document was downloaded instead")

        engine.inlinePdfViewingAvailable = true
        notice.dismiss()
    }
}
