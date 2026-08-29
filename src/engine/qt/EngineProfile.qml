import QtQuick
import QtWebEngine

QtObject {
    id: root

    property string profilePath: ""
    property string downloadDirectory: ""
    property bool acceptDownloads: false
    readonly property var profile: privateProfile

    property WebEngineProfile privateProfile: WebEngineProfile {
        storageName: "tanto-private"
        persistentStoragePath: root.profilePath
        cachePath: root.profilePath + "/cache"
        persistentCookiesPolicy: WebEngineProfile.NoPersistentCookies
        httpCacheType: WebEngineProfile.MemoryHttpCache

        onDownloadRequested: function(download) {
            if (!root.acceptDownloads) return
            if (root.downloadDirectory.length > 0)
                download.downloadDirectory = root.downloadDirectory
            download.accept()
        }
    }
}
