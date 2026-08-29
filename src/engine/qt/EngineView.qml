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
        profile: spaceProfile
        backgroundColor: "white"
        focus: true
        userScripts.collection: [root.editedStateScript]

        onRenderProcessTerminated: function(terminationStatus, exitCode) {
            root.rendererFailed("Renderer stopped with exit code " + exitCode)
        }
    }
}
