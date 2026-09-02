import QtQuick
import qs.Ui as Omarchy

// Settings-facing adapter for the Omarchy dropdown. Keep the small API used by
// Omaweb here so settings controls do not fall back to platform-native styling.
Omarchy.Dropdown {
    id: root

    property var colors
    property string accessibleName: ""
    readonly property int count: options ? options.length : 0

    showLabel: false
    foreground: colors.text
    accent: colors.accent

    Accessible.name: accessibleName
}
