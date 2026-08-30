import QtQuick

Rectangle {
    id: root
    objectName: "navigationCluster"

    property var colors
    property var typography
    property string iconFontFamily
    property bool canGoBack: false
    property bool canGoForward: false
    property bool sidebarCollapsed: false

    signal backRequested()
    signal forwardRequested()
    signal reloadRequested()
    signal sidebarToggled()
    signal commandPanelRequested()

    width: row.implicitWidth + 16
    height: 34
    radius: 2
    color: colors.overlay
    border.width: 1
    border.color: colors.border
    opacity: hover.hovered || row.activeFocus ? 1 : 0.55

    HoverHandler { id: hover }

    Behavior on opacity {
        NumberAnimation { duration: 130 }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 4

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
