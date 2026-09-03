import QtQuick

Rectangle {
    id: root

    property url currentUrl: "about:blank"
    property string pageTitle: currentUrl.toString() === "about:blank" ? "New tab" : currentUrl.toString()
    // The lab runs no engine and so has no icon store. Handing every host one
    // of the drawn stand-ins is what makes the sidebar's chips reviewable
    // without the QtWebEngine build.
    property url pageIconUrl: {
        if (!mockFaviconUrls || mockFaviconUrls.length === 0)
            return ""
        const address = String(root.currentUrl)
        if (address.length === 0 || address.startsWith("about:"))
            return ""
        let hash = 0
        for (let index = 0; index < address.length; ++index)
            hash = (hash * 31 + address.charCodeAt(index)) % 104729
        return mockFaviconUrls[hash % mockFaviconUrls.length]
    }
    property bool loading: false
    // The lab plays nothing, so both sides of the tab's speaker are set by
    // hand: `simulateAudible` stands in for a page that started making sound.
    property bool pageAudible: false
    property bool audioMuted: false
    // The lab starts no page and no process, so both sides of autoplay are set
    // by hand: `autoplayAllowed` is what the shell decides, and
    // `simulateAutoplay` stands in for a page that tried to start on its own —
    // which it may do silently, and is heard only where the shell has not
    // silenced the tab.
    property bool autoplayAllowed: false
    property int autoplayBlockedCount: 0
    // No renderer, so no process to account for. The lab reports a pid a test
    // can name rather than pretending to have one.
    property int renderProcessPid: 0
    property bool canGoBack: false
    property bool canGoForward: false
    property string pageLocalState: ""
    property string profilePath: ""
    property var sharedProfile: null
    property var contentBlocker: null
    property var engineContentBlocker: null
    property var permissionController: null
    readonly property var browserProfile: root.sharedProfile ? root.sharedProfile : root
    readonly property int blockedRequestCount: contentBlocker
        ? contentBlocker.blockedRequestCount(currentUrl) : 0
    property var keyboardNavigationConfiguration: ({})
    property string keyboardNavigationScriptSource: ""
    property bool keyboardNavigationHintModeActive: false
    property string keyboardInput: ""
    property color pageBackgroundColor: "#16151d"
    readonly property bool pageHasFocus: root.activeFocus
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
    readonly property int certificateDecisionsCapability: 1 << 12
    readonly property int thirdPartyCookieControlCapability: 1 << 13
    // The lab is also the engine that cannot do everything. Each everyday page
    // operation is switchable on its own, because the shell has to be
    // reviewable against an adapter that reports the gap rather than only
    // against one that answers for everything.
    property bool findAvailable: true
    property bool zoomAvailable: true
    property bool printingAvailable: true
    property bool siteFullscreenAvailable: true
    property bool inlinePdfViewingAvailable: true
    // The two parts of a site's security contract an engine can be missing.
    // Each is switchable on its own so the shell can be reviewed against an
    // adapter that says it cannot report a certificate failure, and against one
    // that cannot refuse a third-party cookie.
    property bool certificateDecisionsAvailable: true
    property bool thirdPartyCookieControlAvailable: true
    // Off by default and honest about it: the lab keeps nothing on disk, so
    // Site information has no size to show. A test that needs the shell stood
    // up against an engine that does keep a profile turns it on.
    property bool persistentProfilesAvailable: false
    readonly property int capabilities: navigationCapability
        | contentBlockingCapability
        | keyboardPageCommandsCapability
        | (root.inspectorAvailable ? developerToolsCapability : 0)
        | rendererRecoveryCapability
        | (root.findAvailable ? pageFindCapability : 0)
        | (root.zoomAvailable ? pageZoomCapability : 0)
        | (root.printingAvailable ? printingCapability : 0)
        | (root.siteFullscreenAvailable ? siteFullscreenCapability : 0)
        | (root.inlinePdfViewingAvailable ? inlinePdfViewingCapability : 0)
        | (root.persistentProfilesAvailable ? persistentProfilesCapability : 0)
        | (root.certificateDecisionsAvailable ? certificateDecisionsCapability : 0)
        | (root.thirdPartyCookieControlAvailable ? thirdPartyCookieControlCapability : 0)

    // The lab renders nothing, so find counts the plain occurrences of the
    // query in a body of text a test names. That is enough for the interface:
    // the bar reads a count and a position, and neither cares what drew them.
    property string pageText: ""
    property string findQuery: ""
    property int findMatchCount: 0
    property int findActiveMatch: 0
    property real zoomFactor: 1.0
    property bool siteFullscreenActive: false
    property string siteFullscreenOrigin: ""

    // The lab reaches no network, so what a connection is has to be named. The
    // address alone is what the adapter would know: an https page is reached
    // securely unless a certificate failure has been reported for its origin,
    // and anything that is not http or https is Omaweb's own furniture.
    property string certificateErrorOrigin: ""
    property bool lastLoadFailed: false
    readonly property string connectionState: {
        const address = String(root.currentUrl)
        const separator = address.indexOf("://")
        const scheme = separator === -1 ? "" : address.substring(0, separator).toLowerCase()
        if (root.certificateErrorOrigin.length > 0
            && root.certificateErrorOrigin === root.originLabel(root.currentUrl))
            return "certificate-error"
        // A page that never arrived is not a page reached over an unencrypted
        // connection: there is no connection to report either way.
        if (root.lastLoadFailed) return "internal"
        if (scheme !== "http" && scheme !== "https") return "internal"
        return scheme === "https" ? "secure" : "insecure"
    }
    // The lab enables no override, and says so the same way the engine adapter
    // does rather than by being trusted not to.
    readonly property bool insecureContentBlocked: true

    function originLabel(origin) {
        const address = String(origin)
        const scheme = address.indexOf("://")
        const authority = (scheme === -1 ? address : address.substring(scheme + 3))
            .split("/")[0]
        return authority.length > 0 ? authority : address
    }

    signal certificateErrorRaised(string requestId, var failure)
    signal pageSiteDataCleared(string origin, var cleared, string error)
    // The lab has no storage to empty, so what a page would have reported is
    // named. An engine whose page cannot answer reports that instead.
    property var pageSiteData: ["local storage", "databases"]
    property string pageSiteDataRefusal: ""
    property int pageSiteDataClearCount: 0
    function clearPageSiteData() {
        const address = String(root.currentUrl)
        if (address.length === 0 || address.startsWith("about:")) {
            root.pageSiteDataCleared("", [], "there is no page to clear")
            return
        }
        root.pageSiteDataClearCount += 1
        root.pageSiteDataCleared(root.originLabel(root.currentUrl),
            root.pageSiteDataRefusal.length > 0 ? [] : root.pageSiteData,
            root.pageSiteDataRefusal)
    }
    property var pendingCertificateErrors: ({})
    property int nextCertificateErrorId: 0
    property var certificateDecisions: ({})

    // The lab has no certificate to fail, so a failure is named. An engine
    // that cannot report one reports nothing at all, which is what the
    // capability being off has to mean.
    function simulateCertificateError(failure) {
        if (!root.certificateDecisionsAvailable) return ""
        const named = failure || ({})
        const url = String(named.url !== undefined ? named.url : root.currentUrl)
        const requestId = String(++root.nextCertificateErrorId)
        const mainFrame = named.mainFrame !== false
        if (mainFrame) root.certificateErrorOrigin = root.originLabel(url)
        root.pendingCertificateErrors[requestId] = {"url": url, "mainFrame": mainFrame}
        root.certificateErrorRaised(requestId, {
            "url": url,
            "origin": root.originLabel(url),
            "description": String(named.description !== undefined
                ? named.description : "The certificate could not be verified"),
            "overridable": named.overridable !== false,
            "mainFrame": mainFrame,
            "fatal": named.fatal === true
        })
        return requestId
    }

    function respondToCertificateError(requestId, accepted) {
        const pending = root.pendingCertificateErrors[requestId]
        if (!pending) return
        delete root.pendingCertificateErrors[requestId]
        root.certificateDecisions[requestId] = accepted === true
        // A refused failure is a load that never arrived; an accepted one is
        // this load let through, and the connection stays in error either way.
        if (!accepted && pending.mainFrame) root.lastLoadFailed = true
    }

    // The lab runs no engine and so has no inspector. It reports the capability
    // and hands back a drawn stand-in, which is what makes the dock, its width
    // and the tab it follows reviewable without the QtWebEngine build.
    // `inspectorAvailable` is the other engine the contract has to answer for:
    // one that supplies no inspector at all, whose command must be unavailable
    // rather than offering a dock nothing can fill.
    property bool inspectorAvailable: true
    property bool developerToolsAttached: false
    property var developerToolsView: null
    property var developerToolsColors: ({})
    // How many times the page's own target was asked for, so a test can tell
    // opening the dock from inspecting through it.
    property int inspectedElementCount: 0
    property int contextMenuRequestCount: 0
    property int pageGeneration: 0

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
    signal printFinished(string destination, bool succeeded)
    signal userActivated()
    function simulateUserActivation() { root.userActivated() }
    // A page trying to play of its own accord. It gets the sound only where the
    // shell has already said it may.
    function simulateAutoplay() {
        if (!root.autoplayAllowed) {
            root.autoplayBlockedCount += 1
            return false
        }
        root.pageAudible = !root.audioMuted
        return true
    }
    // The lab has no capability to ask for, so a request is named. The engine's
    // part is to ask and to hear the answer; which answers may be remembered is
    // the core's, and the request goes straight through where one already was.
    property int nextPermissionRequestId: 0
    property var permissionAnswers: ({})
    property int permissionsSettledWithoutAsking: 0
    function simulateSitePermission(origin, permission) {
        const decision = root.permissionController
            ? root.permissionController.permissionDecision(origin, permission) : 0
        if (decision !== 0) {
            root.permissionsSettledWithoutAsking += 1
            return ""
        }
        const requestId = String(++root.nextPermissionRequestId)
        root.sitePermissionRequested(requestId, String(origin), String(permission))
        return requestId
    }
    function respondToPermission(requestId, decision) {
        root.permissionAnswers[requestId] = decision
    }
    property bool javaScriptDialogsBlocked: false
    property bool lastPromptAccepted: false
    property var lastPromptResponse: ({})
    property int nextBrowserPromptId: 0
    property var pendingExternalProtocols: ({})
    property int externalOpenCount: 0
    property var lastSelectedFiles: []
    property bool fileSelectionCancelled: false
    property string lastContextAction: ""
    property string lastContextDestination: ""
    function respondToBrowserPrompt(requestId, accepted, response) {
        root.lastPromptAccepted = accepted
        root.lastPromptResponse = response
        if (response.stopPrompts === true) root.javaScriptDialogsBlocked = true
        const external = root.pendingExternalProtocols[requestId]
        if (external) {
            delete root.pendingExternalProtocols[requestId]
            if (accepted) {
                if (response.remember === true && root.permissionController)
                    root.permissionController.rememberExternalProtocolDecision(
                        external.origin, external.scheme)
                root.externalOpenCount += 1
            }
        }
    }
    function simulateJavaScriptPrompt(type, origin, message, defaultText) {
        if (root.javaScriptDialogsBlocked) return
        root.browserPromptRequested(String(++root.nextBrowserPromptId), {
            "kind": "javascript-" + type,
            "origin": String(origin),
            "message": String(message),
            "defaultText": String(defaultText || "")
        })
    }
    function simulateHttpAuthentication(origin, realm) {
        root.browserPromptRequested(String(++root.nextBrowserPromptId), {
            "kind": "http-authentication",
            "origin": String(origin),
            "message": "Sign in to " + String(origin),
            "detail": String(realm)
        })
    }
    function simulateExternalProtocol(application, destination) {
        const address = String(destination)
        const scheme = address.substring(0, address.indexOf(":"))
        if (root.permissionController
            && root.permissionController.externalProtocolAllowed(root.currentUrl, scheme)) {
            root.externalOpenCount += 1
            return
        }
        const requestId = String(++root.nextBrowserPromptId)
        const page = String(root.currentUrl)
        const match = page.match(/^([a-z][a-z0-9+.-]*:\/\/[^/]+)/i)
        const origin = match ? match[1] : page
        root.pendingExternalProtocols[requestId] = {
            "origin": origin, "scheme": scheme, "destination": address
        }
        root.browserPromptRequested(requestId, {
            "kind": "external-protocol",
            "application": String(application),
            "scheme": scheme,
            "origin": origin,
            "destination": address,
            "message": "Open " + String(application) + "?",
            "detail": scheme + " · " + origin + " · " + address
        })
    }
    function simulateFileSelection(mode, mimeTypes) {
        root.fileSelectionRequested(String(++root.nextBrowserPromptId), {
            "mode": String(mode), "mimeTypes": mimeTypes || [], "suggestedName": ""
        })
    }
    function respondToFileSelection(requestId, files) {
        root.lastSelectedFiles = files
        root.fileSelectionCancelled = files.length === 0
    }
    function performPageContextAction(action, destination) {
        root.lastContextAction = String(action)
        root.lastContextDestination = String(destination)
    }

    onCurrentUrlChanged: {
        root.pageGeneration += 1
        root.javaScriptDialogsBlocked = false
        root.lastLoadFailed = false
        // The failure belonged to the origin being left, and nothing wrote the
        // exception down, so arriving somewhere else clears the report.
        if (root.certificateErrorOrigin.length > 0
            && root.certificateErrorOrigin !== root.originLabel(root.currentUrl)) {
            root.certificateErrorOrigin = ""
        }
        pageLocalState = ""
        // The matches were in the page that has just been replaced. The query
        // is the reader's and stays.
        root.forgetFindMatches()
    }

    color: root.pageBackgroundColor

    Keys.onPressed: function(event) {
        if (event.text.length > 0) root.keyboardInput += event.text.toLowerCase()
    }

    function goBack() {}
    function goForward() {}
    function focusPage() { root.forceActiveFocus() }
    function reloadPage() {
        loading = true
        settle.restart()
    }
    function reloadPageBypassingCache() {
        root.bypassedCacheCount += 1
        root.reloadPage()
    }
    function stopLoading() {
        root.stoppedLoadCount += 1
        root.loading = false
        settle.stop()
    }
    // How many times the page was asked to be read again from the network, and
    // to stop reading: three asks that look alike from outside have to be told
    // apart from inside.
    property int bypassedCacheCount: 0
    property int stoppedLoadCount: 0

    function forgetFindMatches() {
        root.findMatchCount = 0
        root.findActiveMatch = 0
    }

    function findText(query, forward) {
        root.findQuery = String(query)
        if (root.findQuery.length === 0) {
            root.forgetFindMatches()
            return
        }
        const haystack = root.pageText.toLowerCase()
        const needle = root.findQuery.toLowerCase()
        let matches = 0
        for (let at = haystack.indexOf(needle); at !== -1;
                at = haystack.indexOf(needle, at + 1)) {
            matches += 1
        }
        root.findMatchCount = matches
        if (matches === 0) {
            root.findActiveMatch = 0
            return
        }
        const step = forward ? 1 : -1
        const next = root.findActiveMatch === 0
            ? (forward ? 1 : matches)
            : ((root.findActiveMatch - 1 + step + matches) % matches) + 1
        root.findActiveMatch = next
    }

    function clearFind() {
        root.findQuery = ""
        root.forgetFindMatches()
    }

    function setZoomFactor(factor) {
        const wanted = Number(factor)
        if (!(wanted > 0)) return
        root.zoomFactor = wanted
    }

    // The lab has no printing system, so rendering is reported rather than
    // done: what the shell has to get right is naming a destination and hearing
    // the answer.
    function printPage(destination) {
        const path = String(destination)
        root.printFinished(path, path.length > 0)
    }

    function exitSiteFullscreen() {
        if (!root.siteFullscreenActive) return
        root.siteFullscreenActive = false
        root.siteFullscreenOrigin = ""
    }

    // The lab has no page to ask for the screen, so a request is named.
    function simulateSiteFullscreen(origin) {
        // The origin is named before the state changes: the shell reports who
        // took the screen the moment it hears that someone did.
        const named = String(origin || "")
        root.siteFullscreenOrigin = named
        root.siteFullscreenActive = named.length > 0
    }
    function configureKeyboardNavigation(configuration) {
        keyboardNavigationConfiguration = configuration
    }
    function checkForEditedFormState(callback) { callback(false) }
    function attachDeveloperTools() {
        if (!root.inspectorAvailable || root.developerToolsAttached) return
        const view = root.mockDeveloperToolsComponent.createObject(root)
        if (!view) return
        root.developerToolsView = view
        root.developerToolsAttached = true
    }
    function detachDeveloperTools() {
        if (!root.developerToolsAttached) return
        root.developerToolsAttached = false
        const view = root.developerToolsView
        root.developerToolsView = null
        if (view) view.destroy()
    }
    function inspectElement() {
        if (!root.inspectorAvailable) return
        root.attachDeveloperTools()
        root.inspectedElementCount += 1
    }
    function simulateDeveloperToolsClose() { root.developerToolsClosed() }
    // The lab has no engine to right-click, so a context is handed over by
    // name. Anything the caller leaves out is what a click on bare page
    // background reports.
    function simulateContextMenu(context) {
        const named = context || ({})
        root.pageContextRequested({
            "x": named.x !== undefined ? named.x : 40,
            "y": named.y !== undefined ? named.y : 30,
            "selectedText": named.selectedText !== undefined ? named.selectedText : "",
            "linkText": named.linkText !== undefined ? named.linkText : "",
            "linkUrl": named.linkUrl !== undefined ? named.linkUrl : "",
            "mediaUrl": named.mediaUrl !== undefined ? named.mediaUrl : "",
            "mediaType": named.mediaType !== undefined ? named.mediaType : "none",
            "editable": named.editable === true
            , "pageGeneration": root.pageGeneration
        })
    }
    function requestPageContextMenu() {
        root.contextMenuRequestCount += 1
        root.simulateContextMenu({"x": root.width / 2, "y": root.height / 2})
    }

    property Component mockDeveloperToolsComponent: Component {
        Rectangle {
            objectName: "mockDeveloperToolsView"
            color: root.developerToolsColors.windowOpaque !== undefined
                ? root.developerToolsColors.windowOpaque : "#16151d"

            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "No inspector: this engine is the UI lab's stand-in"
                color: root.developerToolsColors.mutedText !== undefined
                    ? root.developerToolsColors.mutedText : "#aaa5b7"
                font.pixelSize: 12
            }
        }
    }
    function acceptNewWindowRequest(request) {
        if (request && request.requestedUrl) currentUrl = request.requestedUrl
    }
    function simulateAudible(audible) { root.pageAudible = audible }
    function simulateRendererFailure() {
        rendererFailed("Renderer exited unexpectedly")
    }
    function simulateNewWindowRequest(requestedUrl, auxiliary) {
        const request = {"requestedUrl": requestedUrl}
        if (auxiliary)
            auxiliaryWindowRequested(request, requestedUrl)
        else
            newTabRequested(request, requestedUrl)
    }
    function simulateBackgroundTabRequest(requestedUrl) {
        backgroundTabRequested(requestedUrl)
    }
    function simulateWindowCloseRequest() { windowCloseRequested() }

    Timer {
        id: settle
        interval: 180
        onTriggered: root.loading = false
    }

    Column {
        anchors.centerIn: parent
        spacing: 10

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Omaweb UI lab"
            color: "#25232b"
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentUrl.toString()
            color: "#696473"
            font.pixelSize: 14
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "No browser engine is running"
            color: "#918a9b"
            font.pixelSize: 12
        }
    }
}
