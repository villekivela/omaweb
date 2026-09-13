// PROTOTYPE — a drawn stand-in for the page. The ground never moves, so it
// only has to look like a document.
import QtQuick
import qs.Commons

Rectangle {
    id: page
    property var colors
    property string title: "Generate the website's screenshots"
    property bool miniature: false
    color: colors.windowOpaque
    clip: true

    readonly property real s: miniature ? 0.28 : 1
    Column {
        x: 96 * page.s
        y: 56 * page.s
        width: Math.min(720 * page.s, page.width - 2 * x)
        spacing: 10 * page.s
        Text {
            text: page.title
            color: page.colors.text
            font.family: Style.font.family
            font.pixelSize: 26 * page.s
            font.bold: true
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Text {
            text: "villekivela wants to merge 3 commits into main from feat/124-generate-the-websites-screenshots"
            color: page.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: 12 * page.s
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Item {
            width: 1
            height: 8 * page.s
        }
        Repeater {
            model: 18
            Rectangle {
                width: parent.width * ([1, 0.92, 0.97, 0.6, 1, 0.88, 0.94, 0.4, 1, 0.9, 0.95, 0.72, 1,
                                        0.86, 0.9, 0.5, 0.98, 0.3][index])
                height: 9 * page.s
                radius: 2
                color: page.colors.surface
                opacity: index % 4 === 3 ? 0 : 1
            }
        }
        Item {
            width: 1
            height: 12 * page.s
        }
        Rectangle {
            width: parent.width
            height: 160 * page.s
            radius: 3
            color: page.colors.sidebar
            border.color: page.colors.separator
            Column {
                x: 14 * page.s
                y: 12 * page.s
                spacing: 6 * page.s
                Repeater {
                    model: 7
                    Rectangle {
                        width: (120 + (index * 73) % 300) * page.s
                        height: 8 * page.s
                        radius: 2
                        color: page.colors.surfaceHover
                    }
                }
            }
        }
    }
}
