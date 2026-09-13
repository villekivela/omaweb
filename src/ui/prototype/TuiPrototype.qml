// PROTOTYPE — throwaway. Three variants of the main window in the "TUI for
// the web" direction (docs/product/interface-direction.md), switchable with
// `omaweb-ui-lab --prototype A|B|C` and the floating bar's arrows. Data is in
// memory and invented; nothing here talks to the browser controller.
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import qs.Commons

ApplicationWindow {
    id: window

    // `--prototype B:picker,sidebar` opens the named states for a capture.
    property string variant: "D"
    property string openStates: ""
    readonly property var variants: ["A", "B", "C", "D"]
    readonly property var variantNames: ({
                                             "A": "Lualine — plain, dense, hairlines",
                                             "B": "HUD — brackets, ruled ground, tracked titles",
                                             "C": "Tmux — status on top, floats hang down",
                                             "D": "A + C — bottom line, Spaces listed, numbered tree"
                                         })

    width: 1360
    height: 860
    visible: true
    color: "transparent"
    flags: Qt.platform.os === "osx" ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint :
                                      Qt.Window | Qt.FramelessWindowHint
    title: "Omaweb — TUI prototype " + variant

    readonly property var colors: theme.palette

    // ------------------------------------------------------------ shared state
    // Everything a variant draws. Variants render it; the keys below mutate it.
    QtObject {
        id: proto
        property string mode: "NORMAL"      // NORMAL | PAGE | HINT | PICK
        property string pendingKeys: ""
        property bool whichKeyOpen: false
        property bool pickerOpen: false
        property string pickerSource: "tabs"  // tabs | spaces | settings
        property string query: ""
        property int cursor: 0
        property bool sidebarOpen: false
        property bool statusOnTop: false
        property bool findOpen: false
        property bool promptOpen: false
        property bool loading: false
        property real loadProgress: 0
        property var toasts: []
        property int activeSpace: 0
        property int activeTab: 4
        property int blocked: 17
        property int downloads: 1
        property real downloadProgress: 0.43

        readonly property var spaces: [
            {
                name: "work",
                color: window.colors.accent
            },
            {
                name: "home",
                color: window.colors.syntax ? window.colors.syntax.string : window.colors.accent
            },
            {
                name: "lab",
                color: window.colors.syntax ? window.colors.syntax.number : window.colors.accent
            }
        ]
        // Tabs across every Space, since the picker lists all of them.
        readonly property var tabs: [
            {
                space: 0,
                title: "Inbox",
                origin: "mail.proton.me",
                pinned: true,
                age: "frozen 2 h ago"
            },
            {
                space: 0,
                title: "Calendar",
                origin: "calendar.google.com",
                pinned: true,
                age: "keep active"
            },
            {
                space: 0,
                title: "Notifications",
                origin: "github.com",
                pinned: true,
                age: "frozen 14 min ago"
            },
            {
                space: 0,
                title: "Library",
                origin: "music.youtube.com",
                pinned: true,
                age: "making sound"
            },
            {
                space: 0,
                title: "Generate the website's screenshots · PR #124",
                origin: "github.com",
                pinned: false,
                age: "on show"
            },
            {
                space: 0,
                title: "Qt Quick Scene Graph",
                origin: "doc.qt.io",
                pinned: false,
                age: "frozen 41 min ago"
            },
            {
                space: 0,
                title: "xdg-shell protocol",
                origin: "wayland.app",
                pinned: false,
                age: "frozen 1 h ago"
            },
            {
                space: 0,
                title: "Arch Linux - qt6-webengine",
                origin: "archlinux.org",
                pinned: false,
                age: "frozen 3 h ago"
            },
            {
                space: 0,
                title: "Omarchy",
                origin: "omarchy.org",
                pinned: false,
                age: "frozen yesterday"
            },
            {
                space: 1,
                title: "Front page",
                origin: "news.ycombinator.com",
                pinned: false,
                age: "frozen 2 d ago"
            },
            {
                space: 1,
                title: "Ratatui — build rich TUIs",
                origin: "ratatui.rs",
                pinned: false,
                age: "frozen 2 d ago"
            },
            {
                space: 1,
                title: "r/unixporn",
                origin: "reddit.com",
                pinned: true,
                age: "frozen 5 d ago"
            },
            {
                space: 2,
                title: "localhost:3000",
                origin: "localhost",
                pinned: false,
                age: "keep active"
            },
            {
                space: 2,
                title: "Ladybird",
                origin: "ladybird.org",
                pinned: false,
                age: "frozen 6 h ago"
            }
        ]
        readonly property var currentTabs: tabs.filter(t => t.space === activeSpace)
        readonly property var activeTabInfo: tabs[activeTab]
        readonly property int activeTabIndexInSpace: currentTabs.indexOf(activeTabInfo) + 1

        readonly property var leaderBindings: [
            {
                key: "t",
                label: "tabs"
            },
            {
                key: "s",
                label: "spaces"
            },
            {
                key: "h",
                label: "history"
            },
            {
                key: "1-9",
                label: "pinned tab"
            },
            {
                key: "b",
                label: "sidebar tree"
            },
            {
                key: "o",
                label: "open address"
            },
            {
                key: "d",
                label: "downloads"
            },
            {
                key: ",",
                label: "settings"
            },
            {
                key: "x",
                label: "close tab"
            },
            {
                key: "u",
                label: "reopen closed"
            },
            {
                key: "r",
                label: "reload"
            },
            {
                key: "f",
                label: "hints"
            },
            {
                key: "i",
                label: "page mode"
            },
            {
                key: "n",
                label: "notify (demo)"
            },
            {
                key: "l",
                label: "load (demo)"
            },
            {
                key: "?",
                label: "ask (demo)"
            },
            {
                key: "p",
                label: "private window"
            },
            {
                key: "q",
                label: "quit"
            }
        ]

        function filteredTabs() {
            const q = query.toLowerCase();
            return tabs.map((t, i) => Object.assign({
                                                        index: i
                                                    }, t)).filter(t => q.length === 0
                                                                       || t.title.toLowerCase(
                                                                           ).includes(q)
                                                                       || t.origin.includes(q));
        }
        function toast(text) {
            toasts = toasts.concat([
                                       {
                                           text: text,
                                           id: Date.now()
                                       }
                                   ]);
            toastTimer.restart();
        }
        function leader(text) {
            keys.leaderKey(text);
        }
        function closeAll() {
            whichKeyOpen = false;
            pickerOpen = false;
            findOpen = false;
            promptOpen = false;
            pendingKeys = "";
            query = "";
            cursor = 0;
            mode = "NORMAL";
        }
    }
    Timer {
        id: toastTimer
        interval: 3200
        onTriggered: proto.toasts = []
    }
    Timer {
        interval: 400
        repeat: true
        running: proto.loading
        onTriggered: {
            proto.loadProgress += 0.04;
            if (proto.loadProgress >= 1) {
                proto.loading = false;
                proto.loadProgress = 0;
            }
        }
    }

    // ------------------------------------------------------------ keys
    // Space is the leader. After it, one key chooses. Esc always backs out.
    Item {
        id: keys
        anchors.fill: parent
        focus: !pickerInput.activeFocus
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                proto.closeAll();
                event.accepted = true;
                return;
            }
            if (proto.pickerOpen) {
                pickerKeys(event);
                return;
            }
            if (proto.promptOpen) {
                if (event.text === "y" || event.text === "n") {
                    proto.promptOpen = false;
                    proto.toast(event.text === "y" ? "Cleared browsing data for work" :
                                                     "Kept browsing data");
                }
                event.accepted = true;
                return;
            }
            if (proto.mode === "PAGE") {
                if (event.key === Qt.Key_BracketLeft && event.modifiers & Qt.ControlModifier)
                    proto.closeAll();
                return;
            }
            if (proto.whichKeyOpen) {
                leaderKey(event.text);
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Space) {
                proto.whichKeyOpen = true;
                proto.pendingKeys = "SPC";
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Slash) {
                proto.findOpen = true;
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Left && !event.modifiers) {
                cycle(-1);
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_Right && !event.modifiers) {
                cycle(1);
                event.accepted = true;
                return;
            }
        }
        function leaderKey(text) {
            proto.whichKeyOpen = false;
            proto.pendingKeys = "";
            switch (text) {
            case "t":
                proto.pickerSource = "tabs";
                proto.pickerOpen = true;
                proto.mode = "PICK";
                proto.cursor = proto.activeTab;
                pickerInput.forceActiveFocus();
                break;
            case "s":
                proto.pickerSource = "spaces";
                proto.pickerOpen = true;
                proto.mode = "PICK";
                proto.cursor = proto.activeSpace;
                pickerInput.forceActiveFocus();
                break;
            case ",":
                proto.pickerSource = "settings";
                proto.pickerOpen = true;
                proto.mode = "PICK";
                proto.cursor = 0;
                pickerInput.forceActiveFocus();
                break;
            case "b":
                proto.sidebarOpen = !proto.sidebarOpen;
                break;
            case "n":
                proto.toast("Download finished — qt6-webengine-6.11.1.tar.zst");
                proto.downloads = 0;
                break;
            case "l":
                proto.loading = true;
                proto.loadProgress = 0;
                break;
            case "?":
                proto.promptOpen = true;
                break;
            case "i":
                proto.mode = "PAGE";
                break;
            case "f":
                proto.mode = "HINT";
                break;
            case "1":
            case "2":
            case "3":
            case "4":
            {
                const pinned = proto.currentTabs.filter(t => t.pinned);
                const t = pinned[parseInt(text) - 1];
                if (t)
                    proto.activeTab = proto.tabs.indexOf(t);
                break;
            }
            default:
                proto.toast("SPC " + text + " — not in the prototype");
            }
        }
        function pickerKeys(event) {
            const rows = proto.pickerSource === "tabs" ? proto.filteredTabs().length : proto.pickerSource
                                                         === "spaces" ? proto.spaces.length : 8;
            if (event.key === Qt.Key_Down || (event.key === Qt.Key_N && event.modifiers
                                              & Qt.ControlModifier)) {
                proto.cursor = (proto.cursor + 1) % rows;
                event.accepted = true;
            } else if (event.key === Qt.Key_Up || (event.key === Qt.Key_P && event.modifiers
                                                   & Qt.ControlModifier)) {
                proto.cursor = (proto.cursor - 1 + rows) % rows;
                event.accepted = true;
            } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                if (proto.pickerSource === "tabs") {
                    const t = proto.filteredTabs()[proto.cursor];
                    if (t) {
                        proto.activeTab = t.index;
                        proto.activeSpace = t.space;
                    }
                }
                if (proto.pickerSource === "spaces") {
                    proto.activeSpace = proto.cursor;
                    proto.activeTab = proto.tabs.indexOf(proto.tabs.filter(t => t.space
                                                                                === proto.cursor)[0]);
                }
                proto.closeAll();
                keys.forceActiveFocus();
                event.accepted = true;
            }
        }
        function cycle(step) {
            const i = (window.variants.indexOf(window.variant) + step + window.variants.length)
                  % window.variants.length;
            window.variant = window.variants[i];
        }
    }
    // One hidden input carries the picker's typed query for every variant, so
    // the variants only draw it.
    TextInput {
        id: pickerInput
        visible: false
        text: proto.query
        onTextChanged: {
            proto.query = text;
            proto.cursor = 0;
        }
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                proto.closeAll();
                keys.forceActiveFocus();
                event.accepted = true;
            } else
                keys.pickerKeys(event);
        }
    }
    Connections {
        target: proto
        function onPickerOpenChanged() {
            if (!proto.pickerOpen) {
                pickerInput.text = "";
                keys.forceActiveFocus();
            }
        }
    }

    Component.onCompleted: {
        for (const state of openStates.split(",")) {
            switch (state) {
            case "whichkey":
                proto.whichKeyOpen = true;
                proto.pendingKeys = "SPC";
                break;
            case "picker":
                keys.leaderKey("t");
                break;
            case "spaces":
                keys.leaderKey("s");
                break;
            case "sidebar":
                proto.sidebarOpen = true;
                break;
            case "toast":
                keys.leaderKey("n");
                toastTimer.stop();
                break;
            case "hint":
                proto.mode = "HINT";
                break;
            case "prompt":
                proto.promptOpen = true;
                break;
            case "find":
                proto.findOpen = true;
                break;
            case "top":
                proto.statusOnTop = true;
                break;
            case "loading":
                proto.loading = true;
                proto.loadProgress = 0.6;
                break;
            }
        }
    }

    Loader {
        anchors.fill: parent
        source: "Variant" + window.variant + ".qml"
        onLoaded: {
            item.proto = proto;
            item.colors = window.colors;
        }
    }

    // ------------------------------------------------------------ switcher
    // Not part of the design: a high-contrast pill so it is obviously the
    // prototype's own furniture.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 44
        width: switcherRow.width + 24
        height: 30
        radius: 15
        color: "#f5f5f5"
        border.color: "#111"
        opacity: 0.94
        Row {
            id: switcherRow
            anchors.centerIn: parent
            spacing: 12
            Text {
                text: "◀"
                color: "#111"
                font.pixelSize: 12
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    onClicked: keys.cycle(-1)
                }
            }
            Text {
                text: window.variant + " — " + window.variantNames[window.variant]
                color: "#111"
                font.family: Style.font.family
                font.pixelSize: 12
            }
            Text {
                text: "▶"
                color: "#111"
                font.pixelSize: 12
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -8
                    onClicked: keys.cycle(1)
                }
            }
        }
    }
}
