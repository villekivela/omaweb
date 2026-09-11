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
// `source` must not be an ancestor of this item, or the effect would feed on
// its own output. Nothing is sampled while the surface is hidden: a live
// texture of the whole viewport is not worth a frame nobody sees.
Item {
    id: root

    property Item source: null
    property color tint: "transparent"
    // The source's coordinates are explicit because this surface may be
    // reparented above the item it samples. Full-page surfaces share their
    // source's origin and need no override.
    property rect sourceRect: Qt.rect(0, 0, width, height)
    property real textureScale: 1

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
        blurMax: 48
        // The blur stops at the surface's own edge rather than reaching past
        // it: left to itself MultiEffect enlarges what it draws to fit the
        // blur, which spills over whatever frames the surface.
        autoPaddingEnabled: false
    }

    Rectangle {
        anchors.fill: parent
        color: root.tint
    }
}
