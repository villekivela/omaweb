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
    signal sitePermissionRequested(var responder, string requestId, string origin,
                                   string permission)
    // An Auxiliary window is where an authentication or payment flow finishes,
    // so it is exactly where a certificate failure must not be waved through.
    // It asks the same question of the same rule as an ordinary tab.
    signal certificateErrorRaised(var responder, string requestId, var failure)

    width: 720
    height: 640
    minimumWidth: 420
    minimumHeight: 320
    visible: true
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: engineLoader.item && engineLoader.item.pageTitle.length > 0 ? engineLoader.item.pageTitle :
                                                                         "Omaweb"

    onClosing: Qt.callLater(function () {
        auxiliary.destroy();
    })

    Loader {
        id: engineLoader
        objectName: "auxiliaryEngineLoader"
        anchors.fill: parent

        Component.onCompleted: setSource(auxiliary.engineSource, {
                                             "profilePath": auxiliary.openerEngine.profilePath,
                                             "sharedProfile": auxiliary.openerEngine.browserProfile,
                                             "permissionController": auxiliary.permissionController,
                                             "contentBlocker": auxiliary.contentBlocker,
                                             "engineContentBlocker": auxiliary.engineContentBlocker,
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
                                                                               auxiliary.requestedUrl
                                         })

        onLoaded: {
            if (auxiliary.request)
                item.acceptNewWindowRequest(auxiliary.request);
            item.focusPage();
        }
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
    }
}
