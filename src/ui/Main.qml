import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
    title: window.privateWindow ? "Private — Tanto" : window.windowBrowser.activeTitle + " — Tanto"

    property var windowBrowser: browser
    // The native backdrop reads this to mask its blur to the same rounded rect,
    // so the shell and the platform chrome cannot drift apart.
    property real cornerRadius: 14
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
    property bool omnibarOpen: false
    property bool newTabIntent: false
    property string pendingMoveTabId: ""
    property string pendingMoveSpaceId: ""
    property var privateProfileHost: null
    property var spaceProfileHost: null
    readonly property var spaceProfileHosts: ({})
    property var omnibarSuggestions: []
    property var visibleDownloads: []
    property var visibleSubscriptions: []
    property int visibleBlockedRequestCount: 0
    property var pendingPermissionRequest: null
    property string pendingPermissionOrigin: ""
    property string pendingPermissionType: ""
    property var pendingPermissionResponder: null
    property var downloadRecordIds: ({})
    property bool settingsOpen: false
    property bool spacesMenuOpen: false
    property real spacesMenuX: 0
    property real spacesMenuY: 0
    property bool permissionOpen: false
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

    BrowserCommands {
        id: browserCommands
        window: window
        browser: window.windowBrowser
        keymap: keymap
    }

    function openCommandPanel() {
        commandPanel.beginCommand()
        omnibarOpen = true
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
        window.settingsOpen = true
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

    // A width the reader chose outlives the session that chose it. The clamp
    // runs on the way back in, so a saved width from a wider window or an older
    // build still lands somewhere usable.
    function restoreSidebarWidth() {
        const saved = parseFloat(window.windowBrowser.preference("sidebar-width", ""))
        if (!isNaN(saved)) window.setSidebarWidth(saved)
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

    // A drag reports every pixel it crosses. The store hears the width the
    // hand came to rest at, not the path it took to get there.
    Timer {
        id: sidebarWidthWriter
        interval: 400
        onTriggered: window.windowBrowser.setPreference("sidebar-width",
            String(window.sidebarWidth))
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

    // One profile per Space, kept for as long as the Space's pages are. A
    // profile owns the Space's cookies and cache on disk, so rebuilding it on
    // every switch cost a full teardown and reopen of both.
    function createSpaceProfile() {
        if (window.privateWindow) return
        const spaceId = window.windowBrowser.activeSpaceId
        const existing = window.spaceProfileHosts[spaceId]
        if (existing) {
            window.spaceProfileHost = existing
            return
        }
        const component = Qt.createComponent(engineProfileSource)
        const host = component.createObject(window, {
            "profilePath": window.windowBrowser.activeProfilePath,
            "downloadDirectory": window.windowBrowser.downloadDirectory,
            "acceptDownloads": window.windowBrowser.acceptDownloads,
            "privateBrowsing": false,
            "downloadNamespace": spaceId,
            "engineContentBlocker": engineContentBlocker
        })
        if (!host) return
        host.downloadStarted.connect(window.handleDownloadStarted)
        host.downloadUpdated.connect(window.handleDownloadUpdated)
        window.spaceProfileHosts[spaceId] = host
        window.spaceProfileHost = host
    }

    function retireSpaceProfile(spaceId) {
        const host = window.spaceProfileHosts[spaceId]
        if (!host) return
        delete window.spaceProfileHosts[spaceId]
        if (window.spaceProfileHost === host) window.spaceProfileHost = null
        host.retire()
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
                enabled: keymap.isChord(modelData) || keymap.pageCommandsEnabled
                context: Qt.WindowShortcut
                onActivated: browserCommands.run(
                    keymap.commandFor(modelData),
                    parseInt(modelData.slice(-1), 10) - 1)
            }
        }
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: window.cornerRadius
        color: window.colors.window
        border.width: 1
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
                useFavicons: window.useFavicons
                tintFavicons: window.tintFavicons

                // A drag is already following the pointer; easing it too
                // would make the seam lag behind the hand holding it.
                Behavior on Layout.preferredWidth {
                    enabled: !sidebarResizer.dragging
                    NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                }

                onAddressRequested: window.openOmnibar(false)
                onTabActivated: function(tabId) { window.windowBrowser.activateTab(tabId) }
                onTabCloseRequested: function(tabId) { window.windowBrowser.closeTab(tabId) }
                onSpaceActivated: function(spaceId) { window.windowBrowser.switchSpace(spaceId) }
                onSpacesMenuRequested: function(anchorX, anchorY) {
                    window.spacesMenuX = anchorX
                    window.spacesMenuY = anchorY
                    window.spacesMenuOpen = true
                }
                onSettingsRequested: window.requestSettings()
                onWindowMoveRequested: window.startSystemMove()
                onPageFocusRequested: window.focusPage()
            }

            Item {
                objectName: "engineViewport"
                Layout.fillWidth: true
                Layout.fillHeight: true

                // The shell around it is translucent by theme; a webpage viewport
                // never is, so it gets its own opaque backing rather than
                // inheriting whatever the desktop is showing.
                Rectangle {
                    objectName: "engineBacking"
                    anchors.fill: parent
                    color: window.colors.windowOpaque
                }

                TabEngineHost {
                    id: engineLoader
                    anchors.fill: parent
                    focus: true
                    browserController: window.windowBrowser
                    engineSource: engineViewSource
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
                        if (request) {
                            if (engineLoader.item) {
                                engineLoader.item.acceptNewWindowRequest(request)
                            } else {
                                Qt.callLater(function() {
                                    if (engineLoader.item)
                                        engineLoader.item.acceptNewWindowRequest(request)
                                })
                            }
                        }
                    }

                    onBackgroundTabRequested: function(requestedUrl) {
                        window.windowBrowser.openInputInBackground(requestedUrl)
                    }

                    onSitePermissionRequested: function(engine, requestId, origin, permission) {
                        window.pendingPermissionRequest = requestId
                        window.pendingPermissionResponder = engine
                        window.pendingPermissionOrigin = origin
                        window.pendingPermissionType = permission
                        window.permissionOpen = true
                    }
                }

                PageQuestionBar {
                    objectName: "sitePermissionBar"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    z: 40
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    open: window.permissionOpen
                    glyph: "shield_person"
                    message: window.pendingPermissionOrigin
                        + " asked for a protected browser capability"
                    detail: window.pendingPermissionType
                        + " · remembered for this Space only"
                    actions: [
                        {"label": "Allow once"},
                        {"label": "Always allow", "enabled": !window.privateWindow},
                        {"label": "Block"}
                    ]

                    onActionTriggered: function(index) {
                        window.respondToPermission(index + 1)
                    }
                }

                SettingsPage {
                    objectName: "settingsSurface"
                    anchors.fill: parent
                    z: 45
                    colors: window.colors
                    iconFontFamily: materialSymbols.name
                    browser: window.windowBrowser
                    blocker: contentBlocker
                    keyboard: keyboardNavigation
                    open: window.settingsOpen
                    useFavicons: window.useFavicons
                    tintFavicons: window.tintFavicons

                    onClosed: window.settingsOpen = false
                    onUseFaviconsToggled: function(enabled) { window.setUseFavicons(enabled) }
                    onTintFaviconsToggled: function(enabled) { window.setTintFavicons(enabled) }
                }

                NavigationCluster {
                    visible: !window.settingsOpen
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 20
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

                    // The Space's pages and the profile they run in are put
                    // aside, not thrown away: coming back to a Space should
                    // find it where it was left rather than reloading every
                    // tab from its address.
                    function onSpaceSuspended(spaceId) {
                        engineLoader.suspend()
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

                    function onCloseWindowRequested() {
                        if (window.privateWindow) window.close()
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

        SidebarResizer {
            id: sidebarResizer
            objectName: "sidebarResizer"
            visible: !window.sidebarCollapsed && !window.settingsOpen
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
                    "engineContentBlocker": engineContentBlocker
                })
            }
            const component = Qt.createComponent(Qt.resolvedUrl("Main.qml"))
            component.createObject(window, {
                "windowBrowser": controller,
                "privateWindow": true,
                "profilePathOverride": profilePath,
                "sharedEngineProfile": window.privateProfileHost.profile
            })
        }

        function onPrivateSessionEnding() {
            if (window.privateWindow || windowManager.privateWindowCount > 0
                    || !window.privateProfileHost) return
            window.privateProfileHost.retire()
            window.privateProfileHost = null
        }
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
        window.restoreTabAppearance()
    }

    onClosing: function(close) {
        if (!window.privateWindow || !window.windowBrowser) return
        const controller = window.windowBrowser
        Qt.callLater(function() {
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

    Rectangle {
        objectName: "privateIndicator"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 4
        visible: window.privateWindow
        color: window.colors.privateAccent
        z: 90
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
