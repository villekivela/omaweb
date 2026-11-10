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
    // The engine of the page on show, which picks and counts.
    property var engine: null

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
    // The selectors the host's file hides, read again with the file.
    readonly property var hidden: {
        root.listed;
        return root.styles && root.host.length > 0 ? root.styles.hidden(root.host) : [];
    }
    readonly property bool picking: root.engine !== null && root.engine.picking === true
    readonly property var pick: root.picking && root.engine.pick ? root.engine.pick : ({})

    signal closeRequested

    function focusEditor() {
        editor.forceActiveFocus();
    }

    function startPicking() {
        if (root.engine && root.host.length > 0)
            root.engine.startPicking();
    }

    function stopPicking() {
        if (root.picking)
            root.engine.stopPicking();
    }

    // Each row says how many elements its selector hides on the page on show.
    function countHidden() {
        if (root.engine && root.hidden.length > 0)
            root.engine.countHidden(root.hidden);
    }

    function hiddenCount(selector) {
        const counts = root.engine && root.engine.hiddenCounts ? root.engine.hiddenCounts : ({});
        return counts[selector] !== undefined ? counts[selector] : -1;
    }

    function matchesText(count, valid) {
        if (!valid)
            return qsTr("This selector does not parse");
        if (count === 0)
            return qsTr("Matches nothing on this page");
        if (count === 1)
            return qsTr("Matches 1 element on this page");
        return qsTr("Matches %1 elements on this page").arg(count);
    }

    // The engine last shown, so a picker left running on a page the reader
    // switched away from is stopped there.
    property var shownEngine: null
    onHiddenChanged: root.countHidden()
    onEngineChanged: {
        if (root.shownEngine && root.shownEngine !== root.engine && root.shownEngine.picking)
            root.shownEngine.stopPicking();
        root.shownEngine = root.engine;
        root.countHidden();
    }
    // The picker is for the page it was started on, and leaves with the panel.
    onVisibleChanged: {
        if (!root.visible)
            root.stopPicking();
    }

    Connections {
        target: root.engine
        ignoreUnknownSignals: true
        function onPickHidden(selector) {
            if (root.styles && root.host.length > 0)
                root.styles.hide(root.host, selector);
        }
        function onLoadingChanged() {
            if (!root.engine.loading)
                root.countHidden();
        }
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

    // ---- hidden elements and the picker ------------------------------------
    //
    // Below the editor, in the same column, so the panel reads top to bottom:
    // the file, what it hides, and how to hide more.
    Column {
        id: hiding
        objectName: "siteStyleHiding"
        x: body.x
        y: body.y + body.height + Style.spacing.huge
        width: body.width
        spacing: Style.spacing.lg

        Text {
            visible: root.hidden.length > 0
            text: qsTr("Hidden on this site")
            color: root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
            font.letterSpacing: 1.1
            font.capitalization: Font.AllUppercase
        }

        Repeater {
            model: root.hidden

            Item {
                id: hiddenRow
                required property string modelData
                readonly property int count: root.hiddenCount(modelData)
                readonly property bool previewed: root.engine !== null
                                                  && root.engine.previewedHidden !== undefined
                                                  && root.engine.previewedHidden.indexOf(modelData)
                                                  >= 0

                objectName: "siteStyleHidden-" + modelData
                width: hiding.width
                height: Style.spacing.controlHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: -Style.spacing.md
                    radius: Style.cornerRadius
                    color: root.colors.surfaceHover
                    visible: hiddenRow.previewed || rowHover.hovered
                }
                HoverHandler {
                    id: rowHover
                }
                Text {
                    anchors.left: parent.left
                    anchors.right: rowActions.left
                    anchors.rightMargin: Style.spacing.lg
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: hiddenRow.modelData
                    color: root.colors.text
                    font.family: "monospace"
                    font.pixelSize: Style.font.body
                }
                Row {
                    id: rowActions
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Style.spacing.lg

                    Text {
                        objectName: "siteStyleHiddenCount"
                        anchors.verticalCenter: parent.verticalCenter
                        text: hiddenRow.count < 0 ? "" : hiddenRow.count === 0 ? qsTr("none here") :
                                                                                 qsTr("%1 here").arg(
                                                                                     hiddenRow.count)
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                    Glyph {
                        objectName: "siteStylePreview"
                        anchors.verticalCenter: parent.verticalCenter
                        name: hiddenRow.previewed ? "visibility_off" : "visibility"
                        color: hiddenRow.previewed ? root.colors.accent : root.colors.mutedText

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -Style.spacing.sm
                            enabled: root.engine !== null
                            onClicked: root.engine.previewHidden(hiddenRow.modelData,
                                                                 !hiddenRow.previewed)
                        }

                        Accessible.role: Accessible.CheckBox
                        Accessible.name: qsTr("Show %1 on the page").arg(hiddenRow.modelData)
                        Accessible.checked: hiddenRow.previewed
                    }
                    Glyph {
                        objectName: "siteStyleUndo"
                        anchors.verticalCenter: parent.verticalCenter
                        name: "undo"

                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -Style.spacing.sm
                            onClicked: {
                                if (hiddenRow.previewed)
                                    root.engine.previewHidden(hiddenRow.modelData, false);
                                root.styles.unhide(root.host, hiddenRow.modelData);
                            }
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Stop hiding %1").arg(hiddenRow.modelData)
                    }
                }
            }
        }

        ActionButton {
            objectName: "siteStylePick"
            visible: !root.picking
            enabled: root.engine !== null && root.host.length > 0
            colors: root.colors
            primary: true
            label: qsTr("Pick an element to hide")
            onClicked: root.startPicking()
        }

        Rectangle {
            objectName: "siteStylePickCard"
            visible: root.picking
            width: hiding.width
            height: pickColumn.implicitHeight + 2 * Style.spacing.huge
            radius: Style.cornerRadius
            color: root.colors.surface
            border.width: Style.spacing.hairline
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
                        text: root.pick.selector ? qsTr("Picking") : qsTr(
                                                       "Point at an element on the page")
                        color: root.colors.text
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                        font.weight: Font.DemiBold
                    }
                }

                Omarchy.TextField {
                    id: selectorField
                    objectName: "siteStyleSelector"
                    width: pickColumn.width
                    font.family: "monospace"
                    foreground: root.colors.text
                    accent: root.colors.accent
                    // The picked selector, unless the reader is writing one.
                    Binding on text {
                        when: !selectorField.activeFocus
                        value: root.pick.selector || ""
                        restoreMode: Binding.RestoreNone
                    }
                    onTextEdited: root.engine.setPickSelector(selectorField.text)
                    onAccepted: root.engine.hidePick()
                    Accessible.name: qsTr("Selector to hide")
                }

                Text {
                    objectName: "siteStyleMatches"
                    visible: root.pick.selector !== undefined && root.pick.selector.length > 0
                    text: root.matchesText(root.pick.count || 0, root.pick.valid !== false)
                    color: root.pick.valid === false || root.pick.count === 0 ? root.colors.urgent :
                                                                                root.colors.accent
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }

                // From this element alone to all related ones. It takes the
                // keys the picker does: Left and Right, or [ and ].
                Item {
                    id: widening
                    objectName: "siteStyleWidening"
                    readonly property int levels: Math.max(1, root.pick.levels || 1)
                    readonly property int level: root.pick.level || 0
                    function widen(level) {
                        const next = Math.max(0, Math.min(widening.levels - 1, level));
                        if (next !== widening.level)
                            root.engine.widenPick(next);
                    }
                    width: pickColumn.width
                    height: slider.implicitHeight + Style.spacing.md + thisLabel.height
                    enabled: widening.levels > 1
                    activeFocusOnTab: true
                    Keys.onPressed: function (event) {
                        if (event.key === Qt.Key_Left || event.key === Qt.Key_BracketLeft) {
                            widening.widen(widening.level - 1);
                            event.accepted = true;
                        } else if (event.key === Qt.Key_Right || event.key
                                   === Qt.Key_BracketRight) {

                            widening.widen(widening.level + 1);
                            event.accepted = true;
                        }
                    }
                    Accessible.role: Accessible.Slider
                    Accessible.name: qsTr("Widen from this element to all related ones")

                    Omarchy.PanelSlider {
                        id: slider
                        width: parent.width
                        minimum: 0
                        maximum: widening.levels - 1
                        step: 1
                        integer: true
                        tickCount: widening.levels
                        value: widening.level
                        trackColor: root.colors.separator
                        fillColor: root.colors.accent
                        knobColor: widening.activeFocus ? root.colors.text : root.colors.accent
                        tickColor: root.colors.surface
                        onReleased: function (value) {
                            widening.widen(Math.round(value));
                        }
                    }
                    Text {
                        id: thisLabel
                        anchors.bottom: parent.bottom
                        text: qsTr("This element")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        text: qsTr("All related")
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }
                }

                Row {
                    spacing: Style.spacing.lg

                    ActionButton {
                        objectName: "siteStyleHide"
                        colors: root.colors
                        primary: true
                        label: qsTr("Hide")
                        enabled: root.pick.valid !== false && (root.pick.count || 0) > 0
                        onClicked: root.engine.hidePick()
                    }
                    ActionButton {
                        objectName: "siteStyleStopPicking"
                        colors: root.colors
                        label: qsTr("Stop picking")
                        onClicked: root.stopPicking()
                    }
                }

                Text {
                    width: pickColumn.width
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
