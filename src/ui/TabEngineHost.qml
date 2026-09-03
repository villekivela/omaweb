import QtQuick

Item {
    id: root
    objectName: "engineLoader"

    required property var browserController
    required property url engineSource
    // The table of Space profiles, which the window owns because it routes what
    // comes out of them. This host asks it for the profile a retained tab's
    // Space runs in, and for any Space a data-clearing command names.
    required property var spaceProfiles
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

    // What the page the reader is looking at is doing with the whole screen,
    // read off the engine that draws it rather than kept beside it.
    readonly property bool siteFullscreenActive: root.activeEngine
        ? root.activeEngine.siteFullscreenActive : false
    readonly property string siteFullscreenOrigin: root.activeEngine
        ? root.activeEngine.siteFullscreenOrigin : ""

    signal printFinished(string destination, bool succeeded)
    signal auxiliaryWindowRequested(var engine, var request, url requestedUrl)
    signal newTabRequested(var engine, var request, url requestedUrl)
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(var engine, string requestId, string origin, string permission)
    signal certificateErrorRaised(var engine, string requestId, var failure)
    signal pageContextRequested(var engine, var context)
    signal browserPromptRequested(var engine, string requestId, var prompt)
    signal fileSelectionRequested(var engine, string requestId, var selection)

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

    function requestPageContextMenu() {
        if (activeEngine) activeEngine.requestPageContextMenu()
    }

    // The everyday page operations, each one addressed to the engine of the tab
    // on show. A tab with no engine has no page to find in, print or hand the
    // screen back, so nothing is asked of one — the shell has already said so.
    function findText(query, forward) {
        if (root.activeEngine) root.activeEngine.findText(query, forward)
    }

    function printPage(destination) {
        if (root.activeEngine) root.activeEngine.printPage(destination)
        else root.printFinished(destination, false)
    }

    function exitSiteFullscreen() {
        if (root.activeEngine) root.activeEngine.exitSiteFullscreen()
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

    // Profile-wide data removal is an engine operation, and it reaches Spaces
    // that are not on show — including ones with no profile open, which the
    // table builds on being asked.
    function clearBrowsingData(spaceIds, dataTypes, since) {
        for (let index = 0; index < spaceIds.length; ++index) {
            const host = root.spaceProfiles.hostFor(spaceIds[index])
            if (host) host.clearBrowsingData(dataTypes, since)
        }
    }

    // Putting a Space away costs it its pages, which is the memory policy the
    // browser is built on: only the Space on show keeps live ones. The named
    // tabs are the exceptions and keep theirs, hidden and still running.
    function suspend(spaceId, retainedTabIds) {
        preservingEngines = true
        suspended = true
        activeEngine = null
        const retained = retainedTabIds || []
        for (const tabId in root.engineSpaces) {
            if (root.engineSpaces[tabId] !== spaceId) continue
            if (retained.indexOf(tabId) >= 0) {
                retainedEngines.keep(tabId, spaceId)
                const engine = root.engines[tabId]
                if (engine) engine.visible = false
                continue
            }
            root.discardEngine(tabId)
        }
    }

    function resume() {
        suspended = false
        preservingEngines = false
        retainedEngines.releaseVisibleSpace()
        // Retained tabs come back after the visible Space, not with it: the
        // reader is waiting for the page in front of them, and a renderer
        // started for a Space they cannot see must not be in the way of it.
        Qt.callLater(root.restoreRetainedTabs)
    }

    function restoreRetainedTabs() {
        if (root.suspended) return
        retainedEngines.reconcile()
    }

    // A tab of the Space on show takes that Space's profile, which is what the
    // window handed down. A retained tab of another Space names its own: its
    // pages are that Space's browsing identity and nothing else's.
    function createEngine(tabId, tabUrl, spaceId, profilePath, sharedProfile) {
        if (!engineComponent) engineComponent = Qt.createComponent(root.engineSource)
        const engine = engineComponent.createObject(root, {
            "profilePath": profilePath !== undefined ? profilePath : root.profilePath,
            "currentUrl": tabUrl,
            "sharedProfile": sharedProfile !== undefined ? sharedProfile : root.sharedProfile,
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
        root.engineSpaces[tabId] = spaceId !== undefined ? spaceId : root.spaceId
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
            if (root.engineSpaces[tabId] !== spaceId) continue
            retainedEngines.forget(tabId)
            discardEngine(tabId)
        }
    }

    // A retained tab keeps its engine when its delegate goes away with the
    // Space, and takes it away for good when the tab itself is closed or the
    // core stops retaining it.
    function keepsEngineFor(tabId) {
        return retainedEngines.keeps(tabId)
    }

    // Which pages outlive their Space is a policy of its own, kept beside the
    // engines rather than inside them: this host builds and destroys engines,
    // and that decides which ones ought to exist.
    property RetainedEngines retainedEngines: RetainedEngines {
        host: root
        browser: root.browserController
        spaceProfiles: root.spaceProfiles
        visibleSpaceId: root.spaceId
    }

    function retainedTabReport() {
        return retainedEngines.report()
    }

    Connections {
        target: root.browserController

        function onRetainedTabsChanged() {
            if (!root.suspended) Qt.callLater(root.restoreRetainedTabs)
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
            // Whether this tab's sound is being held back until the reader has
            // dealt with its origin. The core owns it and says so for every tab
            // on that origin at once, so a gesture in one page answers for the
            // next tab showing the same site.
            required property bool tabSoundSuppressed
            // How large this tab's page is drawn. The core owns it and keeps it
            // in the session; the engine is told it whenever it changes and
            // whenever a new engine takes the tab over.
            required property real tabZoom

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
                if (engine) {
                    engine.setZoomFactor(tabSlot.tabZoom)
                    tabSlot.applySoundPolicy()
                }
                showEngine()
            }

            // A page may start playing on its own: a silent video interrupts
            // nobody, and refusing playback outright costs the reader pages
            // that work in every other browser. What waits is the sound — the
            // reader's own muting, and the origin they have not dealt with yet.
            function applySoundPolicy() {
                if (!tabSlot.engine) return
                tabSlot.engine.autoplayAllowed = true
                tabSlot.engine.audioMuted = tabSlot.tabMuted || tabSlot.tabSoundSuppressed
            }

            onTabMutedChanged: tabSlot.applySoundPolicy()
            onTabSoundSuppressedChanged: tabSlot.applySoundPolicy()
            onTabZoomChanged: if (engine) engine.setZoomFactor(tabSlot.tabZoom)

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

            // What a retained page is playing is in the same position as its
            // artwork across a Space switch: the page kept going while the tab
            // it belongs to was reloaded from a store that records nothing
            // about it, and has no reason to report itself again. Only what it
            // is playing is read back — the muting is the session's now, and
            // the engine's own is partly this policy's rather than the
            // reader's, so reading that back would silence the tab for good.
            function restoreEnginePlayback() {
                const kept = root.engines[tabId]
                if (!kept) return
                root.browserController.setTabAudible(tabId, kept.pageAudible)
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
                restoreEnginePlayback()
                loadEngine()
            }

            // A retained tab's engine survives the Space switch that takes its
            // delegate; every other engine of that Space has already been
            // discarded by the suspension, so there is nothing here to hide.
            // Outside a suspension a disappearing delegate means the reader
            // closed the tab, and the page goes with it.
            Component.onDestruction: {
                if (root.preservingEngines) {
                    if (root.engines[tabId]) root.engines[tabId].visible = false
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
                        tabSlot.restoreEnginePlayback()
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
                    tabSlot.applySoundPolicy()
                    tabSlot.engine.configureKeyboardNavigation(
                        root.keyboardConfiguration(tabSlot.engine.currentUrl))
                }

                // The reader dealt with the page themselves, which is what the
                // sound was waiting for. The core answers for every tab on that
                // origin, so the next tab showing the same site hears it too
                // without being touched.
                function onUserActivated() {
                    root.browserController.recordOriginInteraction(tabSlot.engine.currentUrl)
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

                function onPrintFinished(destination, succeeded) {
                    root.printFinished(destination, succeeded)
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

                function onCertificateErrorRaised(requestId, failure) {
                    root.certificateErrorRaised(tabSlot.engine, requestId, failure)
                }

                function onBrowserPromptRequested(requestId, prompt) {
                    root.browserPromptRequested(tabSlot.engine, requestId, prompt)
                }

                function onFileSelectionRequested(requestId, selection) {
                    root.fileSelectionRequested(tabSlot.engine, requestId, selection)
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

        function onReloadBypassingCacheRequested() {
            if (root.activeEngine) root.activeEngine.reloadPageBypassingCache()
        }

        function onStopLoadingRequested() {
            if (root.activeEngine) root.activeEngine.stopLoading()
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
