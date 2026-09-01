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
    readonly property int rendererRecoveryCapability: 1 << 6
    readonly property int capabilities: navigationCapability
        | contentBlockingCapability
        | keyboardPageCommandsCapability
        | rendererRecoveryCapability

    signal rendererFailed(string reason)
    signal newTabRequested(var request, url requestedUrl)
    signal auxiliaryWindowRequested(var request, url requestedUrl)
    signal windowCloseRequested()
    signal backgroundTabRequested(url requestedUrl)
    signal sitePermissionRequested(string requestId, string origin, string permission)
    function respondToPermission(requestId, decision) {}

    onCurrentUrlChanged: pageLocalState = ""

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
    function configureKeyboardNavigation(configuration) {
        keyboardNavigationConfiguration = configuration
    }
    function checkForEditedFormState(callback) { callback(false) }
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
