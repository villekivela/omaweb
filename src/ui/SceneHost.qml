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

    readonly property Item sceneItem: sceneLoader.item
    // Frames drawn, for the tests that check a hidden Scene draws none.
    property int frames: 0
    // Seconds the Scene has run. The clock stops while the Scene does not
    // draw.
    property real time: 0

    readonly property int pitch: root.sceneItem ? root.sceneItem.pitch : 1
    readonly property int sceneWidth: Math.max(1, Math.ceil(root.width / root.pitch))
    readonly property int sceneHeight: Math.max(1, Math.ceil(root.height / root.pitch))
    readonly property bool glassShown: root.glass && !!root.sceneItem && root.sceneItem.glass
                                       === "crt"

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

    // A third-size copy, smoothed, for the glass's bloom.
    ShaderEffectSource {
        id: bloomPicture
        sourceItem: root.glassShown ? sceneLoader : null
        smooth: true
        textureSize: Qt.size(Math.max(1, Math.ceil(root.sceneWidth / 3)), Math.max(1, Math.ceil(
                                                                                       root.sceneHeight
                                                                                       / 3)))
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
        // The refresh band rolls down every seven seconds.
        property real bandY: ((root.time / 7) % 1) * root.sceneHeight * 1.4 - root.sceneHeight * 0.2
        property real bandReach: Math.max(4, root.sceneHeight * 0.07)
        property real bandStrength: moving ? 0.05 : 0
        // The flicker darkens by 2 to 4.5 percent and never flashes.
        property real flicker: moving ? 0.02 + 0.025 * Math.abs(Math.sin(root.time * 37.1)
                                                                * Math.sin(root.time * 11.3)) : 0
        property color bandColour: root.colors ? root.colors.text : "white"

        fragmentShader: "qrc:/omaweb/shaders/crtglass.frag.qsb"
    }
}
