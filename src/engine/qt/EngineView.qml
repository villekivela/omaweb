import QtQuick
import QtWebEngine

Item {
    id: root

    property alias currentUrl: webView.url
    property alias pageTitle: webView.title
    property alias pageIconUrl: webView.icon
    property alias loading: webView.loading
    property alias canGoBack: webView.canGoBack
    property alias canGoForward: webView.canGoForward
    property string profilePath: ""
    property var sharedProfile: null
    property var permissionController: null
    property var contentBlocker: null
    property var engineContentBlocker: null
    readonly property var browserProfile: webView.profile
    readonly property bool pageHasFocus: webView.activeFocus
    readonly property int navigationCapability: 1 << 0
    readonly property int persistentProfilesCapability: 1 << 1
    readonly property int contentBlockingCapability: 1 << 3
    readonly property int keyboardPageCommandsCapability: 1 << 4
    readonly property int rendererRecoveryCapability: 1 << 6
    readonly property int capabilities: navigationCapability
        | persistentProfilesCapability
        | contentBlockingCapability
        | keyboardPageCommandsCapability
        | rendererRecoveryCapability
    property int blockedRequestCount: 0
    property color pageBackgroundColor: "#16151d"
    property var keyboardNavigationConfiguration: ({})
    property string keyboardNavigationScriptSource: ""
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
    signal backgroundTabRequested(url requestedUrl)
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
    function configureKeyboardNavigation(configuration) {
        keyboardNavigationConfiguration = configuration
        applyKeyboardNavigationConfiguration()
    }
    function applyKeyboardNavigationConfiguration() {
        if (!webView || !keyboardNavigationConfiguration.version) return
        webView.runJavaScript("globalThis.__tantoKeyboardNavigation && "
            + "globalThis.__tantoKeyboardNavigation.configure("
            + JSON.stringify(keyboardNavigationConfiguration) + ");")
    }
    function refreshBlockedRequestCount() {
        root.blockedRequestCount = root.contentBlocker
            ? root.contentBlocker.blockedRequestCount(root.currentUrl) : 0
    }
    property bool cosmeticRulesInjected: false
    function applyCosmeticRules() {
        if (!contentBlocker || loading) return
        const css = contentBlocker.cosmeticStyleSheet(currentUrl)
        // With no cosmetic rules for this site there is nothing to add and, if
        // nothing was ever added, nothing to clear either — so skip the script.
        if (css.length === 0 && !cosmeticRulesInjected) return
        cosmeticRulesInjected = css.length > 0
        webView.runJavaScript(
            "(() => { let style = document.getElementById('__tanto_content_blocking');"
            + "if (!style) { style = document.createElement('style');"
            + "style.id = '__tanto_content_blocking'; document.documentElement.append(style); }"
            + "style.textContent = " + JSON.stringify(css) + "; })()")
    }
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

    // A Chromium profile is expensive and owns the Space's cache and cookie
    // store on disk. One per Space is correct; one per tab would have every
    // view contending for the same files. The window hands its Space profile
    // down as sharedProfile, so this one is built only for a view opened
    // without one, and only at the moment the view asks for it.
    property Component ownProfileComponent: Component {
        WebEngineProfile {
            storageName: "tanto-space"
            persistentStoragePath: root.profilePath
            cachePath: root.profilePath + "/cache"
            persistentCookiesPolicy: WebEngineProfile.ForcePersistentCookies
            httpCacheType: WebEngineProfile.DiskHttpCache
        }
    }
    // A plain object rather than a property: the view reads the profile from
    // here while creating it, and a QML property would make that read a
    // dependency of the write and report a binding loop.
    readonly property var ownProfileHolder: ({ instance: null })

    function resolvedProfile() {
        if (root.sharedProfile) return root.sharedProfile
        const holder = root.ownProfileHolder
        if (!holder.instance) {
            holder.instance = root.ownProfileComponent.createObject(root)
            if (root.engineContentBlocker)
                root.engineContentBlocker.attachToProfile(holder.instance)
        }
        return holder.instance
    }

    Component.onCompleted: {
        if (root.contentBlocker)
            root.blockedRequestCount = root.contentBlocker.blockedRequestCount(root.currentUrl)
        Qt.callLater(root.applyKeyboardNavigationConfiguration)
    }

    Connections {
        target: root.contentBlocker
        ignoreUnknownSignals: true

        // A blocked request only moves a counter. Cosmetic rules change when
        // the compiled rule set or a site's own decision changes, so those are
        // the only two that re-inject a stylesheet; doing it per blocked
        // request cost a rule lookup and a script round trip hundreds of times
        // over a single page load, in every open tab at once.
        function onBlockedRequestCountChanged(siteUrl) {
            if (siteUrl.toString().length > 0 && siteUrl.host !== root.currentUrl.host) return
            root.refreshBlockedRequestCount()
        }

        function onConfigurationChanged() {
            root.refreshBlockedRequestCount()
            root.applyCosmeticRules()
        }

        function onRulesChanged() {
            root.refreshBlockedRequestCount()
            root.applyCosmeticRules()
        }
    }

    property var keyboardNavigationScript: {
        const script = WebEngine.script()
        script.name = "Tanto Keyboard navigation"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = root.keyboardNavigationScriptSource
            + "\nglobalThis.__tantoKeyboardNavigation && "
            + "globalThis.__tantoKeyboardNavigation.configure("
            + JSON.stringify(root.keyboardNavigationConfiguration) + ");"
        return script
    }

    WebEngineView {
        id: webView
        objectName: "qtWebView"
        anchors.fill: parent
        profile: root.resolvedProfile()
        // Chromium paints this before a page supplies its own background.
        // Left at white it flashes a bright rectangle through dark chrome on
        // every navigation, so it follows the theme instead.
        backgroundColor: root.pageBackgroundColor
        focus: true
        userScripts.collection: [root.editedStateScript, root.keyboardNavigationScript]

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            root.rendererFailed("Renderer stopped with exit code " + exitCode)
        }

        onLoadingChanged: {
            root.refreshBlockedRequestCount()
            root.applyCosmeticRules()
            if (!loading) root.applyKeyboardNavigationConfiguration()
        }

        onNewWindowRequested: function(request) {
            if (request.destination === WebEngineNewWindowRequest.InNewBackgroundTab)
                root.backgroundTabRequested(request.requestedUrl)
            else if (root.isAuxiliaryDestination(request.destination))
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
