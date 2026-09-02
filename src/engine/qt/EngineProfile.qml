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
    property bool downloadObserversConnected: false
    property bool notificationObserversConnected: false
    readonly property var profile: privateProfile
    signal downloadStarted(string runtimeId, url sourceUrl, string path, string state,
        double receivedBytes, double totalBytes)
    signal downloadUpdated(string runtimeId, string state, double receivedBytes,
        double totalBytes, string error)
    // A notification arrives from a Space's profile rather than from one page,
    // so the origin is all there is to say who sent it. The shell decides
    // whether that origin has a tab entitled to interrupt, presents the
    // desktop's own notification, and asks for it back by id.
    signal notificationPresented(string notificationId, url origin, string title,
        string message)
    property var pendingNotifications: ({})
    property int nextNotificationId: 0

    // The reader answered the notification: the page hears the click, and the
    // shell takes them to the tab.
    function activateNotification(notificationId) {
        const notification = root.pendingNotifications[notificationId]
        if (!notification) return
        delete root.pendingNotifications[notificationId]
        notification.click()
        notification.close()
    }

    function dismissNotification(notificationId) {
        const notification = root.pendingNotifications[notificationId]
        if (!notification) return
        delete root.pendingNotifications[notificationId]
        notification.close()
    }

    function retire() {
        retired = true
        if (activeDownloadCount === 0) root.destroy()
    }

    function clearBrowsingData(dataTypes, since) {
        // Qt exposes cookie, HTTP-cache and visited-link removal at profile
        // scope. It does not expose time-filtered removal or a local-storage
        // and IndexedDB remover; the browser request still reaches this engine
        // boundary without clearing a category the reader did not select.
        if (dataTypes.indexOf("cookies") >= 0)
            privateProfile.cookieStore.deleteAllCookies()
        if (dataTypes.indexOf("cache") >= 0)
            privateProfile.clearHttpCache()
        if (dataTypes.indexOf("history") >= 0)
            privateProfile.clearAllVisitedLinks()
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
        property string preparedDownloadPath: ""
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

        // Chromium hands the notification over and waits: nothing is shown
        // until `show` is called, and a page that is never told otherwise has
        // simply not been answered. That is what lets the shell refuse one from
        // a Space it has put away.
        onPresentNotification: function(notification) {
            const notificationId = String(++root.nextNotificationId)
            root.pendingNotifications[notificationId] = notification
            notification.closed.connect(function() {
                delete root.pendingNotifications[notificationId]
            })
            root.notificationPresented(notificationId, notification.origin,
                notification.title, notification.message)
        }

        onDownloadRequested: function(download) {
            if (!root.acceptDownloads) return
            if (preparedDownloadPath.length > 0) {
                const separator = Math.max(preparedDownloadPath.lastIndexOf("/"),
                    preparedDownloadPath.lastIndexOf("\\"))
                download.downloadDirectory = preparedDownloadPath.substring(0, separator)
                download.downloadFileName = preparedDownloadPath.substring(separator + 1)
                preparedDownloadPath = ""
            } else if (root.downloadDirectory.length > 0) {
                download.downloadDirectory = root.downloadDirectory
            }
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
