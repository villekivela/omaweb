import QtQuick
import QtQuick.Effects

// The wash under an Omaweb surface that takes the whole page area — the shortcut
// sheet, the settings page. The page it covers is blurred where it lies and the
// surface's own colour goes on top, so the reader can still see what they left
// behind instead of losing it behind a solid plate, and without being asked to
// read a webpage through a list of settings.
//
// With no page to sample — a resting Space — only the tint is drawn, and the
// window's own native backdrop blurs the desktop behind it exactly as it does
// for the sidebar. That is why the tint is the sidebar's: a surface that takes
// the whole page area is a plate of the same kind, not a dialog floating over
// one, and the two read as one window rather than two materials.
//
// `source` must not be an ancestor of this item, or the effect would feed on
// its own output. Nothing is sampled while the surface is hidden: a live
// texture of the whole viewport is not worth a frame nobody sees.
Item {
    id: root

    property Item source: null
    property color tint: "transparent"

    readonly property bool sampling: source !== null && source.visible && root.visible

    // Which part of the source lies under this surface, in the source's own
    // coordinates. Stated rather than left to the default: an empty sourceRect
    // means "the whole source item", which is the right answer only while the
    // two share a size and an origin. They do today — both fill the page area —
    // and a caller that insets this surface would otherwise get a sample
    // silently stretched to fit rather than the part it actually covers.
    readonly property rect sampleRect: {
        if (!root.sampling)
            return Qt.rect(0, 0, 0, 0)
        const origin = root.mapToItem(root.source, 0, 0)
        return Qt.rect(origin.x, origin.y, Math.max(1, root.width), Math.max(1, root.height))
    }

    ShaderEffectSource {
        id: pageTexture
        visible: false
        live: root.sampling
        hideSource: false
        recursive: false
        sourceItem: root.sampling ? root.source : null
        sourceRect: root.sampleRect
        width: Math.max(1, root.width)
        height: Math.max(1, root.height)
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
