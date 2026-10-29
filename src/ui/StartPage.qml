import QtQuick
import Omaweb
import qs.Commons

// What Omaweb shows where a page would be when there is none to show: a Space
// at rest, `about:blank`, or a new-tab request that has not been given a
// destination yet. It is the Omnibar at rest over the Scene the reader chose:
// the night road, the night sky, the Game of Life, the vector terrain, the
// radar, hyperspace or Telemetry. The Omnibar itself is the window's own,
// drawn resting on the Scene's horizon, and the Start page's corner names the
// Shortcut sheet; the page is the Scene under it.
//
// It costs no engine. The Scene moves only while the page is on show and the
// window is the reader's. It fills the page area, or the one pane of a split
// it stands in, and never runs under the sidebar: it is drawn at that size, as
// the website draws the road in a viewport, so the road's horizon, sun and
// road are where the website's are. A reader who chooses no Scene gets the
// sidebar's fill instead, or, over a page a new tab was asked from, that page
// blurred under the sheet tint, as the Shortcut sheet shows it.
Item {
    id: root
    objectName: "startPage"

    property var colors
    property bool privateWindow: false
    property bool open: false
    // The Settings interface section's Scene: "crt-road", "night-sky",
    // "game-of-life", "vector-terrain", "radar", "hyperspace", "telemetry", or
    // "none", where the backdrop below takes its place.
    property string sceneId: "crt-road"
    // Each Scene's drawing, by its id.
    readonly property var scenes: ({
                                       "crt-road": nightRoad,
                                       "night-sky": sky,
                                       "game-of-life": life,
                                       "vector-terrain": terrain,
                                       "radar": radar,
                                       "hyperspace": hyperspace,
                                       "telemetry": telemetry
                                   })
    readonly property bool sceneShown: root.sceneId !== "none"
    // How far the resting Omnibar reaches below the horizon, which the night
    // sky's planet, the vector terrain's mountains and the radar's blips keep
    // clear.
    property real omnibarReach: 2
    // How wide the resting Omnibar stands, which the night sky's comets
    // pass the end of. Until the window hands it in, 0, and a Scene keeps
    // the width its own parameters give.
    property real omnibarWidth: 0
    // The Settings interface section's CRT glass over the Scene.
    property bool glassEnabled: true
    // The reader asked for less motion: the Scene holds one still frame, with
    // no clock, no drive and no band or flicker on its glass.
    property bool reducedMotion: false
    // The window is on screen and holds the keyboard.
    property bool windowActive: false
    // A destination was committed from the Omnibar and its page has not
    // painted yet.
    property bool driving: false
    // The tabs of the Space on show, as the browser lists them, and the one a
    // commit is opening: the radar has a blip for each tab and brightens one
    // for the page being opened.
    property var tabs: null
    property string arrivingTabId: ""
    // Their addresses, the tab being opened's apart, read while the radar is
    // on show.
    property var pages: []
    property string arriving: ""
    // The page the Start page was summoned over, blurred under the sheet tint
    // when no Scene is chosen. Must not be an ancestor of this item.
    property Item pageSource: null
    // Whether the page fades as it gives way to a page.
    property bool ease: true
    // Set by a drive and kept while the Start page fades out after it, so the
    // fade is the slower one and the Scene, easing out of its drive, is still
    // moving and lit as the page takes over.
    property bool drove: false
    onDrivingChanged: if (driving)
                          drove = true
    // Fading out after a drive, as the page it drove to takes over.
    readonly property bool leavingAfterDrive: root.drove && !root.open
    // Where the Omnibar's field rests.
    readonly property real horizonY: host.sceneItem ? host.sceneItem.horizonY : height / 2
    readonly property int sceneFrames: host.frames
    readonly property bool sceneRunning: host.drawing
    // The Scene host, while a Scene is drawn: the picture the Omnibar's glass
    // blurs at rest and whose light falls on its rim.
    readonly property Item scene: root.sceneShown ? host : null

    // It fades in under the Omnibar arriving, and out as the page it gave way
    // to takes over, once the window has drawn a frame. While it fades out a
    // click is that page's.
    opacity: open ? 1 : 0
    visible: opacity > 0

    // Once it has gone, the next time it comes back it starts at rest, with
    // the Space's tabs read again for the radar.
    onVisibleChanged: {
        if (!visible)
            drove = false;
        root.readPages();
    }

    // Whether the window has drawn a frame. A window that comes up on a page
    // stands the Start page in while the page's view loads and closes it before
    // anything is drawn. Faded, that close would put its Scene and glass in the
    // window's first frames, over a page already coming, and hold the first of
    // them back (#883). Until then it goes at once.
    property bool windowDrawn: false

    Connections {
        target: root.Window.window
        enabled: !root.windowDrawn

        function onFrameSwapped() {
            root.windowDrawn = true;
        }
    }

    // The fade after a drive is the Scene reporting the page's arrival, as the
    // loading indicator reports a load, so it plays after a key's Return as
    // after a click. Reduced motion stills it, and so does a window that has
    // not drawn yet.
    Behavior on opacity {
        enabled: root.windowDrawn && (root.ease || root.drove && !SystemMotion.reduced)

        // Leaving after a drive is slower: the page is already there under
        // it, so the longer fade costs no waiting.
        NumberAnimation {
            duration: root.leavingAfterDrive ? 420 : 180
            easing.type: root.leavingAfterDrive ? Easing.InOutQuad : Easing.OutCubic
        }
    }

    // Over a page the Start page stands in for it, so the page beneath hears
    // nothing until the Start page has gone.
    MouseArea {
        anchors.fill: parent
        enabled: root.open
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onWheel: function (wheel) {
            wheel.accepted = true;
        }
    }

    PageBackdrop {
        objectName: "startPageBackdrop"
        anchors.fill: parent
        visible: !root.sceneShown
        source: root.pageSource
        tint: root.pageSource ? root.colors.sheet : root.colors.sidebar
    }

    clip: true

    SceneHost {
        id: host
        objectName: "startPageScene"
        width: root.width
        height: root.height
        visible: root.sceneShown
        colors: root.colors
        unlit: root.privateWindow
        glass: root.glassEnabled
        reducedMotion: root.reducedMotion
        running: root.visible && root.sceneShown && root.windowActive
        navigating: root.driving ? 1 : 0
        scene: root.scenes[root.sceneId] || nightRoad
    }

    Binding {
        target: host.sceneItem
        property: "omnibarReach"
        value: root.omnibarReach
        when: !!host.sceneItem && host.sceneItem.omnibarReach !== undefined
    }

    Binding {
        target: host.sceneItem
        property: "omnibarWidth"
        value: root.omnibarWidth
        when: !!host.sceneItem && host.sceneItem.omnibarWidth !== undefined && root.omnibarWidth > 0
    }

    // A Scene that declares `leaving` hears when the Start page fades out
    // after a drive, so it can hold its drive as the page replaces it, as
    // hyperspace holds its streaks.
    Binding {
        target: host.sceneItem
        property: "leaving"
        value: root.leavingAfterDrive
        when: !!host.sceneItem && host.sceneItem.leaving !== undefined
    }

    // The tab list's roles, as BrowserController's TabListModel numbers them.
    readonly property int tabIdRole: Qt.UserRole + 1
    readonly property int tabUrlRole: Qt.UserRole + 3

    // Whether the Scene on show places the Space's tabs, as the radar does.
    readonly property bool scenePlacesTabs: !!host.sceneItem && host.sceneItem.pages !== undefined

    function readPages() {
        if (!root.visible || !root.scenePlacesTabs || !root.tabs)
            return;
        const pages = [];
        let arriving = "";
        for (let row = 0; row < root.tabs.rowCount(); ++row) {
            const index = root.tabs.index(row, 0);
            const url = String(root.tabs.data(index, root.tabUrlRole));
            if (root.tabs.data(index, root.tabIdRole) === root.arrivingTabId)
                arriving = url;
            else
                pages.push(url);
        }
        // A tab's loading and title change its row too; the radar redraws
        // nothing unless an address did.
        if (pages.join("\n") !== root.pages.join("\n"))
            root.pages = pages;
        root.arriving = arriving;
    }

    onScenePlacesTabsChanged: root.readPages()
    onTabsChanged: root.readPages()
    onArrivingTabIdChanged: root.readPages()

    Connections {
        target: root.tabs
        ignoreUnknownSignals: true

        function onRowsInserted() {
            root.readPages();
        }
        function onRowsRemoved() {
            root.readPages();
        }
        function onRowsMoved() {
            root.readPages();
        }
        function onDataChanged() {
            root.readPages();
        }
        function onModelReset() {
            root.readPages();
        }
    }

    Binding {
        target: host.sceneItem
        property: "pages"
        value: root.pages
        when: root.scenePlacesTabs
    }

    Binding {
        target: host.sceneItem
        property: "arriving"
        value: root.arriving
        when: root.scenePlacesTabs
    }

    Component {
        id: nightRoad

        NightRoad {}
    }

    Component {
        id: sky

        NightSky {}
    }

    Component {
        id: life

        GameOfLife {}
    }

    Component {
        id: terrain

        VectorTerrain {}
    }

    Component {
        id: radar

        Radar {}
    }

    Component {
        id: hyperspace

        Hyperspace {}
    }

    Component {
        id: telemetry

        Telemetry {}
    }

    // ---- Telemetry: what the window hands its Scene

    // The Settings interface section's text size and the modules turned off.
    property string telemetryText: "large"
    property var telemetryHidden: []
    // The window's downloads, whose running ones the Scene shows.
    property var downloads: null
    // Functions of the window: the ids of its Spaces, whose refusals the Scene
    // counts, and each Space's name and memory.
    property var spaceIds: null
    property var spaceMemory: null

    readonly property bool telemetryShown: !!host.sceneItem && host.sceneItem.objectName
                                           === "telemetry"
    // While Telemetry is on show it reads the sampler at once, so a new tab
    // shows what the last five minutes held, and again at every sample. The
    // Spaces' memory is asked of the system once a second.
    readonly property bool readingTelemetry: root.visible && root.telemetryShown
    property int telemetrySamples: 0

    function readTelemetry() {
        if (!root.readingTelemetry)
            return;
        // A Private window's Scene is handed nothing from History.
        host.sceneItem.readings = TelemetrySampler.readings(root.spaceIds ? root.spaceIds() : [],
                                                            !root.privateWindow);
        if (root.telemetrySamples % TelemetrySampler.samplesPerSecond === 0 && !root.privateWindow)
            host.sceneItem.spaces = root.spaceMemory ? root.spaceMemory() : [];
        root.telemetrySamples += 1;
    }

    onReadingTelemetryChanged: {
        root.telemetrySamples = 0;
        root.readTelemetry();
    }

    Connections {
        target: TelemetrySampler
        enabled: root.readingTelemetry

        function onSampled() {
            root.readTelemetry();
        }
    }

    Binding {
        target: root.telemetryShown ? host.sceneItem : null
        property: "textSize"
        value: root.telemetryText
        when: root.telemetryShown
    }

    Binding {
        target: root.telemetryShown ? host.sceneItem : null
        property: "hidden"
        value: root.telemetryHidden
        when: root.telemetryShown
    }

    Binding {
        target: root.telemetryShown ? host.sceneItem : null
        property: "downloads"
        value: root.downloads ? root.downloads.runningDownloads : []
        when: root.telemetryShown
    }
}
