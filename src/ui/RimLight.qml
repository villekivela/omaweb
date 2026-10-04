import QtQuick

// The Scene's light on a plate's rim: the gradient the Scene casts, centred on
// its sun, seen through the plate's one-pixel border, and the bloom over it,
// as the website's Omnibar catches the sun (website/styles.css). Everything it
// draws by is the Scene's `light`, which share/scenes/crt-road.json describes
// for the road; this is only how it is drawn. It is laid over the plate it
// lights, reaching past the plate's edge as far as the bloom does, and draws
// nothing while no Scene's light falls on it.
//
// The website's bloom is under the Omnibar's text. Its half inside the plate
// is drawn by a second RimLight that is `inner`: a child of the plate, under
// its content, and clipped with it.
//
// Only the bloom's opacity follows the beat, so a moving beat changes one
// uniform and paints nothing again.
ShaderEffect {
    id: root

    // The Scene's light, or null where no Scene stands behind the plate.
    property var light: null
    // The plate lit, which this item is laid over, and its corner radius.
    property Item plate: null
    property real plateRadius: 0
    // The sun's centre, in the coordinates of the item the plate stands in,
    // or of the plate itself for the inner half.
    property point sun: Qt.point(0, 0)
    // Draws the bloom's half inside the plate, from inside it, rather than the
    // rim and the bloom's half outside.
    property bool inner: false
    // How much of the light is drawn, from 0 to 1: the rim and the bloom
    // fade together.
    property real strength: 1

    // How far the bloom reaches past the plate's edge: half its band and
    // three sigmas of its blur.
    readonly property real reach: root.light ? Math.ceil(root.light.bloom.width / 2 + 3
                                                         * root.light.bloom.blur) : 0
    readonly property var stops: root.light ? root.light.stops : []

    // A stop past the last repeats it, so the shader's unused slots hold
    // the gradient's end.
    function nthStop(index) {
        return root.stops[Math.min(index, root.stops.length - 1)];
    }

    function stop(index) {
        const found = root.nthStop(index);
        if (!found)
            return Qt.vector4d(0, 0, 0, 0);
        const c = found.colour;
        return Qt.vector4d(c.r * c.a, c.g * c.a, c.b * c.a, c.a);
    }

    function at(index) {
        const found = root.nthStop(index);
        return found ? found.position : 1;
    }

    visible: root.light !== null && root.plate !== null
    x: (root.plate && !root.inner ? root.plate.x : 0) - root.reach
    y: (root.plate && !root.inner ? root.plate.y : 0) - root.reach
    width: root.plate ? root.plate.width + 2 * root.reach : 0
    height: root.plate ? root.plate.height + 2 * root.reach : 0
    // Inside the plate it already fades with it.
    opacity: (root.plate && !root.inner ? root.plate.opacity : 1) * root.strength

    property size itemSize: Qt.size(root.width, root.height)
    property rect plateRect: Qt.rect(root.reach, root.reach, root.plate ? root.plate.width : 0,
                                     root.plate ? root.plate.height : 0)
    property vector4d plateArea: Qt.vector4d(plateRect.x, plateRect.y, plateRect.width,
                                             plateRect.height)
    property point sunCentre: Qt.point(root.sun.x - root.x, root.sun.y - root.y)
    property size radii: root.light ? Qt.size(Math.max(1, root.light.across * plateRect.width),
                                              Math.max(1, root.light.reach)) : Qt.size(1, 1)
    // The shader takes six stops; a Scene that names more has its last ones
    // left out.
    property vector4d stop0: root.stop(0)
    property vector4d stop1: root.stop(1)
    property vector4d stop2: root.stop(2)
    property vector4d stop3: root.stop(3)
    property vector4d stop4: root.stop(4)
    property vector4d stop5: root.stop(5)
    property vector4d at0: Qt.vector4d(root.at(0), root.at(1), root.at(2), root.at(3))
    property point at1: Qt.point(root.at(4), root.at(5))
    property real stopCount: Math.min(6, root.stops.length)
    property real bloomWidth: root.light ? root.light.bloom.width : 0
    property real bloomBlur: root.light ? root.light.bloom.blur : 0
    property real bloomOpacity: root.light ? root.light.bloom.opacity : 0
    property real innerHalf: root.inner ? 1 : 0

    fragmentShader: "qrc:/omaweb/shaders/rimlight.frag.qsb"
}
