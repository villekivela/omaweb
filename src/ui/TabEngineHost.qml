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
    // The palette the engine draws its inspector in, so a docked inspector is
    // the same window as the chrome around it rather than the engine's own idea
    // of a colour scheme.
    property var developerToolsColors: ({})
    property color pageBackgroundColor: "#16151d"
    property string spaceId: ""

    readonly property alias item: root.activeEngine
    // The inspector of the tab the core says is being inspected, for the dock
    // to take as a child. Null whenever nothing is attached.
    property var developerToolsView: null
    readonly property string inspectedTabId: root.browserController
        ? root.browserController.developerToolsTabId : ""
    readonly property bool hintModeActive: root.activeEngine
        ? root.activeEngine.keyboardNavigationHintModeActive : false
    property var activeEngine: null
    property bool suspended: true

    signal auxiliaryWindowRequested(var engine, var request, url requestedUrl)
    signal newTabRequested(var engine, var request, url requestedUrl)
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(var engine, string requestId, string origin, string permission)
    signal pageContextRequested(var engine, var context)

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

    // The core owns which tab is inspected; the engines are told about it here.
    // One inspector at a time, so every engine that is not the named one gives
    // its own up — including a tab that had the inspector before this one.
    function syncDeveloperTools() {
        const wanted = root.inspectedTabId
        for (const tabId in root.engines) {
            const engine = root.engines[tabId]
            if (tabId !== wanted && engine.developerToolsAttached) engine.detachDeveloperTools()
        }
        const inspected = wanted.length > 0 ? root.engines[wanted] : null
        if (inspected && !inspected.developerToolsAttached) inspected.attachDeveloperTools()
        root.developerToolsView = inspected ? inspected.developerToolsView : null
    }

    onInspectedTabIdChanged: root.syncDeveloperTools()

    // Asking to inspect the page is asking for the dock as well, so the core
    // hears about the attachment first and the engine is told which node to
    // select only once the core has accepted it. A tab the core refuses — a
    // blank one, which has no page — must not be handed an inspector the core
    // does not know about and would never take away again.
    function inspectElement() {
        if (!root.activeEngine) return
        root.browserController.openDeveloperTools()
        if (root.inspectedTabId !== root.browserController.activeTabId) return
        root.activeEngine.inspectElement()
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
            "developerToolsColors": root.developerToolsColors,
            "visible": false
        })
        if (!engine) return null
        engine.anchors.fill = root
        root.engines[tabId] = engine
        root.engineSpaces[tabId] = root.spaceId
        // A tab can be named as the inspected one before it has an engine to
        // attach to — a Space coming back, or the tab being selected for the
        // first time — so the attachment is made as soon as there is one.
        if (tabId === root.inspectedTabId) root.syncDeveloperTools()
        return engine
    }

    function discardEngine(tabId) {
        const engine = root.engines[tabId]
        if (!engine) return
        // The inspector is the engine's to destroy, and nothing else holds it:
        // the dock only borrowed it.
        if (engine.developerToolsAttached) {
            engine.detachDeveloperTools()
            if (tabId === root.inspectedTabId) root.developerToolsView = null
        }
        if (root.activeEngine === engine) root.activeEngine = null
        delete root.engines[tabId]
        delete root.engineSpaces[tabId]
        // The page that was making the sound is going away with its renderer,
        // and nothing is left to report that it stopped.
        root.browserController.setTabAudible(tabId, false)
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
            // The reader's standing decision about this tab's sound. The
            // engine holds it while the tab has one, and is told again
            // whenever it changes or a new engine takes the tab over.
            required property bool tabMuted

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
            //
            // A delegate is built before it is told which tab it stands for,
            // and an unnamed one stands for none: with no tab named for
            // adoption either, it would match the tab being adopted and be
            // given an engine of its own — one keyed to no tab, so no tab ever
            // shows it, hides it or takes it away, left on top of the page the
            // reader came back to.
            function needsEngine() {
                if (tabSlot.tabId.length === 0) return false
                return !root.blankAddress(tabSlot.tabUrl)
                    || root.adoptingTabId === tabSlot.tabId
            }

            readonly property bool wantsEngine: tabId.length > 0
                && (!root.blankAddress(tabUrl) || root.adoptingTabId === tabId)

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
                if (engine) engine.audioMuted = tabSlot.tabMuted
                showEngine()
            }

            onTabMutedChanged: if (engine) engine.audioMuted = tabSlot.tabMuted

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

            // Sound is in the same position as artwork across a Space switch:
            // the page kept playing, and kept the muting it was given, while
            // the tab it belongs to was reloaded from a store that records
            // neither. Both are read back off the engine that outlived the
            // switch, so the row does not offer to mute a page it has already
            // muted, or stay silent about one that is playing.
            function restoreEngineSound() {
                const retained = root.engines[tabId]
                if (!retained) return
                root.browserController.setTabMuted(tabId, retained.audioMuted)
                root.browserController.setTabAudible(tabId, retained.pageAudible)
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
                restoreEngineSound()
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
                        tabSlot.restoreEngineSound()
                        tabSlot.loadEngine()
                    }
                }
            }

            // An engine names no address at all between pages: the view's
            // address is cleared as a navigation starts and named again when
            // it commits, and a page adopted from a new-window request passes
            // through that gap on its way to the address it was opened for. A
            // tab whose engine is mid-navigation has not lost its page, and
            // saying that it had would blank the tab — which now takes its
            // engine with it, so the page a link opened would be torn down
            // while it loaded and the Start page left standing in its place.
            function reportPageState() {
                if (!tabSlot.engine || String(tabSlot.engine.currentUrl).length === 0) return
                root.browserController.updateTab(
                    tabSlot.tabId, tabSlot.engine.currentUrl, tabSlot.engine.pageTitle)
            }

            Connections {
                target: tabSlot.engine
                ignoreUnknownSignals: true

                function onCurrentUrlChanged() {
                    tabSlot.reportPageState()
                    tabSlot.engine.configureKeyboardNavigation(
                        root.keyboardConfiguration(tabSlot.engine.currentUrl))
                }

                function onPageIconUrlChanged() {
                    root.browserController.setTabIcon(tabSlot.tabId, tabSlot.engine.pageIconUrl)
                }

                function onPageTitleChanged() {
                    tabSlot.reportPageState()
                }

                function onPageAudibleChanged() {
                    root.browserController.setTabAudible(
                        tabSlot.tabId, tabSlot.engine.pageAudible)
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

                function onPageContextRequested(context) {
                    root.pageContextRequested(tabSlot.engine, context)
                }

                // The frontend's own close button, which is the reader saying
                // they are finished with it rather than the tab going away.
                function onDeveloperToolsClosed() {
                    root.browserController.closeDeveloperTools()
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

    onDeveloperToolsColorsChanged: {
        for (const tabId in root.engines)
            root.engines[tabId].developerToolsColors = root.developerToolsColors
    }

    onHintThemeChanged: {
        for (const tabId in root.engines) {
            const engine = root.engines[tabId]
            engine.configureKeyboardNavigation(root.keyboardConfiguration(engine.currentUrl))
        }
    }
}
