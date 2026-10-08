import QtQuick
import QtTest
import "../../src/ui" as Omaweb

// The dither a translucent ground carries over the desktop (#647). Over a
// blurred wallpaper a ground at 0.95 lets a gradient through as a few output
// levels, which show as bands; the dither breaks them up. It is drawn only
// where the ground lets the desktop through and the renderer runs shaders,
// and then it draws the ground itself, so its noise lands after the ground's
// alpha rather than being scaled down by it.
//
// The offscreen suite renders without shaders, so these tests say the
// renderer has them and check what would be drawn. The pixels themselves are
// checked on a real desktop, in the pull request.
TestCase {
    id: testCase
    name: "GroundDither"
    when: windowShown

    // A TestCase is not visible itself, and a hidden surface draws no dither,
    // so the surfaces stand in a window of their own.
    Window {
        id: stage
        width: 400
        height: 300
        visible: true
    }

    Component {
        id: surfaceComponent

        Rectangle {
            property alias dither: dither

            width: 300
            height: 200
            radius: 6
            border.width: 1
            border.color: "#9b87ff"
            color: dither.fill

            Omaweb.GroundDither {
                id: dither
                runsShaders: true
                ground: Qt.rgba(0.2, 0.4, 0.6, 0.92)
            }
        }
    }

    // A translucent ground is drawn by the dither, inside the border and its
    // rounded corners, from the ground's own colour premultiplied by its
    // alpha, as the surface's fill would have been.
    function test_aTranslucentGroundIsDrawnByTheDither() {
        const surface = createTemporaryObject(surfaceComponent, stage.contentItem);
        const dither = surface.dither;
        verify(dither.drawn);
        compare(surface.color, Qt.rgba(0, 0, 0, 0));
        const shader = dither.shader;
        verify(shader !== null);
        compare(shader.mapToItem(surface, 0, 0), Qt.point(1, 1));
        compare(shader.width, 298);
        compare(shader.height, 198);
        compare(shader.radius, 5);
        fuzzyCompare(shader.premultiplied.x, 0.2 * 0.92, 0.003);
        fuzzyCompare(shader.premultiplied.y, 0.4 * 0.92, 0.003);
        fuzzyCompare(shader.premultiplied.z, 0.6 * 0.92, 0.003);
        fuzzyCompare(shader.premultiplied.w, 0.92, 0.003);
    }

    // An opaque ground lets nothing through to band, and a clear one leaves
    // the desktop to the window system's own noise: neither has a dither in
    // the scene, and the surface fills itself.
    function test_anOpaqueOrClearGroundIsNotDithered_data() {
        return [
                    {
                        tag: "opaque",
                        ground: Qt.rgba(0.2, 0.4, 0.6, 1)
                    },
                    {
                        tag: "clear",
                        ground: Qt.rgba(0.2, 0.4, 0.6, 0)
                    }
                ];
    }

    function test_anOpaqueOrClearGroundIsNotDithered(data) {
        const surface = createTemporaryObject(surfaceComponent, stage.contentItem);
        surface.dither.ground = data.ground;
        verify(!surface.dither.drawn);
        compare(surface.color, data.ground);
        compare(surface.dither.shader, null);
    }

    // A border a child draws over the surface's edges, as the kit's gradient
    // or per-side border is, has its own width on each side; the dither keeps
    // inside it rather than painting over it.
    function test_theDitherKeepsInsideABorderDrawnPerSide() {
        const surface = createTemporaryObject(surfaceComponent, stage.contentItem);
        const dither = surface.dither;
        dither.leftInset = 4;
        dither.topInset = 2;
        dither.rightInset = 3;
        dither.bottomInset = 1;
        verify(dither.drawn);
        const shader = dither.shader;
        compare(shader.mapToItem(surface, 0, 0), Qt.point(4, 2));
        compare(shader.width, 293);
        compare(shader.height, 197);
        compare(shader.radius, 2);
    }

    // A ground over a blurred page has Omaweb's own pixels behind it rather
    // than the desktop, so it is not dithered.
    function test_aGroundOverAPageIsNotDithered() {
        const surface = createTemporaryObject(surfaceComponent, stage.contentItem);
        surface.dither.overDesktop = false;
        verify(!surface.dither.drawn);
        compare(surface.color, surface.dither.ground);
        compare(surface.dither.shader, null);
    }

    // A hidden surface draws nothing, so its dither has no shader in the
    // scene, and it is back as the surface shows again.
    function test_aHiddenSurfaceHasNoShader() {
        const surface = createTemporaryObject(surfaceComponent, stage.contentItem);
        surface.visible = false;
        compare(surface.dither.shader, null);
        surface.visible = true;
        verify(surface.dither.shader !== null);
    }

    // A renderer without shaders, as the software one, cannot draw the
    // dither, so the surface keeps its own fill.
    function test_noShadersLeaveTheGroundToTheSurface() {
        const surface = createTemporaryObject(surfaceComponent, stage.contentItem);
        surface.dither.runsShaders = false;
        verify(!surface.dither.drawn);
        compare(surface.color, surface.dither.ground);
        compare(surface.dither.shader, null);
    }
}
