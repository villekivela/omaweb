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
    property var downloadController: null
    property var downloadHolds: null
    property var answeredDownloads: ({})
    property var downloadRequests: ({})
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
    readonly property var siteDataEntries: ["Cookies", "Cookies-journal", "cache"]
    // And what it holds that the clearing cannot take. Qt exposes no removal
    // for local storage, databases or service workers at this boundary, so
    // their size is reported separately rather than folded into a number the
    // action cannot move.
    readonly property var retainedDataEntries: ["Local Storage", "Session Storage", "IndexedDB",
        "databases", "File System", "Service Worker", "Shared Storage", "blob_storage"]
    // The engine has finished taking what it was asked to take. Site
    // information re-reads on this rather than guessing at a delay: Chromium
    // clears asynchronously, and a size that has not moved yet reads as an
    // action that did nothing.
    signal browsingDataCleared
    readonly property var profile: privateProfile
    signal downloadStarted(string runtimeId, url sourceUrl, string path, string state,
                           double receivedBytes, double totalBytes)
    signal downloadUpdated(string runtimeId, string state, double receivedBytes, double totalBytes,
                           string error)
    signal downloadHeld(string token, string disposition, string origin, url sourceUrl,
                        string fileName, string risk)
    signal downloadRefused(url sourceUrl, string fileName, string origin)

    function releaseHeldDownload(token, path) {
        if (!root.downloadHolds)
            return false;
        const details = root.downloadHolds.held(token);
        if (!details || !details.sourceUrl || !details.view)
            return false;
        // The next matching request consumes this answer.
        root.answeredDownloads[String(details.sourceUrl)] = path ? String(path) : "";
        root.downloadHolds.discard(token);
        details.view.runJavaScript("(function(){var link=document.createElement('a');link.href="
                                   + JSON.stringify(String(details.sourceUrl)) + ";link.download="
                                   + JSON.stringify(String(details.fileName))
                                   + ";link.rel='noopener';"
                                   + "(document.body||document.documentElement).appendChild(link);"
                                   + "link.click();link.remove();})()");
        return true;
    }

    function discardHeldDownload(token) {
        return root.downloadHolds ? root.downloadHolds.discard(token) : false;
    }

    function cancelDownload(runtimeId) {
        const request = root.downloadRequests[runtimeId];
        if (!request)
            return false;
        request.cancel();
        return true;
    }

    function retryDownload(runtimeId) {
        const request = root.downloadRequests[runtimeId];
        if (!request)
            return false;
        if (request.state !== WebEngineDownloadRequest.DownloadInterrupted && !request.isPaused)
            return false;
        request.resume();
        return true;
    }
    // A notification arrives from a Space's profile rather than from one page,
    // so the origin is all there is to say who sent it. The shell decides
    // whether that origin has a tab entitled to interrupt, presents the
    // desktop's own notification, and asks for it back by id.
    signal notificationPresented(string notificationId, url origin, string title, string message)
    property var pendingNotifications: ({})
    property int nextNotificationId: 0

    // The reader answered the notification: the page hears the click, and the
    // shell takes them to the tab.
    function activateNotification(notificationId) {
        const notification = root.pendingNotifications[notificationId];
        if (!notification)
            return;
        delete root.pendingNotifications[notificationId];
        notification.click();
        notification.close();
    }

    function dismissNotification(notificationId) {
        const notification = root.pendingNotifications[notificationId];
        if (!notification)
            return;
        delete root.pendingNotifications[notificationId];
        notification.close();
    }

    function retire() {
        retired = true;
        if (activeDownloadCount === 0)
            root.destroy();
    }

    // The reader took an origin's decisions back, so the engine's own record of
    // them goes too. Only the persistent types have one — Chromium keeps camera
    // and microphone in a transient store keyed by the frame that asked, which
    // no public API reaches, so a page already holding one keeps it until the
    // site is opened again.
    function resetOriginPermissions(origin) {
        const persistentTypes = [WebEnginePermission.PermissionType.Notifications,
                                 WebEnginePermission.PermissionType.Geolocation,
                                 WebEnginePermission.PermissionType.ClipboardReadWrite,
                                 WebEnginePermission.PermissionType.LocalFontsAccess];
        for (let index = 0; index < persistentTypes.length; ++index) {
            const permission = privateProfile.queryPermission(origin, persistentTypes[index]);
            if (permission.isValid)
                permission.reset();
        }
    }

    // What this engine can actually take, category by category, each on its own
    // so a category it cannot take is one it reports rather than one that stops
    // the rest. Chromium reaches its cookie store through a C++ accessor and
    // not a property, so cookies go through the cookie policy; the HTTP cache
    // it clears itself, asynchronously; and it exposes no removal at all for
    // local storage, databases or service workers, nor any time filter. The
    // reader is told what stayed rather than left to believe it went.
    function clearBrowsingData(dataTypes, since) {
        const untouched = [];
        if (dataTypes.indexOf("cookies") >= 0) {
            const deleted = root.engineCookiePolicy && root.engineCookiePolicy.deleteAllCookies(
                      root.profile);
            if (!deleted)
                untouched.push("cookies");
        }
        if (dataTypes.indexOf("storage") >= 0)
            untouched.push("storage");
        if (dataTypes.indexOf("cache") >= 0)
            privateProfile.clearHttpCache();
        root.browsingDataCleared();
        return untouched;
    }

    property Component downloadObserver: Component {
        QtObject {
            id: observer
            required property var download
            required property string downloadNamespace
            property string pageUrl: ""
            property bool settled: false

            function stateName() {
                switch (download.state) {
                case WebEngineDownloadRequest.DownloadRequested:
                    return "requested";
                case WebEngineDownloadRequest.DownloadInProgress:
                    return "in-progress";
                case WebEngineDownloadRequest.DownloadCompleted:
                    return "completed";
                case WebEngineDownloadRequest.DownloadCancelled:
                    return "cancelled";
                case WebEngineDownloadRequest.DownloadInterrupted:
                    return "interrupted";
                default:
                    return "unknown";
                }
            }

            function path() {
                return download.downloadDirectory + "/" + download.downloadFileName;
            }

            function runtimeId() {
                return downloadNamespace + ":" + String(download.id);
            }

            function running() {
                return download.state === WebEngineDownloadRequest.DownloadInProgress
                        || download.state === WebEngineDownloadRequest.DownloadRequested;
            }

            function updateRecord() {
                root.downloadUpdated(runtimeId(), stateName(), download.receivedBytes,
                                     download.totalBytes, download.interruptReasonString || "");
                if (!running() && !settled) {
                    settled = true;
                    root.activeDownloadCount -= 1;
                    if (root.downloadController)
                        root.downloadController.noteDownloadSettled(runtimeId());
                } else if (running() && settled) {
                    settled = false;
                    root.activeDownloadCount += 1;
                    if (root.downloadController)
                        root.downloadController.noteDownloadStarted(observer.pageUrl, runtimeId());
                }
                if (download.state === WebEngineDownloadRequest.DownloadCompleted || download.state
                        === WebEngineDownloadRequest.DownloadCancelled) {
                    delete root.downloadRequests[runtimeId()];
                    if (root.retired && root.activeDownloadCount === 0)
                        root.destroy();
                    observer.destroy();
                }
            }

            Component.onCompleted: {
                root.downloadRequests[runtimeId()] = download;
                root.downloadStarted(runtimeId(), download.url, path(), stateName(), download.receivedBytes,
                                     download.totalBytes);
            }

            property Connections downloadConnections: Connections {
                target: observer.download
                function onStateChanged() {
                    observer.updateRecord();
                }
                function onReceivedBytesChanged() {
                    observer.updateRecord();
                }
                function onTotalBytesChanged() {
                    observer.updateRecord();
                }
                function onInterruptReasonChanged() {
                    observer.updateRecord();
                }
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
        persistentCookiesPolicy: root.privateBrowsing ? WebEngineProfile.NoPersistentCookies :
                                                        WebEngineProfile.ForcePersistentCookies
        httpCacheType: root.privateBrowsing ? WebEngineProfile.MemoryHttpCache :
                                              WebEngineProfile.DiskHttpCache
        // Chromium keeps a permission store of its own and, left to itself,
        // answers a second request from it without ever asking the embedder.
        // That would put a decision beyond the reach of everything Omaweb
        // offers for taking one back: the reader resets a permission, reloads,
        // and is never asked again. Asking every time makes Omaweb's own store
        // the only place a site's decisions live, which is also what lets
        // allow-once mean once and clipboard read mean every time.
        persistentPermissionsPolicy: WebEngineProfile.PersistentPermissionsPolicy.AskEveryTime

        // Chromium hands the notification over and waits: nothing is shown
        // until `show` is called, and a page that is never told otherwise has
        // simply not been answered. That is what lets the shell refuse one from
        // a Space it has put away.
        onPresentNotification: function (notification) {
            const notificationId = String(++root.nextNotificationId);
            root.pendingNotifications[notificationId] = notification;
            notification.closed.connect(function () {
                delete root.pendingNotifications[notificationId];
            });
            root.notificationPresented(notificationId, notification.origin, notification.title,
                                       notification.message);
        }

        // Chromium reports the cache removal separately because it finishes
        // separately, and it is the largest part of what was taken.
        onClearHttpCacheCompleted: root.browsingDataCleared()

        onDownloadRequested: function (download) {
            if (!root.acceptDownloads) {
                download.cancel();
                return;
            }
            const pageUrl = download.view ? download.view.url : "";
            const sourceKey = String(download.url);
            const answer = root.answeredDownloads[sourceKey];
            const answered = answer !== undefined;
            if (answered)
                delete root.answeredDownloads[sourceKey];
            let chosenPath = answered && answer.length > 0 ? answer : preparedDownloadPath;
            if (chosenPath.length === 0) {
                const fileName = download.downloadFileName.length > 0 ? download.downloadFileName :
                                                                        download.suggestedFileName;
                const rule = root.downloadController ? root.downloadController.downloadDisposition(
                                                           pageUrl, fileName, download.mimeType,
                                                           root.downloadDirectory, answered) : null;
                const disposition = rule ? rule.disposition : "accept";
                if (disposition === "refuse") {
                    download.cancel();
                    root.downloadRefused(download.url, fileName, rule.origin);
                    return;
                }
                if (disposition !== "accept") {
                    const token = root.downloadHolds ? root.downloadHolds.hold(download) : "";
                    if (token.length === 0) {
                        download.cancel();
                        root.downloadRefused(download.url, fileName, rule ? rule.origin : "");
                        return;
                    }
                    root.downloadHeld(token, disposition, rule.origin, download.url, fileName,
                                      rule.risk);
                    return;
                }
            }
            if (chosenPath.length > 0) {
                const separator = Math.max(chosenPath.lastIndexOf("/"), chosenPath.lastIndexOf(
                                               "\\"));
                download.downloadDirectory = chosenPath.substring(0, separator);
                download.downloadFileName = chosenPath.substring(separator + 1);
                preparedDownloadPath = "";
            } else if (root.downloadDirectory.length > 0) {
                download.downloadDirectory = root.downloadDirectory;
            }
            if (download.downloadFileName.length === 0)
                download.downloadFileName = download.suggestedFileName;
            root.activeDownloadCount += 1;
            const observer = root.downloadObserver.createObject(root, {
                                                                    "download": download,
                                                                    "downloadNamespace":
                                                                    root.downloadNamespace,
                                                                    "pageUrl": String(pageUrl)
                                                                });
            if (root.downloadController)
                root.downloadController.noteDownloadStarted(pageUrl, observer.runtimeId());
            download.accept();
        }
    }

    Component.onCompleted: {
        if (root.engineContentBlocker)
            root.engineContentBlocker.attachToProfile(root.profile);
        if (root.engineCookiePolicy && root.cookieController) {
            root.thirdPartyCookiesBlocked = root.engineCookiePolicy.attachToProfile(root.profile,
                                                                                    root.cookieController,
                                                                                    root.cookieSpaceId);
        }
    }
}
