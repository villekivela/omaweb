import QtQuick

QtObject {
    property string profilePath: ""
    property var engineContentBlocker: null
    // The lab reaches no network and so blocks nothing. It says so rather than
    // being trusted: an engine that cannot refuse a third party is one Site
    // information has to name.
    property var engineCookiePolicy: null
    property var cookieController: null
    property string cookieSpaceId: ""
    readonly property bool thirdPartyCookiesBlocked: false
    // The lab keeps nothing on disk, so it names nothing and Site information
    // has no size to show. A test that needs the shell stood up against an
    // engine that does keep site data names some.
    property var siteDataEntries: []
    property var retainedDataEntries: []
    property var resetPermissionOrigins: []
    // What this engine would not be able to take, so the shell's report of it
    // can be reviewed against an engine that falls short.
    property var untouchedCategories: []
    function resetOriginPermissions(origin) {
        resetPermissionOrigins = resetPermissionOrigins.concat([String(origin)]);
    }
    signal browsingDataCleared
    property string downloadDirectory: ""
    property bool acceptDownloads: false
    property var downloadController: null
    property var downloadHolds: null
    property var answeredDownloads: ({})
    property var downloadRequests: ({})
    property var heldDownloads: ({})
    property int nextHeldDownload: 0
    property var cancelledDownloads: []
    property var retriedDownloads: []
    property bool privateBrowsing: true
    property string downloadNamespace: ""
    property int activeDownloadCount: 0
    property int browsingDataClearCount: 0
    property bool downloadObserversConnected: false
    property bool notificationObserversConnected: false
    readonly property var profile: this
    signal downloadStarted(string runtimeId, url sourceUrl, string path, string state,
                           double receivedBytes, double totalBytes)
    signal downloadUpdated(string runtimeId, string state, double receivedBytes, double totalBytes,
                           string error)
    signal downloadHeld(string token, string disposition, string origin, url sourceUrl,
                        string fileName, string risk)
    signal downloadRefused(url sourceUrl, string fileName, string origin)

    function simulateDownloadRequest(pageUrl, sourceUrl, fileName, mimeType) {
        if (!acceptDownloads)
            return "";
        const sourceKey = String(sourceUrl);
        const answer = answeredDownloads[sourceKey];
        const answered = answer !== undefined;
        if (answered)
            delete answeredDownloads[sourceKey];
        let chosenPath = answered && answer.length > 0 ? answer : "";
        if (chosenPath.length === 0) {
            const rule = downloadController ? downloadController.downloadDisposition(pageUrl,
                                                                                     fileName, mimeType,
                                                                                     downloadDirectory,
                                                                                     answered) :
                                              null;
            const disposition = rule ? rule.disposition : "accept";
            if (disposition === "refuse") {
                downloadRefused(sourceUrl, fileName, rule.origin);
                return "";
            }
            if (disposition !== "accept") {
                const token = "held-" + String(++nextHeldDownload);
                heldDownloads[token] = {
                    "sourceUrl": String(sourceUrl),
                    "fileName": String(fileName),
                    "mimeType": String(mimeType),
                    "pageUrl": String(pageUrl)
                };
                downloadHeld(token, disposition, rule.origin, sourceUrl, fileName, rule.risk);
                return "";
            }
        }
        const runtimeId = downloadNamespace + ":" + String(++nextHeldDownload);
        let path = chosenPath;
        if (path.length === 0)
            path = downloadDirectory + "/" + String(fileName);
        downloadRequests[runtimeId] = path;
        activeDownloadCount += 1;
        if (downloadController)
            downloadController.noteDownloadStarted(pageUrl, runtimeId);
        downloadStarted(runtimeId, sourceUrl, path, "in-progress", 0, 100);
        return runtimeId;
    }

    function simulateDownloadProgress(runtimeId, receivedBytes, totalBytes) {
        if (downloadRequests[runtimeId] === undefined)
            return false;
        downloadUpdated(runtimeId, "in-progress", receivedBytes, totalBytes, "");
        return true;
    }

    function simulateDownloadFinished(runtimeId) {
        if (downloadRequests[runtimeId] === undefined)
            return false;
        activeDownloadCount -= 1;
        if (downloadController)
            downloadController.noteDownloadSettled(runtimeId);
        delete downloadRequests[runtimeId];
        downloadUpdated(runtimeId, "completed", 100, 100, "");
        return true;
    }

    function releaseHeldDownload(token, path) {
        const details = heldDownloads[token];
        if (!details)
            return false;
        delete heldDownloads[token];
        answeredDownloads[details.sourceUrl] = path ? String(path) : "";
        simulateDownloadRequest(details.pageUrl, details.sourceUrl, details.fileName,
                                details.mimeType);
        return true;
    }

    function discardHeldDownload(token) {
        if (heldDownloads[token] === undefined)
            return false;
        delete heldDownloads[token];
        return true;
    }

    function cancelDownload(runtimeId) {
        if (downloadRequests[runtimeId] === undefined)
            return false;
        cancelledDownloads = cancelledDownloads.concat([String(runtimeId)]);
        downloadUpdated(runtimeId, "cancelled", 0, 100, "");
        return true;
    }

    function retryDownload(runtimeId) {
        if (downloadRequests[runtimeId] === undefined)
            return false;
        retriedDownloads = retriedDownloads.concat([String(runtimeId)]);
        downloadUpdated(runtimeId, "in-progress", 0, 100, "");
        return true;
    }
    signal notificationPresented(string notificationId, url origin, string title, string message)
    // The lab has no page to ask, so a notification is named. What the shell
    // did with it is counted here, because refusing one is as much of an answer
    // as showing it.
    property int nextNotificationId: 0
    property var activatedNotifications: []
    property var dismissedNotifications: []
    function simulateNotification(origin, title, message) {
        const notificationId = String(++nextNotificationId);
        notificationPresented(notificationId, origin, String(title), String(message));
        return notificationId;
    }
    function activateNotification(notificationId) {
        activatedNotifications = activatedNotifications.concat([String(notificationId)]);
    }
    function dismissNotification(notificationId) {
        dismissedNotifications = dismissedNotifications.concat([String(notificationId)]);
    }
    function retire() {
        destroy();
    }
    function clearBrowsingData(dataTypes, since) {
        browsingDataClearCount += 1;
        browsingDataCleared();
        return untouchedCategories;
    }
}
