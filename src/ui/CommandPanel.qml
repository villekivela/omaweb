import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import qs.Commons

Item {
    id: root
    objectName: "commandPanel"

    property var colors
    property var commands
    // Answers what the typed text would search, so the panel names the same
    // engine the commit reaches.
    property var browser: null
    property bool open: false
    property bool commandMode: false
    property bool newTabIntent: false
    property string presetText: ""
    property var suggestions: []

    // The item to sample for the blur. It must not be an ancestor of this
    // panel, or the effect source would feed on its own output.
    property Item backdropSource: null

    readonly property bool blurActive: backdropSource !== null && backdropSource.visible

    property var results: []
    property int selected: 0

    // The engine a typed keyword selected, drawn as a chip ahead of the terms
    // the field then holds. Null while the field holds plain text.
    property var engine: null
    // What committing the field would search, or nothing for an address.
    property var intent: ({})
    // The engines whose keyword the typed text could become.
    property var keywordOffers: []
    readonly property string destination: describe(intent)
    // What the reader steps through in address mode: the history rows, then
    // the keywords on offer.
    readonly property var rows: suggestions.map(function (suggestion) {
        return {
            "kind": "history",
            "title": suggestion.title,
            "url": suggestion.url
        };
    }).concat(keywordOffers.map(function (offer) {
        return {
            "kind": "keyword",
            "engineId": offer.engineId,
            "engineName": offer.engineName,
            "keyword": offer.keyword
        };
    }))

    signal dismissed
    signal committed(string text)
    signal queryChanged(string text)

    // Drawn for the length of the retreat as well.
    visible: open || retreating
    property bool retreating: false

    // Where the panel comes from: the address field, or the control that
    // asked for it, in the panel's own coordinates. A panel that grows out
    // of the field the reader pressed is the same field, expanded, rather
    // than a second thing that appeared. An empty origin means the panel
    // arrives from just above its resting place instead.
    property rect origin: Qt.rect(0, 0, 0, 0)
    property bool ease: true
    // 0 at the origin, 1 at rest.
    property real arrival: 1
    readonly property real restWidth: Math.min(660, width - 96)
    readonly property real restX: (width - restWidth) / 2
    readonly property real restY: Math.max(80, height * 0.14)
    readonly property real restHeight: header.height + body.height + footer.height + 2
                                       * panel.border.width
    readonly property bool fromOrigin: origin.width > 0
    function lerp(a, b) {
        return a + (b - a) * arrival;
    }
    NumberAnimation {
        id: arrivalEase
        target: root
        property: "arrival"
        to: 1
        duration: 180
        easing.type: Easing.OutCubic
    }
    // Back to the field it grew from, quicker than it came.
    NumberAnimation {
        id: retreatEase
        target: root
        property: "arrival"
        to: 0
        duration: 120
        easing.type: Easing.InCubic
        onFinished: {
            root.retreating = false;
            root.arrival = 1;
        }
    }

    function beginAddress(preset, forNewTab) {
        commandMode = false;
        newTabIntent = forNewTab;
        presetText = preset;
    }

    function beginCommand() {
        commandMode = true;
        newTabIntent = false;
    }

    onOpenChanged: {
        if (!open) {
            arrivalEase.stop();
            if (!ease) {
                arrival = 1;
                return;
            }
            retreating = true;
            retreatEase.restart();
            return;
        }
        retreatEase.stop();
        retreating = false;
        if (ease) {
            arrival = 0;
            arrivalEase.restart();
        }
        engine = null;
        input.text = commandMode ? "" : presetText;
        refresh();
        Qt.callLater(function () {
            input.forceActiveFocus();
            input.selectAll();
        });
    }

    // Suggestions arrive after the keystroke that asked for them, so a row the
    // reader had stepped onto is a different destination once the answer
    // lands, or gone. The typed text is always a destination, so the selection
    // goes back to it rather than to whatever now sits at that index.
    onSuggestionsChanged: {
        if (!commandMode)
            selected = -1;
    }

    function refresh() {
        results = commandMode ? commands.search(input.text) : [];
        if (commandMode || browser === null) {
            intent = {};
            keywordOffers = [];
        } else {
            intent = browser.searchIntent(typed());
            keywordOffers = engine === null ? browser.searchKeywordOffers(input.text) : [];
        }
        selected = commandMode ? 0 : -1;
    }

    // The text the field stands for: the keyword the chip took, then the
    // terms. A function rather than a binding, because a binding on the text
    // is still the old text while the field's own change handler runs.
    function typed() {
        return engine === null ? input.text : engine.keyword + " " + input.text;
    }

    // Empty terms are the engine's front page, so the reader is told it opens
    // rather than searches.
    function describe(search) {
        if (search.engineId === undefined)
            return "";
        if (search.terms.length === 0)
            return "Open " + search.engineName;
        return "Search " + search.engineName + " for " + search.terms;
    }

    // The space after a keyword is what enters the mode: the field gives the
    // keyword to the chip and keeps the terms.
    function takeKeyword() {
        if (commandMode || browser === null || engine !== null || input.text.indexOf(" ") < 0)
            return false;
        const found = browser.searchIntent(input.text);
        if (found.keyword === undefined || found.keyword.length === 0)
            return false;
        chooseEngine(found);
        input.text = found.terms;
        return true;
    }

    function chooseEngine(offer) {
        engine = {
            "engineId": offer.engineId,
            "engineName": offer.engineName,
            "keyword": offer.keyword
        };
    }

    // Backspace on empty terms undoes the space that entered the mode.
    function releaseKeyword() {
        if (engine === null || input.text.length > 0)
            return false;
        const keyword = engine.keyword;
        engine = null;
        input.text = keyword;
        return true;
    }

    // In address mode the typed text is itself a destination, so it is the
    // selection at -1: the list is what you step into, not what you start in.
    function step(delta) {
        if (commandMode) {
            if (results.length === 0)
                return;
            selected = (selected + delta + results.length) % results.length;
            return;
        }
        if (rows.length === 0)
            return;
        const next = selected + delta;
        selected = next < -1 ? rows.length - 1 : (next >= rows.length ? -1 : next);
    }

    function accept() {
        if (!commandMode) {
            if (selected >= 0 && selected < rows.length) {
                const row = rows[selected];
                if (row.kind === "history") {
                    root.committed(row.url.toString());
                    return;
                }
                // The same as typing the keyword and a space.
                chooseEngine(row);
                input.text = "";
                return;
            }
            const text = typed();
            if (text.trim().length > 0) {
                root.committed(text);
            }
            return;
        }
        if (results.length === 0) {
            return;
        }
        const action = results[selected];
        if (!action.enabled) {
            return;
        }
        root.dismissed();
        commands.invoke(action);
    }

    SheetFloor {}

    Rectangle {
        anchors.fill: parent
        color: "#99000000"
        opacity: root.arrival

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        id: panel
        // Between the origin and rest by `arrival`; the height is what the
        // rows need once it is there, and the frame grows to it.
        x: root.fromOrigin ? root.lerp(root.origin.x, root.restX) : root.restX
        y: root.fromOrigin ? root.lerp(root.origin.y, root.restY) : root.restY - 8 * (1
                                                                                      - root.arrival)
        width: root.fromOrigin ? root.lerp(root.origin.width, root.restWidth) : root.restWidth
        height: root.fromOrigin ? root.lerp(root.origin.height, root.restHeight) : root.restHeight
        opacity: root.fromOrigin ? 1 : root.arrival
        radius: 3
        // With a backdrop the tint goes on top of the blur instead, so the
        // panel itself stays clear.
        color: root.blurActive ? "transparent" : root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        clip: true

        ShaderEffectSource {
            id: backdropTexture
            visible: false
            live: true
            hideSource: false
            recursive: false
            sourceItem: root.blurActive ? root.backdropSource : null
            // Only the slice of the window the panel covers, in the source's
            // coordinates. The source fills the same area as this overlay, so
            // the panel's own position is that mapping.
            sourceRect: root.blurActive ? Qt.rect(panel.x, panel.y, panel.width, panel.height) :
                                          Qt.rect(0, 0, 0, 0)
            width: Math.max(1, panel.width)
            height: Math.max(1, panel.height)
            textureSize: Qt.size(Math.max(1, Math.round(panel.width / 2)), Math.max(1, Math.round(
                                                                                        panel.height
                                                                                        / 2)))
        }

        MultiEffect {
            anchors.fill: parent
            anchors.margins: panel.border.width
            visible: root.blurActive
            source: backdropTexture
            blurEnabled: true
            blur: 1
            blurMax: 48
            // The blur stops at this item's own edge. Left to itself MultiEffect
            // enlarges what it draws to fit the blur, which reaches out over the
            // border the margins above were set to keep clear and softens it.
            autoPaddingEnabled: false
            // Keeps the blur inside the panel's rounded corners rather than
            // squaring them off under the border.
            maskEnabled: true
            maskSource: ShaderEffectSource {
                sourceItem: Rectangle {
                    width: Math.max(1, panel.width - 2 * panel.border.width)
                    height: Math.max(1, panel.height - 2 * panel.border.width)
                    radius: panel.radius
                    color: "black"
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: panel.border.width
            visible: root.blurActive
            radius: panel.radius
            color: root.colors.overlay
        }

        // Every band stops at the border: a rule that ran the full width would
        // cut across the panel's own edge and square off its corners.
        Item {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: panel.border.width
            height: 62

            Text {
                id: prompt
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: root.commandMode ? ":" : (root.newTabIntent ? "+" : ">")
                color: root.colors.accent
                font.family: Style.font.family
                font.pixelSize: 18
            }

            Rectangle {
                id: chip
                objectName: "omnibarEngineChip"
                anchors.left: prompt.right
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: root.engine !== null
                width: visible ? chipLabel.implicitWidth + 16 : 0
                height: 26
                radius: 3
                color: root.colors.surface
                border.width: 1
                border.color: root.colors.accent
                Accessible.role: Accessible.StaticText
                Accessible.name: root.engine === null ? "" : root.engine.engineName

                Text {
                    id: chipLabel
                    anchors.centerIn: parent
                    text: root.engine === null ? "" : root.engine.engineName
                    color: root.colors.accent
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                }
            }

            TextField {
                id: input
                objectName: "omnibarInput"
                anchors.left: chip.right
                anchors.right: modeLabel.left
                anchors.leftMargin: chip.visible ? 8 : 0
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                height: 40
                background: null
                // TextInput aligns to the top by default, which would drop the
                // text off the prompt icon's centre line.
                padding: 0
                verticalAlignment: TextInput.AlignVCenter
                color: root.colors.text
                placeholderText: root.commandMode ? "search every action" : (root.engine !== null
                                                                             ? "search "
                                                                               + root.engine.engineName :
                                                                               (root.newTabIntent
                                                                                ? "address or search — opens in a new tab" :
                                                                                  "address or search"))
                placeholderTextColor: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: 17
                selectByMouse: true
                Accessible.name: root.engine === null ? placeholderText : "Search "
                                                        + root.engine.engineName
                Accessible.description: root.destination

                onTextChanged: {
                    // Handing the keyword to the chip sets the text again, and
                    // that pass is the one that refreshes.
                    if (root.takeKeyword())
                        return;
                    root.refresh();
                    root.queryChanged(text);
                }

                onAccepted: root.accept()

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Backspace && root.releaseKeyword())
                        event.accepted = true;
                }

                Keys.onEscapePressed: function (event) {
                    root.dismissed();
                    event.accepted = true;
                }

                Keys.onDownPressed: function (event) {
                    root.step(1);
                    event.accepted = true;
                }

                Keys.onUpPressed: function (event) {
                    root.step(-1);
                    event.accepted = true;
                }
            }

            SectionLabel {
                id: modeLabel
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                text: root.commandMode ? "command" : (root.newTabIntent ? "new tab" : "this tab")
                // Centred in a bar of its own rather than stacked over rows, so
                // it is centred on its glyphs: the lean a section label carries
                // in a scrolling pane would drop it below the address beside it.
                topPadding: overshoot
                bottomPadding: overshoot
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.colors.separator
            }
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: panel.border.width
            anchors.rightMargin: panel.border.width
            anchors.top: header.bottom
            height: root.commandMode ? Math.min(commandList.contentHeight, 336) + 8 : addressHeight
            // The destination row with its lead, then the rows beneath it.
            readonly property real addressHeight: (destination.visible ? destination.height + 4 :
                                                                         0) + (root.rows.length > 0
                                                                               ? Math.min(
                                                                                     rowList.contentHeight,
                                                                                     200) + 8 : 0)

            ListView {
                id: commandList
                objectName: "commandList"
                anchors.fill: parent
                anchors.topMargin: 4
                visible: root.commandMode
                clip: true
                model: root.results
                currentIndex: root.selected
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    required property int index
                    required property var modelData

                    width: commandList.width
                    height: 30

                    readonly property bool startsGroup: index === 0 || commandList.model[index
                                                                                         - 1].group
                                                        !== modelData.group

                    Rectangle {
                        anchors.fill: parent
                        color: index === root.selected ? root.colors.surface : "transparent"
                    }

                    Rectangle {
                        width: 2
                        height: parent.height
                        anchors.left: parent.left
                        color: index === root.selected ? root.colors.accent : "transparent"
                    }

                    SectionLabel {
                        id: groupLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: 92
                        colors: root.colors
                        text: parent.startsGroup ? modelData.group : ""
                        elide: Text.ElideRight
                        // Centred against the command name beside it, so the
                        // stacked lean would drop the group below its own row.
                        topPadding: overshoot
                        bottomPadding: overshoot
                    }

                    Text {
                        anchors.left: groupLabel.right
                        anchors.leftMargin: 10
                        anchors.right: keys.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.commands.highlight(modelData.title, input.text)
                        textFormat: Text.StyledText
                        color: modelData.enabled ? root.colors.text : root.colors.mutedText
                        opacity: modelData.enabled ? 1 : 0.6
                        elide: Text.ElideRight
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    Text {
                        id: keys
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.keys
                        color: root.colors.mutedText
                        opacity: 0.85
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = index
                        onClicked: root.accept()
                    }
                }
            }

            // Where the typed text goes, which is the selection at -1 read
            // out: the engine a keyword chose, or the default one a search
            // without a keyword falls to.
            Item {
                id: destination
                objectName: "omnibarDestination"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: visible ? 4 : 0
                visible: !root.commandMode && root.destination.length > 0
                height: visible ? 28 : 0
                readonly property string text: root.destination
                Accessible.role: Accessible.Button
                Accessible.name: root.destination

                Rectangle {
                    anchors.fill: parent
                    color: root.selected === -1 || destinationMouse.containsMouse
                           ? root.colors.surface : "transparent"
                }

                Rectangle {
                    width: 2
                    height: parent.height
                    anchors.left: parent.left
                    color: root.selected === -1 ? root.colors.accent : "transparent"
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.destination
                    color: root.selected === -1 ? root.colors.text : root.colors.mutedText
                    elide: Text.ElideRight
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                }

                MouseArea {
                    id: destinationMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: root.selected = -1
                    onClicked: {
                        root.selected = -1;
                        root.accept();
                    }
                }
            }

            ListView {
                id: rowList
                objectName: "omnibarRowList"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: destination.bottom
                anchors.bottom: parent.bottom
                anchors.topMargin: 4
                visible: !root.commandMode && root.rows.length > 0
                clip: true
                model: root.rows
                currentIndex: root.selected
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    required property int index
                    required property var modelData

                    readonly property bool history: modelData.kind === "history"

                    width: rowList.width
                    height: 28
                    Accessible.role: Accessible.Button
                    Accessible.name: history ? "Open history result " + modelData.title : "Search "
                                               + modelData.engineName

                    Rectangle {
                        anchors.fill: parent
                        color: index === root.selected || suggestionMouse.containsMouse
                               ? root.colors.surface : "transparent"
                    }

                    Rectangle {
                        width: 2
                        height: parent.height
                        anchors.left: parent.left
                        color: index === root.selected ? root.colors.accent : "transparent"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: rowKeyword.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.history ? modelData.title + "  ·  " + modelData.url : "Search "
                                               + modelData.engineName
                        color: index === root.selected ? root.colors.text : root.colors.mutedText
                        elide: Text.ElideMiddle
                        font.family: Style.font.family
                        font.pixelSize: Style.font.body
                    }

                    // The keyword sits where a command row shows its keys, so
                    // the panel is how the keywords are learned too.
                    Text {
                        id: rowKeyword
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: parent.history ? "" : modelData.keyword
                        color: root.colors.mutedText
                        opacity: 0.85
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    MouseArea {
                        id: suggestionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = index
                        onClicked: {
                            root.selected = index;
                            root.accept();
                        }
                    }
                }
            }
        }

        Item {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: panel.border.width
            height: 26

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: root.colors.separator
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14

                KeyHint {
                    colors: root.colors
                    text: "↑↓ SELECT"
                }

                KeyHint {
                    colors: root.colors
                    text: "⏎ RUN"
                }

                KeyHint {
                    colors: root.colors
                    text: "ESC CLOSE"
                }
            }

            KeyHint {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                visible: root.commandMode
                text: root.results.length + " ACTIONS"
            }
        }
    }
}
