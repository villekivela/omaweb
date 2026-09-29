import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Shapes
import qs.Commons

Item {
    id: root
    objectName: "omnibar"

    property var colors
    property var commands
    // The sidebar's own favicon settings and icon font, so a site and a
    // command look the same here as where the reader first met them.
    property string iconFontFamily
    property bool useFavicons: true
    property bool tintFavicons: false
    // Answers what the typed text would search, so the panel names the same
    // engine the commit reaches.
    property var browser: null
    property bool open: false
    // The Omnibar narrowed to commands, drawn as the `:` that leads the field.
    property bool commandScope: false
    property bool newTabIntent: false
    property string presetText: ""
    property var suggestions: []

    // The item to sample for the blur. It must not be an ancestor of this
    // panel, or the effect source would feed on its own output.
    property Item backdropSource: null

    // Not at rest: under it is the Start page's road, which moves every frame,
    // and a blur of it would render the window again for each one.
    readonly property bool blurActive: backdropSource !== null && backdropSource.visible &&
                                       !shownResting

    // At rest on the Start page rather than over a page: the field sits on the
    // page area's horizon, nothing is dimmed, and the rest of the window keeps
    // its pointer. `restArea` is the page area and `horizonY` the Start page's
    // horizon, both in this item's coordinates.
    property bool resting: false
    // What the panel looks like: `resting` while it is open, and whatever it
    // was on the way out. An Omnibar at rest that closes fades where it rests
    // rather than jumping to the overlay's place for its retreat.
    property bool shownResting: false
    // The Start page closing takes `resting` and `open` away together, in no
    // set order, so leaving rest waits a turn to see whether the panel is
    // still open.
    onRestingChanged: {
        if (!open)
            return;
        if (resting) {
            shownResting = true;
            return;
        }
        Qt.callLater(function () {
            if (root.open)
                root.shownResting = root.resting;
        });
    }
    property rect restArea: Qt.rect(0, 0, width, height)
    property real horizonY: height / 2
    // Where the keyboard rests when no page has it: the empty field at rest on
    // the Start page. An Agent's click may take it for a moment and give it
    // back without the reader losing anything, which is not true once
    // something is typed.
    readonly property bool pageFocusRest: open && shownResting && !commandScope && engine === null
                                          && input.text.length === 0
    // Whether Escape closes the Omnibar. A Space at rest has nothing behind
    // its Start page to go back to, so there it does not.
    property bool closeable: true

    // What the reader steps through, ranked against the typed text. In
    // command scope that is the commands alone.
    property var rows: []
    property int selected: 0

    // The engine a typed keyword selected, drawn as a chip ahead of the terms
    // the field then holds. Null while the field holds plain text.
    property var engine: null
    // What committing the field would search, or nothing for an address.
    property var intent: ({})
    // The engines whose keyword the typed text could become.
    property var keywordOffers: []
    readonly property string destination: describe(intent)
    // Past this many, commands stop being listed beside the other rows: the
    // reader who wants the whole list has the command scope.
    readonly property int commandsBesideTheRest: 5
    // Among rows that hold the text as strongly, the order the kinds are
    // listed in.
    readonly property var kindOrder: ["tab", "space", "history", "keyword", "command"]

    signal dismissed
    signal committed(string text)
    signal queryChanged(string text)
    // `?` typed into an empty field at rest, which is the Start page's way to
    // the Shortcut sheet.
    signal shortcutsRequested

    // Drawn for the length of the retreat as well.
    visible: open || retreating
    property bool retreating: false

    // The panel drops a little into its place as it fades in, and rises
    // back out as it goes. It no longer grows out of the address field:
    // crossing the window from the sidebar read as a jolt, not as the field
    // opening.
    property bool ease: true
    // 0 on its way in, 1 at rest.
    property real arrival: 1
    // One place whatever it was opened over: the field on the Start page's
    // horizon in the middle of the page area, so opening it over a page puts
    // it where a new tab shows it. The rows grow down from the field and never
    // move it.
    readonly property real restWidth: Math.min(660, restArea.width - 96)
    readonly property real restX: restArea.x + (restArea.width - restWidth) / 2
    readonly property real restY: restArea.y + horizonY - header.height / 2 - panel.border.width
    // What the page area leaves under the field for the rows, keeping a
    // margin off its bottom edge.
    readonly property real roomBelowField: restArea.y + restArea.height - restY - header.height - 2
                                           * panel.border.width - 8 - 24
    // How far below the horizon the field ends, which is where the Start page
    // can draw beneath it.
    readonly property real fieldBelowHorizon: header.height / 2 + panel.border.width
    readonly property real restHeight: header.height + body.height + 2 * panel.border.width
    NumberAnimation {
        id: arrivalEase
        target: root
        property: "arrival"
        to: 1
        duration: 180
        easing.type: Easing.OutCubic
    }
    // Back out, quicker than it came.
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
        commandScope = false;
        newTabIntent = forNewTab;
        presetText = preset;
    }

    function beginCommand() {
        commandScope = true;
        newTabIntent = false;
        presetText = "";
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
        shownResting = resting;
        if (ease) {
            arrival = 0;
            arrivalEase.restart();
        }
        restart();
    }

    // The field as a fresh opening leaves it, for an Omnibar that is already
    // open: the Start page's, when the reader asks for it again.
    function restart() {
        engine = null;
        input.text = commandScope ? "" : presetText;
        refresh();
        Qt.callLater(focusField);
    }

    function focusField() {
        input.forceActiveFocus();
        input.selectAll();
    }

    // Suggestions arrive after the keystroke that asked for them, so a row the
    // reader had stepped onto is a different destination once the answer
    // lands, or gone. The selection goes back to where the text puts it
    // rather than to whatever now sits at that index.
    onSuggestionsChanged: {
        if (!commandScope)
            rank();
    }

    function refresh() {
        if (commandScope || browser === null) {
            intent = {};
            keywordOffers = [];
        } else {
            intent = browser.searchIntent(typed());
            keywordOffers = engine === null ? browser.searchKeywordOffers(input.text) : [];
        }
        rank();
    }

    function rank() {
        if (commandScope) {
            rows = commands.search(input.text).map(asCommand);
            selected = 0;
            return;
        }
        const query = input.text.trim();
        // At rest on the Start page an empty field lists nothing: the open tabs
        // are in the sidebar beside it, and the road is the page.
        if (resting && query.length === 0 && engine === null) {
            rows = [];
            selected = -1;
            return;
        }
        let candidates = suggestions.map(function (suggestion) {
            return {
                "kind": "history",
                "title": suggestion.title,
                "url": suggestion.url.toString()
            };
        }).concat(keywordOffers.map(function (offer) {
            return {
                "kind": "keyword",
                "engineId": offer.engineId,
                "engineName": offer.engineName,
                "keyword": offer.keyword,
                "siteUrl": offer.siteUrl
            };
        }));
        // An unedited preset is the page on show, and terms after a keyword
        // are a search, so neither is asked of the tabs or the commands.
        const widened = engine === null && query.length > 0 && input.text !== presetText;
        if (widened) {
            candidates = candidates.concat(commands.destinations(), commands.actions().map(
                                               asCommand));
        }
        const ranked = [];
        for (let index = 0; index < candidates.length; ++index) {
            const strength = strengthOf(candidates[index], query);
            if (strength > 0)
                ranked.push({
                                "row": candidates[index],
                                "strength": strength,
                                "order": index
                            });
        }
        ranked.sort(function (left, right) {
            return right.strength - left.strength || kindOrder.indexOf(left.row.kind)
                    - kindOrder.indexOf(right.row.kind) || left.order - right.order;
        });
        const next = [];
        let listedCommands = 0;
        for (let index = 0; index < ranked.length; ++index) {
            if (ranked[index].row.kind === "command" && ++listedCommands > commandsBesideTheRest)
                continue;
            next.push(ranked[index].row);
        }
        rows = next;
        // The typed text is the selection, except where it starts an open
        // tab's title or host: the reader is naming that tab, and Return goes
        // to it rather than opening it a second time.
        selected = widened && ranked.length > 0 && ranked[0].row.kind === "tab"
                && ranked[0].strength === 3 ? 0 : -1;
    }

    function asCommand(action) {
        return Object.assign({
                                 "kind": "command"
                             }, action);
    }

    // History and keywords were matched where they came from, so they stay
    // listed however weakly the text reads in them here.
    function strengthOf(row, query) {
        if (row.kind === "keyword")
            return 3;
        const fields = row.kind === "tab" || row.kind === "history" ? [row.title, commands.host(row.url)] :
                                                                      [row.title];
        const found = query.length > 0 ? commands.tier(fields, query) : 0;
        return row.kind === "history" ? Math.max(1, found) : found;
    }

    // A leading `:` is the command scope, never text to search, and the
    // prompt takes it the way the chip takes a keyword.
    function takeScope() {
        if (commandScope || engine !== null || !input.text.startsWith(":"))
            return false;
        commandScope = true;
        input.text = input.text.substring(1);
        return true;
    }

    // Backspace before the first character gives the `:` back, and the same
    // text is asked of everything.
    function releaseScope() {
        if (!commandScope || input.cursorPosition > 0 || input.selectedText.length > 0)
            return false;
        commandScope = false;
        refresh();
        root.queryChanged(input.text);
        return true;
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
            return commandScope || input.text.trim().length === 0 ? "" : "Open " + input.text.trim(
                                                                        );
        if (search.terms.length === 0)
            return "Open " + search.engineName;
        return "Search " + search.engineName + " for " + search.terms;
    }

    // The space after a keyword is what enters the mode: the field gives the
    // keyword to the chip and keeps the terms.
    function takeKeyword() {
        if (commandScope || browser === null || engine !== null || input.text.indexOf(" ") < 0)
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

    // Outside the command scope the typed text is itself a destination, so
    // it is the selection at -1: the list is what you step into, not what you
    // start in.
    function step(delta) {
        if (rows.length === 0)
            return;
        if (commandScope) {
            selected = (selected + delta + rows.length) % rows.length;
            return;
        }
        const next = selected + delta;
        selected = next < -1 ? rows.length - 1 : (next >= rows.length ? -1 : next);
    }

    function accept() {
        if (selected >= 0 && selected < rows.length) {
            const row = rows[selected];
            if (row.kind === "history") {
                root.committed(row.url);
                return;
            }
            if (row.kind === "keyword") {
                // The same as typing the keyword and a space.
                chooseEngine(row);
                input.text = "";
                return;
            }
            if (!row.enabled)
                return;
            root.dismissed();
            commands.invoke(row);
            return;
        }
        if (commandScope)
            return;
        const text = typed();
        if (text.trim().length > 0)
            root.committed(text);
    }

    SheetFloor {
        enabled: !root.shownResting
    }

    Rectangle {
        anchors.fill: parent
        visible: !root.shownResting
        color: "#99000000"
        opacity: root.arrival

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        id: panel
        objectName: "omnibarFrame"
        // The height is what the rows need, and the frame follows it.
        x: root.restX
        y: root.restY - 8 * (1 - root.arrival)
        width: root.restWidth
        height: root.restHeight
        opacity: root.arrival
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

            // The field is drawn as the website's dash: the Omaweb mark as its
            // prompt, the typed text and a block caret glowing in the accent,
            // and a go mark at the end. The mark gives way to `:` in command
            // scope.
            Item {
                id: prompt
                objectName: "omnibarPrompt"
                readonly property string text: root.commandScope ? ":" : "mark"
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 22

                Shape {
                    id: mark
                    objectName: "omnibarMark"
                    // The mark from assets/icons/omaweb.svg, moved to the
                    // origin: 40.4 by 18.4 in its own units.
                    anchors.centerIn: parent
                    width: 40.4
                    height: 18.4
                    scale: parent.width / width
                    visible: !root.commandScope
                    preferredRendererType: Shape.CurveRenderer
                    layer.enabled: true
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: root.colors.accent
                        shadowBlur: 0.5
                        shadowHorizontalOffset: 0
                        shadowVerticalOffset: 0
                    }

                    ShapePath {
                        fillColor: root.colors.accent
                        strokeWidth: -1
                        PathSvg {
                            path: "m 9.187009,0 -9.187009,9.187005 9.187009,9.18763 h 3.69342 l 13.915739,-13.91637 4.72873,4.72874 -9.187009,9.18763 h 6.305601 l 9.18701,-9.18763 -9.187622,-9.187005 H 24.949459 l -13.91574,13.915735 -4.728742,-4.72873 9.18701,-9.187005 z"
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: root.commandScope
                    text: ":"
                    color: root.colors.accent
                    font.family: Style.font.family
                    font.pixelSize: 20
                }
            }

            Rectangle {
                id: chip
                objectName: "omnibarEngineChip"
                anchors.left: prompt.right
                anchors.leftMargin: 12
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
                // The website's phosphor, in the palette's accent: a tight
                // halo on the text and the caret, drawn only while the field
                // changes.
                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: root.colors.accent
                    shadowBlur: 0.4
                    shadowOpacity: 0.7
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                }
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                height: 40
                background: null
                // TextInput aligns to the top by default, which would drop the
                // text off the prompt icon's centre line.
                padding: 0
                verticalAlignment: TextInput.AlignVCenter
                color: root.colors.text
                placeholderText: root.commandScope ? "search every action" : (root.engine !== null
                                                                              ? "search "
                                                                                + root.engine.engineName :
                                                                                (root.newTabIntent
                                                                                 ? "address or search — opens in a new tab" :
                                                                                   "address or search"))
                placeholderTextColor: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: 17
                selectByMouse: true
                // The dash's block caret, blinking while the field has focus.
                cursorDelegate: Rectangle {
                    objectName: "omnibarCaret"
                    width: Math.round(input.font.pixelSize * 0.55)
                    height: Math.round(input.font.pixelSize * 1.1)
                    color: root.colors.accent
                    visible: input.cursorVisible

                    SequentialAnimation on opacity {
                        running: root.open && input.activeFocus
                        loops: Animation.Infinite
                        alwaysRunToEnd: false
                        PropertyAction {
                            value: 0.9
                        }
                        PauseAnimation {
                            duration: 550
                        }
                        PropertyAction {
                            value: 0
                        }
                        PauseAnimation {
                            duration: 550
                        }
                    }
                }
                Accessible.name: root.engine === null ? placeholderText : "Search "
                                                        + root.engine.engineName
                Accessible.description: root.destination

                onTextChanged: {
                    // Handing the `:` to the prompt or the keyword to the chip
                    // sets the text again, and that pass is the one that
                    // refreshes.
                    if (root.takeScope() || root.takeKeyword())
                        return;
                    if (root.resting && !root.commandScope && root.engine === null && text
                            === "?") {
                        text = "";
                        root.shortcutsRequested();
                        return;
                    }
                    root.refresh();
                    root.queryChanged(text);
                }

                onAccepted: root.accept()

                Keys.onPressed: function (event) {
                    if (event.key === Qt.Key_Backspace && (root.releaseKeyword() || root.releaseScope(
                                                               )))
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

            // Commits as Return does, for the pointer. In command scope it
            // runs the selected command.
            Text {
                id: goMark
                objectName: "omnibarGo"
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "→"
                color: root.colors.accent
                opacity: goMouse.containsMouse ? 1 : 0.85
                font.family: Style.font.family
                font.pixelSize: 20
                Accessible.role: Accessible.Button
                Accessible.name: root.commandScope ? "Run" : "Go"

                MouseArea {
                    id: goMouse
                    anchors.fill: parent
                    anchors.margins: -8
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.accept()
                }
            }

            SectionLabel {
                id: modeLabel
                anchors.right: goMark.left
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                text: root.commandScope ? "command" : (root.newTabIntent ? "new tab" : "this tab")
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
            // The rows. Where the typed text goes is not a row of its own: the
            // chip names a keyword's engine, and the field's description says
            // the rest for a screen reader.
            // The field stays on the horizon, so a short page area takes rows
            // off the list rather than moving the field up to fit them.
            height: root.rows.length > 0 ? Math.max(28, Math.min(rowList.contentHeight,
                                                                 root.commandScope ? 336 : 280,
                                                                 root.roomBelowField)) + 8 : 0

            ListView {
                id: rowList
                objectName: "omnibarRowList"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 4
                visible: root.rows.length > 0
                clip: true
                model: root.rows
                currentIndex: root.selected
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    id: row
                    required property int index
                    required property var modelData

                    readonly property bool isSelected: index === root.selected
                    readonly property string title: modelData.kind === "keyword"
                                                    ? modelData.engineName : modelData.title
                    readonly property string host: modelData.kind === "tab" || modelData.kind
                                                   === "history" ? root.commands.host(
                                                                       modelData.url) : ""
                    // The site a tile draws: the page for a tab or a history
                    // row, the engine's own site for a keyword.
                    readonly property string site: modelData.kind === "keyword" ? modelData.siteUrl :
                                                                                  (modelData.url
                                                                                   || "")
                    // What committing the row does, at its right edge. A
                    // command runs, which its keys already say.
                    readonly property string action: ({
                                                          "tab": "switch tab →",
                                                          "space": "switch space →",
                                                          "history": "open →",
                                                          "keyword": "search →",
                                                          "command": ""
                                                      })[modelData.kind]
                    // The keys that reach a command without the Omnibar, and a
                    // keyword in the same place, so the Omnibar is how the
                    // keywords are learned too.
                    readonly property string keys: modelData.kind === "keyword" ? modelData.keyword :
                                                                                  (modelData.kind
                                                                                   === "command"
                                                                                   ? modelData.keys
                                                                                     || "" : "")
                    readonly property bool usable: modelData.enabled !== false

                    width: rowList.width
                    height: 28
                    Accessible.role: Accessible.Button
                    Accessible.name: ({
                                          "tab": "Switch to tab ",
                                          "space": "Switch to Space ",
                                          "history": "Open history result ",
                                          "keyword": "Search ",
                                          "command": "Run "
                                      })[modelData.kind] + row.title
                    // The row shows a history result's host; the whole address
                    // is still there to be heard.
                    Accessible.description: modelData.kind === "history" ? modelData.url : ""

                    Rectangle {
                        anchors.fill: parent
                        color: row.isSelected || rowMouse.containsMouse ? root.colors.surface :
                                                                          "transparent"
                    }

                    Rectangle {
                        width: 2
                        height: parent.height
                        anchors.left: parent.left
                        color: row.isSelected ? root.colors.accent : "transparent"
                    }

                    // The picture the row leads with: a site's tile, a Space's
                    // colour, or its command group's symbol.
                    Item {
                        id: picture
                        objectName: "omnibarRowPicture"
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: 20
                        height: 20

                        SiteTile {
                            objectName: "omnibarRowTile"
                            anchors.fill: parent
                            visible: row.site.length > 0
                            colors: root.colors
                            siteUrl: row.site
                            iconUrl: modelData.kind === "tab" ? modelData.icon : ""
                            useArtwork: root.useFavicons
                            tintArtwork: root.tintFavicons
                        }

                        Rectangle {
                            objectName: "omnibarRowSpaceColor"
                            anchors.centerIn: parent
                            visible: modelData.kind === "space"
                            width: 14
                            height: 14
                            radius: 3
                            color: modelData.color ? modelData.color : root.colors.accent
                        }

                        Text {
                            objectName: "omnibarRowSymbol"
                            anchors.centerIn: parent
                            visible: modelData.kind === "command"
                            text: visible ? root.commands.groupSymbols[modelData.group] || "" : ""
                            color: row.isSelected ? root.colors.text : root.colors.mutedText
                            opacity: row.usable ? 1 : 0.6
                            font.family: root.iconFontFamily
                            font.pixelSize: Style.font.iconLarge
                        }
                    }

                    Item {
                        anchors.left: picture.right
                        anchors.leftMargin: 10
                        anchors.right: rowEdge.left
                        anchors.rightMargin: 12
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        // The title gives way before the host does, since the
                        // host is what names the site.
                        Text {
                            id: rowTitle
                            objectName: "omnibarRowTitle"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, parent.width - (rowHost.visible
                                                                           ? Math.min(
                                                                                 rowHost.implicitWidth,
                                                                                 parent.width / 2)
                                                                             + 10 : 0))
                            text: modelData.kind === "command" ? root.commands.highlight(row.title,
                                                                                         input.text) :
                                                                 row.title
                            textFormat: modelData.kind === "command" ? Text.StyledText :
                                                                       Text.PlainText
                            color: row.usable ? root.colors.text : root.colors.mutedText
                            opacity: row.usable ? 1 : 0.6
                            elide: Text.ElideRight
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }

                        Text {
                            id: rowHost
                            objectName: "omnibarRowHost"
                            anchors.left: rowTitle.right
                            anchors.leftMargin: 10
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: row.host.length > 0
                            text: row.host
                            color: root.colors.mutedText
                            elide: Text.ElideMiddle
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }
                    }

                    Row {
                        id: rowEdge
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        Text {
                            objectName: "omnibarRowKeys"
                            visible: row.keys.length > 0
                            text: row.keys
                            color: root.colors.mutedText
                            opacity: 0.85
                            font.family: Style.font.family
                            font.pixelSize: Style.font.caption
                        }

                        // Bright only on the row Return would commit.
                        Text {
                            objectName: "omnibarRowAction"
                            visible: row.action.length > 0
                            text: row.action
                            color: row.isSelected ? root.colors.text : root.colors.mutedText
                            opacity: row.isSelected ? 1 : 0.85
                            font.family: Style.font.family
                            font.pixelSize: Style.font.caption
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = row.index
                        onClicked: {
                            root.selected = row.index;
                            root.accept();
                        }
                    }
                }
            }
        }
    }
}
