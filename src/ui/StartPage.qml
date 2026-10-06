import QtQuick
import Omaweb
import qs.Commons

// What Omaweb shows where a page would be when there is none to show: a Space
// at rest, `about:blank`, or a new-tab request that has not been given a
// destination yet. It is the Omnibar at rest over the Scene the reader chose:
// the night road, the night sky or the Game of Life. The Omnibar itself is
// the window's own, drawn resting on the Scene's horizon, and its hint row
// names the Shortcut sheet; the page is the Scene under it.
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
    // "game-of-life", or "none", where the backdrop below takes its place.
    property string sceneId: "crt-road"
    // Each Scene's drawing, by its id.
    readonly property var scenes: ({
                                       "crt-road": nightRoad,
                                       "night-sky": sky,
                                       "game-of-life": life
                                   })
    readonly property bool sceneShown: root.sceneId !== "none"
    // How far the resting Omnibar reaches below the horizon, which the night
    // sky keeps clear.
    property real omnibarReach: 40
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
    // Where the Omnibar's field rests.
    readonly property real horizonY: host.sceneItem ? host.sceneItem.horizonY : height / 2
    readonly property int sceneFrames: host.frames
    readonly property bool sceneRunning: host.drawing
    // The Scene host, while a Scene is drawn: the picture the Omnibar's glass
    // blurs at rest and whose light falls on its rim.
    readonly property Item scene: root.sceneShown ? host : null

    // It fades in under the Omnibar arriving, and out as the page it gave way
    // to takes over. While it fades out a click is that page's.
    opacity: open ? 1 : 0
    visible: opacity > 0

    // Once it has gone, the next time it comes back it starts at rest.
    onVisibleChanged: if (!visible)
                          drove = false

    // The fade after a drive is the Scene reporting the page's arrival, as the
    // loading indicator reports a load, so it plays after a key's Return as
    // after a click, and only reduced motion stills it.
    Behavior on opacity {
        enabled: root.ease || root.drove && !SystemMotion.reduced

        // Leaving after a drive is slower: the page is already there under
        // it, so the longer fade costs no waiting.
        NumberAnimation {
            duration: root.drove && !root.open ? 420 : 180
            easing.type: root.drove && !root.open ? Easing.InOutQuad : Easing.OutCubic
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
        when: !!host.sceneItem && host.sceneItem.objectName === "nightSky"
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
}
