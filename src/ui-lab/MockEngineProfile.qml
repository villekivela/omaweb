import QtQuick

QtObject {
    property string profilePath: ""
    property var engineContentBlocker: null
    property string downloadDirectory: ""
    property bool acceptDownloads: false
    property bool privateBrowsing: true
    property string downloadNamespace: ""
    property int activeDownloadCount: 0
    property int browsingDataClearCount: 0
    property bool downloadObserversConnected: false
    property bool notificationObserversConnected: false
    readonly property var profile: this
    signal downloadStarted(string runtimeId, url sourceUrl, string path, string state,
        double receivedBytes, double totalBytes)
    signal downloadUpdated(string runtimeId, string state, double receivedBytes,
        double totalBytes, string error)
    signal notificationPresented(string notificationId, url origin, string title,
        string message)
    // The lab has no page to ask, so a notification is named. What the shell
    // did with it is counted here, because refusing one is as much of an answer
    // as showing it.
    property int nextNotificationId: 0
    property var activatedNotifications: []
    property var dismissedNotifications: []
    function simulateNotification(origin, title, message) {
        const notificationId = String(++nextNotificationId)
        notificationPresented(notificationId, origin, String(title), String(message))
        return notificationId
    }
    function activateNotification(notificationId) {
        activatedNotifications = activatedNotifications.concat([String(notificationId)])
    }
    function dismissNotification(notificationId) {
        dismissedNotifications = dismissedNotifications.concat([String(notificationId)])
    }
    function retire() { destroy() }
    function clearBrowsingData(dataTypes, since) { browsingDataClearCount += 1 }
}
