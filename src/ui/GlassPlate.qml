import QtQuick
import QtQuick.Effects

// A plate of glass: what stands behind it, blurred, under a see-through tint,
// as the website's Omnibar is glass over its road. Without anything to blur
// it is the tint alone.
Item {
    id: root

    // What stands behind the plate. It must not be an ancestor of the plate,
    // or the blur would feed on its own output.
    property Item sourceItem: null
    // The part of `sourceItem` the plate covers, in its coordinates.
    property rect sourceRect: Qt.rect(0, 0, width, height)
    // The tint laid over the blur.
    property color color: "transparent"
    property real radius: 0
    // How far the blur reaches, in logical pixels.
    property real blur: 48

    readonly property bool blurred: root.sourceItem !== null && root.sourceItem.visible

    ShaderEffectSource {
        id: backdrop
        visible: false
        live: true
        hideSource: false
        recursive: false
        sourceItem: root.blurred ? root.sourceItem : null
        sourceRect: root.blurred ? root.sourceRect : Qt.rect(0, 0, 0, 0)
        width: Math.max(1, root.width)
        height: Math.max(1, root.height)
        // Half the plate's pixels: the blur loses nothing a full-sized copy
        // would have kept.
        textureSize: Qt.size(Math.max(1, Math.round(root.width / 2)), Math.max(1, Math.round(
                                                                                   root.height
                                                                                   / 2)))
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.blurred
        source: backdrop
        blurEnabled: true
        blur: 1
        blurMax: Math.round(root.blur)
        // The blur stops at this item's own edge. Left to itself MultiEffect
        // enlarges what it draws to fit the blur, which would reach out over
        // the plate's border and soften it.
        autoPaddingEnabled: false
        // Keeps the blur inside the plate's rounded corners.
        maskEnabled: true
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
        color: root.color
    }
}
