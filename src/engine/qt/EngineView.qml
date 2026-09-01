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
    property bool keyboardNavigationHintModeActive: false
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
        keyboardNavigationHintModeActive = false
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
    property bool genericCosmeticRulesInjected: false
    readonly property string cosmeticElementId: "__tanto_content_blocking"
    readonly property string genericCosmeticElementId: "__tanto_content_blocking_generic"

    // A document-creation script runs before the parser has produced even an
    // <html> element, so the stylesheet cannot simply be appended: it waits for
    // the first element to appear, which is still before the page's own scripts
    // run and before anything is painted. Re-application into a document that
    // is already open takes the same path and appends immediately.
    function cosmeticStyleSnippet(elementId, css) {
        return "(() => {"
            + "const id = " + JSON.stringify(elementId) + ";"
            + "const css = " + JSON.stringify(css) + ";"
            + "const apply = () => {"
            + "const parent = document.head || document.documentElement;"
            + "if (!parent) return false;"
            + "let style = document.getElementById(id);"
            + "if (!style) {"
            + "style = document.createElement('style'); style.id = id;"
            + "parent.append(style);"
            + "}"
            + "style.textContent = css;"
            + "return true;"
            + "};"
            + "if (apply()) return;"
            + "const observer = new MutationObserver(() => {"
            + "if (apply()) observer.disconnect();"
            + "});"
            + "observer.observe(document, { childList: true, subtree: true });"
            + "})()"
    }

    // A scriptlet is a function from the vendored uBlock Origin library that a
    // `##+js(...)` rule names and supplies arguments for; the engine returns
    // the library's source for it together with the call. It runs in the
    // page's own world because that is the point — the code it neutralises is
    // the page's — but in a scope of its own, so the names it declares are not
    // the page's to see.
    //
    // `scriptletGlobals` is the one name the library expects its host to
    // supply: uBlock Origin passes its own extension's origin, a logging
    // channel, and a cache of the sandbox it builds. Tanto passes an empty
    // object, which is what says "no web-accessible resources, no logging
    // channel" — the library reads it defensively and builds the rest itself.
    function scriptletSnippet(source) {
        if (source.length === 0) return ""
        return "(() => {\nconst scriptletGlobals = {};\n" + source + "\n})();\n"
    }

    // Hiding rules have to be in the document before its own markup renders,
    // or the ads they cover appear and then vanish; a scriptlet has to be
    // there before the page's first script, or the anti-adblock check it
    // neutralises has already run. Document creation is the only injection
    // point early enough for either, and the script has to be rebuilt for each
    // navigation because both depend on the host being loaded.
    property var blockingScript: null
    function installBlockingScript(url) {
        if (!contentBlocker) return
        const css = contentBlocker.cosmeticStyleSheet(url)
        const scriptlets = contentBlocker.scriptletSource(url)
        const script = WebEngine.script()
        script.name = "Tanto content blocking"
        script.injectionPoint = WebEngineScript.DocumentCreation
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        // The stylesheet goes first: hiding what the page is about to render
        // does not depend on a scriptlet, and a scriptlet that throws must not
        // take the hiding with it.
        script.sourceCode = (css.length > 0
                ? root.cosmeticStyleSnippet(root.cosmeticElementId, css) + ";\n" : "")
            + root.scriptletSnippet(scriptlets)
        root.blockingScript = script
        webView.userScripts.collection = [
            root.editedStateScript, root.keyboardNavigationScript, script]
        // The document about to be created carries whatever this script adds
        // and nothing else, so what the last one had is no longer there.
        root.cosmeticRulesInjected = css.length > 0
        root.genericCosmeticRulesInjected = false
    }

    // Re-application into a document that is already open, for a rule set or a
    // per-site decision that changed under it. A fresh load takes the document
    // creation path above instead. Only the stylesheet is re-applied: a
    // scriptlet that missed the page's own scripts has nothing left to
    // intercept, and one already run cannot be taken back, so both directions
    // wait for the next navigation.
    function applyCosmeticRules() {
        if (!contentBlocker || loading) return
        const css = contentBlocker.cosmeticStyleSheet(currentUrl)
        // With no cosmetic rules for this site there is nothing to add and, if
        // nothing was ever added, nothing to clear either — so skip the script.
        if (css.length === 0 && !cosmeticRulesInjected) return
        cosmeticRulesInjected = css.length > 0
        webView.runJavaScript(root.cosmeticStyleSnippet(root.cosmeticElementId, css))
    }

    // The generic rules are the ones written against no particular site, and
    // sending all of them cost a 617 KB stylesheet on every page. The page
    // reports the classes and ids it actually carries, and only the generic
    // rules those could trigger come back. A site with a $generichide
    // exception is surveyed not at all.
    function clearGenericCosmeticRules() {
        if (!genericCosmeticRulesInjected) return
        root.genericCosmeticRulesInjected = false
        webView.runJavaScript(root.cosmeticStyleSnippet(root.genericCosmeticElementId, ""))
    }
    function surveyGenericCosmeticRules() {
        // Turning blocking off for a site, or a rule set that no longer hides
        // anything here, has to take back what the last survey hid.
        if (!contentBlocker || !contentBlocker.cosmeticSurveyWanted(currentUrl)) {
            root.clearGenericCosmeticRules()
            return
        }
        const surveyed = currentUrl
        webView.runJavaScript(
            "(() => {"
            + "const classes = new Set(), ids = new Set();"
            + "for (const element of document.querySelectorAll('[class], [id]')) {"
            + "if (element.id) ids.add(element.id);"
            + "for (const name of element.classList) classes.add(name);"
            + "}"
            + "return { classes: Array.from(classes), ids: Array.from(ids) };"
            + "})()",
            function(survey) {
                // The page can navigate away while the survey is in flight,
                // and its classes say nothing about where the view landed.
                if (!survey || !root.contentBlocker || surveyed !== root.currentUrl) return
                const css = root.contentBlocker.genericCosmeticStyleSheet(
                    surveyed, survey.classes, survey.ids)
                if (css.length === 0) {
                    root.clearGenericCosmeticRules()
                    return
                }
                root.genericCosmeticRulesInjected = true
                webView.runJavaScript(
                    root.cosmeticStyleSnippet(root.genericCosmeticElementId, css))
            })
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

    // A $popup rule is written against the windows a page opens, which is most
    // of them: a link that asks for a new tab counts, because that is how an
    // ad link opens. A background tab does not, because it takes a middle- or
    // ctrl-click to produce one and that is the user asking, not the page.
    function popupRefused(destination, requestedUrl) {
        return destination !== WebEngineNewWindowRequest.InNewBackgroundTab
            && root.contentBlocker
            && root.contentBlocker.shouldBlockPopup(requestedUrl, root.currentUrl)
    }

    // A Chromium profile is expensive and owns the Space's cache and cookie
    // store on disk. One per Space is correct; one per tab would have every
    // view contending for the same files. The window hands its Space profile
    // down as sharedProfile, so this one is built only for a view opened
    // without one, and only at the moment the view asks for it.
    property Component ownProfileComponent: Component {
        WebEngineProfile {
            storageName: "tanto-space"
            // See EngineProfile.qml: without this the profile is memory-only
            // and every cookie dies with the process.
            offTheRecord: false
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
            root.surveyGenericCosmeticRules()
        }

        function onRulesChanged() {
            root.refreshBlockedRequestCount()
            root.applyCosmeticRules()
            root.surveyGenericCosmeticRules()
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

        onLoadingChanged: function(loadRequest) {
            root.refreshBlockedRequestCount()
            if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                root.installBlockingScript(loadRequest.url)
                return
            }
            root.applyCosmeticRules()
            if (loadRequest.status === WebEngineView.LoadSucceededStatus) {
                root.surveyGenericCosmeticRules()
            }
            if (!loading) root.applyKeyboardNavigationConfiguration()
        }

        onNewWindowRequested: function(request) {
            if (root.popupRefused(request.destination, request.requestedUrl))
                return
            if (request.destination === WebEngineNewWindowRequest.InNewBackgroundTab)
                root.backgroundTabRequested(request.requestedUrl)
            else if (root.isAuxiliaryDestination(request.destination))
                root.auxiliaryWindowRequested(request, request.requestedUrl)
            else
                root.newTabRequested(request, request.requestedUrl)
        }

        onWindowCloseRequested: root.windowCloseRequested()

        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceId) {
            if (message === "__tanto_keyboard_hint_mode__:1")
                root.keyboardNavigationHintModeActive = true
            else if (message === "__tanto_keyboard_hint_mode__:0")
                root.keyboardNavigationHintModeActive = false
        }

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
