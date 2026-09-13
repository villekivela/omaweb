// PROTOTYPE — throwaway. The shared root for the interface-direction
// variants (docs/product/interface-direction.md): in-memory data, chord
// handling and the floating switcher. Variants render `proto`; nothing here
// talks to the browser controller. `omaweb-ui-lab --prototype X:picker,sidebar`
// loads variant X with the named states open, for a capture.
//
// Every command is a chord, the way Omaweb's own keyboard navigation works.
// There is no leader, no mode and no prefix to discover: variants A to G,
// which were built on one, are in this branch's history and out of the
// running.
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import qs.Commons

ApplicationWindow {
    id: window

    property string variant: ""
    property string openStates: ""
    readonly property var variants: []
    readonly property var variantNames: ({})

    width: 1360
    height: 860
    visible: true
    color: "transparent"
    flags: Qt.platform.os === "osx" ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint :
                                      Qt.Window | Qt.FramelessWindowHint
    title: "Omaweb — prototype " + variant

    readonly property var colors: theme.palette

    // ------------------------------------------------------------ shared state
    QtObject {
        id: proto
        property bool hintsShowing: false
        property bool pickerOpen: false
        property string pickerSource: "tabs"  // tabs | spaces | settings
        property string query: ""
        property int cursor: 0
        property bool sidebarOpen: false
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
        // Tabs across every Space, since a picker lists all of them.
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

        // The chords, for any variant that wants to show them beside what
        // they run.
        readonly property var chords: [
            {
                key: "Ctrl+K",
                label: "tabs",
                command: "tabs"
            },
            {
                key: "Ctrl+S",
                label: "spaces",
                command: "spaces"
            },
            {
                key: "Ctrl+B",
                label: "sidebar tree",
                command: "sidebar"
            },
            {
                key: "Ctrl+F",
                label: "find",
                command: "find"
            },
            {
                key: "F",
                label: "link hints",
                command: "hints"
            },
            {
                key: "Alt+1-9",
                label: "pinned tab",
                command: "pin1"
            },
            {
                key: "Ctrl+,",
                label: "settings",
                command: "settings"
            },
            {
                key: "Ctrl+N",
                label: "notify (demo)",
                command: "notify"
            },
            {
                key: "Ctrl+G",
                label: "load (demo)",
                command: "load"
            },
            {
                key: "Ctrl+/",
                label: "ask (demo)",
                command: "ask"
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
        function run(command) {
            keys.run(command);
        }
        function closeAll() {
            hintsShowing = false;
            pickerOpen = false;
            findOpen = false;
            promptOpen = false;
            query = "";
            cursor = 0;
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

    // ------------------------------------------------------------ chords
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
            if (event.modifiers & Qt.ControlModifier) {
                switch (event.key) {
                case Qt.Key_K:
                    run("tabs");
                    break;
                case Qt.Key_S:
                    run("spaces");
                    break;
                case Qt.Key_B:
                    run("sidebar");
                    break;
                case Qt.Key_F:
                    run("find");
                    break;
                case Qt.Key_N:
                    run("notify");
                    break;
                case Qt.Key_G:
                    run("load");
                    break;
                case Qt.Key_Slash:
                case Qt.Key_Question:
                    run("ask");
                    break;
                case Qt.Key_Comma:
                    run("settings");
                    break;
                default:
                    return;
                }
                event.accepted = true;
                return;
            }
            if ((event.modifiers & Qt.AltModifier) && event.key >= Qt.Key_1 && event.key
                    <= Qt.Key_9) {
                run("pin" + (event.key - Qt.Key_0));
                event.accepted = true;
                return;
            }
            if (event.key === Qt.Key_F && !event.modifiers) {
                run("hints");
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
        // One place every command runs from, whether by chord or by click.
        function run(command) {
            switch (command) {
            case "tabs":
                proto.pickerSource = "tabs";
                proto.pickerOpen = true;
                proto.cursor = proto.activeTab;
                pickerInput.forceActiveFocus();
                break;
            case "spaces":
                proto.pickerSource = "spaces";
                proto.pickerOpen = true;
                proto.cursor = proto.activeSpace;
                pickerInput.forceActiveFocus();
                break;
            case "settings":
                proto.pickerSource = "settings";
                proto.pickerOpen = true;
                proto.cursor = 0;
                pickerInput.forceActiveFocus();
                break;
            case "sidebar":
                proto.sidebarOpen = !proto.sidebarOpen;
                break;
            case "find":
                proto.findOpen = !proto.findOpen;
                break;
            case "hints":
                proto.hintsShowing = !proto.hintsShowing;
                break;
            case "notify":
                proto.toast("Download finished — qt6-webengine-6.11.1.tar.zst");
                proto.downloads = 0;
                break;
            case "load":
                proto.loading = true;
                proto.loadProgress = 0;
                break;
            case "ask":
                proto.promptOpen = true;
                break;
            default:
                if (command.startsWith("pin")) {
                    const pinned = proto.currentTabs.filter(t => t.pinned);
                    const t = pinned[parseInt(command.slice(3)) - 1];
                    if (t)
                        proto.activeTab = proto.tabs.indexOf(t);
                } else {
                    proto.toast(command + " — not in the prototype");
                }
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
            if (window.variants.length === 0)
                return;
            const i = (window.variants.indexOf(window.variant) + step + window.variants.length)
                  % window.variants.length;
            window.variant = window.variants[i];
        }
    }
    // One hidden input carries the picker's typed query for every variant.
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
            case "picker":
                keys.run("tabs");
                break;
            case "spaces":
                keys.run("spaces");
                break;
            case "sidebar":
                proto.sidebarOpen = true;
                break;
            case "toast":
                keys.run("notify");
                toastTimer.stop();
                break;
            case "hint":
                proto.hintsShowing = true;
                break;
            case "prompt":
                proto.promptOpen = true;
                break;
            case "find":
                proto.findOpen = true;
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
        source: window.variant.length ? "Variant" + window.variant + ".qml" : ""
        onLoaded: {
            item.proto = proto;
            item.colors = window.colors;
        }
    }
    Text {
        visible: window.variant.length === 0
        anchors.centerIn: parent
        text: "No variant loaded. The next round's variants register in `variants`."
        color: window.colors.mutedText
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    // ------------------------------------------------------------ switcher
    Rectangle {
        visible: window.variants.length > 0
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
                text: window.variant + " — " + (window.variantNames[window.variant] || "")
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
