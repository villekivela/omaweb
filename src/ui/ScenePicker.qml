import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// The Start page's Scenes as a row of small thumbnails, each a still of its
// Scene drawn by the Scene itself, without the CRT glass, which does not read
// at this size, and None as the sidebar's plain fill. Each is named under it,
// and the one in force is marked in the accent.
//
// A click chooses. As the kit's ButtonGroup, the row is one Tab stop: Left and
// Right, or h and l, walk between the thumbnails from the one in force, and
// Return or Space chooses the one walked to.
Row {
    id: root

    property var colors
    property string value: ""
    property string accessibleName: ""
    // Which thumbnail the keyboard is on while the row has focus, or -1.
    property int cursor: -1

    // Each thumbnail is the Scene drawn at `zoom` times its size and shrunk,
    // so it shows the composition a page area of that size would, in pixels
    // as coarse as a page's.
    readonly property int thumbnailWidth: 128
    readonly property int thumbnailHeight: 80
    readonly property int zoom: 4

    readonly property var options: [
        {
            value: "crt-road",
            label: qsTr("Night road", "Start page Scene"),
            scene: roadScene
        },
        {
            value: "night-sky",
            label: qsTr("Night sky", "Start page Scene"),
            scene: skyScene
        },
        {
            value: "none",
            label: qsTr("None", "Start page Scene: the sidebar's fill"),
            scene: null
        }
    ]

    signal changed(string value)

    function chosenIndex() {
        for (let index = 0; index < root.options.length; ++index)
            if (root.options[index].value === root.value)
                return index;
        return 0;
    }

    spacing: Style.spacing.lg
    activeFocusOnTab: true

    Accessible.role: Accessible.Grouping
    Accessible.name: root.accessibleName

    onActiveFocusChanged: root.cursor = activeFocus ? root.chosenIndex() : -1

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_H) {
            root.cursor = Math.max(0, root.cursor - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_L) {
            root.cursor = Math.min(root.options.length - 1, root.cursor + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key
                   === Qt.Key_Space) {
            if (root.cursor >= 0)
                root.changed(root.options[root.cursor].value);
            event.accepted = true;
        }
    }

    Component {
        id: roadScene

        NightRoad {}
    }

    Component {
        id: skyScene

        NightSky {}
    }

    Repeater {
        model: root.options

        Column {
            id: option

            required property var modelData
            required property int index

            readonly property bool chosen: modelData.value === root.value
            readonly property bool walkedTo: root.activeFocus && root.cursor === index

            objectName: "sceneThumbnail-" + modelData.value
            spacing: Style.spacing.sm

            Accessible.role: Accessible.RadioButton
            Accessible.name: modelData.label
            Accessible.checked: chosen
            Accessible.onPressAction: root.changed(modelData.value)

            Omarchy.BorderSurface {
                id: frame

                readonly property real edge: 2

                width: root.thumbnailWidth + 2 * edge
                height: root.thumbnailHeight + 2 * edge
                radius: Style.cornerRadius
                color: "transparent"
                borderSpec: option.walkedTo ? Border.controlSpec("focus", root.colors.text,
                                                                 root.colors.accent) :
                                              option.chosen ? Border.flat(root.colors.accent, edge) :
                                                              Border.controlSpec("normal",
                                                                                 root.colors.text,
                                                                                 root.colors.accent)

                Item {
                    x: frame.edge
                    y: frame.edge
                    width: root.thumbnailWidth
                    height: root.thumbnailHeight
                    clip: true

                    Rectangle {
                        anchors.fill: parent
                        visible: !option.modelData.scene
                        color: root.colors.sidebar
                    }

                    // A still: no clock, and the frame a reader who asked
                    // for less motion sees. Made only while it can be seen,
                    // and off the frame that opens Settings, which a Scene
                    // made there would hold up.
                    Loader {
                        active: !!option.modelData.scene && root.visible
                        asynchronous: true
                        sourceComponent: SceneHost {
                            width: root.thumbnailWidth * root.zoom
                            height: root.thumbnailHeight * root.zoom
                            scale: 1 / root.zoom
                            transformOrigin: Item.TopLeft
                            colors: root.colors
                            scene: option.modelData.scene
                            reducedMotion: true
                            glass: false
                        }
                    }
                }
            }

            Text {
                width: frame.width
                horizontalAlignment: Text.AlignHCenter
                text: option.modelData.label
                elide: Text.ElideRight
                color: option.chosen ? root.colors.accent : root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            TapHandler {
                onTapped: root.changed(option.modelData.value)
            }
        }
    }
}
