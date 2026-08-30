import QtQuick

Text {
    id: root

    property var colors
    property var typography

    color: colors.mutedText
    font.family: typography.family
    font.pixelSize: typography.labelSize
    font.letterSpacing: typography.labelSpacing
    font.capitalization: Font.AllUppercase
    font.weight: Font.Medium
    Accessible.role: Accessible.StaticText
    Accessible.name: text
}
