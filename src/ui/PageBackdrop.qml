import QtQuick
import QtQuick.Effects

// The wash under an Omaweb surface that takes the whole page area — the shortcut
// sheet, the settings page. The page it covers is blurred where it lies and the
// surface's own colour goes on top, so the reader can still see what they left
// behind instead of losing it behind a solid plate, and without being asked to
// read a webpage through a list of settings.
//
// With no page to sample, a resting Space, only the tint is drawn and the
// desktop behind it is left to the window system, which blurs it or not exactly
// as it does for the sidebar. That is why the tint is the sidebar's: a surface
// that takes the whole page area is a plate of the same kind, not a dialog
// floating over one, and the two read as one window rather than two materials.
//
// A bar at the top of the page, a page question or a page prompt, stands on
// one too, with no tint: its own translucent ground goes on top, and the blur
// keeps the page's text from being read through it. The Omnibar's glass is
// one as well, with rounded corners, blurring the page or, at rest, the road.
//
// `source` must not be an ancestor of this item, or the effect would feed on
// its own output. Nothing is sampled while the surface is hidden: a live
// texture of the whole viewport is not worth a frame nobody sees.
Item {
    id: root

    property Item source: null
    property color tint: "transparent"
    // The source's coordinates are explicit because this surface may be
    // reparented above the item it samples. Full-page surfaces and the bars at
    // the top of the page share their source's origin and need no override.
    property rect sourceRect: Qt.rect(0, 0, width, height)
    property real textureScale: 1
    // How far the blur reaches, and the corners the blur and the tint are
    // kept inside.
    property real blur: 48
    property real radius: 0

    readonly property bool sampling: source !== null && source.visible && root.visible

    ShaderEffectSource {
        id: pageTexture
        visible: false
        live: root.sampling
        hideSource: false
        recursive: false
        sourceItem: root.sampling ? root.source : null
        sourceRect: root.sampling ? root.sourceRect : Qt.rect(0, 0, 0, 0)
        width: Math.max(1, root.width)
        height: Math.max(1, root.height)
        textureSize: Qt.size(Math.max(1, Math.round(width * root.textureScale)), Math.max(1, Math.round(
                                                                                              height * root.textureScale)))
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.sampling
        source: pageTexture
        blurEnabled: true
        blur: 1
        blurMax: Math.round(root.blur)
        // The blur stops at the surface's own edge rather than reaching past
        // it: left to itself MultiEffect enlarges what it draws to fit the
        // blur, which spills over whatever frames the surface.
        autoPaddingEnabled: false
        maskEnabled: root.radius > 0
        maskSource: ShaderEffectSource {
            sourceItem: Rectangle {
                width: Math.max(1, root.width)
                height: Math.max(1, root.height)
                radius: root.radius
                color: "black"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.tint
    }
}
