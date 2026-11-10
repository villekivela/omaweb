import QtQuick
import QtQuick.Controls
import Omaweb
import qs.Commons
import qs.Ui as Omarchy

// Where the reader writes a site's style and hides parts of its pages: a panel
// docked opposite the sidebar, beside the page it styles. It stays open across
// navigation and tab switches and follows the host of the page on show.
//
// What it edits is a file, `styles/<host>.css` (`SiteStyles`), so the reader's
// own editor, the live reload and Sync see each change too.
Rectangle {
    id: root

    property var colors
    property string iconFontFamily
    // The reader's site styles (`SiteStyles`).
    property var styles: null
    // The address of the page on show.
    property url pageUrl
    // Against the window's left edge, when the sidebar is on the right.
    property bool dockedLeft: false

    // The host the panel writes: the page's registrable site, unless the
    // reader picked a narrower one for this site and the page is under it.
    readonly property string site: root.styles ? root.styles.siteFor(root.pageUrl) : ""
    readonly property string pageHost: {
        const match = String(root.pageUrl).match(
                  /^[a-z][a-z0-9+.-]*:\/\/(?:[^@\/]*@)?([^:\/?#]+)/i);
        return match ? match[1].toLowerCase() : "";
    }
    // The narrower host picked for each site, for as long as the panel stands.
    property var chosenHosts: ({})
    readonly property string host: {
        const chosen = root.chosenHosts[root.site];
        if (chosen && (root.pageHost === chosen || root.pageHost.endsWith("." + chosen)))
            return chosen;
        return root.site;
    }

    function chooseHost(host) {
        const chosen = Object.assign({}, root.chosenHosts);
        if (host === root.site)
            delete chosen[root.site];
        else
            chosen[root.site] = host;
        root.chosenHosts = chosen;
    }
    // Read again whenever a style changes, here or in its file.
    readonly property var listed: root.styles ? root.styles.styles : []
    readonly property var entry: {
        for (const style of root.listed) {
            if (style.host === root.host)
                return style;
        }
        return null;
    }
    readonly property bool enabled_: root.entry === null || root.entry.enabled
    readonly property bool unsaved: root.styles !== null && root.styles.unsaved.indexOf(root.host)
                                    >= 0
    readonly property bool hostsOpen: hostList.visible

    signal closeRequested

    function focusEditor() {
        editor.forceActiveFocus();
    }

    // The editor follows the file, unless the text in it is what was just
    // typed: a read of it then would move the cursor under the reader's hands.
    function reload() {
        const source = root.styles && root.host.length > 0 ? root.styles.source(root.host) : "";
        if (editor.text !== source) {
            editor.loading = true;
            editor.text = source;
            editor.loading = false;
        }
    }

    onHostChanged: root.reload()
    onListedChanged: root.reload()
    Component.onCompleted: root.reload()

    color: root.colors.sidebar

    // The seam the page stops at.
    Rectangle {
        x: root.dockedLeft ? root.width - width : 0
        width: Style.spacing.hairline
        height: root.height
        color: root.colors.separator
    }

    component Glyph: Text {
        property string name
        text: name
        font.family: root.iconFontFamily
        font.pixelSize: Style.font.icon
        color: root.colors.mutedText
    }

    Column {
        id: body
        x: Style.spacing.rowPaddingX * 2
        y: Style.spacing.huge
        width: root.width - 2 * x
        spacing: Style.spacing.lg

        // ---- the file, its switch, closing ----------------------------------

        Item {
            width: body.width
            height: hostButton.height

            Rectangle {
                id: hostButton
                objectName: "siteStyleHostButton"
                width: hostRow.implicitWidth + 2 * Style.spacing.lg
                height: Style.spacing.controlHeight
                radius: Style.cornerRadius
                color: root.hostsOpen || hostArea.containsMouse ? root.colors.surfaceHover :
                                                                  root.colors.surface

                Row {
                    id: hostRow
                    anchors.centerIn: parent
                    spacing: Style.spacing.md

                    Glyph {
                        anchors.verticalCenter: parent.verticalCenter
                        name: "palette"
                        color: root.enabled_ && root.entry !== null ? root.colors.accent :
                                                                      root.colors.mutedText
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.host.length > 0 ? root.host : qsTr("No site")
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

                MouseArea {
                    id: hostArea
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: root.host.length > 0
                    onClicked: hostList.visible = !hostList.visible
                }

                Accessible.role: Accessible.ButtonDropDown
                Accessible.name: qsTr("Style the pages of %1").arg(root.host)
                Accessible.onPressAction: hostList.visible = !hostList.visible
            }

            Omarchy.ToggleSwitch {
                id: styleSwitch
                objectName: "siteStyleSwitch"
                anchors.right: closeButton.left
                anchors.rightMargin: Style.spacing.lg
                anchors.verticalCenter: parent.verticalCenter
                visible: root.entry !== null
                checked: root.enabled_
                foreground: root.colors.text
                accent: root.colors.accent
                onToggled: root.styles.setEnabled(root.host, !root.enabled_)
                Accessible.role: Accessible.Switch
                Accessible.name: qsTr("Apply the style for %1").arg(root.host)
                Accessible.checked: checked
            }

            Glyph {
                id: closeButton
                objectName: "siteStyleClose"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                name: "close"
                color: closeArea.containsMouse ? root.colors.text : root.colors.mutedText

                MouseArea {
                    id: closeArea
                    anchors.fill: parent
                    anchors.margins: -Style.spacing.md
                    hoverEnabled: true
                    onClicked: root.closeRequested()
                }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Close the Site style panel")
                Accessible.onPressAction: root.closeRequested()
            }
        }

        Text {
            objectName: "siteStyleCaption"
            width: body.width
            wrapMode: Text.WordWrap
            color: root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            text: root.host.length === 0 ? qsTr("Open a page to style its site.") : !root.enabled_ ? qsTr(
                                                                                                         "Off. styles/%1.css is kept, and applies again when switched on.").arg(
                                                                                                         root.host) :
                                                                                                     qsTr("styles/%1.css · styles %1 and every host under it").arg(
                                                                                                         root.host)
        }

        // ---- the editor -----------------------------------------------------

        ScrollView {
            id: editorScroll
            width: body.width
            height: Math.max(Style.spacing.controlHeight * 4, root.height * 0.38)
            enabled: root.host.length > 0
            opacity: root.enabled_ ? 1 : 0.55
            clip: true

            MultilineField {
                id: editor
                objectName: "siteStyleEditor"

                // A text set from the file, which is not the reader typing.
                property bool loading: false

                colors: root.colors
                font.family: "monospace"
                font.pixelSize: Style.font.caption
                placeholder: qsTr("CSS for %1").arg(root.host)
                accessibleName: qsTr("CSS for %1").arg(root.host)
                onTextChanged: {
                    if (!editor.loading && root.styles && root.host.length > 0)
                        root.styles.edit(root.host, editor.text);
                }

                CssHighlighter {
                    textDocument: editor.textDocument
                    commentColor: root.colors.mutedText
                    selectorColor: root.colors.accent
                    propertyColor: root.colors.text
                    valueColor: root.colors.urgent
                }
            }
        }

        Row {
            spacing: Style.spacing.lg

            ActionButton {
                objectName: "siteStyleOpenInEditor"
                colors: root.colors
                label: qsTr("Open in editor")
                enabled: root.host.length > 0
                onClicked: root.styles.openInEditor(root.host)
            }

            Text {
                objectName: "siteStyleSaved"
                anchors.verticalCenter: parent.verticalCenter
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
                text: root.unsaved ? qsTr("Not saved yet") : root.entry !== null ? qsTr("Saved") :
                                                                                   ""
            }
        }
    }

    // ---- the host picker, open over the panel -------------------------------

    Rectangle {
        id: hostList
        objectName: "siteStyleHosts"
        visible: false
        x: body.x
        y: body.y + hostButton.height + Style.spacing.sm
        width: Math.min(body.width, Style.spacing.controlHeight * 10)
        height: hostColumn.implicitHeight + 2 * Style.spacing.md
        radius: Style.cornerRadius
        color: root.colors.overlayOpaque
        border.width: Style.spacing.hairline
        border.color: root.colors.separator
        z: 5

        Column {
            id: hostColumn
            x: Style.spacing.md
            y: Style.spacing.md
            width: parent.width - 2 * Style.spacing.md

            Repeater {
                model: hostList.visible && root.styles ? root.styles.hostsFor(root.pageUrl) : []

                Rectangle {
                    id: hostRowItem
                    required property var modelData
                    readonly property bool chosen: modelData.host === root.host

                    objectName: "siteStyleHost-" + modelData.host
                    width: hostColumn.width
                    height: Style.spacing.popupRowHeight + Style.spacing.md
                    radius: Style.cornerRadius
                    color: chosen || rowArea.containsMouse ? root.colors.surfaceHover :
                                                             "transparent"

                    Glyph {
                        id: tick
                        x: Style.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        name: "check"
                        color: root.colors.accent
                        opacity: hostRowItem.chosen ? 1 : 0
                    }
                    Text {
                        anchors.left: tick.right
                        anchors.leftMargin: Style.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        text: hostRowItem.modelData.host
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Style.spacing.md
                        anchors.verticalCenter: parent.verticalCenter
                        readonly property bool applies: hostRowItem.modelData.exists
                                                        && root.styles.enabled(
                                                            hostRowItem.modelData.host)
                        text: applies ? qsTr("applies") : hostRowItem.modelData.exists ? qsTr("off") :
                                                                                         qsTr("no file yet")
                        color: applies ? root.colors.accent : root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    MouseArea {
                        id: rowArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.chooseHost(hostRowItem.modelData.host);
                            hostList.visible = false;
                        }
                    }
                }
            }
        }
    }
}
