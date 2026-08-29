// THROWAWAY PROTOTYPE — see README.md in this directory.
// Three variants of Tanto's chrome, switchable from the floating bottom bar.
// They disagree about sidebar structure, the Space switcher, and where the
// security / permission / blocking status lives.
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: window

    width: 1360
    height: 860
    minimumWidth: 840
    minimumHeight: 560
    visible: true
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: browser.activeTitle + " — Tanto prototype"

    property var colors: theme.palette
    property bool omnibarOpen: false
    property bool newTabIntent: false

    readonly property var variants: [
        { key: "A", name: "Stack", source: "VariantStack.qml" },
        { key: "B", name: "Rail + flyout", source: "VariantRail.qml" },
        { key: "C", name: "Outline", source: "VariantOutline.qml" }
    ]

    property int variantIndex: {
        const requested = String(prototypeVariant).toUpperCase()
        for (let index = 0; index < variants.length; ++index) {
            if (variants[index].key === requested) return index
        }
        return 0
    }

    readonly property var variant: variants[variantIndex]

    function stepVariant(delta) {
        variantIndex = (variantIndex + delta + variants.length) % variants.length
    }

    FontLoader {
        id: materialSymbols
        source: iconFontSource
    }

    MockChromeState {
        id: mock
    }

    function openOmnibar(forNewTab) {
        newTabIntent = forNewTab
        omnibar.presetText = forNewTab ? "" : browser.activeUrl.toString()
        omnibarOpen = true
    }

    function closeOmnibar() {
        omnibarOpen = false
        newTabIntent = false
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+L" : "Ctrl+L"
        onActivated: window.openOmnibar(false)
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+T" : "Ctrl+T"
        onActivated: window.openOmnibar(true)
    }

    Shortcut {
        sequence: Qt.platform.os === "osx" ? "Meta+W" : "Ctrl+W"
        onActivated: browser.closeActiveTab()
    }

    Shortcut {
        sequence: "Left"
        enabled: !window.omnibarOpen
        onActivated: window.stepVariant(-1)
    }

    Shortcut {
        sequence: "Right"
        enabled: !window.omnibarOpen
        onActivated: window.stepVariant(1)
    }

    Rectangle {
        id: shell
        anchors.fill: parent
        radius: 14
        color: window.colors.window
        border.width: 1
        border.color: window.colors.border
        clip: true

        Loader {
            id: variantLoader
            anchors.fill: parent
            source: window.variant.source

            onLoaded: {
                item.colors = Qt.binding(function() { return window.colors })
                item.mock = mock
                item.iconFont = materialSymbols.name
                item.omnibarRequested.connect(window.openOmnibar)
            }
        }
    }

    PrototypeOmnibar {
        id: omnibar
        anchors.fill: parent
        z: 50
        colors: window.colors
        open: window.omnibarOpen
        newTabIntent: window.newTabIntent
        onDismissed: window.closeOmnibar()
        onCommitted: function(text) {
            browser.openInput(text, window.newTabIntent)
            window.closeOmnibar()
        }
    }

    PrototypeSwitcher {
        z: 60
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        colors: window.colors
        variantKey: window.variant.key
        variantName: window.variant.name
        stateText: (variantLoader.item ? variantLoader.item.variantState : "loading")
            + "  ·  " + mock.activeSpace.name + "  ·  " + mock.statusSummary
        onStep: function(delta) { window.stepVariant(delta) }
    }

    MouseArea {
        width: 5
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.LeftEdge)
        z: 100
    }

    MouseArea {
        width: 5
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeHorCursor
        onPressed: window.startSystemResize(Qt.RightEdge)
        z: 100
    }

    MouseArea {
        height: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.TopEdge)
        z: 100
    }

    MouseArea {
        height: 5
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        cursorShape: Qt.SizeVerCursor
        onPressed: window.startSystemResize(Qt.BottomEdge)
        z: 100
    }
}
