import QtQuick
import QtWebEngine

Item {
    id: root

    property alias currentUrl: webView.url
    property alias pageTitle: webView.title
    property alias loading: webView.loading
    property alias canGoBack: webView.canGoBack
    property alias canGoForward: webView.canGoForward
    property string profilePath: ""
    property var sharedProfile: null
    property var permissionController: null
    readonly property var browserProfile: webView.profile
    readonly property bool pageHasFocus: webView.activeFocus
    readonly property int capabilities: 67
    property var editedStateScript: {
        const script = WebEngine.script()
        script.name = "Tanto edited form state"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = "globalThis.__tantoContentEditableEdited = false;"
            + "document.addEventListener('input', event => {"
            + "if (event.target && event.target.isContentEditable) "
            + "globalThis.__tantoContentEditableEdited = true;"
            + "}, true);"
        return script
    }

    signal rendererFailed(string reason)
    signal newTabRequested(var request, url requestedUrl)
    signal auxiliaryWindowRequested(var request, url requestedUrl)
    signal windowCloseRequested()
    signal sitePermissionRequested(string requestId, string origin, string permission)
    property var pendingPermissions: ({})
    property int nextPermissionRequestId: 0

    function respondToPermission(requestId, decision) {
        const request = pendingPermissions[requestId]
        if (!request) return
        delete pendingPermissions[requestId]
        if (decision === 1 || decision === 2) request.grant()
        else request.deny()
    }

    function goBack() { webView.goBack() }
    function goForward() { webView.goForward() }
    function focusPage() { webView.forceActiveFocus() }
    function reloadPage() { webView.reload() }
    function checkForEditedFormState(callback) {
        webView.runJavaScript(
            "(() => {"
            + "for (const field of document.querySelectorAll('input, textarea')) {"
            + "if (field.type === 'checkbox' || field.type === 'radio') {"
            + "if (field.checked !== field.defaultChecked) return true;"
            + "} else if (field.value !== field.defaultValue) return true;"
            + "}"
            + "for (const option of document.querySelectorAll('select option')) {"
            + "if (option.selected !== option.defaultSelected) return true;"
            + "}"
            + "return Boolean(globalThis.__tantoContentEditableEdited);"
            + "})()",
            callback)
    }
    function acceptNewWindowRequest(request) {
        if (request) request.openIn(webView)
    }
    function isAuxiliaryDestination(destination) {
        return destination === WebEngineNewWindowRequest.InNewDialog
    }

    WebEngineProfile {
        id: spaceProfile
        storageName: "tanto-space"
        persistentStoragePath: root.profilePath
        cachePath: root.profilePath + "/cache"
        persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
        httpCacheType: WebEngineProfile.DiskHttpCache
    }

    WebEngineView {
        id: webView
        objectName: "qtWebView"
        anchors.fill: parent
        profile: root.sharedProfile ? root.sharedProfile : spaceProfile
        backgroundColor: "white"
        focus: true
        userScripts.collection: [root.editedStateScript]

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            root.rendererFailed("Renderer stopped with exit code " + exitCode)
        }

        onNewWindowRequested: function(request) {
            if (root.isAuxiliaryDestination(request.destination))
                root.auxiliaryWindowRequested(request, request.requestedUrl)
            else
                root.newTabRequested(request, request.requestedUrl)
        }

        onWindowCloseRequested: root.windowCloseRequested()

        onPermissionRequested: function(request) {
            const permission = String(request.permissionType)
            const decision = root.permissionController
                ? root.permissionController.permissionDecision(request.origin, permission)
                : 0
            if (decision === 1 || decision === 2)
                request.grant()
            else if (decision === 3)
                request.deny()
            else {
                const requestId = String(++root.nextPermissionRequestId)
                root.pendingPermissions[requestId] = request
                root.sitePermissionRequested(requestId, request.origin.toString(), permission)
            }
        }
    }
}
