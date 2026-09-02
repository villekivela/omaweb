import QtQuick
import QtWebEngine
import Omaweb

Item {
    id: root

    property alias currentUrl: webView.url
    property alias pageTitle: webView.title
    property alias pageIconUrl: webView.icon
    property alias loading: webView.loading
    // Chromium keeps a page "recently audible" for a moment after the sound
    // stops, which is what keeps the tab's speaker from flickering through a
    // pause between clips. Muting is the embedder's to set and survives
    // navigation within the view, so the tab keeps its decision across pages.
    readonly property bool pageAudible: webView.recentlyAudible
    property alias audioMuted: webView.audioMuted
    // Whether this page may start playing without a gesture of its own. It is
    // the shell's decision rather than the engine's default, and the shell
    // allows it: a silent video interrupts nobody. Whether it is heard is a
    // separate answer, given through the muting above, because the engine's own
    // gesture requirement cannot tell the two apart.
    property bool autoplayAllowed: false
    // The process drawing this page, so the shell can say what a retained tab
    // actually costs rather than only that it is running. Zero while the view
    // has no renderer, which is the honest answer for a page that is not up.
    readonly property int renderProcessPid: webView.renderProcessPid
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
    readonly property int developerToolsCapability: 1 << 5
    readonly property int rendererRecoveryCapability: 1 << 6
    readonly property int pageFindCapability: 1 << 7
    readonly property int pageZoomCapability: 1 << 8
    readonly property int printingCapability: 1 << 9
    readonly property int siteFullscreenCapability: 1 << 10
    readonly property int inlinePdfViewingCapability: 1 << 11
    readonly property int capabilities: navigationCapability
        | persistentProfilesCapability
        | contentBlockingCapability
        | keyboardPageCommandsCapability
        | developerToolsCapability
        | rendererRecoveryCapability
        | pageFindCapability
        | pageZoomCapability
        | printingCapability
        | siteFullscreenCapability
        | inlinePdfViewingCapability
    property int blockedRequestCount: 0
    property color pageBackgroundColor: "#16151d"
    property var keyboardNavigationConfiguration: ({})
    property string keyboardNavigationScriptSource: ""
    property bool keyboardNavigationHintModeActive: false
    property var editedStateScript: {
        const script = WebEngine.script()
        script.name = "Omaweb edited form state"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = "globalThis.__omawebContentEditableEdited = false;"
            + "document.addEventListener('input', event => {"
            + "if (event.target && event.target.isContentEditable) "
            + "globalThis.__omawebContentEditableEdited = true;"
            + "}, true);"
        return script
    }

    // The inspector Chromium supplies, and the palette it is drawn in. The
    // view is the adapter's to build and destroy; where it sits is the shell's,
    // so the shell takes it as a child once it exists.
    property bool developerToolsAttached: false
    property var developerToolsView: null
    property var developerToolsColors: ({})
    // Whether Chromium is holding a node from a context menu. `InspectElement`
    // reads that node and dereferences it without checking first, so asking for
    // it when no menu has been opened takes the whole browser down. A keyboard
    // request never has one, which is exactly the case that has to be caught.
    property bool contextMenuTargetKnown: false
    // The reader asked to point at something before the inspector had loaded.
    property bool elementPickPending: false
    property int pageGeneration: 0
    property string lastContextMediaType: "none"

    // What the reader is looking for in this page, and where the search has
    // reached. One adapter draws one tab, so this is find belonging to a tab
    // without anything keeping a table of tabs: hiding the interface leaves
    // both standing, and only a navigation takes the matches away.
    property string findQuery: ""
    property int findMatchCount: 0
    property int findActiveMatch: 0

    // How large this tab's page is drawn. The shell owns the value — it is the
    // tab's, and outlives the page in it — and the view is told what it is.
    property alias zoomFactor: webView.zoomFactor

    // Fullscreen the site asked for, which is not the reader asking for the
    // window. The origin is named so the shell can say who took the screen, and
    // the page is told when the reader takes it back.
    property bool siteFullscreenActive: false
    property string siteFullscreenOrigin: ""

    // What the reader pointed at, as plain values rather than as the engine's
    // own request object: a position, the addresses under the pointer, the
    // selection, and whether the target takes typing. Omaweb draws the menu, so
    // the engine's own never appears and nothing about Chromium's menu model
    // crosses this line.
    signal pageContextRequested(var context)
    signal developerToolsClosed()
    signal rendererFailed(string reason)
    signal newTabRequested(var request, url requestedUrl)
    signal auxiliaryWindowRequested(var request, url requestedUrl)
    signal windowCloseRequested()
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(string requestId, string origin, string permission)
    signal browserPromptRequested(string requestId, var prompt)
    signal fileSelectionRequested(string requestId, var selection)
    // The page has been rendered, or it has not. Either way the shell hears
    // about it: a print that produced nothing is not a print that quietly
    // didn't happen.
    signal printFinished(string destination, bool succeeded)
    // The reader dealt with this page themselves. What that earns the origin is
    // the shell's to decide and remember; the adapter only reports it.
    signal userActivated()
    property var pendingPermissions: ({})
    property int nextPermissionRequestId: 0
    property var pendingBrowserPrompts: ({})
    property int nextBrowserPromptId: 0
    property bool javaScriptDialogsBlocked: false
    property var pendingFileSelections: ({})
    property var externalProtocolOrigins: ({})

    function respondToPermission(requestId, decision) {
        const request = pendingPermissions[requestId]
        if (!request) return
        delete pendingPermissions[requestId]
        if (decision === 1 || decision === 2) request.grant()
        else request.deny()
    }

    function respondToBrowserPrompt(requestId, accepted, response) {
        const pending = root.pendingBrowserPrompts[requestId]
        if (!pending) return
        delete root.pendingBrowserPrompts[requestId]
        if (pending.kind.startsWith("javascript-") && response.stopPrompts === true)
            root.javaScriptDialogsBlocked = true
        if (pending.kind === "external-protocol") {
            if (accepted) {
                if (response.remember === true && root.permissionController)
                    root.permissionController.rememberExternalProtocolDecision(
                        pending.origin, pending.scheme)
                ExternalProtocolHandler.open(pending.destination)
            }
        } else if (!accepted) {
            pending.request.dialogReject()
        } else if (pending.kind === "http-authentication") {
            pending.request.dialogAccept(String(response.user || ""),
                String(response.password || ""))
        } else {
            pending.request.dialogAccept(String(response.text || ""))
        }
    }

    function originAddress(url) {
        const match = String(url).match(/^([a-z][a-z0-9+.-]*:\/\/[^/]+)/i)
        return match ? match[1] : String(url)
    }

    function requestExternalProtocol(destination, mainFrame) {
        const address = String(destination)
        const separator = address.indexOf(":")
        const scheme = separator > 0 ? address.substring(0, separator).toLowerCase() : ""
        const rememberedOrigin = root.externalProtocolOrigins[address]
        delete root.externalProtocolOrigins[address]
        const origin = mainFrame ? root.originAddress(webView.url) : String(rememberedOrigin || "")
        if (scheme.length === 0) return
        if (root.permissionController
            && origin.length > 0
            && root.permissionController.externalProtocolAllowed(origin, scheme)) {
            ExternalProtocolHandler.open(address)
            return
        }
        const requestId = String(++root.nextBrowserPromptId)
        const application = ExternalProtocolHandler.applicationName(address)
        const displayOrigin = origin.length > 0 ? origin : "Unknown embedded origin"
        root.pendingBrowserPrompts[requestId] = {
            "kind": "external-protocol",
            "origin": origin,
            "scheme": scheme,
            "destination": address
        }
        root.browserPromptRequested(requestId, {
            "kind": "external-protocol",
            "application": application,
            "scheme": scheme,
            "origin": displayOrigin,
            "destination": address,
            "rememberable": origin.length > 0,
            "message": "Open " + application + "?",
            "detail": scheme + " · " + displayOrigin + " · " + address
        })
    }

    function respondToFileSelection(requestId, files) {
        const request = root.pendingFileSelections[requestId]
        if (!request) return
        delete root.pendingFileSelections[requestId]
        if (files.length === 0) {
            request.dialogReject()
            return
        }
        const paths = []
        for (let index = 0; index < files.length; ++index)
            paths.push(root.localPath(files[index]))
        request.dialogAccept(paths)
    }

    function localPath(fileUrl) {
        let path = decodeURIComponent(String(fileUrl).replace(/^file:\/\//, ""))
        if (Qt.platform.os === "windows" && path.startsWith("/")) path = path.substring(1)
        return path
    }

    function performPageContextAction(action, destination) {
        if (action === "copy-image") {
            webView.triggerWebAction(WebEngineView.CopyImageToClipboard)
            return
        }
        const path = root.localPath(destination)
        if (path.length === 0) return
        const profile = root.resolvedProfile()
        if (!profile || profile.preparedDownloadPath === undefined) return
        profile.preparedDownloadPath = path
        if (action === "save-link")
            webView.triggerWebAction(WebEngineView.DownloadLinkToDisk)
        else if (action === "save-media") {
            const webAction = root.lastContextMediaType === "image"
                ? WebEngineView.DownloadImageToDisk : WebEngineView.DownloadMediaToDisk
            webView.triggerWebAction(webAction)
        }
    }

    // Chromium's inspector is a webpage of its own, and it runs in the profile
    // of the page it inspects: a Private tab's inspector keeps its state in the
    // temporary Private profile and loses it with the session, as the page
    // does. It is built when it is first asked for rather than with the tab —
    // one inspector costs a second renderer process.
    property Component developerToolsComponent: Component {
        WebEngineView {
            objectName: "qtDeveloperToolsView"
            profile: root.resolvedProfile()
            backgroundColor: root.developerToolsBackgroundColor()
            userScripts.collection: [root.developerToolsThemeScript]

            // The frontend's own close button asks its window to close, which
            // is the reader saying they are finished with it.
            onWindowCloseRequested: root.developerToolsClosed()

            onLoadingChanged: function(loadRequest) {
                if (loadRequest.status !== WebEngineView.LoadSucceededStatus) return
                root.applyDeveloperToolsTheme()
                root.pickElement()
            }
        }
    }

    function attachDeveloperTools() {
        if (root.developerToolsAttached) return
        const view = root.developerToolsComponent.createObject(root)
        if (!view) return
        root.developerToolsView = view
        webView.devToolsView = view
        root.developerToolsAttached = true
    }

    function detachDeveloperTools() {
        if (!root.developerToolsAttached) return
        root.developerToolsAttached = false
        root.elementPickPending = false
        webView.devToolsView = null
        const view = root.developerToolsView
        root.developerToolsView = null
        // The shell has taken the view as a child by now, so nothing else
        // would ever take it away.
        if (view) view.destroy()
    }

    // Two ways to name what to inspect, and the reader has already chosen
    // between them. Opening a context menu over a node is Chromium naming it,
    // and `InspectElement` is the action that reads it back. Asking from the
    // keyboard names nothing, so the inspector's own element picker is what
    // answers: the reader points at what they meant, which is what the key does
    // in every other browser.
    function inspectElement() {
        const alreadyOpen = root.developerToolsAttached
        root.attachDeveloperTools()
        if (!root.developerToolsAttached) return
        if (!root.contextMenuTargetKnown) {
            root.elementPickPending = true
            if (alreadyOpen) root.pickElement()
            return
        }
        if (alreadyOpen) {
            webView.triggerWebAction(WebEngineView.InspectElement)
            return
        }
        // The inspector has to exist before the page can be told to reveal a
        // node in it.
        Qt.callLater(function() {
            if (root.developerToolsAttached && root.contextMenuTargetKnown)
                webView.triggerWebAction(WebEngineView.InspectElement)
        })
    }

    // The frontend's own picker, asked for the way its host asks for it. It
    // waits for the frontend to finish loading, because a page that has not run
    // its scripts has no picker to enter.
    function pickElement() {
        if (!root.elementPickPending || !root.developerToolsView) return
        if (root.developerToolsView.loading) return
        root.elementPickPending = false
        root.developerToolsView.runJavaScript(
            "globalThis.DevToolsAPI && globalThis.DevToolsAPI.enterInspectElementMode();")
    }

    // The engine's own enumeration never leaves the adapter; the shell reads a
    // name, and an engine with a kind Omaweb has no name for reports none.
    function mediaTypeName(mediaType) {
        switch (mediaType) {
        case ContextMenuRequest.MediaTypeImage: return "image"
        case ContextMenuRequest.MediaTypeVideo: return "video"
        case ContextMenuRequest.MediaTypeAudio: return "audio"
        case ContextMenuRequest.MediaTypeCanvas: return "canvas"
        case ContextMenuRequest.MediaTypeFile: return "file"
        }
        return "none"
    }

    function requestContextFromFrame(frame, generation) {
        frame.runJavaScript(`(() => {
            if (!document.hasFocus()) return null;
            const target = document.activeElement || document.body;
            const link = target && target.closest ? target.closest('a[href]') : null;
            const media = target && target.closest
                ? target.closest('img[src],video[src],audio[src]') : null;
            const rect = target && target.getBoundingClientRect
                ? target.getBoundingClientRect() : {left: innerWidth / 2, top: innerHeight / 2,
                    width: 0, height: 0};
            return {
                x: Math.round(rect.left + rect.width / 2),
                y: Math.round(rect.top + rect.height / 2),
                selectedText: String(getSelection() || ''),
                linkText: link ? String(link.textContent || '') : '',
                linkUrl: link ? link.href : '',
                mediaUrl: media ? (media.currentSrc || media.src || '') : '',
                mediaType: media ? media.tagName.toLowerCase().replace('img', 'image') : 'none',
                editable: Boolean(target && (target.isContentEditable
                    || /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)))
            };
        })()`, function(context) {
            if (!context || generation !== root.pageGeneration) return
            context.pageGeneration = generation
            root.pageContextRequested(context)
        })
        for (let index = 0; index < frame.children.length; ++index)
            root.requestContextFromFrame(frame.children[index], generation)
    }

    // Qt runs the query inside each frame, so a focused element keeps its own
    // origin's DOM even when the embedding page cannot read it.
    function requestPageContextMenu() {
        root.contextMenuTargetKnown = false
        root.requestContextFromFrame(webView.mainFrame, root.pageGeneration)
    }

    function goBack() { webView.goBack() }
    function goForward() { webView.goForward() }
    function focusPage() { webView.forceActiveFocus() }
    function reloadPage() { webView.reload() }
    // Reading the page again and reading it again from the network are two
    // different asks, and so is stopping: a stopped load leaves the page and
    // everything typed into it exactly where it was.
    function reloadPageBypassingCache() {
        webView.triggerWebAction(WebEngineView.ReloadAndBypassCache)
    }
    function stopLoading() { webView.stop() }

    // The matches, and not the query: a navigation invalidates where the search
    // had reached, but not what the reader was looking for.
    function forgetFindMatches() {
        root.findMatchCount = 0
        root.findActiveMatch = 0
    }

    function findText(query, forward) {
        root.findQuery = String(query)
        if (root.findQuery.length === 0) {
            root.forgetFindMatches()
            webView.findText("")
            return
        }
        webView.findText(root.findQuery, forward ? 0 : WebEngineView.FindBackward)
    }

    function clearFind() {
        root.findQuery = ""
        root.forgetFindMatches()
        webView.findText("")
    }

    function setZoomFactor(factor) {
        const wanted = Number(factor)
        if (!(wanted > 0)) return
        webView.zoomFactor = wanted
    }

    function printPage(destination) {
        const path = String(destination)
        if (path.length === 0) {
            root.printFinished("", false)
            return
        }
        webView.printToPdf(path)
    }

    // Who a request came from, as the reader would recognise them. The engine
    // names the origin as a url, which JavaScript sees as a plain string with
    // no host to read off it, so the authority is taken from the string — the
    // host, and the port where there is one, because two development servers
    // on one host are two different sites.
    function originLabel(origin) {
        const address = String(origin)
        const scheme = address.indexOf("://")
        const authority = (scheme === -1 ? address : address.substring(scheme + 3))
            .split("/")[0]
        return authority.length > 0 ? authority : address
    }

    function exitSiteFullscreen() {
        if (!root.siteFullscreenActive) return
        root.siteFullscreenActive = false
        root.siteFullscreenOrigin = ""
        webView.fullScreenCancelled()
    }
    function configureKeyboardNavigation(configuration) {
        keyboardNavigationHintModeActive = false
        keyboardNavigationConfiguration = configuration
        applyKeyboardNavigationConfiguration()
    }
    function applyKeyboardNavigationConfiguration() {
        if (!webView || !keyboardNavigationConfiguration.version) return
        webView.runJavaScript("globalThis.__omawebKeyboardNavigation && "
            + "globalThis.__omawebKeyboardNavigation.configure("
            + JSON.stringify(keyboardNavigationConfiguration) + ");")
    }
    function refreshBlockedRequestCount() {
        root.blockedRequestCount = root.contentBlocker
            ? root.contentBlocker.blockedRequestCount(root.currentUrl) : 0
    }
    property bool cosmeticRulesInjected: false
    property bool genericCosmeticRulesInjected: false
    readonly property string cosmeticElementId: "__omaweb_content_blocking"
    readonly property string genericCosmeticElementId: "__omaweb_content_blocking_generic"

    // A script that runs before the parser has produced even an <html> element
    // cannot simply append a stylesheet: it waits for the first element to
    // appear, which is still before the page's own scripts run and before
    // anything is painted. Re-application into a document that is already open
    // takes the same path and appends immediately.
    function styleSheetSnippet(elementId, css) {
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
    // channel, and a cache of the sandbox it builds. Omaweb passes an empty
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
        script.name = "Omaweb content blocking"
        script.injectionPoint = WebEngineScript.DocumentCreation
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        // The stylesheet goes first: hiding what the page is about to render
        // does not depend on a scriptlet, and a scriptlet that throws must not
        // take the hiding with it.
        script.sourceCode = (css.length > 0
                ? root.styleSheetSnippet(root.cosmeticElementId, css) + ";\n" : "")
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
        webView.runJavaScript(root.styleSheetSnippet(root.cosmeticElementId, css))
    }

    // The generic rules are the ones written against no particular site, and
    // sending all of them cost a 617 KB stylesheet on every page. The page
    // reports the classes and ids it actually carries, and only the generic
    // rules those could trigger come back. A site with a $generichide
    // exception is surveyed not at all.
    function clearGenericCosmeticRules() {
        if (!genericCosmeticRulesInjected) return
        root.genericCosmeticRulesInjected = false
        webView.runJavaScript(root.styleSheetSnippet(root.genericCosmeticElementId, ""))
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
                    root.styleSheetSnippet(root.genericCosmeticElementId, css))
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
            + "return Boolean(globalThis.__omawebContentEditableEdited);"
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
            storageName: "omaweb-space"
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

    readonly property string developerToolsElementId: "__omaweb_developer_tools"

    // The palette arrives from the shell and may be empty until the theme has
    // loaded, so every colour read here names what to draw with instead.
    function paletteColor(source, name, fallback) {
        const value = source ? source[name] : undefined
        return (value === undefined || String(value).length === 0) ? fallback : String(value)
    }

    function developerToolsColor(name, fallback) {
        return root.paletteColor(root.developerToolsColors, name, fallback)
    }

    function developerToolsSyntaxColor(name, fallback) {
        return root.paletteColor(root.developerToolsColors
            ? root.developerToolsColors.syntax : null, name, fallback)
    }

    function developerToolsBackgroundColor() {
        return root.developerToolsColor("windowOpaque", String(root.pageBackgroundColor))
    }

    // The frontend ships a light face and a dark one and picks between them
    // from a setting Omaweb does not own. Which one is right here is a question
    // about Omaweb's own window, so it is answered from the window's colour and
    // the answer is written onto the frontend's root element.
    function developerToolsDark() {
        const surface = Qt.color(root.developerToolsBackgroundColor())
        return (0.2126 * surface.r + 0.7152 * surface.g + 0.0722 * surface.b) < 0.5
    }

    // Chromium's inspector is a webpage, and its whole design system is custom
    // properties declared on that page's `:root`. Naming them again, later and
    // marked important, is the whole of the theming: no patched frontend, no
    // debugging protocol, and nothing that breaks when the frontend adds a
    // token Omaweb has never heard of.
    //
    // The theme names nine syntax colours and the frontend has twenty-odd
    // tokens, so several tokens share one: they are the constructs that read
    // alike, and a terminal palette has no more hues to tell them apart with.
    // An attribute's value is drawn as the string it is, which is what every
    // editor does and what keeps the inspector reading like the one beside it.
    function developerToolsTokens() {
        const text = root.developerToolsColor("text", "#f3f1fa")
        const muted = root.developerToolsColor("mutedText", "#aaa5b7")
        const body = root.developerToolsBackgroundColor()
        const panel = root.developerToolsColor("sidebarOpaque", body)
        const surface = root.developerToolsColor("surface", "#302e3d")
        const hover = root.developerToolsColor("surfaceHover", "#3d394e")
        const border = root.developerToolsColor("border", "#4a4658")
        const accent = root.developerToolsColor("accent", "#9b87ff")
        const urgent = root.developerToolsColor("urgent", "#e06c75")
        const keyword = root.developerToolsSyntaxColor("keyword", accent)
        const string = root.developerToolsSyntaxColor("string", text)
        const number = root.developerToolsSyntaxColor("number", text)
        const comment = root.developerToolsSyntaxColor("comment", muted)
        const tag = root.developerToolsSyntaxColor("tag", text)
        const attribute = root.developerToolsSyntaxColor("attribute", text)
        const variable = root.developerToolsSyntaxColor("variable", text)
        const method = root.developerToolsSyntaxColor("function", accent)
        const type = root.developerToolsSyntaxColor("type", text)
        const punctuation = root.developerToolsSyntaxColor("punctuation", muted)
        return {
            // The panel bodies, the toolbars above them, and the raised
            // surfaces the frontend stacks on top.
            "--sys-color-cdt-base-container": body,
            "--sys-color-cdt-base": panel,
            "--sys-color-base": panel,
            "--sys-color-base-container": panel,
            "--sys-color-header-container": panel,
            "--sys-color-base-container-elevated": surface,
            "--sys-color-surface": surface,
            "--sys-color-surface1": surface,
            "--sys-color-surface2": surface,
            "--sys-color-surface3": hover,
            "--sys-color-surface4": hover,
            "--sys-color-surface5": hover,
            "--sys-color-surface-variant": hover,
            "--sys-color-neutral-container": surface,
            "--sys-color-omnibox-container": surface,
            "--sys-color-tonal-container": hover,
            // Type on all of them.
            "--sys-color-on-surface": text,
            "--sys-color-on-base": text,
            "--sys-color-on-tonal-container": text,
            "--sys-color-on-surface-subtle": muted,
            "--sys-color-on-surface-secondary": muted,
            "--sys-color-on-surface-primary": accent,
            // Every hairline the frontend draws.
            "--sys-color-divider": border,
            "--sys-color-divider-prominent": border,
            "--sys-color-divider-on-tonal-container": border,
            "--sys-color-on-base-divider": border,
            "--sys-color-outline": border,
            "--sys-color-neutral-outline": border,
            "--sys-color-tonal-outline": border,
            // What the frontend picks out, and what it warns with.
            "--sys-color-primary": accent,
            "--sys-color-primary-bright": accent,
            "--sys-color-state-focus-ring": accent,
            "--sys-color-state-text-highlight": accent,
            "--sys-color-state-on-text-highlight": body,
            "--sys-color-state-hover-on-subtle": hover,
            "--sys-color-state-header-hover": hover,
            "--sys-color-inverse-surface": text,
            "--sys-color-inverse-on-surface": body,
            "--sys-color-error": urgent,
            "--sys-color-error-bright": urgent,
            "--sys-color-error-outline": urgent,
            "--sys-color-on-surface-error": urgent,
            "--sys-color-token-deleted": urgent,
            // Source, markup and stylesheets.
            "--sys-color-token-keyword": keyword,
            "--sys-color-token-atom": number,
            "--sys-color-token-pseudo-element": keyword,
            "--sys-color-token-string": string,
            "--sys-color-token-inserted": string,
            "--sys-color-token-number": number,
            "--sys-color-token-comment": comment,
            "--sys-color-token-meta": comment,
            "--sys-color-token-subtle": punctuation,
            "--sys-color-token-tag": tag,
            "--sys-color-token-attribute": attribute,
            "--sys-color-token-property": method,
            "--sys-color-token-attribute-value": string,
            "--sys-color-token-string-special": string,
            "--sys-color-token-variable": variable,
            "--sys-color-token-property-special": method,
            "--sys-color-token-definition": method,
            "--sys-color-token-builtin": variable,
            "--sys-color-token-variable-special": method,
            "--sys-color-token-type": type,
        }
    }

    // Chromium themes its own inspector through the palette underneath the
    // tokens rather than through the tokens themselves: the frontend links
    // `devtools://theme/colors.css`, which is the browser's own UI theme
    // rendered as a Material tonal ramp, and every one of its two hundred-odd
    // design tokens is a tone of that ramp. Naming only the tokens Omaweb knows
    // about leaves everything else — buttons, badges, selections, the panels
    // Omaweb has never heard of — in Chrome's blues and greys. So the ramp is
    // named too, and the tokens on top of it are the exceptions Omaweb is sure
    // about rather than the whole of the theming.
    //
    // A tone is a lightness: tone 0 is black and tone 100 is white, whatever
    // hue the family carries. So each family keeps its colour's hue and
    // saturation and takes its lightness from the tone.
    readonly property var developerToolsToneLadder: [0, 10, 15, 20, 25, 30, 35, 40, 50, 60,
        70, 80, 90, 94, 95, 98, 99, 100]

    function developerToolsTone(base, tone, saturationCeiling) {
        const colour = Qt.color(base)
        // A grey has no hue to keep, and Qt reports it as none rather than as
        // zero. Asking for a tone of it can only mean a grey of that lightness.
        const achromatic = colour.hslHue < 0
        const hue = achromatic ? 0 : colour.hslHue
        const saturation = achromatic
            ? 0
            : Math.min(colour.hslSaturation,
                saturationCeiling === undefined ? 1 : saturationCeiling)
        return String(Qt.hsla(hue, saturation, tone / 100, 1))
    }

    // Which of Omaweb's colours each of the frontend's palette families is a
    // ramp of. The neutrals carry the window's own tint and little more of it
    // than that, or every surface in the inspector would be tinted twice.
    function developerToolsRamps() {
        const accent = root.developerToolsColor("accent", "#9b87ff")
        const keyword = root.developerToolsSyntaxColor("keyword", accent)
        const window = root.developerToolsBackgroundColor()
        return {
            "neutral": [window, 0.08],
            "neutral-variant": [window, 0.12],
            "primary": [accent, undefined],
            "secondary": [accent, undefined],
            "tertiary": [keyword, undefined],
            "error": [root.developerToolsColor("urgent", "#e06c75"), undefined],
            "blue": [root.developerToolsSyntaxColor("function", accent), undefined],
            "green": [root.developerToolsSyntaxColor("string", accent), undefined],
            "yellow": [root.developerToolsSyntaxColor("number", accent), undefined],
            "orange": [root.developerToolsSyntaxColor("number", accent), undefined],
            "pink": [root.developerToolsSyntaxColor("tag", accent), undefined],
            "purple": [keyword, undefined],
            "indigo": [keyword, undefined],
            "cyan": [root.developerToolsSyntaxColor("type", accent), undefined],
        }
    }

    function developerToolsPaletteDeclarations() {
        const ramps = root.developerToolsRamps()
        let declarations = ""
        for (const family in ramps) {
            const base = ramps[family][0]
            const ceiling = ramps[family][1]
            for (const tone of root.developerToolsToneLadder) {
                declarations += "--ref-palette-" + family + tone + ":"
                    + root.developerToolsTone(base, tone, ceiling) + " !important;"
            }
        }
        return declarations
    }

    function developerToolsStyleSheet() {
        const tokens = root.developerToolsTokens()
        let declarations = root.developerToolsPaletteDeclarations()
        for (const name in tokens) declarations += name + ":" + tokens[name] + " !important;"
        // The frontend names its type per platform, at a selector of its own
        // that an ordinary `:root` rule would lose to, so these are marked as
        // the rest are. Omaweb's whole interface is drawn in one family, and the
        // inspector docked inside it is part of that window: its panel labels
        // take the family too, not only the source it lists. The sizes stay the
        // frontend's own above, because its layout is built around them.
        const font = (root.developerToolsColors && root.developerToolsColors.font) || ({})
        const family = String(font.family || "")
        if (family.length > 0) {
            const quoted = JSON.stringify(family) + ", monospace"
            for (const name of ["--default-font-family", "--monospace-font-family",
                    "--source-code-font-family", "--report-font-family",
                    "--report-font-family-monospace"]) {
                declarations += name + ":" + quoted + " !important;"
            }
        }
        const size = parseInt(font.size, 10)
        if (!isNaN(size) && size > 0) {
            declarations += "--monospace-font-size:" + size + "px !important;"
            declarations += "--source-code-font-size:" + size + "px !important;"
        }
        return ":root{" + declarations + "}"
    }

    // The frontend's DOM tree draws every bracket, equals sign and quote in the
    // same colour as a tag's name, because one class carries both: the name is
    // a span inside the `<...>` the class wraps. An editor keeps structure
    // quiet and lets the content speak, which is most of why the two do not
    // read alike, and no design token can separate them — only a rule can.
    //
    // Those spans live in shadow trees, where a rule in the document cannot
    // reach them. So the frontend's own `attachShadow` is wrapped before any of
    // its scripts run, and every tree it opens from then on adopts one more
    // stylesheet. Nothing is rewritten and nothing is read back: the frontend
    // builds exactly what it would have built, in Omaweb's colours.
    function developerToolsMarkupStyleSheet() {
        const punctuation = root.developerToolsSyntaxColor("punctuation",
            root.developerToolsColor("mutedText", "#aaa5b7"))
        const tag = root.developerToolsSyntaxColor("tag",
            root.developerToolsColor("text", "#f3f1fa"))
        return ".webkit-html-tag{color:" + punctuation + " !important}"
            + ".webkit-html-tag-name,.webkit-html-close-tag-name{color:"
            + tag + " !important}"
    }

    function developerToolsShadowSnippet() {
        return "(() => {"
            + "const css = " + JSON.stringify(root.developerToolsMarkupStyleSheet()) + ";"
            + "const host = globalThis.__omawebDeveloperToolsShadow;"
            + "if (host) { host.sheet.replaceSync(css); return; }"
            + "if (typeof CSSStyleSheet !== 'function') return;"
            + "let sheet;"
            + "try { sheet = new CSSStyleSheet(); sheet.replaceSync(css); }"
            + "catch (error) { return; }"
            + "globalThis.__omawebDeveloperToolsShadow = { sheet: sheet };"
            + "const attachShadow = Element.prototype.attachShadow;"
            + "Element.prototype.attachShadow = function(options) {"
            + "const shadow = attachShadow.call(this, options);"
            + "try { shadow.adoptedStyleSheets = [...shadow.adoptedStyleSheets, sheet]; }"
            + "catch (error) {}"
            + "return shadow;"
            + "};"
            + "})();\n"
    }

    function developerToolsThemeSnippet() {
        return "(() => {"
            + "const element = document.documentElement;"
            + "if (element) element.classList.toggle('theme-with-dark-background', "
            + (root.developerToolsDark() ? "true" : "false") + ");"
            + "})();\n"
            + root.developerToolsShadowSnippet()
            + root.styleSheetSnippet(root.developerToolsElementId,
                root.developerToolsStyleSheet())
    }

    // The frontend has to open in Omaweb's colours rather than arrive in
    // Chromium's and change, so the sheet is in the document before the
    // frontend's own scripts run. It is re-applied on a live theme change,
    // which is the only time the colours move under an open inspector.
    property var developerToolsThemeScript: {
        const script = WebEngine.script()
        script.name = "Omaweb developer tools theme"
        script.injectionPoint = WebEngineScript.DocumentCreation
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = true
        script.sourceCode = root.developerToolsThemeSnippet()
        return script
    }

    function applyDeveloperToolsTheme() {
        if (!root.developerToolsView) return
        root.developerToolsView.runJavaScript(root.developerToolsThemeSnippet())
    }

    onDeveloperToolsColorsChanged: root.applyDeveloperToolsTheme()

    property var keyboardNavigationScript: {
        const script = WebEngine.script()
        script.name = "Omaweb Keyboard navigation"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = root.keyboardNavigationScriptSource
            + "\nglobalThis.__omawebKeyboardNavigation && "
            + "globalThis.__omawebKeyboardNavigation.configure("
            + JSON.stringify(root.keyboardNavigationConfiguration) + ");"
        return script
    }

    // A first-hand gesture on the page, reported once per document. Chromium
    // has its own record of user activation but does not hand it out, and the
    // shell needs it per origin rather than per page, so the page says so.
    property var userActivationScript: {
        const script = WebEngine.script()
        script.name = "Omaweb user activation"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = false
        script.sourceCode = `(() => {
            let reported = false;
            const report = () => {
                if (reported) return;
                reported = true;
                console.info('__omaweb_user_activation__');
            };
            for (const name of ['pointerdown', 'keydown', 'touchstart'])
                document.addEventListener(name, report, {capture: true, passive: true});
        })();`
        return script
    }

    property var externalProtocolOriginScript: {
        const script = WebEngine.script()
        script.name = "Omaweb external protocol origin"
        script.injectionPoint = WebEngineScript.DocumentReady
        script.worldId = WebEngineScript.MainWorld
        script.runsOnSubFrames = true
        script.sourceCode = `document.addEventListener('click', event => {
            const link = event.target && event.target.closest
                ? event.target.closest('a[href]') : null;
            if (!link) return;
            const scheme = String(link.protocol || '').replace(':', '').toLowerCase();
            if (['http', 'https', 'file', 'about', 'data', 'omaweb'].includes(scheme)) return;
            console.info('__omaweb_external_protocol__' + JSON.stringify({
                destination: link.href, origin: location.origin
            }));
        }, true);`
        return script
    }

    WebEngineView {
        id: webView
        objectName: "qtWebView"
        anchors.fill: parent
        profile: root.resolvedProfile()
        // Chromium draws a PDF in a sandboxed viewer of its own, with find,
        // zoom, print and download inside it. Without this the profile
        // downloads the document instead, which is what an engine with no such
        // viewer does.
        settings.pdfViewerEnabled: true
        // A page may ask for the screen. Omaweb answers the request rather than
        // the engine, so the shell can say whose page took it and hand it back.
        settings.fullScreenSupportEnabled: true
        // Opening one selected local document must not give that document a
        // directory-wide read capability. Related resources need a server or
        // an explicit file-selection grant of their own.
        settings.localContentCanAccessFileUrls: false
        settings.localContentCanAccessRemoteUrls: false
        // Chromium paints this before a page supplies its own background.
        // Left at white it flashes a bright rectangle through dark chrome on
        // every navigation, so it follows the theme instead.
        backgroundColor: root.pageBackgroundColor
        focus: true
        userScripts.collection: [root.editedStateScript, root.keyboardNavigationScript,
            root.externalProtocolOriginScript, root.userActivationScript]
        // Chromium's autoplay policy is per view. Requiring a gesture blocks
        // muted autoplay along with audible autoplay, so the shell decides
        // instead: it turns the requirement off once the page has nothing left
        // to gain by starting — either the tab is muted or the reader has
        // already dealt with the origin.
        settings.playbackRequiresUserGesture: !root.autoplayAllowed

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            root.rendererFailed("Renderer stopped with exit code " + exitCode)
        }

        // Chromium keeps the node the menu was opened over, and Omaweb has to
        // know that it has one: nothing on the view reports it, and the action
        // that reads it crashes when there is none. Accepting the request is
        // what stops the engine drawing a menu of its own over Omaweb's.
        onContextMenuRequested: function(request) {
            root.contextMenuTargetKnown = true
            root.lastContextMediaType = root.mediaTypeName(request.mediaType)
            request.accepted = true
            root.pageContextRequested({
                "x": request.position.x,
                "y": request.position.y,
                "selectedText": request.selectedText,
                "linkText": request.linkText,
                "linkUrl": request.linkUrl,
                "mediaUrl": request.mediaUrl,
                "mediaType": root.mediaTypeName(request.mediaType),
                "editable": request.isContentEditable,
                "pageGeneration": root.pageGeneration
            })
        }

        onLoadingChanged: function(loadRequest) {
            root.refreshBlockedRequestCount()
            if (loadRequest.status === WebEngineView.LoadStartedStatus) {
                root.pageGeneration += 1
                root.javaScriptDialogsBlocked = false
                // The node Chromium is holding belonged to the page being
                // replaced. What is at those coordinates now is not what the
                // reader pointed at, so the next keyboard request picks again.
                root.contextMenuTargetKnown = false
                // The matches were in the page being replaced. The query is the
                // reader's and stays, ready to run against what arrives.
                root.forgetFindMatches()
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

        onNavigationRequested: function(request) {
            const address = String(request.url)
            const scheme = address.substring(0, address.indexOf(":")).toLowerCase()
            if (scheme === "http" || scheme === "https" || scheme === "file"
                || scheme === "about" || scheme === "data" || scheme === "omaweb") return
            request.reject()
            root.requestExternalProtocol(request.url, request.isMainFrame)
        }

        onWindowCloseRequested: root.windowCloseRequested()

        onFindTextFinished: function(result) {
            root.findMatchCount = result.numberOfMatches
            root.findActiveMatch = result.activeMatch
        }

        onPdfPrintingFinished: function(filePath, success) {
            root.printFinished(filePath, success)
        }

        // Accepted here and reported to the shell, which is what makes site
        // fullscreen a state Omaweb is in rather than something the engine did
        // to the window behind its back.
        // Accepted last, once Omaweb is in the state it is accepting. Accepting
        // first would leave a page laid out for a screen it has been promised
        // and not given if anything here failed, and the reader looking at a
        // broken page in a window that never changed.
        onFullScreenRequested: function(request) {
            // The origin is named before the state changes: the shell reports
            // who took the screen the moment it hears that someone did.
            root.siteFullscreenOrigin = request.toggleOn
                ? root.originLabel(request.origin) : ""
            root.siteFullscreenActive = request.toggleOn
            request.accept()
        }

        onJavaScriptConsoleMessage: function(level, message, lineNumber, sourceId) {
            if (message.startsWith("__omaweb_external_protocol__")) {
                try {
                    const report = JSON.parse(message.substring(
                        "__omaweb_external_protocol__".length))
                    root.externalProtocolOrigins[String(report.destination)] = String(report.origin)
                } catch (error) {
                    console.warn("Could not read external protocol origin: " + error)
                }
            } else if (message === "__omaweb_user_activation__") {
                root.userActivated()
            } else if (message === "__omaweb_keyboard_hint_mode__:1")
                root.keyboardNavigationHintModeActive = true
            else if (message === "__omaweb_keyboard_hint_mode__:0")
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


        onJavaScriptDialogRequested: function(request) {
            request.accepted = true
            if (root.javaScriptDialogsBlocked) {
                request.dialogReject()
                return
            }
            let suffix = "alert"
            if (request.type === JavaScriptDialogRequest.DialogTypeConfirm) suffix = "confirm"
            else if (request.type === JavaScriptDialogRequest.DialogTypePrompt) suffix = "prompt"
            else if (request.type === JavaScriptDialogRequest.DialogTypeBeforeUnload)
                suffix = "before-unload"
            const requestId = String(++root.nextBrowserPromptId)
            const kind = "javascript-" + suffix
            root.pendingBrowserPrompts[requestId] = {"request": request, "kind": kind}
            root.browserPromptRequested(requestId, {
                "kind": kind,
                "origin": request.securityOrigin.toString(),
                "message": request.message,
                "defaultText": request.defaultText
            })
        }

        onAuthenticationDialogRequested: function(request) {
            request.accepted = true
            const requestId = String(++root.nextBrowserPromptId)
            root.pendingBrowserPrompts[requestId] = {
                "request": request, "kind": "http-authentication"
            }
            root.browserPromptRequested(requestId, {
                "kind": "http-authentication",
                "origin": request.url.toString(),
                "message": "Sign in to " + root.originLabel(request.url),
                "detail": request.realm
            })
        }


        onFileDialogRequested: function(request) {
            request.accepted = true
            const requestId = String(++root.nextBrowserPromptId)
            let mode = "open"
            if (request.mode === FileDialogRequest.FileModeOpenMultiple) mode = "open-multiple"
            else if (request.mode === FileDialogRequest.FileModeUploadFolder) mode = "folder"
            else if (request.mode === FileDialogRequest.FileModeSave) mode = "save"
            root.pendingFileSelections[requestId] = request
            root.fileSelectionRequested(requestId, {
                "mode": mode,
                "mimeTypes": request.acceptedMimeTypes,
                "suggestedName": request.defaultFileName
            })
        }
    }
}
