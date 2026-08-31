import QtQuick
import qs.Commons
import qs.Ui as Omarchy

Item {
    id: root
    objectName: (pinned ? "pinned-" : "tab-") + tabId

    required property string tabId
    required property string tabTitle
    required property url tabUrl
    required property url tabIconUrl
    required property bool pinned
    required property bool active
    required property bool loading
    property var colors
    property string iconFontFamily
    property bool useFavicons: true
    property bool tintFavicons: true

    signal activated(string tabId)
    signal closeRequested(string tabId)

    height: pinned ? 44 : 36
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTab
    Accessible.name: pinned ? "Pinned: " + tabTitle : tabTitle
    Accessible.onPressAction: root.activated(root.tabId)

    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
            root.activated(root.tabId)
            event.accepted = true
        }
    }

    Omarchy.Button {
        id: tabButton
        anchors.fill: parent
        active: root.active
        hasCursor: root.activeFocus || hoverArea.containsMouse
        bordered: root.pinned
        foreground: root.colors.text
        background: "transparent"
        accent: root.colors.accent
        horizontalPadding: 0
        verticalPadding: 0
    }

    SiteTile {
        id: tile
        implicitWidth: root.pinned ? 22 : 18
        implicitHeight: root.pinned ? 22 : 18
        anchors.left: parent.left
        anchors.leftMargin: root.pinned ? (parent.width - width) / 2 : 8
        anchors.verticalCenter: parent.verticalCenter
        colors: root.colors
        siteUrl: root.tabUrl
        iconUrl: root.tabIconUrl
        highlighted: root.active
        useArtwork: root.useFavicons
        tintArtwork: root.tintFavicons
    }

    // The title names the page; its address is already in the address button
    // whenever the tab is the active one, and reading it twice crowds the row.
    Text {
        visible: !root.pinned
        anchors.left: tile.right
        anchors.leftMargin: 9
        anchors.right: parent.right
        anchors.rightMargin: closeButton.width + 10
        anchors.verticalCenter: parent.verticalCenter
        text: root.tabTitle.length > 0 ? root.tabTitle : tile.host
        color: root.active ? root.colors.text : root.colors.mutedText
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    MouseArea {
        id: hoverArea
        objectName: "tabPointer-" + root.tabId
        anchors.fill: parent
        z: 10
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: function(mouse) {
            root.forceActiveFocus()
            const overClose = !root.pinned
                && mouse.x >= root.width - closeButton.width - closeButton.anchors.rightMargin
            if (overClose) {
                root.closeRequested(root.tabId)
            } else {
                root.activated(root.tabId)
            }
        }

        Omarchy.BorderSurface {
            id: closeButton
            objectName: "close-" + root.tabId
            property string accessibleName: "Close " + root.tabTitle
            readonly property bool hot: hoverArea.containsMouse
                && hoverArea.mouseX >= root.width - width - anchors.rightMargin
            property color foreground: hoverArea.containsMouse
                ? root.colors.mutedText : "transparent"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            visible: !root.pinned
            radius: Style.cornerRadius
            color: hot ? Style.hoverFillFor(root.colors.mutedText, root.colors.accent)
                : "transparent"
            borderSpec: hot
                ? Border.controlSpec("hover-cursor", root.colors.mutedText, root.colors.accent)
                : Border.none()
            Accessible.role: Accessible.Button
            Accessible.name: accessibleName
            Accessible.onPressAction: root.closeRequested(root.tabId)

            Text {
                anchors.centerIn: parent
                text: "close"
                color: closeButton.foreground
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.icon
            }
        }
    }

}
