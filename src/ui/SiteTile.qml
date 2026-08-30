import QtQuick

Rectangle {
    id: root

    property var colors
    property var typography
    property url siteUrl
    property bool highlighted: false

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

    implicitWidth: 20
    implicitHeight: 20
    radius: 2
    color: highlighted ? tint : Qt.rgba(tint.r, tint.g, tint.b, 0.18)
    Accessible.ignored: true

    Text {
        anchors.centerIn: parent
        text: root.code
        color: root.highlighted ? root.colors.windowOpaque : root.tint
        font.family: root.typography.family
        font.pixelSize: root.typography.smallSize
        font.weight: Font.DemiBold
    }
}
