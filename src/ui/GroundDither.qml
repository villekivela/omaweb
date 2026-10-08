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
    // Whether the surface's border is drawn by a child declared before this
    // one, as the kit's BorderOverlay is, which the dither would paint over.
    property bool overlaidBorder: false
    // Whether the renderer runs shaders; the software one does not.
    property bool shaders: GraphicsInfo.shaderType === GraphicsInfo.RhiShader

    readonly property bool drawn: root.shaders && root.overDesktop && !root.overlaidBorder
                                  && root.ground.a > 0 && root.ground.a < 1
    // What the surface fills itself with.
    readonly property color fill: root.drawn ? "transparent" : root.ground

    // The shader drawing the ground, while there is one.
    readonly property Item shader: loader.item

    readonly property Rectangle surface: root.parent as Rectangle
    readonly property real inset: root.surface ? root.surface.border.width : 0

    anchors.fill: parent
    // Drawn before the surface's other children by being declared first. A
    // negative z drew nothing on a surface with no fill of its own.
    anchors.margins: root.inset

    Loader {
        id: loader
        anchors.fill: parent
        active: root.drawn

        sourceComponent: ShaderEffect {
            objectName: "groundDither"

            property size itemSize: Qt.size(width, height)
            property real radius: root.surface ? Math.max(0, root.surface.radius - root.inset) : 0
            property vector4d premultiplied: Qt.vector4d(root.ground.r * root.ground.a, root.ground.g
                                                         * root.ground.a, root.ground.b
                                                         * root.ground.a, root.ground.a)

            fragmentShader: "qrc:/omaweb/shaders/dither.frag.qsb"
        }
    }
}
