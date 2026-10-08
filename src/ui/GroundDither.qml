import QtQuick

// The dither under a translucent ground, so the blurred desktop seen through
// it does not band (#647). Through a ground at 0.95 a wallpaper's gradient
// arrives as a few output levels over a wide area, and the window system's own
// noise, added before the window's alpha, is scaled down with it to nothing.
//
// A surface takes it as its first visual child and fills itself with `fill`:
//
//     Rectangle {
//         color: dither.fill
//         GroundDither { id: dither; ground: root.colors.overlay }
//     }
//
// Where the ground lets the desktop through, the dither draws the ground
// itself, inside the surface's border and rounded corners, with the noise
// added after the ground's alpha, and the surface's own fill is left clear.
// An opaque ground has nothing behind it to band, and a clear one leaves the
// window system's noise at full strength, so neither has a shader in the scene
// and the surface fills itself as before. So does every ground under a
// renderer that runs no shaders.
//
// The ground's alpha decides, not which surface it is, so a theme's opacities
// and the reader's own for the sidebar are both honoured.
Item {
    id: root

    property color ground: "transparent"
    // Whether the desktop is what shows through. A ground over a blurred page
    // has Omaweb's own pixels behind it, which are not the desktop's to band.
    property bool overDesktop: true
    // Whether the renderer runs shaders; the software one does not.
    property bool runsShaders: GraphicsInfo.shaderType === GraphicsInfo.RhiShader

    readonly property bool drawn: root.runsShaders && root.overDesktop && root.ground.a > 0
                                  && root.ground.a < 1
    readonly property color fill: root.drawn ? "transparent" : root.ground

    readonly property Item shader: loader.item

    readonly property Rectangle surface: root.parent as Rectangle
    // How far in from each edge the ground starts: inside the surface's
    // border. The kit's gradient or per-side border is drawn by a child of its
    // own, declared before this one, so a surface with one passes its widths.
    property real leftInset: root.surface ? root.surface.border.width : 0
    property real topInset: root.leftInset
    property real rightInset: root.leftInset
    property real bottomInset: root.leftInset

    // Drawn before the surface's other children by being declared first. A
    // negative z drew nothing on a surface with no fill of its own.
    anchors.fill: parent
    anchors.leftMargin: root.leftInset
    anchors.topMargin: root.topInset
    anchors.rightMargin: root.rightInset
    anchors.bottomMargin: root.bottomInset

    function headroom(channel: real): real {
        return Math.max(0, Math.min(1 / 255, channel, root.ground.a - channel));
    }

    Loader {
        id: loader
        anchors.fill: parent
        // Nothing is drawn on a hidden surface.
        active: root.drawn && root.visible

        sourceComponent: ShaderEffect {

            property size itemSize: Qt.size(width, height)
            property real radius: root.surface ? Math.max(0, root.surface.radius - Math.max(
                                                              root.leftInset, root.topInset,
                                                              root.rightInset, root.bottomInset)) :
                                                 0
            property vector4d premultiplied: Qt.vector4d(root.ground.r * root.ground.a, root.ground.g
                                                         * root.ground.a, root.ground.b
                                                         * root.ground.a, root.ground.a)
            // How far the noise reaches on each premultiplied channel: one
            // output level, or less where the channel is nearer 0 or the
            // ground's alpha. A premultiplied channel past the alpha is not a
            // colour: Qt's unpremultiply wraps it past 255 to 0, a speckle (#647).
            // A straight channel at 255 has no room and is not dithered.
            property vector3d amplitude: Qt.vector3d(root.headroom(premultiplied.x), root.headroom(
                                                         premultiplied.y), root.headroom(
                                                         premultiplied.z))

            fragmentShader: "qrc:/omaweb/shaders/dither.frag.qsb"
        }
    }
}
