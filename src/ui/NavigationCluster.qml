import QtQuick
import QtQuick.Effects

Rectangle {
    id: root
    objectName: "navigationCluster"

    property var colors
    property var typography
    property string iconFontFamily
    property bool canGoBack: false
    property bool canGoForward: false
    property bool sidebarCollapsed: false

    // The item to sample for the blur. It must not be an ancestor of this
    // strip, or the effect source would feed on its own output.
    property Item backdropSource: null

    readonly property bool blurActive: backdropSource !== null && backdropSource.visible

    signal backRequested()
    signal forwardRequested()
    signal reloadRequested()
    signal sidebarToggled()
    signal commandPanelRequested()

    width: row.implicitWidth + 16
    height: 34
    radius: 2
    // With a backdrop the tint goes on top of the blur instead, so the strip
    // itself stays clear.
    color: blurActive ? "transparent" : colors.overlay
    border.width: 1
    border.color: colors.border

    HoverHandler { id: hover }

    ShaderEffectSource {
        id: backdropTexture
        visible: false
        live: true
        hideSource: false
        recursive: false
        sourceItem: root.blurActive ? root.backdropSource : null
        // Only the slice of the page the strip covers, in the source's
        // coordinates. The strip is a sibling of the source, so its own
        // position is that mapping.
        sourceRect: root.blurActive
            ? Qt.rect(root.x, root.y, root.width, root.height)
            : Qt.rect(0, 0, 0, 0)
        width: Math.max(1, root.width)
        height: Math.max(1, root.height)
        textureSize: Qt.size(Math.max(1, Math.round(root.width / 2)),
                             Math.max(1, Math.round(root.height / 2)))
    }

    MultiEffect {
        anchors.fill: parent
        anchors.margins: root.border.width
        visible: root.blurActive
        source: backdropTexture
        blurEnabled: true
        blur: 1
        blurMax: 48
        // Keeps the blur inside the strip's rounded corners rather than
        // squaring them off under the border.
        maskEnabled: true
        maskSource: ShaderEffectSource {
            sourceItem: Rectangle {
                width: Math.max(1, root.width - 2 * root.border.width)
                height: Math.max(1, root.height - 2 * root.border.width)
                radius: root.radius
                color: "black"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: root.border.width
        visible: root.blurActive
        radius: root.radius
        color: root.colors.overlay
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4

        // The strip recedes by fading its contents, not itself: dropping the
        // whole item's opacity would let the page bleed sharply through the
        // blurred copy of itself.
        opacity: hover.hovered || row.activeFocus ? 1 : 0.55

        Behavior on opacity {
            NumberAnimation { duration: 130 }
        }

        ChromeButton {
            objectName: "backButton"
            width: 28
            height: 26
            label: "arrow_back"
            accessibleName: "Back"
            fontFamily: root.iconFontFamily
            foreground: root.colors.text
            hoverBackground: root.colors.surfaceHover
            enabled: root.canGoBack
            onClicked: root.backRequested()
        }

        ChromeButton {
            objectName: "forwardButton"
            width: 28
            height: 26
            label: "arrow_forward"
            accessibleName: "Forward"
            fontFamily: root.iconFontFamily
            foreground: root.colors.text
            hoverBackground: root.colors.surfaceHover
            enabled: root.canGoForward
            onClicked: root.forwardRequested()
        }

        ChromeButton {
            objectName: "reloadButton"
            width: 28
            height: 26
            label: "refresh"
            accessibleName: "Reload"
            fontFamily: root.iconFontFamily
            foreground: root.colors.text
            hoverBackground: root.colors.surfaceHover
            onClicked: root.reloadRequested()
        }

        Rectangle {
            width: 1
            height: 18
            anchors.verticalCenter: parent.verticalCenter
            color: root.colors.border
        }

        ChromeButton {
            objectName: "collapseButton"
            width: 28
            height: 26
            label: root.sidebarCollapsed ? "left_panel_open" : "left_panel_close"
            accessibleName: root.sidebarCollapsed ? "Show sidebar" : "Hide sidebar"
            fontFamily: root.iconFontFamily
            foreground: root.colors.mutedText
            hoverBackground: root.colors.surfaceHover
            onClicked: root.sidebarToggled()
        }

        ChromeButton {
            objectName: "commandPanelButton"
            width: 28
            height: 26
            label: "search"
            accessibleName: "Command panel"
            fontFamily: root.iconFontFamily
            foreground: root.colors.mutedText
            hoverBackground: root.colors.surfaceHover
            onClicked: root.commandPanelRequested()
        }
    }
}
