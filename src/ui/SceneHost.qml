import QtQuick

// The Scene host: what stands behind the Start page. A Scene is a drawing and
// nothing else (NightRoad.qml says what it receives and declares). The host
// sizes it in the Scene's own pixels, keeps its clock, decides when it may
// draw, and shows its small picture square-pixelled across the host, through
// the CRT glass when the Scene declares one and the reader has not turned it
// off. It is the website's host (website/scene.js) in Qt Quick, so the two
// show one Scene the same way.
Item {
    id: root

    // The Scene, made once for the host.
    property Component scene
    property var colors
    // Whether the Scene may move: on show, in the window the reader is using.
    // Off, no frame is drawn and the clock stands still.
    property bool running: false
    // How hard the reader is navigating, 0 to 1: 1 from a commit until the
    // page paints.
    property real navigating: 0
    // The reader asked for less motion: one frame that reads on its own.
    property bool reducedMotion: false
    // The reader's say over the glass the Scene declares.
    property bool glass: true
    // A Private window's Scene has its lights off.
    property bool unlit: false
    // The reader's choice for each option the Scene declares; a missing or
    // unknown one takes the option's first value.
    property var chosen: ({})

    // The part of the host the reader sees, which the glass is framed by: a
    // host wider than the window frames its vignette on the window, not on
    // itself.
    property rect frame: Qt.rect(0, 0, width, height)

    // The width the Scene is drawn at. It follows the host's width once that
    // has stood still for a moment, and at once when nothing is drawn yet or
    // the host is hidden: a Scene is drawn again for a size, and a page area
    // that a dragged seam changes at every frame would draw it again at every
    // frame. Meanwhile the picture is stretched to the host.
    property real drawnWidth: width

    onWidthChanged: {
        if (!root.visible || !root.sceneItem)
            root.drawnWidth = root.width;
        else
            settle.restart();
    }
    onVisibleChanged: if (!visible)
                          root.drawnWidth = root.width

    Timer {
        id: settle
        interval: 120
        onTriggered: root.drawnWidth = root.width
    }

    readonly property Item sceneItem: sceneLoader.item
    // The light the Scene casts on the page, which the page lays on the
    // plates it names as lit, or null for a Scene that casts none. Its
    // positions are in the host's own coordinates: the Scene's drawing ones,
    // stretched with its picture while the host is wider or narrower than the
    // width it was drawn at.
    readonly property var light: root.sceneItem && root.sceneItem.light ? root.stretched(
                                                                              root.sceneItem.light) :
                                                                          null

    function stretched(light) {
        const k = root.width / (root.sceneWidth * root.pitch);
        return Object.assign({}, light, {
                                 "centre": Qt.point(light.centre.x * k, light.centre.y)
                             });
    }
    // The Scene's frames, a tick of its clock each, for the tests that check a
    // hidden Scene draws none.
    property int frames: 0
    // Seconds the Scene has run. The clock stops while the Scene does not
    // draw.
    property real time: 0

    readonly property int pitch: root.sceneItem ? root.sceneItem.pitch : 1
    readonly property int sceneWidth: Math.max(1, Math.ceil(root.drawnWidth / root.pitch))
    readonly property int sceneHeight: Math.max(1, Math.ceil(root.height / root.pitch))
    readonly property bool glassShown: root.glass && !!root.sceneItem && root.sceneItem.glass
                                       === "crt" && !!root.sceneItem.crt
    // The glass's amounts, which a Scene that declares the glass gives as
    // `crt`: share/scenes/crt-road.json's, which the website's glass reads too.
    readonly property var crt: root.glassShown ? root.sceneItem.crt : null

    readonly property bool drawing: root.running && !root.reducedMotion && !!root.sceneItem

    // Whether the theme's ground is dark, as the website decides it.
    function darkGround(colour) {
        const c = Qt.color(colour);
        return (Math.max(c.r, c.g, c.b) + Math.min(c.r, c.g, c.b)) / 2 <= 0.6;
    }

    function options(declared) {
        const out = {};
        for (const name in declared) {
            const values = declared[name];
            const choice = root.chosen[name];
            out[name] = values.indexOf(choice) >= 0 ? choice : values[0];
        }
        return out;
    }

    // A tick at the Scene's rate rather than a frame callback: a frame
    // callback keeps the window drawing at the display's rate, sixty frames a
    // second for a Scene that changes thirty times in one. The clock advances
    // by the time that passed, at most a tenth of a second, so a stalled tick
    // does not leap the road forward.
    Timer {
        id: clock

        property real last: 0

        interval: Math.round(1000 / (root.sceneItem && root.sceneItem.fps ? root.sceneItem.fps :
                                                                            60))
        repeat: true
        running: root.drawing
        onRunningChanged: last = Date.now()
        onTriggered: {
            const now = Date.now();
            root.time += Math.min(0.1, Math.max(0, now - last) / 1000);
            last = now;
            root.frames += 1;
        }
    }

    Loader {
        id: sceneLoader
        width: root.sceneWidth
        height: root.sceneHeight
        sourceComponent: root.scene
    }

    Binding {
        target: root.sceneItem
        property: "colors"
        value: root.colors
        when: !!root.sceneItem
    }

    Binding {
        target: root.sceneItem
        property: "dark"
        value: root.colors ? root.darkGround(root.colors.windowOpaque) : true
        when: !!root.sceneItem
    }

    Binding {
        target: root.sceneItem
        property: "time"
        value: root.time
        when: !!root.sceneItem
    }

    // The website's radio pulses its Scene with the song's beat. The browser
    // plays no music, so its Scenes hear none.
    Binding {
        target: root.sceneItem
        property: "beat"
        value: 0
        when: !!root.sceneItem
    }

    Binding {
        target: root.sceneItem
        property: "navigating"
        value: root.reducedMotion ? 0 : Math.max(0, Math.min(1, root.navigating))
        when: !!root.sceneItem
    }

    Binding {
        target: root.sceneItem
        property: "reducedMotion"
        value: root.reducedMotion
        when: !!root.sceneItem
    }

    Binding {
        target: root.sceneItem
        property: "options"
        value: root.sceneItem ? root.options(root.sceneItem.declaredOptions) : ({})
        when: !!root.sceneItem
    }

    Binding {
        target: root.sceneItem
        property: "unlit"
        value: root.unlit
        when: !!root.sceneItem
    }

    // The Scene's picture, a texel to each of its pixels.
    ShaderEffectSource {
        id: picture
        objectName: "sceneDisplay"
        anchors.fill: parent
        visible: !root.glassShown
        sourceItem: sceneLoader
        hideSource: true
        smooth: false
        textureSize: Qt.size(root.sceneWidth, root.sceneHeight)
    }

    // A smaller copy, smoothed, for the glass's bloom.
    ShaderEffectSource {
        id: bloomPicture
        objectName: "bloomPicture"

        readonly property real scale: root.crt ? root.crt.bloom.scale : 1

        sourceItem: root.glassShown ? sceneLoader : null
        smooth: true
        textureSize: Qt.size(Math.max(1, Math.ceil(root.sceneWidth / scale)), Math.max(1, Math.ceil(
                                                                                           root.sceneHeight
                                                                                           / scale)))
    }

    ShaderEffect {
        id: crtGlass
        objectName: "crtGlass"
        anchors.fill: parent
        visible: root.glassShown

        readonly property bool moving: !root.reducedMotion
        property var source: picture
        property var bloom: bloomPicture
        property size sceneSize: Qt.size(root.sceneWidth, root.sceneHeight)
        property real glassHeight: height
        // The frame in the glass's own coordinates, 0 to 1.
        property rect frame: Qt.rect(root.frame.x / Math.max(1, width), root.frame.y / Math.max(1,
                                                                                                height), root.frame.width
                                     / Math.max(1, width), root.frame.height / Math.max(1, height))
        property real bloomMix: root.crt ? root.crt.bloom.opacity : 0
        property real scanEvery: root.crt ? root.crt.scanlines.every : 1
        property real scanShade: root.crt ? root.crt.scanlines.shade : 0
        property real vignetteClear: root.crt ? root.crt.vignette.clear : 1
        property real vignetteShade: root.crt ? root.crt.vignette.shade : 0
        // The refresh band rolls down the picture, and the flicker darkens it
        // a little, never less than its least and never a flash.
        readonly property var band: root.crt ? root.crt.band : null
        readonly property var flickers: root.crt ? root.crt.flicker : null
        property real bandY: band ? ((root.time / band.every) % 1) * root.sceneHeight * band.travel
                                    + root.sceneHeight * band.from : 0
        property real bandReach: band ? Math.max(band.reachAtLeast, root.sceneHeight * band.reach) :
                                        1
        property real bandStrength: moving && band ? band.strength : 0
        property real flicker: moving && flickers ? flickers.least + flickers.range * Math.abs(
                                                        Math.sin(root.time
                                                                 * flickers.frequencies[0])
                                                        * Math.sin(root.time
                                                                   * flickers.frequencies[1])) : 0
        property color bandColour: root.colors ? root.colors.text : "white"

        fragmentShader: "qrc:/omaweb/shaders/crtglass.frag.qsb"
    }
}
