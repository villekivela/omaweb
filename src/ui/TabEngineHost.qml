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
    property color pageBackgroundColor: "#16151d"
    property string spaceId: ""

    readonly property alias item: root.activeEngine
    property var activeEngine: null
    property bool suspended: true

    signal auxiliaryWindowRequested(var engine, var request, url requestedUrl)
    signal newTabRequested(var engine, var request, url requestedUrl)
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(var engine, string requestId, string origin, string permission)

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
            "keyboardNavigationConfiguration": root.keyboardManager.configurationForUrl(tabUrl),
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
                if (root.suspended || !everActive) return
                engine = root.engines[tabId] || root.createEngine(tabId, tabSlot.tabUrl)
                showEngine()
            }

            onTabUrlChanged: {
                if (engine && engine.currentUrl !== tabUrl) engine.currentUrl = tabUrl
            }

            onActiveChanged: {
                if (active) {
                    everActive = true
                    loadEngine()
                } else if (engine) {
                    engine.visible = false
                }
            }

            Component.onCompleted: loadEngine()

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
                        root.keyboardManager.configurationForUrl(tabSlot.engine.currentUrl))
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
                    root.keyboardManager.configurationForUrl(root.activeEngine.currentUrl))
            }
        }
    }
}
