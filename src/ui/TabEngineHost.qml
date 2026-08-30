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

    readonly property alias item: root.activeEngine
    property var activeEngine: null
    property bool suspended: true

    signal auxiliaryWindowRequested(var engine, var request, url requestedUrl)
    signal newTabRequested(var engine, var request, url requestedUrl)
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(var engine, string requestId, string origin, string permission)

    function focusPage() {
        if (activeEngine) activeEngine.focusPage()
    }

    function checkForEditedFormState(callback) {
        if (activeEngine) activeEngine.checkForEditedFormState(callback)
        else callback(false)
    }

    function suspend() {
        suspended = true
        activeEngine = null
    }

    function resume() {
        suspended = false
    }

    Repeater {
        model: root.browserController ? root.browserController.tabs : null

        Item {
            id: tabSlot

            required property string tabId
            required property url tabUrl
            required property bool active

            anchors.fill: parent
            visible: active
            z: active ? 1 : 0

            function loadEngine() {
                if (root.suspended || engineLoader.item) return
                engineLoader.setSource(root.engineSource, {
                    "profilePath": root.profilePath,
                    "currentUrl": tabSlot.tabUrl,
                    "sharedProfile": root.sharedProfile,
                    "permissionController": root.permissionController,
                    "contentBlocker": root.blocker,
                    "engineContentBlocker": root.engineBlocker,
                    "keyboardNavigationConfiguration": root.keyboardManager.configurationForUrl(
                        tabSlot.tabUrl),
                    "keyboardNavigationScriptSource": root.keyboardManager.pageScript
                })
            }

            function unloadEngine() {
                if (root.activeEngine === engineLoader.item) root.activeEngine = null
                engineLoader.source = ""
            }

            onTabUrlChanged: {
                if (engineLoader.item && engineLoader.item.currentUrl !== tabUrl)
                    engineLoader.item.currentUrl = tabUrl
            }

            onActiveChanged: {
                if (active && engineLoader.item) {
                    root.activeEngine = engineLoader.item
                    Qt.callLater(root.focusPage)
                }
            }

            Component.onCompleted: loadEngine()
            Component.onDestruction: {
                if (root.activeEngine === engineLoader.item) root.activeEngine = null
            }

            Connections {
                target: root

                function onSuspendedChanged() {
                    if (root.suspended) tabSlot.unloadEngine()
                    else tabSlot.loadEngine()
                }
            }

            Loader {
                id: engineLoader
                anchors.fill: parent
                focus: tabSlot.active

                onLoaded: {
                    if (tabSlot.active) {
                        root.activeEngine = item
                        item.focusPage()
                    }
                }
            }

            Connections {
                target: engineLoader.item
                ignoreUnknownSignals: true

                function onCurrentUrlChanged() {
                    root.browserController.updateTab(
                        tabSlot.tabId, engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                    engineLoader.item.configureKeyboardNavigation(
                        root.keyboardManager.configurationForUrl(engineLoader.item.currentUrl))
                }

                function onPageIconUrlChanged() {
                    root.browserController.setTabIcon(
                        tabSlot.tabId, engineLoader.item.pageIconUrl)
                }

                function onPageTitleChanged() {
                    root.browserController.updateTab(
                        tabSlot.tabId, engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                }

                function onLoadingChanged() {
                    root.browserController.setTabLoading(tabSlot.tabId, engineLoader.item.loading)
                    if (!engineLoader.item.loading) {
                        root.browserController.recordVisit(
                            engineLoader.item.currentUrl, engineLoader.item.pageTitle)
                    }
                }

                function onRendererFailed(reason) {
                    root.browserController.reportTabRendererFailure(tabSlot.tabId, reason)
                }

                function onAuxiliaryWindowRequested(request, requestedUrl) {
                    root.auxiliaryWindowRequested(engineLoader.item, request, requestedUrl)
                }

                function onNewTabRequested(request, requestedUrl) {
                    root.newTabRequested(engineLoader.item, request, requestedUrl)
                }

                function onBackgroundTabRequested(requestedUrl) {
                    root.backgroundTabRequested(requestedUrl)
                }

                function onSitePermissionRequested(requestId, origin, permission) {
                    root.sitePermissionRequested(
                        engineLoader.item, requestId, origin, permission)
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
