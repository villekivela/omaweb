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
    readonly property var profile: this
    signal downloadStarted(string runtimeId, url sourceUrl, string path, string state,
        double receivedBytes, double totalBytes)
    signal downloadUpdated(string runtimeId, string state, double receivedBytes,
        double totalBytes, string error)
    function retire() { destroy() }
    function clearBrowsingData(dataTypes, since) { browsingDataClearCount += 1 }
}
