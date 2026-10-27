import QtQuick

// The Scene host: what stands behind the Start page. A Scene is a drawing and
// nothing else (NightRoad.qml says what it receives and declares). The host
// sizes it in the Scene's own pixels, keeps its clock, decides when it may
// draw, and shows its small picture square-pixelled across the host, through
// the CRT glass when the Scene declares one and the reader has not turned it
// off, graded toward a second theme hue with the glass or without it. It is
// the website's host (website/scene.js) in Qt Quick, so the two show one Scene
// the same way.
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
    // The Scene's frames, a step of its clock each, for the tests that check a
    // hidden Scene draws none.
    property int frames: 0
    // Seconds the Scene has run. The clock stops while the Scene does not
    // draw.
    property real time: 0

    readonly property int pitch: root.sceneItem ? root.sceneItem.pitch : 1
    readonly property int sceneWidth: Math.max(1, Math.ceil(root.drawnWidth / root.pitch))
    readonly property int sceneHeight: Math.max(1, Math.ceil(root.height / root.pitch))
    // The display's pixels to each logical one.
    readonly property real devicePixelRatio: Window.window ? Window.window.devicePixelRatio :
                                                             Screen.devicePixelRatio
    // A Scene of text and fine lines, as Telemetry, declares `devicePixels`:
    // its picture is taken at the display's own pixels rather than its own,
    // which a pitch of 1 would still draw soft at a fractional scale.
    readonly property bool devicePixels: !!root.sceneItem && root.sceneItem.devicePixels === true
    readonly property size pictureSize: root.devicePixels ? Qt.size(Math.ceil(root.sceneWidth
                                                                              * root.devicePixelRatio),
                                                                    Math.ceil(root.sceneHeight
                                                                              * root.devicePixelRatio)) :
                                                            Qt.size(root.sceneWidth,
                                                                    root.sceneHeight)
    readonly property bool glassShown: root.glass && !!root.sceneItem && root.sceneItem.glass
                                       === "crt" && !!root.sceneItem.crt
    // The glass's amounts, which a Scene that declares the glass gives as
    // `crt`: share/scenes/crt-road.json's, which the website's glass reads too.
    readonly property var crt: root.glassShown ? root.sceneItem.crt : null
    // A raster Scene is drawn by a beam, one line to each of its rows, through
    // a phosphor mask. A vector Scene's screen traced its lines with the beam
    // and had no raster, and its phosphor persisted: it leaves a trail while
    // the Scene moves, and none under reduced motion.
    readonly property bool vectorScreen: root.glassShown && root.sceneItem.screen === "vector"
    readonly property bool trailShown: root.vectorScreen && root.drawing

    // One of the glass's amounts, or `fallback` where the Scene's `crt` has
    // none: a vector Scene declares no scanlines or mask.
    function amount(group, name, fallback) {
        return root.crt && root.crt[group] ? root.crt[group][name] : fallback;
    }

    // The grade toward a second theme hue, as a sunset: the bright parts take
    // the hue, more of them lower in the frame. A Scene declares its amounts
    // in `crt` with the glass's, and the grade is drawn whether the glass is
    // or not, since it is about colour, not the tube. A Private window's
    // Scene has none, so the window keeps looking different from an ordinary
    // one.
    readonly property var grade: !root.unlit && root.sceneItem && root.sceneItem.crt
                                 && root.sceneItem.crt.grade ? root.sceneItem.crt.grade : null
    readonly property color gradeHue: root.secondHue(root.colors)
    readonly property real gradeTop: root.grade ? root.grade.top : 0
    readonly property real gradeDepth: root.grade ? root.grade.depth : 1
    readonly property vector3d gradeHighlights: root.gradeRange("highlights")
    readonly property vector3d gradeMidtones: root.gradeRange("midtones")

    function gradeRange(name) {
        const range = root.grade ? root.grade[name] : null;
        return range ? Qt.vector3d(range.from, range.to, range.strength) : Qt.vector3d(0, 1, 0);
    }

    // The theme's keyword colour, or, where it would add nothing, its hue
    // within 30 degrees of the accent's or itself nearly grey, the accent's
    // hue turned half way round at the theme's tint.
    function secondHue(colors) {
        const accent = Qt.color(colors && colors.accent ? colors.accent : "white");
        const keyword = colors && colors.syntax && colors.syntax.keyword ? Qt.color(
                                                                               colors.syntax.keyword) :
                                                                           null;
        if (keyword && keyword.hslSaturation >= 0.15) {
            const apart = Math.abs(keyword.hslHue - accent.hslHue);
            if (accent.hslHue < 0 || Math.min(apart, 1 - apart) >= 30 / 360)
                return keyword;
        }
        const tint = colors && colors.tint ? colors.tint : {};
        return Qt.hsla((Math.max(0, accent.hslHue) + 0.5) % 1, tint.saturation !== undefined
                       ? tint.saturation : 0.34, tint.lightness !== undefined ? tint.lightness :
                                                                                0.66, 1);
    }

    // The part of the host the reader sees in the host's own coordinates, 0
    // to 1, which the vignette and the grade are measured in.
    readonly property rect unitFrame: Qt.rect(root.frame.x / Math.max(1, width), root.frame.y
                                              / Math.max(1, height), root.frame.width / Math.max(1,
                                                                                                 width), root.frame.height
                                              / Math.max(1, height))

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

    // Whether a frame is due at `now`, the last having been drawn at `last`,
    // both in milliseconds, for a Scene of `fps` frames a second. A little
    // early counts, so thirty frames a second land on every other frame of a
    // sixty hertz display rather than slipping to every third.
    function frameDue(now, last, fps) {
        return !fps || now - last >= 1000 / fps - 2;
    }

    // The clock advances on the window's frames, as the website's does on the
    // browser's, and the Scene draws on those that are due. A frame that is
    // not due changes nothing, and the window draws nothing for it. The clock
    // advances by the time that passed, at most a tenth of a second, so a
    // stalled frame does not leap the road forward.
    FrameAnimation {
        id: clock

        // When the Scene last drew, in milliseconds of the animation, or
        // before its first frame the frame before.
        property real drawn: -1

        running: root.drawing
        onRunningChanged: drawn = -1
        onTriggered: {
            const now = elapsedTime * 1000;
            if (drawn < 0)
                drawn = now - frameTime * 1000;
            else if (!root.frameDue(now, drawn, root.sceneItem ? root.sceneItem.fps : 0))
                return;
            root.time += Math.min(0.1, Math.max(0, now - drawn) / 1000);
            drawn = now;
            root.frames += 1;
            if (root.trailShown)
                trailPicture.scheduleUpdate();
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

    // The Scene's picture, a texel to each of its pixels. It is drawn under
    // the grade too, which covers it, so a renderer that draws no shader, as
    // the software one, still shows the Scene.
    ShaderEffectSource {
        id: picture
        objectName: "sceneDisplay"
        anchors.fill: parent
        visible: !root.glassShown
        sourceItem: sceneLoader
        hideSource: true
        smooth: false
        textureSize: root.pictureSize
    }

    // With the glass off, the picture graded.
    ShaderEffect {
        id: sceneGrade
        objectName: "sceneGrade"
        anchors.fill: parent
        visible: !root.glassShown && !!root.grade

        property var source: picture
        property rect frame: root.unitFrame
        property color gradeHue: root.gradeHue
        property real gradeTop: root.gradeTop
        property real gradeDepth: root.gradeDepth
        property vector3d gradeHighlights: root.gradeHighlights
        property vector3d gradeMidtones: root.gradeMidtones

        fragmentShader: "qrc:/omaweb/shaders/grade.frag.qsb"
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

    // A copy at the Scene's full size with its mipmaps, for the halation: a
    // blurred level is then an average of the picture, where a smaller copy
    // would pick points from it and put the glow off what casts it.
    ShaderEffectSource {
        id: haloPicture
        objectName: "haloPicture"
        sourceItem: root.glassShown ? sceneLoader : null
        smooth: true
        mipmap: true
        textureSize: Qt.size(root.sceneWidth, root.sceneHeight)
    }

    // The halation cut at its threshold before the glass blurs it, as the
    // website cuts it: a texel to each `reach` logical pixels, the mipmap
    // level that averages so many of the Scene's.
    ShaderEffect {
        id: haloCut
        objectName: "haloCut"

        // How many of the Scene's pixels a texel of the cut copy spans.
        readonly property real span: Math.max(1, root.amount("halation", "reach", 1) / root.pitch)

        width: Math.max(1, Math.ceil(root.sceneWidth / span))
        height: Math.max(1, Math.ceil(root.sceneHeight / span))
        visible: root.glassShown

        property var source: haloPicture
        property real lod: Math.log2(span)
        property real threshold: root.amount("halation", "threshold", 0)
        property real sceneHeight: root.sceneHeight
        property real bandY: crtGlass.bandY
        property real bandReach: crtGlass.bandReach
        property real bandStrength: crtGlass.bandStrength * crtGlass.raster
        property color bandColour: crtGlass.bandColour
        property real flicker: crtGlass.flicker

        fragmentShader: "qrc:/omaweb/shaders/halation.frag.qsb"
    }

    // The cut copy, which the glass spreads smoothly over the picture.
    ShaderEffectSource {
        id: haloCutPicture
        objectName: "haloCutPicture"
        sourceItem: root.glassShown ? haloCut : null
        hideSource: true
        smooth: true
        textureSize: Qt.size(haloCut.width, haloCut.height)
    }

    // A vector Scene's persistence: each frame keeps the brighter of the
    // picture and the last frame, faded. The copy is taken at the Scene's
    // frames rather than live, since a live copy of itself would draw the
    // window at the display's rate.
    ShaderEffectSource {
        id: trailPicture
        objectName: "trailPicture"
        sourceItem: root.trailShown ? trailEffect : null
        recursive: true
        live: false
        hideSource: true
        smooth: false
        textureSize: Qt.size(root.sceneWidth, root.sceneHeight)
        onSourceItemChanged: scheduleUpdate()
        onTextureSizeChanged: scheduleUpdate()
    }

    ShaderEffect {
        id: trailEffect
        objectName: "trailEffect"
        width: root.sceneWidth
        height: root.sceneHeight
        visible: root.trailShown

        property var current: picture
        property var previous: trailPicture
        property real fade: root.amount("trail", "fade", 0)

        fragmentShader: "qrc:/omaweb/shaders/trail.frag.qsb"
    }

    ShaderEffect {
        id: crtGlass
        objectName: "crtGlass"
        anchors.fill: parent
        visible: root.glassShown

        readonly property bool moving: !root.reducedMotion
        property var source: root.trailShown ? trailPicture : picture
        property var bloom: bloomPicture
        property var halo: haloCutPicture
        property size sceneSize: Qt.size(root.sceneWidth, root.sceneHeight)
        property real glassHeight: height
        property rect frame: root.unitFrame
        property real bloomMix: root.crt ? root.crt.bloom.opacity : 0
        // 1 on a raster Scene, 0 on a vector one, which has no scanlines,
        // mask or band.
        property real raster: root.vectorScreen ? 0 : 1
        // The picture is brightened by the beam's and the mask's lifts before
        // it is clipped, as the website's filter brightens it. A vector Scene
        // declares neither, and the website lifts it not at all.
        property real lift: root.amount("scanlines", "lift", 1) * root.amount("mask", "lift", 1)
        // The beam's lines stand one to each of the Scene's rows, each as wide
        // as the beam half way between its narrowest and widest, as the
        // website's one layer of lines draws them. A Scene drawn at device
        // pixels has rows too fine for a line each: its lines stand its
        // scanlines' `pitch` of device pixels apart, 6 as the road's at 1.5x.
        property real scanPeriod: root.devicePixels ? root.amount("scanlines", "pitch", 6)
                                                      / root.devicePixelRatio : height
                                                      / root.sceneHeight
        property real scanShade: root.amount("scanlines", "shade", 0)
        property real beamWidth: (root.amount("scanlines", "narrowest", 1) + root.amount("scanlines",
                                                                                         "widest", 1))
                                 / 2
        property real beamFalloff: root.amount("scanlines", "falloff", 1)
        property real maskDim: root.amount("mask", "dim", 1)
        property real haloStrength: root.amount("halation", "strength", 0)
        // The halation is blurred by the website's `spacing` logical pixels,
        // whatever the Scene's pitch: a faceplate scatters light over a
        // physical distance. The glass's binomial taps spread that far when
        // they stand the square root of 2 times it apart.
        readonly property real haloSpacing: root.amount("halation", "spacing", 0) / root.pitch
                                            * Math.SQRT2
        property size haloStep: Qt.size(haloSpacing / root.sceneWidth, haloSpacing
                                        / root.sceneHeight)
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
        property color gradeHue: root.gradeHue
        property real gradeTop: root.gradeTop
        property real gradeDepth: root.gradeDepth
        property vector3d gradeHighlights: root.gradeHighlights
        property vector3d gradeMidtones: root.gradeMidtones

        fragmentShader: "qrc:/omaweb/shaders/crtglass.frag.qsb"
    }
}
