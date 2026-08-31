import QtQuick
import QtQuick.Effects
import qs.Commons

Rectangle {
    id: root

    property var colors
    property url siteUrl
    property url iconUrl
    property bool highlighted: false
    property bool useArtwork: true
    property bool tintArtwork: true

    readonly property string host: {
        const value = String(siteUrl).replace(/^[a-z]+:\/\//, "")
        return value.split("/")[0].split(":")[0]
    }

    readonly property string code: {
        const parts = host.split(".").filter(function(part) { return part !== "www" })
        const name = parts.length > 1 ? parts[parts.length - 2] : parts[0]
        return name.length > 0 ? name.substring(0, 2).toUpperCase() : "··"
    }

    // Site identity without site artwork: the hue comes from the host, the
    // saturation and lightness from the theme, so tabs stay one palette.
    readonly property color tint: {
        let hash = 0
        for (let index = 0; index < host.length; ++index) {
            hash = (hash * 31 + host.charCodeAt(index)) % 3600
        }
        const tuning = colors.tint
        return Qt.hsla(hash / 3600,
            tuning && tuning.saturation !== undefined ? tuning.saturation : 0.32,
            tuning && tuning.lightness !== undefined ? tuning.lightness : 0.62,
            1)
    }

    readonly property bool showsArtwork: useArtwork && artwork.status === Image.Ready

    implicitWidth: 20
    implicitHeight: 20
    radius: 2
    // The chip is the stand-in for missing artwork. Real artwork needs no
    // plate behind it: tinted, its own shape already reads against the sidebar.
    color: showsArtwork
        ? "transparent"
        : (highlighted ? tint : Qt.rgba(tint.r, tint.g, tint.b, 0.18))
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
        colorizationColor: root.tint
    }

    Text {
        anchors.centerIn: parent
        visible: !root.showsArtwork
        text: root.code
        color: root.highlighted ? root.colors.windowOpaque : root.tint
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.weight: Font.DemiBold
    }
}
