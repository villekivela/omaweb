import QtQuick

Item {
    id: root
    objectName: "engineLoader"

    required property var browserController
    required property url engineSource
    required property string profilePath
    required property var sharedProfile
    required property var permissionController
    required property var blocker
    required property var engineBlocker
    required property var keyboardManager
    property var hintTheme: ({})
    property color pageBackgroundColor: "#16151d"
    property string spaceId: ""

    readonly property alias item: root.activeEngine
    readonly property bool hintModeActive: root.activeEngine
        ? root.activeEngine.keyboardNavigationHintModeActive : false
    property var activeEngine: null
    property bool suspended: true

    signal auxiliaryWindowRequested(var engine, var request, url requestedUrl)
    signal newTabRequested(var engine, var request, url requestedUrl)
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(var engine, string requestId, string origin, string permission)

    function keyboardConfiguration(url) {
        const configuration = Object.assign({}, root.keyboardManager.configurationForUrl(url))
        configuration.hintTheme = root.hintTheme
        return configuration
    }

    // Engines belong to the host, not to the tab row that shows them.
    // Switching Space replaces the whole tab model, so a delegate-owned engine
    // would be torn down and every page reloaded on the way back. These outlive
    // their delegate and are handed back when the Space returns.
    readonly property var engines: ({})
    // The Space each engine was opened in, so deleting a Space takes its pages
    // with it.
    readonly property var engineSpaces: ({})
    // Set while a Space is being put away: a delegate disappearing then means
    // the Space changed, not that the user closed the tab.
    property bool preservingEngines: false
    property var engineComponent: null

    // A page that opens a window completes the request against the engine of
    // the tab that window becomes. That tab starts blank and a blank tab is
    // given no engine, so it is named here: naming it builds the engine, and
    // the request then reaches that tab rather than whichever page was showing
    // when the window was asked for.
    property string adoptingTabId: ""

    // A tab with no address to load. `about:blank` is what a tab holds before
    // the reader commits a destination, what the last tab in a Space falls back
    // to, and what a page's new window starts as, so it is the one address that
    // is never worth an engine.
    function blankAddress(url) {
        const value = String(url)
        return value.length === 0 || value === "about:blank"
    }

    function adoptNewWindowRequest(tabId, request) {
        root.adoptingTabId = tabId
        const engine = root.engines[tabId]
        if (engine) {
            root.adoptingTabId = ""
            engine.acceptNewWindowRequest(request)
            return
        }
        Qt.callLater(function() {
            const late = root.engines[tabId]
            if (!late) return
            root.adoptingTabId = ""
            late.acceptNewWindowRequest(request)
        })
    }

    function focusPage() {
        if (activeEngine) activeEngine.focusPage()
    }

    function checkForEditedFormState(callback) {
        if (activeEngine) activeEngine.checkForEditedFormState(callback)
        else callback(false)
    }

    function suspend() {
        preservingEngines = true
        suspended = true
        activeEngine = null
    }

    function resume() {
        suspended = false
        preservingEngines = false
    }

    function createEngine(tabId, tabUrl) {
        if (!engineComponent) engineComponent = Qt.createComponent(root.engineSource)
        const engine = engineComponent.createObject(root, {
            "profilePath": root.profilePath,
            "currentUrl": tabUrl,
            "sharedProfile": root.sharedProfile,
            "permissionController": root.permissionController,
            "contentBlocker": root.blocker,
            "engineContentBlocker": root.engineBlocker,
            "keyboardNavigationConfiguration": root.keyboardConfiguration(tabUrl),
            "keyboardNavigationScriptSource": root.keyboardManager.pageScript,
            "pageBackgroundColor": root.pageBackgroundColor,
            "visible": false
        })
        if (!engine) return null
        engine.anchors.fill = root
        root.engines[tabId] = engine
        root.engineSpaces[tabId] = root.spaceId
        return engine
    }

    function discardEngine(tabId) {
        const engine = root.engines[tabId]
        if (!engine) return
        if (root.activeEngine === engine) root.activeEngine = null
        delete root.engines[tabId]
        delete root.engineSpaces[tabId]
        engine.destroy()
    }

    function discardEnginesForSpace(spaceId) {
        for (const tabId in root.engineSpaces) {
            if (root.engineSpaces[tabId] === spaceId) discardEngine(tabId)
        }
    }

    Repeater {
        model: root.browserController ? root.browserController.tabs : null

        Item {
            id: tabSlot

            required property string tabId
            required property url tabUrl
            required property bool active

            // A restored Space can hold many tabs, and each engine costs a
            // renderer process and a page load. Only a tab the user has
            // actually looked at gets one; the rest keep their saved title and
            // address until they are first selected.
            property bool everActive: active
            property var engine: null

            // A blank tab has no page, and the shortcut sheet stands in for it.
            // An engine here would spend a renderer process on an empty
            // document nobody can see, so a tab gets one once it has an address
            // to load — or once it is the tab a page's new-window request has to
            // be handed to.
            //
            // Asked as a function as well as a binding, because a change
            // handler for `tabUrl` cannot trust a binding that depends on
            // `tabUrl` to have been re-evaluated yet: QML does not order a
            // property's change handlers against the bindings that read it.
            function needsEngine() {
                return !root.blankAddress(tabSlot.tabUrl)
                    || root.adoptingTabId === tabSlot.tabId
            }

            readonly property bool wantsEngine: !root.blankAddress(tabUrl)
                || root.adoptingTabId === tabId

            function showEngine() {
                if (!engine) return
                engine.visible = tabSlot.active
                engine.z = tabSlot.active ? 1 : 0
                if (tabSlot.active) {
                    root.activeEngine = engine
                    Qt.callLater(root.focusPage)
                }
            }

            function loadEngine() {
                if (root.suspended || !everActive || !needsEngine()) return
                engine = root.engines[tabId] || root.createEngine(tabId, tabSlot.tabUrl)
                showEngine()
            }

            // Site artwork belongs to the loaded page rather than to the saved
            // session, so the core drops it when a Space switch reloads the
            // Space's tabs from its store. A page kept alive across that switch
            // has already reported its icon and has no reason to report it
            // again, which would leave the tab wearing its lettered tile for a
            // site whose artwork is still in memory. So a retained engine hands
            // its icon back on the way in. Not only the tab the returning Space
            // shows: a background tab keeps its engine too, and does not
            // reattach until it is next selected.
            function restoreReportedIcon() {
                const retained = root.engines[tabId]
                if (!retained || String(retained.pageIconUrl).length === 0) return
                root.browserController.setTabIcon(tabId, retained.pageIconUrl)
            }

            onTabUrlChanged: {
                // A tab that has lost its address has lost its page, and the
                // renderer that drew it goes too rather than idling behind the
                // sheet that stands in for it.
                if (!needsEngine()) {
                    if (engine) {
                        root.discardEngine(tabId)
                        engine = null
                    }
                    return
                }
                if (!engine) loadEngine()
                else if (engine.currentUrl !== tabUrl) engine.currentUrl = tabUrl
            }

            onWantsEngineChanged: if (wantsEngine) loadEngine()

            onActiveChanged: {
                if (active) {
                    everActive = true
                    loadEngine()
                    // A tab with no engine is showing nothing, and the host has
                    // to say so. Leaving the last tab's engine as the active one
                    // would have the window believe a page is up: the sheet that
                    // stands in for a blank tab would stay away, and back and
                    // forward would answer for another tab's history.
                    if (!engine) root.activeEngine = null
                } else if (engine) {
                    engine.visible = false
                }
            }

            Component.onCompleted: {
                restoreReportedIcon()
                loadEngine()
            }

            // The engine survives a Space switch and goes away with its tab.
            Component.onDestruction: {
                if (root.preservingEngines) {
                    if (engine) engine.visible = false
                    if (root.activeEngine === engine) root.activeEngine = null
                } else {
                    root.discardEngine(tabId)
                }
            }

            Connections {
                target: root

                function onSuspendedChanged() {
                    if (root.suspended) {
                        if (tabSlot.engine) tabSlot.engine.visible = false
                    } else {
                        tabSlot.restoreReportedIcon()
                        tabSlot.loadEngine()
                    }
                }
            }

            Connections {
                target: tabSlot.engine
                ignoreUnknownSignals: true

                function onCurrentUrlChanged() {
                    root.browserController.updateTab(
                        tabSlot.tabId, tabSlot.engine.currentUrl, tabSlot.engine.pageTitle)
                    tabSlot.engine.configureKeyboardNavigation(
                        root.keyboardConfiguration(tabSlot.engine.currentUrl))
                }

                function onPageIconUrlChanged() {
                    root.browserController.setTabIcon(tabSlot.tabId, tabSlot.engine.pageIconUrl)
                }

                function onPageTitleChanged() {
                    root.browserController.updateTab(
                        tabSlot.tabId, tabSlot.engine.currentUrl, tabSlot.engine.pageTitle)
                }

                function onLoadingChanged() {
                    root.browserController.setTabLoading(tabSlot.tabId, tabSlot.engine.loading)
                    if (!tabSlot.engine.loading) {
                        root.browserController.recordVisit(
                            tabSlot.engine.currentUrl, tabSlot.engine.pageTitle)
                    }
                }

                function onRendererFailed(reason) {
                    root.browserController.reportTabRendererFailure(tabSlot.tabId, reason)
                }

                function onAuxiliaryWindowRequested(request, requestedUrl) {
                    root.auxiliaryWindowRequested(tabSlot.engine, request, requestedUrl)
                }

                function onNewTabRequested(request, requestedUrl) {
                    root.newTabRequested(tabSlot.engine, request, requestedUrl)
                }

                function onBackgroundTabRequested(requestedUrl) {
                    root.backgroundTabRequested(requestedUrl)
                }

                function onSitePermissionRequested(requestId, origin, permission) {
                    root.sitePermissionRequested(
                        tabSlot.engine, requestId, origin, permission)
                }
            }
        }
    }

    Connections {
        target: root.browserController

        function onBackRequested() {
            if (root.activeEngine) root.activeEngine.goBack()
        }

        function onForwardRequested() {
            if (root.activeEngine) root.activeEngine.goForward()
        }

        function onReloadRequested() {
            if (root.activeEngine) root.activeEngine.reloadPage()
        }
    }

    Connections {
        target: root.keyboardManager

        function onConfigurationChanged() {
            if (root.activeEngine) {
                root.activeEngine.configureKeyboardNavigation(
                    root.keyboardConfiguration(root.activeEngine.currentUrl))
            }
        }
    }

    onHintThemeChanged: {
        for (const tabId in root.engines) {
            const engine = root.engines[tabId]
            engine.configureKeyboardNavigation(root.keyboardConfiguration(engine.currentUrl))
        }
    }
}
