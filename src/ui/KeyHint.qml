import QtQuick
import qs.Commons

Text {
    id: root

    property var colors

    color: colors.mutedText
    opacity: 0.75
    font.family: Style.font.family
    font.pixelSize: Style.font.caption
    font.letterSpacing: 0.6
    Accessible.ignored: true
}
