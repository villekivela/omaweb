import QtQuick
import qs.Commons

// What HTTPS-only mode shows where a page's own address could not be had over
// HTTPS. The engine draws an error page of its own for the failed load; this
// stands over it and says what happened in Omaweb's words: the host, why, and
// what the reader can do. Going back is the safe answer and comes first.
// Loading the plain address covers the one load; letting the site through for
// good is kept for the Space and is not offered in a Private window, which
// remembers nothing.
Rectangle {
    id: root
    objectName: "httpsOnlyPage"

    property var colors
    property string iconFontFamily
    property bool privateWindow: false
    // The engine's account of the failure: `plainUrl`, `host`, `reason` and
    // the engine's own words in `error`. Empty when there is none.
    property var failure: ({})
    readonly property bool open: !!root.failure && root.failure.plainUrl !== undefined
    readonly property string host: root.open ? String(root.failure.host) : ""
    readonly property string headline: {
        switch (root.failure.reason) {
        case "downgrade":
            return root.host + " sent this page back to plain HTTP";
        case "form":
            return root.host + " asked for a form to be sent over plain HTTP";
        default:
            return root.host + " could not be reached over HTTPS";
        }
    }

    signal goBack
    signal loadOnce
    signal loadAlways

    visible: root.open
    color: root.colors.windowOpaque

    // The page is not the reader's to scroll or click through while this
    // stands over it.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Style.space(48), Style.space(560))
        spacing: Style.spacing.lg

        Text {
            text: "no_encryption"
            color: root.colors.urgent
            font.family: root.iconFontFamily
            font.pixelSize: Style.font.display
        }

        Text {
            objectName: "httpsOnlyHeadline"
            width: parent.width
            text: root.headline
            color: root.colors.text
            wrapMode: Text.Wrap
            font.family: Style.font.family
            font.pixelSize: Style.font.title
            font.weight: Font.DemiBold
        }

        Text {
            objectName: "httpsOnlyDetail"
            width: parent.width
            text: (String(root.failure.error || "").length > 0 && root.failure.reason === "unreachable"
                   ? String(root.failure.error) + ". " : "")
                  + "HTTPS-only mode is on. Over plain HTTP, anyone on the network between you "
                  + "and " + root.host + " can read the page and change it."
            color: root.colors.mutedText
            wrapMode: Text.Wrap
            font.family: Style.font.family
            font.pixelSize: Style.font.body
        }

        Flow {
            width: parent.width
            spacing: Style.spacing.md

            ActionButton {
                objectName: "httpsOnlyBack"
                colors: root.colors
                primary: true
                label: "Go back"
                onClicked: root.goBack()
            }

            ActionButton {
                objectName: "httpsOnlyLoadOnce"
                colors: root.colors
                label: "Load over HTTP once"
                onClicked: root.loadOnce()
            }

            ActionButton {
                objectName: "httpsOnlyLoadAlways"
                visible: !root.privateWindow
                colors: root.colors
                label: "Always use HTTP for " + root.host + " in this Space"
                onClicked: root.loadAlways()
            }
        }
    }
}
