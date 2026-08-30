import QtQuick

QtObject {
    id: root

    property var palette
    property int revision: 0

    readonly property var block: palette && palette.font ? palette.font : ({})

    readonly property var families: block.families !== undefined
        ? block.families
        : ["SF Mono", "Menlo", "Courier"]
    readonly property int size: block.size !== undefined ? block.size : 12
    readonly property int smallSize: block.smallSize !== undefined ? block.smallSize : 10
    readonly property int headingSize: block.headingSize !== undefined ? block.headingSize : 15
    readonly property int labelSize: block.labelSize !== undefined ? block.labelSize : 9
    readonly property real labelSpacing: block.labelSpacing !== undefined ? block.labelSpacing : 1.8
    readonly property int iconSize: block.iconSize !== undefined ? block.iconSize : 17

    readonly property string family: {
        revision
        const available = Qt.fontFamilies()
        for (let index = 0; index < families.length; ++index) {
            if (available.indexOf(families[index]) !== -1) {
                return families[index]
            }
        }
        return families.length > 0 ? families[0] : "Menlo"
    }
}
