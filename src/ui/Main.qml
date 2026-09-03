import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs as Dialogs
import Omaweb

ApplicationWindow {
    id: window
    objectName: privateWindow ? "privateBrowserWindow" : "mainBrowserWindow"

    width: 1360
    height: 860
    minimumWidth: 840
    minimumHeight: 560
    color: "transparent"
    flags: Qt.platform.os === "osx"
        ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint
        : Qt.Window | Qt.FramelessWindowHint
    topPadding: 0
    visible: true
    title: window.privateWindow ? "Private — Omaweb" : window.windowBrowser.activeTitle + " — Omaweb"

    property var windowBrowser: browser
    // The native backdrop reads this to mask its blur to the same rounded rect,
    // so the shell and the platform chrome cannot drift apart. A window filling
    // the screen has no corners to round, and rounding them there would notch
    // the desktop through at all four.
    readonly property real shellCornerRadius: 14
    property real cornerRadius: window.visibility === Window.FullScreen
        ? 0 : window.shellCornerRadius
    property bool privateWindow: false
    property string profilePathOverride: ""
    property var sharedEngineProfile: null
    property var colors: privateWindow ? privatePalette(theme.palette) : theme.palette
    property bool sidebarCollapsed: false
    property bool useFavicons: true
    property bool tintFavicons: true
    // The reader owns the sidebar's width. It is clamped rather than free: too
    // narrow and a tab row stops being readable, too wide and the page it is
    // an outline of loses the window.
    readonly property real sidebarMinimumWidth: 220
    readonly property real sidebarMaximumWidth:
        Math.max(sidebarMinimumWidth, Math.min(560, window.width * 0.5))
    readonly property real sidebarDefaultWidth: 292
    property real sidebarWidth: sidebarDefaultWidth
    // Developer tools are docked to the right of the page they inspect, and the
    // reader owns that seam as they own the sidebar's. The floor is what the
    // inspector's own toolbar needs before its panels start collapsing.
    readonly property real developerToolsMinimumWidth: 320
    readonly property real developerToolsMaximumWidth:
        Math.max(developerToolsMinimumWidth, Math.min(920, window.width * 0.6))
    readonly property real developerToolsDefaultWidth: 480
    property real developerToolsWidth: developerToolsDefaultWidth
    // The dock stands only for the tab on show, and only once the engine has
    // handed its inspector over. A Space being put away takes its pages off the
    // screen before the next Space's arrive, and the inspector goes with them
    // rather than hanging over an empty viewport.
    readonly property bool developerToolsOpen: window.windowBrowser.activeTabInspected
        && engineLoader.item !== null
        && engineLoader.developerToolsView !== null
    // An engine without an inspector leaves the command unavailable rather than
    // offering a dock nothing can fill.
    readonly property bool developerToolsAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities
            & engineLoader.item.developerToolsCapability) !== 0
    // The everyday page operations an engine may or may not have. Each is read
    // off the adapter itself, so a command Omaweb cannot carry out here is listed
    // and unavailable rather than doing nothing when it is run.
    readonly property bool findAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities & engineLoader.item.pageFindCapability) !== 0
    readonly property bool zoomAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities & engineLoader.item.pageZoomCapability) !== 0
    // Two halves have to hold: an engine that can render the page for printing,
    // and a desktop with a print dialog to answer.
    readonly property bool printingAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities & engineLoader.item.printingCapability) !== 0
        && PagePrinter.available
    // A PDF the engine draws in its own sandbox, with find, zoom, print and
    // download inside it. An engine without one downloads the document, and
    // says so.
    readonly property bool inlinePdfViewingAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities
            & engineLoader.item.inlinePdfViewingCapability) !== 0
    // The two halves of a site's security contract an engine can be missing,
    // and whether it keeps a Space's site data on disk at all. Site information
    // says which of its lines the engine cannot answer for rather than drawing
    // a reassuring blank.
    readonly property bool certificateDecisionsAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities
            & engineLoader.item.certificateDecisionsCapability) !== 0
    readonly property bool thirdPartyCookieControlAvailable: engineLoader.item !== null
        && (engineLoader.item.capabilities
            & engineLoader.item.thirdPartyCookieControlCapability) !== 0
    readonly property bool siteDataOnDisk: engineLoader.item !== null
        && (engineLoader.item.capabilities
            & engineLoader.item.persistentProfilesCapability) !== 0
    // The permission policies the core answers with, named here so nothing in
    // the interface compares against a bare number.
    readonly property int permissionRefused: 0
    readonly property int permissionAskedEachTime: 1
    readonly property int permissionRememberable: 2
    // Bumped whenever the core's record of granted certificate exceptions
    // changes, and read by the state below so that the state follows it. A
    // binding cannot see into an invokable on its own.
    property int certificateExceptionGeneration: 0
    // What the connection to the page on show is, read off the engine drawing
    // it — and then contradicted where Omaweb knows better. An engine keeps an
    // accepted certificate for as long as its profile lives and offers no way
    // back, so once the reader has waived a check the engine stops reporting
    // it and starts calling the connection secure. Omaweb's own record of the
    // waiver is what keeps the address trigger honest about it.
    readonly property string connectionState: {
        const generation = window.certificateExceptionGeneration
        const reported = engineLoader.item
            ? engineLoader.item.connectionState : "internal"
        if (reported === "internal" || generation < 0) return reported
        return window.windowBrowser.certificateExceptionInEffect(
            window.windowBrowser.activeUrl) ? "certificate-error" : reported
    }
    readonly property bool insecureContentBlocked: engineLoader.item === null
        || engineLoader.item.insecureContentBlocked

    // No page to show: the tab on show is blank and no engine is drawing it.
    // That covers a resting Space and an `about:blank` the reader navigated to
    // — and leaves out the blank tab a page opened, which has its engine
    // already and is about to be a page.
    readonly property bool pagelessViewport: window.windowBrowser.activeTabBlank
        && !engineLoader.item
    property bool omnibarOpen: false
    property bool newTabIntent: false
    // Which tabs have the find bar showing, by tab id. Find belongs to a tab,
    // so opening it on one page does not open it over the next — and a tab that
    // has been closed takes its entry with it rather than leaving the map to
    // grow for the life of the session.
    property var tabsShowingFind: ({})
    property bool findOpen: false
    // Fullscreen the reader asked for, which is not fullscreen a site asked
    // for. Keeping them apart is what lets a site hand the screen back without
    // taking the reader's own fullscreen with it.
    property bool browserFullscreen: false
    // The sidebar was showing when a site took the screen, so it comes back
    // when the screen does.
    property bool sidebarHiddenForFullscreen: false
    property string pendingMoveTabId: ""
    property string pendingMoveSpaceId: ""
    // The row the tab menu was opened on, and where it hangs. A menu is about
    // one tab, which is not always the tab on show.
    property string tabMenuTabId: ""
    property real tabMenuX: 0
    property real tabMenuY: 0
    property bool tabMenuOpen: false
    property var privateProfileHost: null
    // The window that opened this Private window, so it can be dropped from
    // that window's list once it closes. Empty in every other window.
    property var opener: null
    // A Private window is a browsing window in its own right, not a panel of
    // the window that opened it. Declaring one under another Window would give
    // it a transient parent, which a compositor reads as a dialog belonging to
    // the opener and floats out of the tiling. So a Private window is created
    // without a parent, and this list is what keeps it alive and closable.
    readonly property var privateWindows: []
    property var spaceProfileHost: null
    property var omnibarSuggestions: []
    property var visibleDownloads: []
    // What the retained-tab list is showing. Rebuilt when the retained set
    // changes and while the list is open, because a renderer's resident memory
    // moves on its own and a number that never moves is worse than none.
    property var visibleRetainedTabs: []
    property var visibleSubscriptions: []
    property int visibleBlockedRequestCount: 0
    property var pendingPermissionRequest: null
    property string pendingPermissionOrigin: ""
    property string pendingPermissionType: ""
    property var pendingPermissionResponder: null
    property var downloadRecordIds: ({})
    property bool settingsOpen: false
    property bool historyOpen: false
    property bool shortcutsOpen: false
    property bool spacesMenuOpen: false
    property real spacesMenuX: 0
    property real spacesMenuY: 0
    // What the reader pointed at on the page, and the menu Omaweb draws for it.
    property var pageContext: null
    property var pageContextEngine: null
    property bool pageMenuOpen: false
    property real pageMenuX: 0
    property real pageMenuY: 0
    property var pageMenuActions: []
    property bool permissionOpen: false
    // A certificate failure the engine is holding a load for. Refusing is the
    // default and has already happened; what is decided here is only whether
    // Omaweb will offer the reader a way past this one.
    property var pendingCertificateFailure: ({})
    property string pendingCertificateFailureId: ""
    property var pendingCertificateResponder: null
    property bool certificateQuestionOpen: false
    property var pendingBrowserPrompt: ({})
    property var pendingBrowserPromptResponder: null
    property string pendingBrowserPromptId: ""
    property string pendingBrowserPromptTabId: ""
    property bool browserPromptOpen: false
    property var browserPromptsByTab: ({})
    property var pendingFileSelection: ({})
    property var pendingFileSelectionResponder: null
    property string pendingFileSelectionId: ""
    property string pendingFileSelectionTabId: ""
    property var pendingSaveEngine: null
    property string pendingSaveAction: ""
    property string pendingSaveTabId: ""
    property int pendingSaveGeneration: -1
    // One dialog is open at a time, so one panel serves them all and the
    // question it is asking is the only thing that changes.
    property string dialogMode: ""
    property var moveTargets: []

    function privatePalette(source) {
        const palette = Object.assign({}, source)
        palette.window = source.privateWindow
        palette.windowOpaque = source.privateWindowOpaque
        palette.sidebar = source.privateSidebar
        palette.sidebarOpaque = source.privateSidebarOpaque
        palette.sheet = source.privateSheet
        palette.sheetOpaque = source.privateSheetOpaque
        palette.surface = source.privateSurface
        palette.surfaceHover = source.privateSurfaceHover
        palette.accent = source.privateAccent
        return palette
    }

    FontLoader {
        id: materialSymbols
        objectName: "materialSymbolsFont"
        source: iconFontSource
    }

    KeyMap {
        id: keymap
        configuration: keyboardNavigation
    }

    readonly property var commands: browserCommands
    readonly property var notifications: siteNotifications
    readonly property var profiles: spaceProfiles

    BrowserCommands {
        id: browserCommands
        window: window
        browser: window.windowBrowser
        keymap: keymap
    }

    Dialogs.FileDialog {
        id: openFileDialog
        objectName: "openFileDialog"
        title: "Open file"
        fileMode: Dialogs.FileDialog.OpenFile
        nameFilters: ["Web pages, text, images, and PDF (*.html *.htm *.txt *.png *.jpg *.jpeg *.gif *.webp *.svg *.pdf)"]
        onAccepted: window.openLocalFile(selectedFile)
    }

    Dialogs.FileDialog {
        id: pageFileDialog
        objectName: "pageFileDialog"
        title: "Choose file"
        fileMode: Dialogs.FileDialog.OpenFile
        onAccepted: {
            const files = []
            for (let index = 0; index < selectedFiles.length; ++index)
                files.push(String(selectedFiles[index]))
            window.respondToFileSelection(files)
        }
        onRejected: window.respondToFileSelection([])
    }

    Dialogs.FolderDialog {
        id: pageFolderDialog
        objectName: "pageFolderDialog"
        title: "Choose folder"
        onAccepted: window.respondToFileSelection([String(selectedFolder)])
        onRejected: window.respondToFileSelection([])
    }

    Dialogs.FileDialog {
        id: saveTargetDialog
        objectName: "saveTargetDialog"
        title: "Save as"
        fileMode: Dialogs.FileDialog.SaveFile
        onAccepted: window.completeTargetSave(selectedFile)
        onRejected: {
            window.pendingSaveEngine = null
            window.pendingSaveAction = ""
            window.pendingSaveTabId = ""
            window.pendingSaveGeneration = -1
        }
    }

    function openCommandPanel() {
        commandPanel.beginCommand()
        omnibarOpen = true
    }

    // Everything a row can be asked on its own. The ordinary rows and the pins
    // are offered different lists: a pin has no close and no rows below it, and
    // Keep active is a pin's setting alone.
    function tabMenuActionsFor(tabId) {
        if (tabId.length === 0) return []
        if (window.windowBrowser.tabPinned(tabId)) {
            const keptActive = window.windowBrowser.tabKeepActive(tabId)
            return [
                {"label": "Duplicate tab", "command": "duplicate-tab"},
                {"label": keptActive ? "Stop keeping active" : "Keep active",
                    "command": "keep-tab-active"},
                {"label": "Unpin tab", "command": "pin-tab"},
                {"separator": true},
                {"label": "Move to another Space", "command": "move-tab",
                    "enabled": !window.privateWindow}
            ]
        }
        return [
            {"label": "Duplicate tab", "command": "duplicate-tab"},
            {"label": "Pin tab", "command": "pin-tab", "enabled": !window.privateWindow},
            {"label": "Move to another Space", "command": "move-tab",
                "enabled": !window.privateWindow},
            {"separator": true},
            {"label": "Close other tabs", "command": "close-other-tabs"},
            {"label": "Close tabs below", "command": "close-tabs-below"},
            {"label": "Close tab", "command": "close-tab", "destructive": true}
        ]
    }

    // The command panel's way in: the menu belongs to the tab on show, and
    // hangs off that tab's row where there is a row on screen to hang it off.
    function openActiveTabMenu() {
        const row = sidebar.activeTabItem
        if (!window.sidebarCollapsed && row && row.visible) {
            row.openMenu(0, row.height)
            return
        }
        window.openTabMenu(window.windowBrowser.activeTabId,
            window.width / 2, window.height / 2)
    }

    function openTabMenu(tabId, anchorX, anchorY) {
        window.tabMenuTabId = tabId
        window.tabMenuX = anchorX
        window.tabMenuY = anchorY
        window.tabMenuOpen = true
    }

    // A menu row names the command the registry names, so the vocabulary is
    // one vocabulary. Every command here is about a named tab, and the ones the
    // registry states as being about the tab on show are given that tab first.
    function runTabMenu(index) {
        const actions = window.tabMenuActionsFor(window.tabMenuTabId)
        const action = actions[index]
        const tabId = window.tabMenuTabId
        window.tabMenuOpen = false
        if (!action || tabId.length === 0) return
        switch (action.command) {
        case "duplicate-tab": window.windowBrowser.duplicateTab(tabId); break
        case "close-other-tabs": window.windowBrowser.closeOtherTabs(tabId); break
        case "close-tabs-below": window.windowBrowser.closeTabsBelow(tabId); break
        case "close-tab": window.windowBrowser.closeTab(tabId); break
        case "keep-tab-active":
            window.windowBrowser.setTabKeepActive(tabId,
                !window.windowBrowser.tabKeepActive(tabId))
            break
        default:
            window.windowBrowser.activateTab(tabId)
            window.commands.run(action.command, -1)
        }
    }

    function refreshRetainedTabs() {
        window.visibleRetainedTabs = engineLoader.retainedTabReport()
    }

    // Stopping one from the list is the same decision as the row's own Keep
    // active, made about a tab in a Space that is not on show — so the setting
    // is written to that Space's store rather than to the tab model, which
    // holds the Space the reader is looking at.
    function releaseRetainedTab(tabId) {
        window.windowBrowser.releaseRetainedTab(tabId)
        window.refreshRetainedTabs()
    }

    function requestMoveTab() {
        if (privateWindow) return
        const spaces = window.windowBrowser.spaces
        const targets = []
        for (let row = 0; row < spaces.rowCount(); ++row) {
            const index = spaces.index(row, 0)
            const spaceId = spaces.data(index, Qt.UserRole + 1)
            if (spaceId === window.windowBrowser.activeSpaceId) continue
            targets.push({"id": spaceId, "label": spaces.data(index, Qt.UserRole + 2)})
        }
        window.moveTargets = targets
        window.dialogMode = targets.length > 0 ? "move" : ""
    }

    function requestNewSpace() {
        if (!privateWindow) window.dialogMode = "new"
    }

    function requestSettings() {
        window.historyOpen = false
        window.settingsOpen = true
    }

    function requestHistory() {
        if (window.privateWindow) return
        window.settingsOpen = false
        window.shortcutsOpen = false
        window.historyOpen = true
    }

    // The sheet a resting Space shows is the same sheet, so asking for it while
    // it is already standing in for the page has nothing to add and nothing to
    // toggle off.
    function requestShortcuts() {
        if (window.pagelessViewport) return
        window.shortcutsOpen = !window.shortcutsOpen
    }

    // The two halves of the shell, each one key away from the other: the
    // outline of what is open, and the page itself.
    function focusSidebar() {
        window.sidebarCollapsed = false
        sidebar.focusOutline()
    }

    function focusPage() {
        engineLoader.focusPage()
    }

    function openPageContextMenu() {
        engineLoader.requestPageContextMenu()
    }

    // The address of the page on show, and nothing else with it. A blank tab
    // has no address worth putting on the clipboard, and clearing what the
    // reader had there is not what asking to copy it means.
    // The menu is built from what was under the pointer: a link offers what you
    // do with a link, a selection offers copying, and what the page can always
    // do comes last. Every row carries the command it runs, so the list and the
    // doing cannot drift apart.
    function pageMenuFor(context) {
        const rows = []
        const link = context.linkUrl ? String(context.linkUrl) : ""
        const media = context.mediaUrl ? String(context.mediaUrl) : ""
        const selection = String(context.selectedText || "")
        if (link.length > 0) {
            rows.push({"label": "Open link in new tab", "run": "open-link"})
            rows.push({"label": "Open link in background", "run": "open-link-background"})
            rows.push({"label": "Copy link address", "run": "copy-link"})
            rows.push({"label": "Save link as", "run": "save-link"})
            rows.push({"separator": true})
        }
        if (media.length > 0) {
            rows.push({"label": "Open " + context.mediaType + " in new tab", "run": "open-media"})
            rows.push({"label": "Copy " + context.mediaType + " address", "run": "copy-media"})
            if (context.mediaType === "image")
                rows.push({"label": "Copy image", "run": "copy-image"})
            rows.push({"label": "Save " + context.mediaType + " as", "run": "save-media"})
            rows.push({"separator": true})
        }
        if (selection.length > 0) {
            rows.push({"label": "Copy", "run": "copy-selection"})
            rows.push({"separator": true})
        }
        rows.push({"label": "Back", "command": "back",
            "enabled": engineLoader.item ? engineLoader.item.canGoBack : false})
        rows.push({"label": "Forward", "command": "forward",
            "enabled": engineLoader.item ? engineLoader.item.canGoForward : false})
        rows.push({"label": "Reload", "command": "reload"})
        if (String(window.windowBrowser.activeUrl).startsWith("https://")) {
            rows.push({"label": "Retry over insecure HTTP", "run": "retry-insecure"})
        }
        rows.push({"separator": true})
        rows.push({"label": "Copy address", "command": "copy-address",
            "enabled": !window.windowBrowser.activeTabBlank})
        rows.push({"separator": true})
        rows.push({"label": "Inspect element", "command": "inspect-element",
            "enabled": window.developerToolsAvailable})
        return rows
    }

    function openPageMenu(engine, context) {
        // The engine reports the point in its own coordinates; the menu lives
        // in the window, so the point has to travel with it.
        const point = engineLoader.mapToItem(shell, context.x, context.y)
        window.pageContext = context
        window.pageContextEngine = engine
        window.pageMenuActions = window.pageMenuFor(context)
        window.pageMenuX = point.x
        window.pageMenuY = point.y
        window.pageMenuOpen = true
    }

    function runPageMenu(index) {
        const action = window.pageMenuActions[index]
        const context = window.pageContext
        const engine = window.pageContextEngine
        window.pageMenuOpen = false
        if (!action || !context || engine !== engineLoader.item
            || Number(context.pageGeneration) !== Number(engine.pageGeneration)) return
        if (action.command) {
            browserCommands.run(action.command, -1)
            return
        }
        switch (action.run) {
        case "open-link":
            window.windowBrowser.openInput(String(context.linkUrl), true); break
        case "open-link-background":
            window.windowBrowser.openInputInBackground(context.linkUrl); break
        case "copy-link": SystemClipboard.copyText(String(context.linkUrl)); break
        case "open-media":
            window.windowBrowser.openInput(String(context.mediaUrl), true); break
        case "copy-media": SystemClipboard.copyText(String(context.mediaUrl)); break
        case "copy-selection": SystemClipboard.copyText(String(context.selectedText)); break
        case "copy-image": engine.performPageContextAction("copy-image", ""); break
        case "save-link": window.requestTargetSave(engine, "save-link", context.linkUrl); break
        case "save-media": window.requestTargetSave(engine, "save-media", context.mediaUrl); break
        case "retry-insecure": window.windowBrowser.retryActiveUrlInsecurely(); break
        }
    }

    function copyAddress() {
        if (window.windowBrowser.activeTabBlank) return
        SystemClipboard.copyText(window.windowBrowser.activeUrl.toString())
    }

    // A statement about the page, over the page, that takes itself away. What
    // Omaweb has just done, and what it could not do.
    function showNotice(glyph, message, detail, duration) {
        pageNotice.show(glyph, message, detail, duration)
    }

    // A command that cannot run here says so. Doing nothing at all would leave
    // the reader to guess whether the key reached the browser. A tab with no
    // page is not an engine that lacks something, and does not say it is.
    function reportUnavailable(what) {
        window.showNotice("block", what + " is not available",
            engineLoader.item
                ? "This engine does not offer it"
                : "There is no page here")
    }

    // Find belongs to one tab. The bar's openness is per tab, and the query and
    // the match position are the tab's engine's, so coming back to a tab finds
    // the search exactly where it was left.
    function refreshFindOpen() {
        const tabs = window.windowBrowser.tabs
        const showing = ({})
        for (let row = 0; row < tabs.rowCount(); ++row) {
            const tabId = tabs.data(tabs.index(row, 0), Qt.UserRole + 1)
            if (window.tabsShowingFind[tabId] === true) showing[tabId] = true
        }
        window.tabsShowingFind = showing
        window.findOpen = showing[window.windowBrowser.activeTabId] === true
    }

    function openFind() {
        if (!window.findAvailable) {
            window.reportUnavailable("Find")
            return
        }
        window.tabsShowingFind[window.windowBrowser.activeTabId] = true
        window.refreshFindOpen()
        Qt.callLater(findBar.focusField)
    }

    function closeFind() {
        delete window.tabsShowingFind[window.windowBrowser.activeTabId]
        window.refreshFindOpen()
        window.focusPage()
    }

    function stepFind(forward) {
        if (!window.findAvailable) {
            window.reportUnavailable("Find")
            return
        }
        if (!window.findOpen || findBar.text.length === 0) {
            window.openFind()
            return
        }
        engineLoader.findText(findBar.text, forward)
    }

    function stepZoom(direction) {
        if (!window.zoomAvailable) {
            window.reportUnavailable("Zoom")
            return
        }
        window.windowBrowser.stepActiveZoom(direction)
        window.showZoomNotice()
    }

    function resetZoom() {
        if (!window.zoomAvailable) {
            window.reportUnavailable("Zoom")
            return
        }
        window.windowBrowser.resetActiveZoom()
        window.showZoomNotice()
    }

    function showZoomNotice() {
        window.showNotice("zoom_in", "Page zoom "
            + Math.round(window.windowBrowser.activeTabZoom * 100) + "%",
            "this tab only")
    }

    // A PDF is drawn inside the engine's own sandbox where there is one, and
    // downloaded where there is not. Either way the reader is told which
    // happened rather than left wondering where the document went.
    property string reportedPdfAddress: ""

    function reportPdfHandling(url) {
        if (!engineLoader.item) return
        const address = String(url).split("?")[0].split("#")[0]
        if (!address.toLowerCase().endsWith(".pdf")
            || address === window.reportedPdfAddress) {
            return
        }
        window.reportedPdfAddress = address
        if (window.inlinePdfViewingAvailable) return
        window.showNotice("download", "This engine cannot show PDFs",
            "The document was downloaded instead", 4200)
    }

    function reloadBypassingCache() {
        if (!engineLoader.item) {
            window.reportUnavailable("Reload bypassing cache")
            return
        }
        window.windowBrowser.requestReloadBypassingCache()
    }

    function stopLoading() {
        if (!engineLoader.item) {
            window.reportUnavailable("Stop loading")
            return
        }
        window.windowBrowser.requestStopLoading()
    }

    // The engine renders the page into a file; the desktop's own print dialog,
    // with its PDF destination, is what the reader answers.
    function printPage() {
        if (!window.printingAvailable) {
            // Two halves, and the reader is told which one is missing.
            window.showNotice("block", "Print is not available",
                PagePrinter.available
                    ? "This engine cannot render a page for printing"
                    : "This desktop has no print dialog to answer")
            return
        }
        const destination = PagePrinter.reserveDestination(window.windowBrowser.activeTitle)
        if (destination.length === 0) {
            window.showNotice("print_disabled", "Printing failed",
                "Omaweb could not make a file to render the page into")
            return
        }
        engineLoader.printPage(destination)
    }

    function presentPrint(destination, succeeded) {
        if (!succeeded) {
            PagePrinter.discard(destination)
            window.showNotice("print_disabled", "Printing failed",
                "The page could not be rendered for printing")
            return
        }
        if (!PagePrinter.present(destination, window.windowBrowser.activeTitle)) {
            window.showNotice("print_disabled", "Printing failed",
                "This desktop has no print dialog to present")
        }
    }

    // The reader's own fullscreen. A site's is `siteFullscreenActive`, and the
    // two are tracked apart so handing one back never takes the other away.
    function toggleBrowserFullscreen() {
        window.browserFullscreen = !window.browserFullscreen
        window.applyFullscreen()
    }

    function applyFullscreen() {
        window.visibility = (window.browserFullscreen || engineLoader.siteFullscreenActive)
            ? Window.FullScreen : Window.Windowed
    }

    // The window is also the desktop's to move: a menu command, ⌃⌘F, or the
    // green button all take it in and out of fullscreen without asking Omaweb.
    // What Omaweb believes is read back off the window afterwards, or the next
    // fullscreen command would toggle the wrong way and appear to do nothing.
    onVisibilityChanged: window.reconcileFullscreen()

    function reconcileFullscreen() {
        const filling = window.visibility === Window.FullScreen
        if (!filling && engineLoader.siteFullscreenActive) {
            // The screen was handed back by a route the page knows nothing
            // about, and a page left believing it still has the screen draws
            // for one. Telling it is also what gives the outline back.
            window.exitSiteFullscreen()
            return
        }
        window.browserFullscreen = filling && !engineLoader.siteFullscreenActive
    }

    function exitSiteFullscreen() {
        engineLoader.exitSiteFullscreen()
    }

    function toggleDeveloperTools() {
        if (!window.developerToolsAvailable) return
        window.windowBrowser.toggleDeveloperTools()
    }

    function inspectElement() {
        if (!window.developerToolsAvailable) return
        engineLoader.inspectElement()
    }

    function setDeveloperToolsWidth(width) {
        window.developerToolsWidth = Math.round(
            Math.max(window.developerToolsMinimumWidth,
                Math.min(window.developerToolsMaximumWidth, width)))
    }

    function setSidebarWidth(width) {
        window.sidebarWidth = Math.round(Math.max(window.sidebarMinimumWidth,
            Math.min(window.sidebarMaximumWidth, width)))
    }

    // Widening is also the way back from a hidden sidebar: asking for more of
    // something that is not there means show it.
    function nudgeSidebar(step) {
        if (window.sidebarCollapsed) {
            if (step < 0) return
            window.sidebarCollapsed = false
        }
        window.setSidebarWidth(window.sidebarWidth + step)
    }

    // A window narrow enough to break the clamp pulls the sidebar back in
    // with it, so the page is never squeezed out of its own window.
    onSidebarMaximumWidthChanged: window.setSidebarWidth(window.sidebarWidth)
    onDeveloperToolsMaximumWidthChanged:
        window.setDeveloperToolsWidth(window.developerToolsWidth)

    // A width the reader chose outlives the session that chose it. The clamp
    // runs on the way back in, so a saved width from a wider window or an older
    // build still lands somewhere usable.
    function restoreSidebarWidth() {
        const saved = parseFloat(window.windowBrowser.preference("sidebar-width", ""))
        if (!isNaN(saved)) window.setSidebarWidth(saved)
    }

    function restoreDeveloperToolsWidth() {
        const saved = parseFloat(
            window.windowBrowser.preference("developer-tools-width", ""))
        if (!isNaN(saved)) window.setDeveloperToolsWidth(saved)
    }

    function restoreTabAppearance() {
        window.useFavicons = window.windowBrowser.preference("use-favicons", "true") === "true"
        window.tintFavicons = window.windowBrowser.preference("tint-favicons", "true") === "true"
    }

    function setUseFavicons(enabled) {
        window.useFavicons = enabled
        window.windowBrowser.setPreference("use-favicons", enabled ? "true" : "false")
    }

    function setTintFavicons(enabled) {
        window.tintFavicons = enabled
        window.windowBrowser.setPreference("tint-favicons", enabled ? "true" : "false")
    }

    onSidebarWidthChanged: sidebarWidthWriter.restart()
    onDeveloperToolsWidthChanged: developerToolsWidthWriter.restart()

    // A drag reports every pixel it crosses. The store hears the width the
    // hand came to rest at, not the path it took to get there.
    Timer {
        id: sidebarWidthWriter
        interval: 400
        onTriggered: window.windowBrowser.setPreference("sidebar-width",
            String(window.sidebarWidth))
    }

    Timer {
        id: developerToolsWidthWriter
        interval: 400
        onTriggered: window.windowBrowser.setPreference("developer-tools-width",
            String(window.developerToolsWidth))
    }

    // 1 allow once, 2 always allow, 3 block — the decisions BrowserController
    // stores, in the order the bar offers them.
    function respondToPermission(decision) {
        window.windowBrowser.setPermissionDecision(
            window.pendingPermissionOrigin, window.pendingPermissionType, decision)
        if (window.pendingPermissionResponder) {
            window.pendingPermissionResponder.respondToPermission(
                window.pendingPermissionRequest, decision)
        }
        window.permissionOpen = false
        window.pendingPermissionResponder = null
    }

    // The engine is blocking the load and waiting. An exception is offered only
    // for what the core's rule allows: an overridable, non-fatal failure in the
    // main frame of a Local-development site. Everything else is refused here,
    // and nothing about an answer is written down.
    function showCertificateError(engine, requestId, failure, inFront) {
        // A question about a page nobody is looking at is a question the reader
        // cannot answer, so it is refused: a retained tab in another Space
        // reaches no bar. An Auxiliary window is in front of them by
        // definition, and says so.
        const visible = inFront === true || engine === engineLoader.item
        const offerable = visible
            && window.windowBrowser.mayOfferCertificateException(failure.url,
                failure.overridable === true, failure.mainFrame === true,
                failure.fatal === true)
        if (!offerable) {
            engine.respondToCertificateError(requestId, false)
            return
        }
        window.pendingCertificateFailure = failure
        window.pendingCertificateFailureId = requestId
        window.pendingCertificateResponder = engine
        window.certificateQuestionOpen = true
    }

    function respondToCertificateError(accepted) {
        if (window.pendingCertificateResponder) {
            window.pendingCertificateResponder.respondToCertificateError(
                window.pendingCertificateFailureId, accepted)
        }
        // The engine will now keep the accepted certificate for as long as its
        // profile lives and stop reporting the failure. Recording the waiver
        // is what lets the address trigger keep saying the check was waived.
        if (accepted) {
            window.windowBrowser.recordCertificateException(
                window.pendingCertificateFailure.url)
        }
        window.certificateQuestionOpen = false
        window.pendingCertificateResponder = null
        window.pendingCertificateFailureId = ""
    }

    function showBrowserPrompt(engine, requestId, prompt) {
        if (engine !== engineLoader.item) {
            engine.respondToBrowserPrompt(requestId, false, {})
            return
        }
        const tabId = window.windowBrowser.activeTabId
        const prompts = Object.assign({}, window.browserPromptsByTab)
        prompts[tabId] = {
            "responder": engine,
            "requestId": requestId,
            "prompt": prompt,
            "generation": engine.pageGeneration
        }
        window.browserPromptsByTab = prompts
        window.presentBrowserPromptForActiveTab()
    }

    function presentBrowserPromptForActiveTab() {
        const tabId = window.windowBrowser.activeTabId
        const pending = window.browserPromptsByTab[tabId]
        if (!pending) {
            window.browserPromptOpen = false
            window.pendingBrowserPromptResponder = null
            window.pendingBrowserPromptId = ""
            window.pendingBrowserPromptTabId = ""
            window.pendingBrowserPrompt = ({})
            return
        }
        if (!pending.responder
            || Number(pending.generation) !== Number(pending.responder.pageGeneration)) {
            const prompts = Object.assign({}, window.browserPromptsByTab)
            delete prompts[tabId]
            window.browserPromptsByTab = prompts
            if (pending.responder)
                pending.responder.respondToBrowserPrompt(pending.requestId, false, {})
            window.presentBrowserPromptForActiveTab()
            return
        }
        window.pendingBrowserPromptResponder = pending.responder
        window.pendingBrowserPromptId = pending.requestId
        window.pendingBrowserPromptTabId = tabId
        window.pendingBrowserPrompt = pending.prompt
        window.browserPromptOpen = true
    }

    function respondToBrowserPrompt(accepted, text, user, password, stopPrompts, remember) {
        const responder = window.pendingBrowserPromptResponder
        const tabId = window.pendingBrowserPromptTabId
        if (responder) {
            responder.respondToBrowserPrompt(window.pendingBrowserPromptId, accepted, {
                "text": text,
                "user": user,
                "password": password,
                "stopPrompts": stopPrompts,
                "remember": remember
            })
        }
        const prompts = Object.assign({}, window.browserPromptsByTab)
        delete prompts[tabId]
        window.browserPromptsByTab = prompts
        window.presentBrowserPromptForActiveTab()
    }

    function openLocalFile(fileUrl) {
        const address = String(fileUrl)
        if (!address.startsWith("file:")) return
        window.windowBrowser.openInput(address, false)
    }

    function requestOpenFile() {
        openFileDialog.open()
    }

    function showFileSelection(engine, requestId, selection) {
        if (engine !== engineLoader.item) {
            engine.respondToFileSelection(requestId, [])
            return
        }
        window.pendingFileSelectionResponder = engine
        window.pendingFileSelectionId = requestId
        window.pendingFileSelectionTabId = window.windowBrowser.activeTabId
        window.pendingFileSelection = selection
        if (selection.mode === "folder") {
            pageFolderDialog.open()
            return
        }
        pageFileDialog.fileMode = selection.mode === "open-multiple"
            ? Dialogs.FileDialog.OpenFiles
            : (selection.mode === "save"
                ? Dialogs.FileDialog.SaveFile : Dialogs.FileDialog.OpenFile)
        pageFileDialog.open()
    }

    function respondToFileSelection(files) {
        const responder = window.pendingFileSelectionResponder
        const requestId = window.pendingFileSelectionId
        window.pendingFileSelectionResponder = null
        window.pendingFileSelectionId = ""
        window.pendingFileSelectionTabId = ""
        window.pendingFileSelection = ({})
        if (responder) responder.respondToFileSelection(requestId, files)
        if (pageFileDialog.visible) pageFileDialog.close()
        if (pageFolderDialog.visible) pageFolderDialog.close()
    }

    function cancelTabModalRequests() {
        if (window.browserPromptOpen)
            window.respondToBrowserPrompt(false, "", "", "", false, false)
        if (window.pendingFileSelectionResponder)
            window.respondToFileSelection([])
        window.pageMenuOpen = false
        window.pendingSaveEngine = null
        window.pendingSaveAction = ""
        window.pendingSaveTabId = ""
        window.pendingSaveGeneration = -1
        if (saveTargetDialog.visible) saveTargetDialog.close()
    }

    function reconcileTabModalRequests() {
        const active = window.windowBrowser.activeTabId
        window.presentBrowserPromptForActiveTab()
        if (window.pendingFileSelectionResponder
            && window.pendingFileSelectionTabId !== active)
            window.respondToFileSelection([])
        if (window.pendingSaveEngine && window.pendingSaveTabId !== active) {
            window.pendingSaveEngine = null
            window.pendingSaveAction = ""
            window.pendingSaveTabId = ""
            window.pendingSaveGeneration = -1
            if (saveTargetDialog.visible) saveTargetDialog.close()
        }
        window.pageMenuOpen = false
    }

    function requestTargetSave(engine, action, url) {
        window.pendingSaveEngine = engine
        window.pendingSaveAction = action
        window.pendingSaveTabId = window.windowBrowser.activeTabId
        window.pendingSaveGeneration = Number(engine.pageGeneration)
        const address = String(url).split("?")[0].split("#")[0]
        const slash = address.lastIndexOf("/")
        const suggested = slash >= 0 && slash + 1 < address.length
            ? address.substring(slash + 1) : "download"
        saveTargetDialog.currentFile = "file://" + window.windowBrowser.downloadDirectory
            + "/" + suggested
        saveTargetDialog.open()
    }

    function completeTargetSave(fileUrl) {
        const engine = window.pendingSaveEngine
        if (engine && engine === engineLoader.item
            && window.pendingSaveGeneration === Number(engine.pageGeneration))
            engine.performPageContextAction(window.pendingSaveAction, String(fileUrl))
        window.pendingSaveEngine = null
        window.pendingSaveAction = ""
        window.pendingSaveTabId = ""
        window.pendingSaveGeneration = -1
        if (saveTargetDialog.visible) saveTargetDialog.close()
    }

    function stepTab(delta) {
        const tabs = window.windowBrowser.tabs
        const count = tabs.rowCount()
        if (count === 0) return
        let current = 0
        for (let row = 0; row < count; ++row) {
            if (tabs.data(tabs.index(row, 0), Qt.UserRole + 6)) {
                current = row
                break
            }
        }
        const next = (current + delta + count) % count
        window.windowBrowser.activateTab(tabs.data(tabs.index(next, 0), Qt.UserRole + 1))
    }

    function activateTabAt(position) {
        const tabs = window.windowBrowser.tabs
        if (position < 0 || position >= tabs.rowCount()) return
        window.windowBrowser.activateTab(tabs.data(tabs.index(position, 0), Qt.UserRole + 1))
    }

    function stepSpace(delta) {
        if (privateWindow) return
        const spaces = window.windowBrowser.spaces
        const count = spaces.rowCount()
        if (count === 0) return
        let current = 0
        for (let row = 0; row < count; ++row) {
            if (spaces.data(spaces.index(row, 0), Qt.UserRole + 4)) {
                current = row
                break
            }
        }
        const next = (current + delta + count) % count
        window.windowBrowser.switchSpace(spaces.data(spaces.index(next, 0), Qt.UserRole + 1))
    }

    function activateSpaceAt(position) {
        if (privateWindow) return
        const spaces = window.windowBrowser.spaces
        if (position < 0 || position >= spaces.rowCount()) return
        window.windowBrowser.switchSpace(spaces.data(spaces.index(position, 0), Qt.UserRole + 1))
    }

    function openOmnibar(forNewTab) {
        newTabIntent = forNewTab
        const preset = forNewTab ? "" : window.windowBrowser.activeUrl.toString()
        omnibarSuggestions = window.privateWindow
            ? [] : window.windowBrowser.historySuggestions(preset)
        commandPanel.beginAddress(preset, forNewTab)
        omnibarOpen = true
    }

    // The profile the Space on show runs in. Which is the same table every
    // other Space's profile is in, so coming back to a Space finds the one it
    // was left with rather than a new one.
    function createSpaceProfile() {
        if (window.privateWindow) return
        const host = spaceProfiles.hostFor(window.windowBrowser.activeSpaceId)
        if (host) window.spaceProfileHost = host
    }

    // Whoever built a Space's profile — this window on the way to showing that
    // Space, or the engine host on the way to retaining a tab in one — the
    // downloads and notifications that come out of it are the window's to
    // route, so every profile passes through here once.
    function adoptSpaceProfile(spaceId, host) {
        if (!host) return
        if (!host.downloadObserversConnected) {
            host.downloadStarted.connect(window.handleDownloadStarted)
            host.downloadUpdated.connect(window.handleDownloadUpdated)
            host.downloadObserversConnected = true
        }
        siteNotifications.watch(spaceId, host)
    }

    function retireSpaceProfile(spaceId) {
        const retired = spaceProfiles.retire(spaceId)
        if (retired && window.spaceProfileHost === retired) window.spaceProfileHost = null
    }

    function handleDownloadStarted(runtimeId, sourceUrl, path, state, receivedBytes, totalBytes) {
        const recordId = window.windowBrowser.recordDownload(runtimeId, sourceUrl, path,
            state, receivedBytes, totalBytes)
        if (recordId.length > 0) window.downloadRecordIds[runtimeId] = recordId
    }

    function handleDownloadUpdated(runtimeId, state, receivedBytes, totalBytes, error) {
        const recordId = window.downloadRecordIds[runtimeId]
        if (recordId) window.windowBrowser.updateDownload(recordId, state,
            receivedBytes, totalBytes, error)
    }

    function closeOmnibar() {
        omnibarOpen = false
        newTabIntent = false
        engineLoader.focusPage()
    }

    // Every binding — chord, single key, or sequence — comes from the keyboard
    // configuration, so rebinding is editing assets/keybindings/default.json.
    // Chords are always live. Single keys follow the Keyboard navigation
    // setting, because only they can be confused with typing on a page.
    Repeater {
        model: Object.keys(keymap.browserBindings)

        Item {
            required property string modelData

            Shortcut {
                sequence: keymap.keySequence(modelData)
                enabled: keymap.isChord(modelData)
                    || (keymap.pageCommandsEnabled && !engineLoader.hintModeActive)
                context: Qt.WindowShortcut
                onActivated: browserCommands.run(
                    keymap.commandFor(modelData),
                    parseInt(modelData.slice(-1), 10) - 1)
            }
        }
    }

    // Site-requested fullscreen always exits with Escape, whatever the page
    // does with the key. It is not a keymap binding: a reader who has lost the
    // window to a page must not have to know what their keymap says.
    Shortcut {
        sequence: "Esc"
        enabled: engineLoader.siteFullscreenActive && !window.omnibarOpen
            && !window.settingsOpen && !window.historyOpen && !window.pageMenuOpen
            && !window.permissionOpen && !window.certificateQuestionOpen
            && window.dialogMode.length === 0
        context: Qt.WindowShortcut
        onActivated: window.exitSiteFullscreen()
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: window.cornerRadius
        color: window.colors.window
        // A window filling the screen has no edge to draw: the frame belongs to
        // a window sitting on a desktop, not to one that is the desktop.
        border.width: window.visibility === Window.FullScreen ? 0 : 1
        border.color: window.colors.border
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            SpaceOutline {
                id: sidebar
                objectName: "sidebar"
                Layout.fillHeight: true
                Layout.preferredWidth: window.sidebarCollapsed ? 0 : window.sidebarWidth
                visible: Layout.preferredWidth > 0
                colors: window.colors
                iconFontFamily: materialSymbols.name
                browser: window.windowBrowser
                privateWindow: window.privateWindow
                collapsed: window.sidebarCollapsed
                blockedRequestCount: window.visibleBlockedRequestCount
                connectionState: window.connectionState
                certificateDecisionsAvailable: window.certificateDecisionsAvailable
                thirdPartyCookieControlAvailable: window.thirdPartyCookieControlAvailable
                siteDataOnDisk: window.siteDataOnDisk
                insecureContentBlocked: window.insecureContentBlocked
                cookiePolicy: engineCookiePolicy
                canGoBack: engineLoader.item ? engineLoader.item.canGoBack : false
                canGoForward: engineLoader.item ? engineLoader.item.canGoForward : false
                useFavicons: window.useFavicons
                tintFavicons: window.tintFavicons
                settingsAttention: settingsSurface.needsAttention

                // A drag is already following the pointer; easing it too
                // would make the seam lag behind the hand holding it.
                Behavior on Layout.preferredWidth {
                    enabled: !sidebarResizer.dragging
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }

                onAddressRequested: window.openOmnibar(false)
                onTabActivated: function(tabId) { window.windowBrowser.activateTab(tabId) }
                onTabCloseRequested: function(tabId) { window.windowBrowser.closeTab(tabId) }
                // The speaker is the one place a row's sound can be given
                // back, so it answers for both reasons a tab is silent: the
                // reader's own muting, and an origin they have not dealt with.
                onTabMuteToggled: function(tabId) {
                    if (window.windowBrowser.tabSoundSuppressed(tabId)) {
                        window.windowBrowser.grantTabSound(tabId)
                        return
                    }
                    window.windowBrowser.toggleTabMuted(tabId)
                }
                onTabDropped: function(tabId, destination) {
                    window.windowBrowser.moveTab(tabId, destination)
                }
                onTabMenuRequested: function(tabId, anchorX, anchorY) {
                    window.openTabMenu(tabId, anchorX, anchorY)
                }
                onSpaceActivated: function(spaceId) { window.windowBrowser.switchSpace(spaceId) }
                onSpacesMenuRequested: function(anchorX, anchorY) {
                    window.spacesMenuX = anchorX
                    window.spacesMenuY = anchorY
                    window.spacesMenuOpen = true
                }
                onSettingsRequested: window.requestSettings()
                onBackRequested: window.windowBrowser.requestBack()
                onForwardRequested: window.windowBrowser.requestForward()
                onReloadRequested: window.windowBrowser.requestReload()
                onSidebarToggled: window.sidebarCollapsed = !window.sidebarCollapsed
                onCommandPanelRequested: window.openCommandPanel()
                onWindowMoveRequested: window.startSystemMove()
                onPageFocusRequested: window.focusPage()
            }

            Item {
                objectName: "engineViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true

                // The shell around it is translucent by theme; a webpage viewport
                // never is, so it gets its own opaque backing rather than
                // inheriting whatever the desktop is showing. A Space at rest
                // has no webpage to back, and the start page over it is
                // translucent like the sidebar, so the backing goes away with
                // the page rather than sealing the desktop out of an empty
                // viewport.
                Rectangle {
                    objectName: "engineBacking"
                    anchors.fill: parent
                    visible: !window.pagelessViewport
                    color: window.colors.windowOpaque
                }

                TabEngineHost {
                    id: engineLoader
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    // The page gives up the width the dock takes rather than
                    // being covered by it, so nothing the inspector points at
                    // is hidden behind the inspector.
                    anchors.right: developerToolsDock.visible
                        ? developerToolsDock.left : parent.right
                    focus: true
                    browserController: window.windowBrowser
                    engineSource: engineViewSource
                    spaceProfiles: spaceProfiles
                    profilePath: window.profilePathOverride.length > 0
                        ? window.profilePathOverride
                        : window.windowBrowser.activeProfilePath
                    sharedProfile: window.privateWindow
                        ? window.sharedEngineProfile
                        : (window.spaceProfileHost ? window.spaceProfileHost.profile : null)
                    permissionController: window.windowBrowser
                    blocker: contentBlocker
                    engineBlocker: engineContentBlocker
                    keyboardManager: keyboardNavigation
                    hintTheme: window.colors
                    developerToolsColors: window.colors
                    // Chromium's own pre-paint colour, so a navigation never
                    // flashes a bright frame through the dark shell.
                    pageBackgroundColor: window.colors.windowOpaque
                    spaceId: window.windowBrowser.activeSpaceId

                    onAuxiliaryWindowRequested: function(engine, request, requestedUrl) {
                        auxiliaryWindowComponent.createObject(window, {
                            "openerEngine": engine,
                            "request": request,
                            "requestedUrl": requestedUrl
                        })
                    }

                    onNewTabRequested: function(engine, request, requestedUrl) {
                        const destination = requestedUrl.toString().length > 0
                            ? requestedUrl.toString()
                            : "about:blank"
                        window.windowBrowser.openInput(request ? "about:blank" : destination, true)
                        // The tab the request opened is the active one, and it
                        // is named rather than left to `item`: the tab starts
                        // blank, a blank tab is given no engine, and the
                        // request has to reach that tab's engine and not
                        // whichever page happened to be showing.
                        if (request) {
                            engineLoader.adoptNewWindowRequest(
                                window.windowBrowser.activeTabId, request)
                        }
                    }

                    onBackgroundTabRequested: function(requestedUrl) {
                        window.windowBrowser.openInputInBackground(requestedUrl)
                    }

                    onPageContextRequested: function(engine, context) {
                        window.openPageMenu(engine, context)
                    }

                    onPrintFinished: function(destination, succeeded) {
                        window.presentPrint(destination, succeeded)
                    }

                    // A site taking the screen is a state the window is in, not
                    // something the engine did to it behind Omaweb's back: the
                    // window goes fullscreen, the outline stands aside, and the
                    // reader is told whose page is holding it and how to leave.
                    onSiteFullscreenActiveChanged: {
                        if (engineLoader.siteFullscreenActive) {
                            window.sidebarHiddenForFullscreen = !window.sidebarCollapsed
                            window.sidebarCollapsed = true
                            window.applyFullscreen()
                            window.showNotice("fullscreen",
                                engineLoader.siteFullscreenOrigin
                                    + " is showing this page fullscreen",
                                "press esc to leave", 4200)
                            return
                        }
                        window.applyFullscreen()
                        if (window.sidebarHiddenForFullscreen) {
                            window.sidebarCollapsed = false
                            window.sidebarHiddenForFullscreen = false
                        }
                        pageNotice.dismiss()
                    }

                    onSitePermissionRequested: function(engine, requestId, origin, permission) {
                        window.pendingPermissionRequest = requestId
                        window.pendingPermissionResponder = engine
                        window.pendingPermissionOrigin = origin
                        window.pendingPermissionType = permission
                        window.permissionOpen = true
                    }

                    onCertificateErrorRaised: function(engine, requestId, failure) {
                        window.showCertificateError(engine, requestId, failure)
                    }

                    onBrowserPromptRequested: function(engine, requestId, prompt) {
                        window.showBrowserPrompt(engine, requestId, prompt)
                    }

                    onFileSelectionRequested: function(engine, requestId, selection) {
                        window.showFileSelection(engine, requestId, selection)
                    }
                }

                DeveloperToolsDock {
                    id: developerToolsDock
                    objectName: "developerToolsDock"
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: window.developerToolsWidth
                    visible: window.developerToolsOpen && !window.settingsOpen
                        && !window.historyOpen
                    z: 4
                    colors: window.colors
                    developerToolsView: engineLoader.developerToolsView
                }

                PanelResizer {
                    id: developerToolsResizer
                    objectName: "developerToolsResizer"
                    visible: developerToolsDock.visible
                    enabled: visible
                    height: parent.height
                    x: developerToolsDock.x - width / 2
                    z: 6
                    colors: window.colors
                    measureFromRight: true
                    panelName: "Developer tools"
                    currentWidth: window.developerToolsWidth
                    minimumWidth: window.developerToolsMinimumWidth
                    maximumWidth: window.developerToolsMaximumWidth
                    defaultWidth: window.developerToolsDefaultWidth

                    onWidthRequested: function(width) {
                        window.setDeveloperToolsWidth(width)
                    }
                    onPageFocusRequested: window.focusPage()
                }

                StartPage {
                    id: startPage
                    anchors.fill: parent
                    z: 30
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    commands: browserCommands
                    keymap: keymap
                    privateWindow: window.privateWindow
                    open: (window.pagelessViewport || window.shortcutsOpen)
                        && !window.settingsOpen && !window.historyOpen
                    overPage: !window.pagelessViewport
                    // The page behind the sheet, not the viewport that owns
                    // both, so the blur never samples itself. There is nothing
                    // to sample where there is no page.
                    pageSource: window.pagelessViewport ? null : engineLoader

                    onClosed: {
                        window.shortcutsOpen = false
                        window.focusPage()
                    }
                }

                FindBar {
                    id: findBar
                    objectName: "findBar"
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.top: parent.top
                    anchors.topMargin: 16
                    z: 41
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    // The bar stands for the tab on show, and only where there
                    // is a page to search.
                    open: window.findOpen && window.findAvailable
                        && !window.settingsOpen && !window.historyOpen
                    query: engineLoader.item ? engineLoader.item.findQuery : ""
                    matchCount: engineLoader.item ? engineLoader.item.findMatchCount : 0
                    activeMatch: engineLoader.item ? engineLoader.item.findActiveMatch : 0

                    onSearchRequested: function(text, forward) {
                        engineLoader.findText(text, forward)
                    }
                    onClosed: window.closeFind()
                }

                PageNotice {
                    id: pageNotice
                    objectName: "pageNotice"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 16
                    z: 42
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                }

                PageQuestionBar {
                    id: permissionBar
                    objectName: "sitePermissionBar"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    z: 40
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    open: window.permissionOpen
                    glyph: "shield_person"
                    // What Omaweb will do with the answer is the core's rule,
                    // not the bar's: a capability whose use the reader cannot
                    // see being spent is never offered a persistent answer, and
                    // the bar says so instead of quietly dropping the button.
                    readonly property int policy: window.pendingPermissionType.length > 0
                        ? window.windowBrowser.permissionPolicy(window.pendingPermissionType)
                        : 0
                    message: window.pendingPermissionOrigin
                        + " asked for a protected browser capability"
                    detail: window.pendingPermissionType
                        + (permissionBar.policy === window.permissionRememberable
                            ? " · remembered for this Space only"
                            : " · asked every time, never remembered")
                    actions: permissionBar.policy === window.permissionRememberable
                        ? [
                            {"label": "Allow once", "decision": 1},
                            {"label": "Always allow", "decision": 2,
                                "enabled": !window.privateWindow},
                            {"label": "Block", "decision": 3}
                        ]
                        : [
                            {"label": "Allow once", "decision": 1},
                            {"label": "Block", "decision": 3}
                        ]

                    onActionTriggered: function(index) {
                        window.respondToPermission(permissionBar.actions[index].decision)
                    }
                }

                // A certificate failure the reader is being offered a way past.
                // It reaches this bar only where the core's rule allows one, so
                // the question is always about a Local-development site's own
                // main frame and always about this load alone.
                PageQuestionBar {
                    objectName: "certificateQuestionBar"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    z: 41
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    open: window.certificateQuestionOpen
                    glyph: "warning"
                    message: String(window.pendingCertificateFailure.origin || "")
                        + " could not prove its certificate"
                    detail: String(window.pendingCertificateFailure.description || "")
                        + " · local development site · this load only, never remembered"
                    actions: [
                        {"label": "Continue once"},
                        {"label": "Block"}
                    ]

                    onActionTriggered: function(index) {
                        window.respondToCertificateError(index === 0)
                    }
                }

                PagePromptBar {
                    objectName: "browserPromptBar"
                    anchors.fill: parent
                    z: 43
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    open: window.browserPromptOpen
                    prompt: window.pendingBrowserPrompt

                    onAnswered: function(accepted, text, user, password, stopPrompts, remember) {
                        window.respondToBrowserPrompt(
                            accepted, text, user, password, stopPrompts, remember)
                    }
                }

                SettingsPage {
                    id: settingsSurface
                    objectName: "settingsSurface"
                    anchors.fill: parent
                    z: 45
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    browser: window.windowBrowser
                    blocker: contentBlocker
                    keyboard: keyboardNavigation
                    open: window.settingsOpen
                    // As the sheet does: the page itself, never the viewport
                    // that owns both.
                    pageSource: window.pagelessViewport ? null : engineLoader
                    useFavicons: window.useFavicons
                    tintFavicons: window.tintFavicons
                    retainedTabs: window.visibleRetainedTabs

                    onClosed: window.settingsOpen = false
                    onRetainedTabReleased: function(tabId) {
                        window.releaseRetainedTab(tabId)
                    }
                    onUseFaviconsToggled: function(enabled) { window.setUseFavicons(enabled) }
                    onTintFaviconsToggled: function(enabled) { window.setTintFavicons(enabled) }
                }

                HistoryPage {
                    id: historySurface
                    anchors.fill: parent
                    z: 46
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    browser: window.windowBrowser
                    open: window.historyOpen
                    pageSource: window.pagelessViewport ? null : engineLoader
                    onClosed: window.historyOpen = false
                }

                // The outline carries these commands while it is open; the
                // strip is what the chromeless state has instead — except
                // where a site has been given the screen, which is the one
                // state that has no browser chrome over it at all.
                NavigationCluster {
                    visible: !window.settingsOpen && !window.historyOpen
                        && window.sidebarCollapsed
                        && !engineLoader.siteFullscreenActive
                    // Where the outline's own controls were: the strip stands
                    // in for the top of the sidebar, so hiding the sidebar
                    // leaves the commands where the reader was already
                    // reaching for them rather than moving them to the floor.
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 16
                    z: 5
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    // The page behind the strip, not the viewport that owns
                    // both, so the blur never samples itself.
                    backdropSource: engineLoader
                    canGoBack: engineLoader.item ? engineLoader.item.canGoBack : false
                    canGoForward: engineLoader.item ? engineLoader.item.canGoForward : false
                    sidebarCollapsed: window.sidebarCollapsed

                    onBackRequested: window.windowBrowser.requestBack()
                    onForwardRequested: window.windowBrowser.requestForward()
                    onReloadRequested: window.windowBrowser.requestReload()
                    onSidebarToggled: window.sidebarCollapsed = !window.sidebarCollapsed
                    onCommandPanelRequested: window.openCommandPanel()
                }

                Connections {
                    target: window.windowBrowser

                    function onTabMoveConfirmationRequested(tabId, destinationSpaceId) {
                        window.pendingMoveTabId = tabId
                        window.pendingMoveSpaceId = destinationSpaceId
                        window.dialogMode = "confirm-move"
                    }

                    // Only the Space on show keeps live pages. Putting one away
                    // takes its renderers with it, except the tabs the core
                    // names: a Pinned tab marked Keep active, and the tab an
                    // inspector is attached to. The profile stays, so coming
                    // back does not reopen the Space's cookies and cache.
                    function onSpaceSuspended(spaceId, retainedTabIds) {
                        engineLoader.suspend(spaceId, retainedTabIds)
                    }

                    function onSpaceRestored(spaceId) {
                        if (spaceId === window.windowBrowser.activeSpaceId) {
                            Qt.callLater(function() {
                                window.createSpaceProfile()
                                engineLoader.resume()
                            })
                        }
                    }

                    function onSpaceDiscarded(spaceId) {
                        engineLoader.discardEnginesForSpace(spaceId)
                        window.retireSpaceProfile(spaceId)
                    }

                    function onEngineDataClearRequested(spaceIds, dataTypes, since) {
                        engineLoader.clearBrowsingData(spaceIds, dataTypes, since)
                    }

                    function onCloseWindowRequested() {
                        if (window.privateWindow) window.close()
                    }

                    // The find bar stands for one tab, so it comes and goes
                    // with the tab it was opened on.
                    function onActiveTabChanged() {
                        window.reconcileTabModalRequests()
                        window.refreshFindOpen()
                        window.reportPdfHandling(window.windowBrowser.activeUrl)
                    }
                }

                Rectangle {
                    anchors.fill: parent
                    visible: window.windowBrowser.activeRendererFailed
                    color: window.colors.windowOpaque
                    z: 10

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 12

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "This page stopped working"
                            color: window.colors.text
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            Accessible.role: Accessible.Heading
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: window.windowBrowser.activeRendererFailureReason
                            color: window.colors.mutedText
                            font.pixelSize: 13
                        }

                        ChromeButton {
                            objectName: "recoverButton"
                            Layout.alignment: Qt.AlignHCenter
                            implicitWidth: 96
                            label: "Reload"
                            accessibleName: "Reload crashed page"
                            foreground: window.colors.text
                            accent: window.colors.accent
                            background: window.colors.surface
                            onClicked: window.windowBrowser.recoverActiveTab()
                        }
                    }
                }


                Rectangle {
                    objectName: "browserErrorBanner"
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(620, parent.width - 40)
                    height: window.windowBrowser.errorMessage.length > 0 ? 52 : 0
                    visible: height > 0
                    radius: 10
                    color: window.colors.surface
                    border.width: 1
                    border.color: window.colors.border
                    z: 20

                    Text {
                        anchors.fill: parent
                        anchors.margins: 12
                        text: window.windowBrowser.errorMessage
                        color: window.colors.text
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        MouseArea {
            x: sidebar.width
            width: parent.width - x
            height: parent.height
            visible: sidebar.statusOpen
            z: 50
            onClicked: sidebar.statusOpen = false
        }

        PanelResizer {
            id: sidebarResizer
            objectName: "sidebarResizer"
            visible: !window.sidebarCollapsed && !window.settingsOpen && !window.historyOpen
            enabled: visible
            height: parent.height
            x: sidebar.x + sidebar.width - width / 2
            z: 6
            colors: window.colors
            currentWidth: window.sidebarWidth
            minimumWidth: window.sidebarMinimumWidth
            maximumWidth: window.sidebarMaximumWidth
            defaultWidth: window.sidebarDefaultWidth

            onWidthRequested: function(width) { window.setSidebarWidth(width) }
            onPageFocusRequested: window.focusPage()
        }
    }

    Component {
        id: auxiliaryWindowComponent

        AuxiliaryWindow {
            engineSource: engineViewSource
            permissionController: window.windowBrowser
            contentBlocker: contentBlocker
            engineContentBlocker: engineContentBlocker
            onSitePermissionRequested: function(responder, requestId, origin, permission) {
                window.pendingPermissionRequest = requestId
                window.pendingPermissionResponder = responder
                window.pendingPermissionOrigin = origin
                window.pendingPermissionType = permission
                window.permissionOpen = true
            }

            onCertificateErrorRaised: function(responder, requestId, failure) {
                window.showCertificateError(responder, requestId, failure, true)
            }
        }
    }

    Connections {
        target: windowManager

        function onPrivateWindowRequested(controller, profilePath) {
            if (window.privateWindow) return
            if (!window.privateProfileHost) {
                const profileComponent = Qt.createComponent(engineProfileSource)
                window.privateProfileHost = profileComponent.createObject(window, {
                    "profilePath": profilePath,
                    "downloadDirectory": windowManager.privateDownloadDirectory,
                    "acceptDownloads": windowManager.acceptPrivateDownloads,
                    "privateBrowsing": true,
                    "engineContentBlocker": engineContentBlocker,
                    // A Private window has no Space of its own, so its
                    // third-party allowances key on the empty name its shared
                    // session already uses for Site permissions.
                    "engineCookiePolicy": engineCookiePolicy,
                    "cookieController": controller,
                    "cookieSpaceId": ""
                })
                // A Private page is not given the desktop's notification
                // centre. A notification would put the origin into a list that
                // outlives the private session and is read by whoever is at
                // the machine, which is the one thing a Private window
                // promises not to do. The shared private profile is watched
                // like any other so that its pages hear their notifications
                // close, and refused because the window refuses them.
                siteNotifications.watch("", window.privateProfileHost)
            }
            const component = Qt.createComponent(Qt.resolvedUrl("Main.qml"))
            const opened = component.createObject(null, {
                "windowBrowser": controller,
                "privateWindow": true,
                "opener": window,
                "profilePathOverride": profilePath,
                "sharedEngineProfile": window.privateProfileHost.profile
            })
            if (opened) window.privateWindows.push(opened)
        }

        function onPrivateSessionEnding() {
            if (window.privateWindow || windowManager.privateWindowCount > 0
                    || !window.privateProfileHost) return
            window.privateProfileHost.retire()
            window.privateProfileHost = null
        }
    }

    Connections {
        target: window.windowBrowser

        function onRetainedTabsChanged() { window.refreshRetainedTabs() }
        function onCertificateExceptionsChanged() {
            window.certificateExceptionGeneration += 1
        }
    }

    // A renderer's resident memory is a moving number, so the open list asks
    // again rather than showing what was true when it was opened. Nothing ticks
    // while the list is closed.
    Timer {
        running: window.settingsOpen && !window.privateWindow
        interval: 2000
        repeat: true
        triggeredOnStart: true
        onTriggered: window.refreshRetainedTabs()
    }

    Connections {
        target: contentBlocker

        function refreshBlockedRequestCount() {
            window.visibleBlockedRequestCount = contentBlocker.blockedRequestCount(
                window.windowBrowser.activeUrl)
        }

        // Rebuilding the subscription list means copying every list's title,
        // address and status into new values. That belongs to the settings
        // page, not to a counter that moves on every blocked request.
        function onSubscriptionsChanged() {
            window.visibleSubscriptions = contentBlocker.subscriptions
        }

        function onBlockedRequestCountChanged(siteUrl) { refreshBlockedRequestCount() }
        function onRulesChanged() { refreshBlockedRequestCount() }
    }

    Component.onCompleted: {
        window.createSpaceProfile()
        window.visibleSubscriptions = contentBlocker.subscriptions
        engineLoader.resume()
        // Last, and on its own: how wide a panel was left is never a reason
        // for the page not to come up.
        window.restoreSidebarWidth()
        window.restoreDeveloperToolsWidth()
        window.restoreTabAppearance()
    }

    function forgetPrivateWindow(instance) {
        const index = window.privateWindows.indexOf(instance)
        if (index !== -1) window.privateWindows.splice(index, 1)
    }

    onClosing: function(close) {
        // Parentless windows outlive their opener unless they are asked not to.
        if (!window.privateWindow) {
            for (const openWindow of window.privateWindows.slice()) openWindow.close()
            return
        }
        if (!window.windowBrowser) return
        const controller = window.windowBrowser
        const opener = window.opener
        Qt.callLater(function() {
            if (opener) opener.forgetPrivateWindow(window)
            window.destroy()
            windowManager.releasePrivateWindow(controller)
        })
    }

    ChromeMenu {
        id: spacesMenu
        objectName: "spacesMenu"
        anchors.fill: parent
        z: 55
        colors: window.colors
        open: window.spacesMenuOpen
        anchorX: window.spacesMenuX
        anchorY: window.spacesMenuY
        items: [
            {"label": "New Space"},
            {"label": "Rename " + window.windowBrowser.activeSpaceName},
            {"label": "Move this tab to a Space"},
            {"label": "Delete " + window.windowBrowser.activeSpaceName, "destructive": true}
        ]

        onDismissed: window.spacesMenuOpen = false

        onTriggered: function(index) {
            window.spacesMenuOpen = false
            switch (index) {
            case 0: window.requestNewSpace(); break
            case 1: window.dialogMode = "rename"; break
            case 2: window.requestMoveTab(); break
            case 3: window.dialogMode = "delete"; break
            }
        }
    }

    SpaceProfiles {
        id: spaceProfiles
        browser: window.windowBrowser
        // A Private window runs one shared temporary profile rather than a
        // Space's, so there is nothing here for it to build.
        profileSource: window.privateWindow ? "" : engineProfileSource
        contentBlocker: engineContentBlocker
        cookiePolicy: engineCookiePolicy
        owner: window

        onCreated: function(spaceId, host) { window.adoptSpaceProfile(spaceId, host) }
    }

    SiteNotifications {
        id: siteNotifications
        browser: window.windowBrowser
        allowed: !window.privateWindow

        onActivationRequested: function(spaceId, tabId) {
            window.windowBrowser.activateNotificationTarget(spaceId, tabId)
            window.raise()
            window.requestActivate()
        }
    }

    ChromeMenu {
        id: tabMenu
        objectName: "tabMenu"
        anchors.fill: parent
        z: 56
        colors: window.colors
        open: window.tabMenuOpen
        fromPointer: true
        itemWidth: 224
        anchorX: window.tabMenuX
        anchorY: window.tabMenuY
        items: window.tabMenuActionsFor(window.tabMenuTabId)

        onDismissed: window.tabMenuOpen = false
        onTriggered: function(index) { window.runTabMenu(index) }
    }

    ChromeMenu {
        id: pageMenu
        objectName: "pageMenu"
        anchors.fill: parent
        z: 56
        colors: window.colors
        open: window.pageMenuOpen
        fromPointer: true
        itemWidth: 248
        anchorX: window.pageMenuX
        anchorY: window.pageMenuY
        items: window.pageMenuActions

        onDismissed: {
            window.pageMenuOpen = false
            window.focusPage()
        }
        onTriggered: function(index) { window.runPageMenu(index) }
    }

    CommandDialog {
        id: spaceDialog
        objectName: "spaceDialog"
        anchors.fill: parent
        z: 60
        colors: window.colors
        open: window.dialogMode.length > 0
        destructive: window.dialogMode === "delete" || window.dialogMode === "confirm-move"
        inputVisible: window.dialogMode === "new" || window.dialogMode === "rename"
            || window.dialogMode === "delete"
        selectPreset: window.dialogMode === "rename"
        presetText: window.dialogMode === "rename" ? window.windowBrowser.activeSpaceName : ""

        label: {
            switch (window.dialogMode) {
            case "new": return "new space"
            case "rename": return "rename space"
            case "delete": return "delete space"
            case "move": return "move tab to a space"
            case "confirm-move": return "discard edited form state"
            }
            return ""
        }

        placeholder: {
            switch (window.dialogMode) {
            case "new": return "name the Space"
            case "rename": return window.windowBrowser.activeSpaceName
            case "delete": return "type " + window.windowBrowser.activeSpaceName + " to delete it"
            }
            return ""
        }

        message: {
            if (window.dialogMode === "delete") {
                return window.windowBrowser.activeSpaceName + " keeps its tabs, its session, "
                    + "its logins and its engine data. Deleting it cannot be undone."
            }
            if (window.dialogMode === "confirm-move") {
                return "This page has edited form state. Moving it reloads the page under the "
                    + "destination identity and discards those edits."
            }
            return ""
        }

        confirmHint: {
            switch (window.dialogMode) {
            case "new": return "⏎ create the Space"
            case "rename": return "⏎ rename the Space"
            case "delete": return "⏎ delete " + window.windowBrowser.activeSpaceName
            case "move": return "↑↓ choose      ⏎ move the tab"
            case "confirm-move": return "⏎ discard the edits and move"
            }
            return ""
        }

        rows: window.dialogMode === "move" ? window.moveTargets : []

        onDismissed: window.dialogMode = ""

        onAccepted: function(text) {
            switch (window.dialogMode) {
            case "new":
                const spaceId = window.windowBrowser.createSpace(text)
                if (spaceId.length > 0) window.windowBrowser.switchSpace(spaceId)
                break
            case "rename":
                window.windowBrowser.renameSpace(window.windowBrowser.activeSpaceId, text)
                break
            case "delete":
                window.windowBrowser.deleteSpace(window.windowBrowser.activeSpaceId, text)
                break
            case "confirm-move":
                window.windowBrowser.confirmTabMoveToSpace(
                    window.pendingMoveTabId, window.pendingMoveSpaceId)
                break
            }
            window.dialogMode = ""
        }

        onRowActivated: function(index) {
            const target = window.moveTargets[index]
            if (!target) return
            const tabId = window.windowBrowser.activeTabId
            window.dialogMode = ""
            engineLoader.checkForEditedFormState(function(hasEditedFormState) {
                window.windowBrowser.requestTabMoveToSpace(
                    tabId, target.id, hasEditedFormState)
            })
        }
    }

    CommandPanel {
        id: commandPanel
        anchors.fill: parent
        z: 50
        colors: window.colors
        commands: browserCommands
        // The window content behind the overlay, not the overlay's own parent,
        // so the blur never samples itself.
        backdropSource: shell
        open: window.omnibarOpen
        suggestions: window.omnibarSuggestions

        onDismissed: window.closeOmnibar()
        onQueryChanged: function(text) {
            if (commandPanel.commandMode) return
            window.omnibarSuggestions = window.privateWindow
                ? [] : window.windowBrowser.historySuggestions(text)
        }
        onCommitted: function(text) {
            window.windowBrowser.openInput(text, window.newTabIntent)
            window.closeOmnibar()
        }
    }

    MouseArea {
        width: 5
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.LeftEdge)
        z: 100
    }

    MouseArea {
        width: 5
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.RightEdge)
        z: 100
    }

    MouseArea {
        height: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.TopEdge)
        z: 100
    }

    MouseArea {
        height: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.BottomEdge)
        z: 100
    }
}
