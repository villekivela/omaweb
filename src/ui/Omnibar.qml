import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Shapes
import Omaweb
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
    // The browser's latest Engine suggestion answer: the engine it asked,
    // the terms, and that engine's proposals, four at most.
    property var engineSuggestions: ({})

    // The item to sample for the blur over a page. It must not be an
    // ancestor of this panel, or the effect source would feed on its own
    // output.
    property Item backdropSource: null
    // The Start page's road while it stands behind the panel, null over a
    // page: the Scene host, whose light falls on the panel's rim, and
    // `roadOrigin`, where it stands in this item's coordinates.
    property Item road: null
    property point roadOrigin: Qt.point(0, 0)
    // The Scene's light on the rim, where the road is behind the panel.
    readonly property var sunlight: road !== null ? road.light : null

    // At rest the glass blurs the road alone rather than the window: the road
    // moves every frame, and a blur of the window would render all of it
    // again for each one.
    readonly property Item glassSource: shownResting ? road : backdropSource
    readonly property bool blurActive: glassSource !== null && glassSource.visible

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
    // What the reader steps through, ranked against the typed text. In
    // command scope that is the commands alone.
    property var rows: []
    property int selected: 0
    // The open tabs' icons by host, read when the rows are ranked.
    property var siteIcons: ({})
    // Every other Space's tabs, read from the session for each opening and
    // each Space switch rather than for each keystroke, since the read asks
    // the store for every Space.
    property var awayTabs: []
    // The Spaces an Agent made, and what the Agents are attached to, so a
    // Space is named in the colour the footer draws it in.
    property var agentSpaceIds: []
    property var agentActivity: ({})

    // A Space of the reader's is drawn in the theme's colour for its palette
    // name. An Agent Space has none on screen: the Agent accent while an Agent
    // is attached to one of its tabs, muted while none is.
    function spaceColourOf(spaceId, colourName) {
        if (agentSpaceIds.indexOf(spaceId) >= 0) {
            for (const tabId in agentActivity) {
                if (agentActivity[tabId].spaceId === spaceId)
                    return colors.agentAccent;
            }
            return colors.mutedText;
        }
        const spaces = colors.spaces;
        return spaces && spaces[colourName] ? spaces[colourName] : colors.accent;
    }

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
    // listed in. A tab Omaweb put away is ranked as History is.
    readonly property var kindOrder: ["tab", "space", "history", "putaway", "keyword", "command"]

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
        clearField();
        Qt.callLater(focusField);
    }

    // The field as a fresh opening leaves it, wherever the keyboard is.
    function clearField() {
        readAwayTabs();
        engine = null;
        input.text = commandScope ? "" : presetText;
        refresh();
    }

    function readAwayTabs() {
        awayTabs = browser === null ? [] : browser.awaySpaceTabs();
    }

    Connections {
        target: root.browser
        enabled: root.open

        function onActiveSpaceChanged() {
            root.readAwayTabs();
            if (!root.commandScope)
                root.rank();
        }
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
    onEngineSuggestionsChanged: {
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
        siteIcons = commands.siteIcons();
        let candidates = suggestions.map(function (suggestion) {
            return {
                "kind": "history",
                "title": suggestion.title,
                "url": suggestion.url.toString()
            };
        }).concat(putAwayRows(engine === null ? query : "")).concat(keywordOffers.map(function (
            offer) {

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
            candidates = candidates.concat(commands.destinations(awayTabs), commands.actions().map(
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
        // The typed text is the selection, except where it starts an open
        // tab's title or host: the reader is naming that tab, and Return goes
        // to it rather than opening it a second time. Another Space's tab is
        // named only where no tab of the Space on show holds the text.
        let named = widened && ranked.length > 0 && ranked[0].row.kind === "tab"
            && ranked[0].strength === 3 ? ranked[0].row : null;
        if (named !== null && named.spaceId && ranked.some(function (entry) {
            return entry.row.kind === "tab" && !entry.row.spaceId;
        }))
            named = null;
        keepSpaceOnShowTabsFirst(ranked);
        const next = [];
        let listedCommands = 0;
        for (let index = 0; index < ranked.length; ++index) {
            if (ranked[index].row.kind === "command" && ++listedCommands > commandsBesideTheRest)
                continue;
            next.push(ranked[index].row);
        }
        rows = next.concat(proposedRows());
        selected = named === null ? -1 : next.indexOf(named);
    }

    // The Space's put-away tabs that hold the typed text in their title or
    // address, as the store answers History, so they are listed beside it.
    function putAwayRows(query) {
        const needle = query.toLowerCase();
        if (browser === null || needle.length === 0)
            return [];
        return browser.putAwayTabs.filter(function (tab) {
            return tab.title.toLowerCase().indexOf(needle) >= 0 || tab.url.toString().toLowerCase().indexOf(
                        needle) >= 0;
        }).map(function (tab) {
            return {
                "kind": "putaway",
                "id": tab.id,
                "title": tab.title,
                "url": tab.url.toString()
            };
        });
    }

    // The tabs of the Space on show come before any other Space's, however
    // weakly they hold the text, and the others keep their Space order. Tab
    // rows trade places only among themselves, so every other row stays
    // where its strength put it.
    function keepSpaceOnShowTabsFirst(ranked) {
        const slots = [];
        const local = [];
        const away = [];
        for (let index = 0; index < ranked.length; ++index) {
            if (ranked[index].row.kind !== "tab")
                continue;
            slots.push(index);
            (ranked[index].row.spaceId ? away : local).push(ranked[index]);
        }
        away.sort(function (left, right) {
            return left.order - right.order;
        });
        const ordered = local.concat(away);
        for (let slot = 0; slot < slots.length; ++slot)
            ranked[slots[slot]] = ordered[slot];
    }

    // Engine suggestions come after every row of the reader's own, and only
    // for the search the text makes now. An answer stays listed while the
    // reader types on past the terms it was asked for, so the rows do not
    // blink out on each keystroke, but not once the text has left them or a
    // keyword has chosen another engine. A proposal of the terms themselves
    // is the typed text again, which Return already is.
    function proposedRows() {
        const answer = engineSuggestions;
        const terms = (intent.terms || "").toLowerCase();
        if (!answer.suggestions || terms.length === 0 || answer.engineId !== intent.engineId)
            return [];
        if (!terms.startsWith(answer.terms.toLowerCase()))
            return [];
        return answer.suggestions.filter(function (suggestion) {
            return suggestion.toLowerCase() !== terms;
        }).map(function (suggestion) {
            return {
                "kind": "suggestion",
                "title": suggestion,
                "typed": intent.terms,
                "engineId": answer.engineId,
                "engineName": answer.engineName,
                "siteUrl": answer.siteUrl
            };
        });
    }

    // What the engine proposed beyond what was typed is bold. The proposal
    // is the engine's text, so it is escaped before any markup is added:
    // styled text would otherwise draw what an engine sent as tags.
    function proposalMarkup(proposal, typed) {
        const kept = proposal.toLowerCase().startsWith(typed.toLowerCase()) ? typed.length : 0;
        return escaped(proposal.substring(0, kept)) + "<b>" + escaped(proposal.substring(kept))
                + "</b>";
    }

    // What a screen reader hears for a row: what committing it does, and to
    // what.
    function spokenName(row, title) {
        if (row.kind === "suggestion")
            return "Search " + row.engineName + " for " + title;
        if (row.kind === "putaway")
            return qsTr("Reopen put-away tab %1").arg(title);
        const verbs = {
            "tab": "Switch to tab ",
            "space": "Switch to Space ",
            "history": "Open history result ",
            "keyword": "Search ",
            "command": "Run "
        };
        return verbs[row.kind] + title;
    }

    // A command shows where the typed letters fell and a proposal what it
    // adds to them. Every other title is plain text.
    function titleText(row, title) {
        if (row.kind === "command")
            return commands.highlight(title, input.text);
        if (row.kind === "suggestion")
            return proposalMarkup(title, row.typed);
        return title;
    }

    function escaped(text) {
        return text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    function asCommand(action) {
        return Object.assign({
                                 "kind": "command"
                             }, action);
    }

    // A row for a page, which is read by its title and its site's host.
    function namesAPage(kind) {
        return kind === "tab" || kind === "history" || kind === "putaway";
    }

    // History, put-away tabs and keywords were matched where they came from,
    // so they stay listed however weakly the text reads in them here.
    function strengthOf(row, query) {
        if (row.kind === "keyword")
            return 3;
        const fields = namesAPage(row.kind) ? [row.title, commands.host(row.url)] : [row.title];
        const found = query.length > 0 ? commands.tier(fields, query) : 0;
        return row.kind === "history" || row.kind === "putaway" ? Math.max(1, found) : found;
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
            if (row.kind === "putaway") {
                root.dismissed();
                browser.reopenPutAwayTab(row.id);
                return;
            }
            if (row.kind === "suggestion") {
                // A search of the engine that proposed it, whatever it
                // reads as: never an address to open.
                root.committed(browser.searchAddress(row.engineId, row.title));
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
            // A command chosen here was asked for by name, so it is a key's
            // decision even when its row was clicked: the Omnibar goes, and
            // what the command does arrives, settled.
            InputOrigin.pointer = false;
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
        color: "transparent"
        border.width: 1
        // The sun's rim light is laid over a quiet edge, as the website's is;
        // with no sun behind it the edge is the accent.
        border.color: root.sunlight !== null ? root.colors.border : root.colors.accent
        clip: true

        // Glass: what is behind blurred under the overlay. Over the road it
        // lets a little more through, as the floating sidebar does over a
        // page, and blurs as little as the website's: the road is the page.
        PageBackdrop {
            objectName: "omnibarGlass"

            readonly property bool overRoad: root.glassSource !== null && root.glassSource
                                             === root.road
            readonly property point origin: overRoad ? root.roadOrigin : Qt.point(0, 0)
            readonly property color overlay: root.colors.overlay

            anchors.fill: parent
            anchors.margins: panel.border.width
            radius: panel.radius
            source: root.blurActive ? root.glassSource : null
            textureScale: 0.5
            sourceRect: Qt.rect(panel.x + x - origin.x, panel.y + y - origin.y, width, height)
            blur: overRoad ? 14 : 48
            tint: overRoad ? Qt.rgba(overlay.r, overlay.g, overlay.b, Math.min(overlay.a, 0.8)) :
                             overlay

        }

        // The bloom's half inside the edge, over the glass and under the text,
        // as the website's is.
        RimLight {
            objectName: "omnibarInnerBloom"
            inner: true
            plate: panel
            plateRadius: panel.radius
            light: root.sunlight
            sun: root.sunlight !== null ? Qt.point(root.roadOrigin.x + root.sunlight.centre.x - panel.x,
                                                   root.roadOrigin.y + root.sunlight.centre.y
                                                   - panel.y) : Qt.point(0, 0)
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
                    objectName: "omnibarColon"
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
                    readonly property string host: root.namesAPage(modelData.kind)
                                                   ? root.commands.host(modelData.url) : ""
                    // The site a tile draws: the page for a tab or a history
                    // row, the engine's own site for a keyword or an Engine
                    // suggestion.
                    readonly property string site: modelData.kind === "keyword" || modelData.kind
                                                   === "suggestion" ? modelData.siteUrl : (
                                                                          modelData.url || "")
                    // What committing the row does, at its right edge. A
                    // command runs, which its keys already say.
                    readonly property string action: ({
                                                          "tab": "switch tab →",
                                                          "space": "switch space →",
                                                          "history": "open →",
                                                          "putaway": qsTr("reopen →"),
                                                          "keyword": "search →",
                                                          "suggestion": "search →",
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
                    // Another Space's tab names its Space, since committing
                    // the row switches to it.
                    readonly property string spaceName: modelData.kind === "tab"
                                                        && modelData.spaceName
                                                        ? modelData.spaceName : ""

                    width: rowList.width
                    height: 28
                    Accessible.role: Accessible.Button
                    Accessible.name: root.spokenName(modelData, row.title) + (row.spaceName.length
                                                                              > 0 ? " in "
                                                                                    + row.spaceName :
                                                                                    "")
                    // The row shows a history result's host; the whole address
                    // is still there to be heard.
                    Accessible.description: modelData.kind === "history" || modelData.kind
                                            === "putaway" ? modelData.url : ""

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
                            // A tab has its own icon. A history or keyword
                            // row borrows the icon of an open tab on its
                            // site, and otherwise the one the Space stored
                            // for the page or its site. A site the Space
                            // never loaded draws the host code.
                            iconUrl: modelData.kind === "tab" ? modelData.icon :
                                                                root.siteIcons[root.commands.host(
                                                                                   row.site)] || (
                                                                    root.browser
                                                                    ? root.browser.storedFavicon(
                                                                          row.site) : "")
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
                            color: root.spaceColourOf(modelData.argument, modelData.color)
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
                        id: rowText
                        // What the title and the host share once another
                        // Space's name has its place.
                        readonly property real sharedWidth: width - rowSpace.reservedWidth

                        anchors.left: picture.right
                        anchors.leftMargin: 10
                        anchors.right: rowEdge.left
                        anchors.rightMargin: 12
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom

                        // The title gives way before the host does, since the
                        // host is what names the site, and both give way
                        // before another Space's name, which says where
                        // committing the row goes.
                        Text {
                            id: rowTitle
                            objectName: "omnibarRowTitle"
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, rowText.sharedWidth - (rowHost.visible
                                                                                  ? Math.min(
                                                                                        rowHost.implicitWidth,
                                                                                        rowText.sharedWidth
                                                                                        / 2) + 10 :
                                                                                    0))
                            text: root.titleText(modelData, row.title)
                            textFormat: modelData.kind === "command" || modelData.kind
                                        === "suggestion" ? Text.StyledText : Text.PlainText
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
                            anchors.rightMargin: rowSpace.reservedWidth
                            visible: row.host.length > 0
                            text: row.host
                            color: root.colors.mutedText
                            elide: Text.ElideMiddle
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }

                        Text {
                            id: rowSpace
                            objectName: "omnibarRowSpace"
                            readonly property real reservedWidth: visible ? implicitWidth + 10 : 0
                            x: (rowHost.visible ? rowHost.x + Math.min(rowHost.implicitWidth,
                                                                       rowHost.width) :
                                                  rowTitle.width) + 10
                            anchors.verticalCenter: parent.verticalCenter
                            visible: row.spaceName.length > 0
                            text: row.spaceName
                            color: root.spaceColourOf(modelData.spaceId, modelData.spaceColor)
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

    // The sun's light on the panel's rim, from where the road's sun stands,
    // and its bloom's half outside the edge.
    RimLight {
        objectName: "omnibarRim"
        plate: panel
        plateRadius: panel.radius
        light: root.sunlight
        sun: root.sunlight !== null ? Qt.point(root.roadOrigin.x + root.sunlight.centre.x,
                                               root.roadOrigin.y + root.sunlight.centre.y) :
                                      Qt.point(0, 0)
    }
}
