import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: auxiliary
    objectName: "auxiliaryWindow"

    required property url engineSource
    required property var openerEngine
    required property var request
    required property url requestedUrl
    required property var permissionController
    required property var contentBlocker
    required property var engineContentBlocker
    required property var cookiePolicy
    signal sitePermissionRequested(var responder, string requestId, string origin,
                                   string permission)
    // An Auxiliary window is where an authentication or payment flow finishes,
    // so it is exactly where a certificate failure must not be waved through.
    // It asks the same question of the same rule as an ordinary tab.
    signal certificateErrorRaised(var responder, string requestId, var failure)
    // The core's Agent rules and the id this window has there, when an Agent
    // tab's page opened it (ADR 0051). The Agent drives it by that id as it
    // drives the tab, and it is the Agent's for as long as the core lists it.
    property var agentControl: null
    property string agentWindowId: ""
    readonly property bool agentDriven: auxiliary.agentWindowId.length > 0
                                        && auxiliary.agentControl !== null
                                        && auxiliary.agentControl.agentWindowIds.indexOf(
                                            auxiliary.agentWindowId) >= 0
    onAgentDrivenChanged: auxiliary.markAgentEngine()
    // The window's palette, for the frame an Agent's window wears.
    property var colors
    // The tab that opened this window and the connection driving it, which
    // stay the same for as long as it is the Agent's.
    readonly property var agentWindowInfo: auxiliary.agentDriven
                                           ? auxiliary.agentControl.agentWindow(
                                                 auxiliary.agentWindowId) : ({})
    // What the Agent is doing, as its tab reports it, so the window is marked
    // as its tab is. Before the tab has a report, the connection's name.
    readonly property var agentReport: auxiliary.reportOf(auxiliary.agentDriven,
                                                          auxiliary.agentWindowInfo,
                                                          auxiliary.agentControl
                                                          ? auxiliary.agentControl.agentActivity :
                                                            null)

    function reportOf(driven, info, activity) {
        if (!driven)
            return null;
        const report = activity ? activity[info.openerTabId] : undefined;
        return report || {
            "name": String(info.connection || ""),
            "act": ""
        };
    }

    function markAgentEngine() {
        const engine = engineLoader.item;
        if (!engine || engine.agentOwned === undefined)
            return;
        engine.agentOwned = auxiliary.agentDriven;
        engine.agentDownloadDirectory = auxiliary.agentDriven ? String(
                                                                    auxiliary.agentControl.agentWindow(
                                                                        auxiliary.agentWindowId).downloadDirectory
                                                                    || "") : "";
    }

    width: 720
    height: 640
    minimumWidth: 420
    minimumHeight: 320
    visible: true
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: engineLoader.item && engineLoader.item.pageTitle.length > 0 ? engineLoader.item.pageTitle :
                                                                         "Omaweb"

    onClosing: {
        if (auxiliary.agentControl && auxiliary.agentWindowId.length > 0)
            auxiliary.agentControl.windowClosed(auxiliary.agentWindowId);
        Qt.callLater(function () {
            auxiliary.destroy();
        });
    }

    Loader {
        id: engineLoader
        objectName: "auxiliaryEngineLoader"
        anchors.fill: parent

        Component.onCompleted: setSource(auxiliary.engineSource, {
                                             "profilePath": auxiliary.openerEngine.profilePath,
                                             "sharedProfile": auxiliary.openerEngine.browserProfile,
                                             "permissionController": auxiliary.permissionController,
                                             "spaceId": auxiliary.openerEngine.spaceId,
                                             "contentBlocker": auxiliary.contentBlocker,
                                             "engineContentBlocker": auxiliary.engineContentBlocker,
                                             "engineCookiePolicy": auxiliary.cookiePolicy,
                                             "keyboardNavigationConfiguration": Object.assign({},
                                                                                              keyboardNavigation.configurationForUrl(
                                                                                                  auxiliary.requestedUrl),
                                                                                              {
                                                                                                  "hintTheme":
                                                                                                  auxiliary.openerEngine.keyboardNavigationConfiguration.hintTheme
                                                                                              }),
                                             "keyboardNavigationScriptSource":
                                             keyboardNavigation.pageScript,
                                             "currentUrl": auxiliary.request ? "about:blank" :
                                                                               auxiliary.requestedUrl,
                                             // An Agent's window never takes the reader's keyboard.
                                             "pageTakesFocus": auxiliary.agentWindowId.length === 0
                                         })

        onLoaded: {
            auxiliary.markAgentEngine();
            if (auxiliary.request)
                item.acceptNewWindowRequest(auxiliary.request);
            if (auxiliary.agentWindowId.length === 0)
                item.focusPage();
        }
    }

    // An Agent's window is marked as its tab's page is: framed in the Agent
    // accent, with who is driving it and what it did last.
    AgentPageFrame {
        objectName: "auxiliaryAgentFrame"
        anchors.fill: parent
        z: 1
        visible: auxiliary.agentDriven && auxiliary.colors !== undefined
        colors: auxiliary.colors
        agent: auxiliary.agentReport
    }

    Connections {
        target: engineLoader.item
        ignoreUnknownSignals: true

        function onWindowCloseRequested() {
            auxiliary.close();
        }
        function onSitePermissionRequested(requestId, origin, permission) {
            auxiliary.sitePermissionRequested(engineLoader.item, requestId, origin, permission);
        }

        function onCertificateErrorRaised(requestId, failure) {
            auxiliary.certificateErrorRaised(engineLoader.item, requestId, failure);
        }

        function onAgentVerbAnswered(requestId, answer) {
            if (auxiliary.agentControl)
                auxiliary.agentControl.answerPage(requestId, answer);
        }

        function onPageConsoleMessage(level, message, lineNumber, sourceId, document) {
            if (auxiliary.agentDriven)
                auxiliary.agentControl.recordConsoleMessage(auxiliary.agentWindowId, String(
                                                                document), level, message, sourceId,
                                                            lineNumber);
        }
        function onPageGenerationChanged() {
            if (auxiliary.agentDriven)
                auxiliary.agentControl.startConsoleDocument(auxiliary.agentWindowId, String(
                                                                engineLoader.item.pageGeneration));
        }
    }

    Connections {
        target: auxiliary.agentControl
        ignoreUnknownSignals: true

        function onPageRequested(requestId, request) {
            if (request.tabId !== auxiliary.agentWindowId)
                return;
            const engine = engineLoader.item;
            if (!engine) {
                auxiliary.agentControl.answerPage(requestId, {
                                                      "ok": false,
                                                      "code": "no-page",
                                                      "error": "The window has no page yet."
                                                  });
                return;
            }
            engine.agentDownloadDirectory = String(request.downloadDirectory || "");
            engine.answerAgentVerb(requestId, request.verb, request.arguments);
        }

        function onPageRequestsCancelled() {
            if (engineLoader.item)
                engineLoader.item.cancelAgentVerbs();
        }

        function onPageRequestsCancelledIn(targetIds) {
            if (engineLoader.item && targetIds.indexOf(auxiliary.agentWindowId) >= 0)
                engineLoader.item.cancelAgentVerbs();
        }

        function onWindowCloseRequested(windowId) {
            if (windowId === auxiliary.agentWindowId)
                auxiliary.close();
        }
    }
}
