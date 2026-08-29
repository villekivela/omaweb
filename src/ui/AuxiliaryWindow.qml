import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: auxiliary
    objectName: "auxiliaryWindow"

    required property url engineSource
    required property var openerEngine
    required property var request
    required property url requestedUrl

    width: 720
    height: 640
    minimumWidth: 420
    minimumHeight: 320
    visible: true
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint
    title: engineLoader.item && engineLoader.item.pageTitle.length > 0
        ? engineLoader.item.pageTitle
        : "Tanto"

    onClosing: Qt.callLater(function() { auxiliary.destroy() })

    Loader {
        id: engineLoader
        objectName: "auxiliaryEngineLoader"
        anchors.fill: parent

        Component.onCompleted: setSource(auxiliary.engineSource, {
            "profilePath": auxiliary.openerEngine.profilePath,
            "sharedProfile": auxiliary.openerEngine.browserProfile,
            "currentUrl": auxiliary.request ? "about:blank" : auxiliary.requestedUrl
        })

        onLoaded: {
            if (auxiliary.request) item.acceptNewWindowRequest(auxiliary.request)
            item.focusPage()
        }
    }

    Connections {
        target: engineLoader.item
        ignoreUnknownSignals: true

        function onWindowCloseRequested() { auxiliary.close() }
    }
}
