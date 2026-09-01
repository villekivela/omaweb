import QtQuick
import QtQuick.Effects
import Tanto
import qs.Commons

Rectangle {
    id: root

    property var colors
    property url siteUrl
    property url iconUrl
    property bool highlighted: false
    property bool useArtwork: true
    property bool tintArtwork: true

    // Set where the mark is all the tile draws, as on a pinned tab: no plate
    // behind it, and the mark itself in the site's colour at full strength
    // rather than a chip the colour has to be muted to sit inside.
    property bool siteColoredMark: false

    readonly property string host: {
        const value = String(siteUrl).replace(/^[a-z]+:\/\//, "")
        return value.split("/")[0].split(":")[0]
    }

    readonly property string code: {
        const parts = host.split(".").filter(function(part) { return part !== "www" })
        const name = parts.length > 1 ? parts[parts.length - 2] : parts[0]
        return name.length > 0 ? name.substring(0, 2).toUpperCase() : "··"
    }

    // The theme owns how strong a site colour may be; only the hue is the
    // site's to choose, so every tab stays one palette.
    readonly property real tintSaturation: colors.tint && colors.tint.saturation !== undefined
        ? colors.tint.saturation : 0.32
    readonly property real tintLightness: colors.tint && colors.tint.lightness !== undefined
        ? colors.tint.lightness : 0.62

    // Site identity for artwork: the hue comes from the host, which is the only
    // thing available before the artwork itself has loaded.
    readonly property color hostTint: {
        let hash = 0
        for (let index = 0; index < host.length; ++index) {
            hash = (hash * 31 + host.charCodeAt(index)) % 3600
        }
        return Qt.hsla(hash / 3600, tintSaturation, tintLightness, 1)
    }

    // With artwork switched off the chip is all the site gets, so it takes the
    // site's own colour: the hue of the favicon Tanto is not drawing. An icon
    // with no colour to give — a white or black mark, or none at all — leaves
    // the chip neutral rather than inventing one.
    readonly property color chipTint: useArtwork
        ? hostTint
        : (faviconTint.valid ? faviconTint.color : colors.mutedText)

    readonly property bool showsArtwork: useArtwork && artwork.status === Image.Ready

    // The colour a whole surface may take for this site: the favicon's own
    // hue where the icon offers one, the hashed host hue where it does not,
    // so a site that has yet to load an icon is still told apart.
    readonly property color siteTint: faviconTint.valid ? faviconTint.color : hostTint

    // Whether the site has actually said what colour it is. A hashed hue is a
    // way of telling tiles apart, not the site's own colour, so callers that
    // paint the site's colour onto their own surface wait for this.
    readonly property bool hasSiteColor: faviconTint.valid

    FaviconTint {
        id: faviconTint
        source: (root.siteColoredMark || !root.useArtwork) ? root.iconUrl : ""
        saturation: root.tintSaturation
        lightness: root.tintLightness
    }

    implicitWidth: 20
    implicitHeight: 20
    radius: 2
    // The chip is the stand-in for missing artwork. Real artwork needs no
    // plate behind it: tinted, its own shape already reads against the sidebar.
    color: showsArtwork || siteColoredMark
        ? "transparent"
        : (highlighted ? chipTint : Qt.rgba(chipTint.r, chipTint.g, chipTint.b, 0.18))
    Accessible.ignored: true

    Image {
        id: artwork
        anchors.centerIn: parent
        width: parent.width
        height: width
        source: root.useArtwork ? root.iconUrl : ""
        sourceSize.width: 32
        sourceSize.height: 32
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        smooth: true
        visible: root.showsArtwork && !root.tintArtwork
    }

    // Tinting keeps the site's shape but brings its artwork into the sidebar
    // palette. The setting can leave the original colours intact instead.
    MultiEffect {
        anchors.fill: artwork
        source: artwork
        visible: root.showsArtwork && root.tintArtwork
        saturation: -1.0
        colorization: 1.0
        colorizationColor: root.siteTint
    }

    Text {
        anchors.centerIn: parent
        visible: !root.showsArtwork
        text: root.code
        color: root.siteColoredMark
            ? (root.hasSiteColor ? root.siteTint : root.colors.mutedText)
            : (root.highlighted ? root.colors.windowOpaque : root.chipTint)
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.weight: Font.DemiBold
    }
}
