import QtQuick
import QtWebEngine

QtObject {
    id: root

    property string profilePath: ""
    property var engineContentBlocker: null
    // Third-party cookies are blocked by the engine's own filter, and whether
    // an origin has been given an allowance is the core's answer for one Space.
    // Both are handed in rather than reached for: a profile may be built for a
    // Space nobody is looking at.
    property var engineCookiePolicy: null
    property var cookieController: null
    property string cookieSpaceId: ""
    // Whether the engine's third-party filter is actually attached. Reported
    // rather than assumed: a profile the filter could not be attached to is one
    // Site information has to stop promising anything about.
    property bool thirdPartyCookiesBlocked: false
    property string downloadDirectory: ""
    property bool acceptDownloads: false
    property bool privateBrowsing: true
    property string downloadNamespace: ""
    property int activeDownloadCount: 0
    property bool retired: false
    property bool downloadObserversConnected: false
    property bool notificationObserversConnected: false
    // Where this engine keeps the site data the browser's clearing action can
    // actually take. Chromium's layout is Chromium's business, so the engine
    // names it and Omaweb, which knows only where it put the profile, measures
    // it. Counting the whole profile instead would report a number the action
    // cannot move. The cache sits beside the profile rather than inside it,
    // which is where EngineProfile puts it.
    readonly property var siteDataEntries: [
        "Cookies", "Cookies-journal",
        "Local Storage", "Session Storage", "IndexedDB", "databases",
        "File System", "Service Worker", "Shared Storage",
        "cache"
    ]
    // The engine has finished taking what it was asked to take. Site
    // information re-reads on this rather than guessing at a delay: Chromium
    // clears asynchronously, and a size that has not moved yet reads as an
    // action that did nothing.
    signal browsingDataCleared()
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
        root.browsingDataCleared()
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
        storageName: root.privateBrowsing ? "omaweb-private" : "omaweb-space"
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

        // Chromium reports the cache removal separately because it finishes
        // separately, and it is the largest part of what was taken.
        onClearHttpCacheCompleted: root.browsingDataCleared()

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
        if (root.engineCookiePolicy && root.cookieController) {
            root.thirdPartyCookiesBlocked = root.engineCookiePolicy.attachToProfile(
                root.profile, root.cookieController, root.cookieSpaceId)
        }
    }
}
