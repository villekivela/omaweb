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
    readonly property int contentBlockingCapability: 1 << 3
    readonly property int keyboardPageCommandsCapability: 1 << 4
    readonly property int developerToolsCapability: 1 << 5
    readonly property int rendererRecoveryCapability: 1 << 6
    readonly property int pageFindCapability: 1 << 7
    readonly property int pageZoomCapability: 1 << 8
    readonly property int printingCapability: 1 << 9
    readonly property int siteFullscreenCapability: 1 << 10
    readonly property int inlinePdfViewingCapability: 1 << 11
    // The lab is also the engine that cannot do everything. Each everyday page
    // operation is switchable on its own, because the shell has to be
    // reviewable against an adapter that reports the gap rather than only
    // against one that answers for everything.
    property bool findAvailable: true
    property bool zoomAvailable: true
    property bool printingAvailable: true
    property bool siteFullscreenAvailable: true
    property bool inlinePdfViewingAvailable: true
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

    signal pageContextRequested(var context)
    signal developerToolsClosed()
    signal rendererFailed(string reason)
    signal newTabRequested(var request, url requestedUrl)
    signal auxiliaryWindowRequested(var request, url requestedUrl)
    signal windowCloseRequested()
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(string requestId, string origin, string permission)
    signal printFinished(string destination, bool succeeded)
    function respondToPermission(requestId, decision) {}

    onCurrentUrlChanged: {
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
        })
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
            text: "Tanto UI lab"
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
