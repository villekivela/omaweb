import QtQuick

Text {
    id: root

    property var colors
    property var typography

    color: colors.mutedText
    opacity: 0.75
    font.family: typography.family
    font.pixelSize: typography.smallSize
    font.letterSpacing: 0.6
    Accessible.ignored: true
}
