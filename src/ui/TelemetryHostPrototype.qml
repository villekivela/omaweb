import QtQuick
import QtQuick.Window

// PROTOTYPE (#814), throwaway: the real Scene host and its CRT glass, with two
// things changed for the Telemetry Scene only, from outside, so SceneHost.qml
// and the shaders stay as they are (#812 owns them):
//
// - the Scene's picture is taken at device pixels, not logical ones, so text
//   and lines are drawn at the display's full resolution;
// - one scanline every `scanDevicePixels` device pixels, rather than one to
//   each of the Scene's rows, so a glyph spans several scanlines.
//
// The implementation would make these two SceneHost parameters instead.
SceneHost {
    id: host

    property bool fullResolution: false
    property real scanDevicePixels: 6
    readonly property real ratio: Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1

    function named(item, name) {
        for (const child of item.children) {
            if (child.objectName === name)
                return child;
        }
        return null;
    }

    Component.onCompleted: {
        const picture = host.named(host, "sceneDisplay");
        const glass = host.named(host, "crtGlass");
        picture.textureSize = Qt.binding(() => host.fullResolution ? Qt.size(Math.ceil(
                                                                                 host.sceneWidth
                                                                                 * host.ratio),
                                                                             Math.ceil(
                                                                                 host.sceneHeight
                                                                                 * host.ratio)) :
                                                                     Qt.size(host.sceneWidth,
                                                                             host.sceneHeight));
        glass.scanPeriod = Qt.binding(() => host.fullResolution ? host.scanDevicePixels
                                                                  / host.ratio : glass.height
                                                                  / host.sceneHeight);
    }
}
