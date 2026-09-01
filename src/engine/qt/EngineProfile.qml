import QtQuick
import QtWebEngine

QtObject {
    id: root

    property string profilePath: ""
    property var engineContentBlocker: null
    property string downloadDirectory: ""
    property bool acceptDownloads: false
    property bool privateBrowsing: true
    property string downloadNamespace: ""
    property int activeDownloadCount: 0
    property bool retired: false
    readonly property var profile: privateProfile
    signal downloadStarted(string runtimeId, url sourceUrl, string path, string state,
        double receivedBytes, double totalBytes)
    signal downloadUpdated(string runtimeId, string state, double receivedBytes,
        double totalBytes, string error)

    function retire() {
        retired = true
        if (activeDownloadCount === 0) root.destroy()
    }

    property Component downloadObserver: Component {
        QtObject {
            id: observer
            required property var download
            required property string downloadNamespace
            property bool finished: false

            function stateName() {
                switch (download.state) {
                case WebEngineDownloadRequest.DownloadRequested: return "requested"
                case WebEngineDownloadRequest.DownloadInProgress: return "in-progress"
                case WebEngineDownloadRequest.DownloadCompleted: return "completed"
                case WebEngineDownloadRequest.DownloadCancelled: return "cancelled"
                case WebEngineDownloadRequest.DownloadInterrupted: return "interrupted"
                default: return "unknown"
                }
            }

            function path() {
                return download.downloadDirectory + "/" + download.downloadFileName
            }

            function updateRecord() {
                root.downloadUpdated(downloadNamespace + ":" + String(download.id), stateName(),
                    download.receivedBytes, download.totalBytes,
                    download.interruptReasonString || "")
                if (download.isFinished && !finished) {
                    finished = true
                    root.activeDownloadCount -= 1
                    if (root.retired && root.activeDownloadCount === 0) root.destroy()
                    observer.destroy()
                }
            }

            Component.onCompleted: {
                root.downloadStarted(downloadNamespace + ":" + String(download.id), download.url,
                    path(), stateName(), download.receivedBytes, download.totalBytes)
            }

            property Connections downloadConnections: Connections {
                target: observer.download
                function onStateChanged() { observer.updateRecord() }
                function onReceivedBytesChanged() { observer.updateRecord() }
                function onTotalBytesChanged() { observer.updateRecord() }
                function onInterruptReasonChanged() { observer.updateRecord() }
            }
        }
    }

    property WebEngineProfile privateProfile: WebEngineProfile {
        storageName: root.privateBrowsing ? "tanto-private" : "tanto-space"
        // A QML-declared profile is off-the-record by default, whatever its
        // storage name and cookie policy say: an off-the-record one keeps
        // everything in memory and silently downgrades the cookie policy to
        // NoPersistentCookies, so a login lasts only as long as the process.
        // Declared after the storage name, which the switch to disk needs.
        offTheRecord: root.privateBrowsing
        persistentStoragePath: root.profilePath
        cachePath: root.profilePath + "/cache"
        persistentCookiesPolicy: root.privateBrowsing
            ? WebEngineProfile.NoPersistentCookies
            : WebEngineProfile.ForcePersistentCookies
        httpCacheType: root.privateBrowsing
            ? WebEngineProfile.MemoryHttpCache
            : WebEngineProfile.DiskHttpCache

        onDownloadRequested: function(download) {
            if (!root.acceptDownloads) return
            if (root.downloadDirectory.length > 0)
                download.downloadDirectory = root.downloadDirectory
            if (download.downloadFileName.length === 0)
                download.downloadFileName = download.suggestedFileName
            root.activeDownloadCount += 1
            root.downloadObserver.createObject(root, {
                "download": download,
                "downloadNamespace": root.downloadNamespace
            })
            download.accept()
        }
    }

    Component.onCompleted: {
        if (root.engineContentBlocker)
            root.engineContentBlocker.attachToProfile(root.profile)
    }
}
