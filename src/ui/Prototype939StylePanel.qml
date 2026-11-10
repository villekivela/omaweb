import QtQuick
import qs.Commons
import qs.Ui as Omarchy

// PROTOTYPE #939, throwaway: a mockup of the Site style panel for the
// maintainer to judge. It reads nothing and writes nothing; every row is
// canned. `mode` is the lab's OMAWEB_PROTOTYPE_939.
Rectangle {
    id: root

    property var colors
    property string iconFontFamily
    property string mode: "panel"
    property bool privateWindow: false

    readonly property bool picking: mode === "pick" || mode === "pick-wide"
    readonly property bool hostsOpen: mode === "hosts"
    readonly property bool styleOff: mode === "off"
    readonly property int pad: Style.spacing.rowPaddingX * 2

    readonly property string css: "/* Wider measure, quieter links */\n"
                                  + "article {\n  max-width: 64rem;\n}\n\n"
                                  + "a:not(:hover) {\n  text-decoration: none;\n}\n\n"
                                  + "/* Hidden by Omaweb */\n"
                                  + ".cookie-notice { display: none !important; }\n"
                                  + "#feedback-widget { display: none !important; }\n"
                                  + "/* End hidden by Omaweb */\n"

    function highlight(text) {
        const esc = s => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        const lines = text.split("\n");
        const out = [];
        for (const line of lines) {
            let html;
            if (/^\s*\/\*/.test(line)) {
                html = "<font color='" + root.colors.mutedText + "'>" + esc(line) + "</font>";
            } else if (/\{/.test(line)) {
                const at = line.indexOf("{");
                html = "<font color='" + root.colors.accent + "'>" + esc(line.slice(0, at))
                        + "</font>" + esc(line.slice(at)).replace(/([a-z-]+):\s*([^;]+);/g,
                                                                  "<font color='"
                                                                  + root.colors.text
                                                                  + "'>$1</font>: <font color='"
                                                                  + root.colors.urgent
                                                                  + "'>$2</font>;");
            } else {
                html = esc(line).replace(/([a-z-]+):\s*([^;]+);/, "<font color='"
                                         + root.colors.text + "'>$1</font>: <font color='"
                                         + root.colors.urgent + "'>$2</font>;");
            }
            out.push(html.replace(/^ +/, m => "&nbsp;".repeat(m.length)));
        }
        return out.join("<br>");
    }

    color: root.colors.sidebar

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: root.colors.separator
    }

    component Glyph: Text {
        property string name
        text: name
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.icon
        color: root.colors.mutedText
    }

    component Caption: Text {
        color: root.colors.mutedText
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
        font.letterSpacing: 1.1
        font.capitalization: Font.AllUppercase
    }

    Column {
        id: body
        x: root.pad
        y: Style.spacing.huge
        width: root.width - 2 * root.pad
        spacing: Style.spacing.lg

        // ---- header: the file, its switch, close -------------------------

        Item {
            width: parent.width
            height: hostButton.height

            Rectangle {
                id: hostButton
                width: hostRow.width + 2 * Style.spacing.lg
                height: Style.spacing.controlHeight
                radius: 4
                color: root.hostsOpen ? root.colors.surfaceHover : root.colors.surface

                Row {
                    id: hostRow
                    anchors.centerIn: parent
                    spacing: Style.spacing.md

                    Glyph {
                        name: "palette"
                        anchors.verticalCenter: parent.verticalCenter
                        color: root.styleOff ? root.colors.mutedText : root.colors.accent
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "qt.io"
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        font.weight: Font.DemiBold
                    }
                    Glyph {
                        anchors.verticalCenter: parent.verticalCenter
                        name: root.hostsOpen ? "arrow_drop_up" : "arrow_drop_down"
                    }
                }
            }

            Omarchy.ToggleSwitch {
                anchors.right: closeGlyph.left
                anchors.rightMargin: Style.spacing.lg
                anchors.verticalCenter: parent.verticalCenter
                checked: !root.styleOff
                foreground: root.colors.text
                accent: root.colors.accent
            }

            Glyph {
                id: closeGlyph
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                name: "close"
            }
        }

        Text {
            width: parent.width
            wrapMode: Text.WordWrap
            text: root.styleOff ? qsTr(
                                      "Off. styles/qt.io.css is kept, and applies again when switched on.") :
                                  qsTr("styles/qt.io.css · styles doc.qt.io and every qt.io page")
            color: root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        // ---- the editor ---------------------------------------------------

        Rectangle {
            width: parent.width
            height: root.picking ? 190 : 300
            radius: 4
            color: root.colors.windowOpaque
            border.width: 1
            border.color: root.picking ? root.colors.separator : root.colors.accent
            opacity: root.styleOff ? 0.55 : 1
            clip: true

            Column {
                id: gutter
                x: Style.spacing.lg
                y: Style.spacing.lg
                Repeater {
                    model: root.css.split("\n").length - 1
                    Text {
                        required property int index
                        text: index + 1
                        width: 18
                        horizontalAlignment: Text.AlignRight
                        color: root.colors.mutedText
                        opacity: 0.6
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                        lineHeight: 1.35
                        height: editorText.lineHeightPx
                    }
                }
            }

            Text {
                id: editorText
                readonly property real lineHeightPx: Math.round(Style.font.caption * 1.35 + 2)
                anchors.left: gutter.right
                anchors.leftMargin: Style.spacing.lg
                y: Style.spacing.lg
                textFormat: Text.StyledText
                text: root.highlight(root.css)
                color: root.colors.text
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
                lineHeightMode: Text.FixedHeight
                lineHeight: lineHeightPx
            }

            // The cursor, so the box reads as a place to type.
            Rectangle {
                visible: false
                x: editorText.x + 20 * (Style.font.caption * 0.6)
                y: editorText.y + 1 * editorText.lineHeightPx + 2
                width: 2
                height: editorText.lineHeightPx - 4
                color: root.colors.accent
            }
        }

        Row {
            spacing: Style.spacing.lg
            ActionButton {
                colors: root.colors
                label: qsTr("Open in editor")
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Saved")
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }

        // ---- hidden elements ----------------------------------------------

        Caption {
            topPadding: Style.spacing.md
            text: qsTr("Hidden on this site")
        }

        Repeater {
            model: [
                {
                    "selector": ".cookie-notice",
                    "count": qsTr("1 here")
                },
                {
                    "selector": "#feedback-widget",
                    "count": qsTr("1 here")
                }
            ]

            Item {
                required property var modelData
                required property int index
                width: body.width
                height: Style.spacing.controlHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: -Style.spacing.md
                    radius: 4
                    color: root.colors.surfaceHover
                    visible: index === 0 && root.mode === "panel"
                }
                Text {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.selector
                    color: root.colors.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                }
                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Style.spacing.lg
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.count
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                    // Preview: hold to see it back on the page.
                    Glyph {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "visibility"
                    }
                    Glyph {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "undo"
                    }
                }
            }
        }

        // ---- pick ---------------------------------------------------------

        ActionButton {
            visible: !root.picking
            colors: root.colors
            primary: true
            label: qsTr("Pick an element to hide")
        }

        Rectangle {
            visible: root.picking
            width: body.width
            height: pickColumn.implicitHeight + 2 * Style.spacing.huge
            radius: 6
            color: root.colors.surface
            border.width: 1
            border.color: root.colors.accent

            Column {
                id: pickColumn
                x: Style.spacing.huge
                y: Style.spacing.huge
                width: parent.width - 2 * Style.spacing.huge
                spacing: Style.spacing.lg

                Row {
                    spacing: Style.spacing.md
                    Glyph {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "ads_click"
                        color: root.colors.accent
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Picking")
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        font.weight: Font.DemiBold
                    }
                }

                Omarchy.TextField {
                    width: parent.width
                    text: root.mode === "pick-wide" ? "article .figure" : "#scene-graph-diagram"
                    foreground: root.colors.text
                    accent: root.colors.accent
                }

                Text {
                    text: root.mode === "pick-wide" ? qsTr("Matches 2 elements on this page") : qsTr(
                                                          "Matches 1 element on this page")
                    color: root.colors.accent
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }

                Omarchy.PanelSlider {
                    width: parent.width
                    minimum: 0
                    maximum: 2
                    step: 1
                    integer: true
                    tickCount: 3
                    value: root.mode === "pick-wide" ? 1 : 0
                    trackColor: root.colors.separator
                    fillColor: root.colors.accent
                    knobColor: root.colors.accent
                    tickColor: root.colors.surface
                }

                Item {
                    width: parent.width
                    height: thisLabel.height
                    Text {
                        id: thisLabel
                        text: qsTr("This element")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                    Text {
                        anchors.right: parent.right
                        text: qsTr("All related")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                }

                Row {
                    spacing: Style.spacing.lg
                    ActionButton {
                        colors: root.colors
                        primary: true
                        label: qsTr("Hide")
                    }
                    ActionButton {
                        colors: root.colors
                        label: qsTr("Stop picking")
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    lineHeight: 1.3
                    text: qsTr(
                              "↑ ↓ parent and child · ← → siblings\n[ ] narrow and widen · Enter hides · Esc stops")
                    color: root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }
            }
        }
    }

    // ---- the host picker, open over the panel -------------------------------

    Rectangle {
        visible: root.hostsOpen
        x: root.pad
        y: Style.spacing.huge + Style.spacing.controlHeight + Style.spacing.sm
        width: Math.min(body.width, 330)
        height: hostList.implicitHeight + 2 * Style.spacing.md
        radius: 6
        color: root.colors.overlayOpaque
        border.width: 1
        border.color: root.colors.separator
        z: 5

        Column {
            id: hostList
            x: Style.spacing.md
            y: Style.spacing.md
            width: parent.width - 2 * Style.spacing.md

            Repeater {
                model: [
                    {
                        "host": "doc.qt.io",
                        "note": qsTr("no file yet"),
                        "chosen": false
                    },
                    {
                        "host": "qt.io",
                        "note": qsTr("applies · 13 lines"),
                        "chosen": true
                    }
                ]

                Rectangle {
                    required property var modelData
                    width: hostList.width
                    height: Style.spacing.popupRowHeight + Style.spacing.md
                    radius: 4
                    color: modelData.chosen ? root.colors.surfaceHover : "transparent"

                    Glyph {
                        id: tick
                        x: Style.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        name: "check"
                        color: root.colors.accent
                        opacity: modelData.chosen ? 1 : 0
                    }
                    Text {
                        anchors.left: tick.right
                        anchors.leftMargin: Style.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.host
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Style.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.note
                        color: modelData.chosen ? root.colors.accent : root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                }
            }
        }
    }
}
