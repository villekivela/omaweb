import QtQuick

Rectangle {
    id: root

    property url currentUrl: "about:blank"
    property string pageTitle: currentUrl.toString() === "about:blank" ? "New tab" : currentUrl.toString()
    property bool loading: false
    property bool canGoBack: false
    property bool canGoForward: false
    property string profilePath: ""
    readonly property bool pageHasFocus: root.activeFocus
    readonly property int capabilities: 65

    signal rendererFailed(string reason)

    color: "#f4f2ed"

    function goBack() {}
    function goForward() {}
    function focusPage() { root.forceActiveFocus() }
    function reloadPage() {
        loading = true
        settle.restart()
    }
    function checkForEditedFormState(callback) { callback(false) }
    function simulateRendererFailure() {
        rendererFailed("Renderer exited unexpectedly")
    }

    Timer {
        id: settle
        interval: 180
        onTriggered: root.loading = false
    }

    Column {
        anchors.centerIn: parent
        spacing: 10

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Tanto UI lab"
            color: "#25232b"
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.currentUrl.toString()
            color: "#696473"
            font.pixelSize: 14
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "No browser engine is running"
            color: "#918a9b"
            font.pixelSize: 12
        }
    }
}
