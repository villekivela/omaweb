import QtQuick

// One engine profile per Space, kept for as long as that Space's pages are.
//
// A profile owns a Space's cookies and cache on disk, so rebuilding one on
// every switch cost a full teardown and reopen of both. Two things ask for a
// profile and neither owns the table: the window, on its way to showing a
// Space, and the engine host, on its way to keeping a retained tab running in
// a Space that may never have been visited. So the table lives here, and
// whoever builds a profile says so once, in one place — the downloads and
// notifications that come out of it are the window's to route whichever
// asked first.
QtObject {
    id: root

    required property var browser
    // The adapter component a profile is built from. Empty in a window that
    // runs no engine, where nothing can be built and nothing asks.
    property url profileSource
    property var contentBlocker: null
    // The engine's third-party filter. A profile is where the blocking is
    // attached, and the Space is what the allowances are keyed by.
    property var cookiePolicy: null
    property var downloadHolds: null
    // Where created profiles are parented, so they outlive the call that asked
    // for one and go away with the window rather than with a tab.
    property var owner: null

    signal created(string spaceId, var host)

    property Connections downloadDirectoryConnections: Connections {
        target: root.browser
        ignoreUnknownSignals: true

        function onDownloadDirectoryChanged() {
            for (const spaceId in root.hosts)
                root.hosts[spaceId].downloadDirectory = root.browser.downloadDirectory
        }
    }

    readonly property var hosts: ({})

    function hostFor(spaceId) {
        const existing = root.hosts[spaceId]
        if (existing)
            return existing
        if (!root.profileSource || String(root.profileSource).length === 0)
            return null
        const component = Qt.createComponent(root.profileSource)
        const host = component.createObject(root.owner ? root.owner : root, {
                                                "profilePath": root.browser.profilePathForSpace(
                                                                   spaceId),
                                                "downloadDirectory": root.browser.downloadDirectory,
                                                "acceptDownloads": root.browser.acceptDownloads,
                                                "privateBrowsing": false,
                                                "downloadNamespace": spaceId,
                                                "engineContentBlocker": root.contentBlocker,
                                                "engineCookiePolicy": root.cookiePolicy,
                                                "cookieController": root.browser,
                                                "cookieSpaceId": spaceId,
                                                "downloadController": root.browser,
                                                "downloadHolds": root.downloadHolds
                                            })
        if (!host)
            return null
        root.hosts[spaceId] = host
        root.created(spaceId, host)
        return host
    }

    // A Space that has been deleted takes its profile with it, and the profile
    // takes itself away once the downloads it is still carrying are finished.
    function retire(spaceId) {
        const host = root.hosts[spaceId]
        if (!host)
            return null
        delete root.hosts[spaceId]
        host.retire()
        return host
    }
}
