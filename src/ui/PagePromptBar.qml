import QtQuick
import QtQuick.Controls
import qs.Commons

Item {
    id: root

    property var colors
    property string iconFontFamily
    property bool open: false
    property var prompt: ({})
    readonly property string kind: String(prompt.kind || "")
    readonly property bool asksForText: kind === "javascript-prompt"
    readonly property bool asksForCredentials: kind === "http-authentication"
    readonly property bool canStop: kind.startsWith("javascript-")
    readonly property bool canRemember: kind === "external-protocol" && prompt.rememberable
                                        !== false

    signal answered(bool accepted, string text, string user, string password, bool stopPrompts,
                    bool remember)

    visible: open
    focus: open

    onOpenChanged: {
        if (!open)
            return
        answer.text = String(prompt.defaultText || "")
        user.text = ""
        password.text = ""
        stopPrompts.checked = false
        remember.checked = false
        Qt.callLater(function () {
            if (root.asksForCredentials)
                user.forceActiveFocus()
            else if (root.asksForText)
                answer.forceActiveFocus()
            else
                root.forceActiveFocus()
        })
    }

    function submit(accepted) {
        root.answered(accepted, answer.text, user.text, password.text, stopPrompts.checked,
                      remember.checked)
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.submit(false)
            event.accepted = true
        }
    }

    MouseArea {
        anchors.fill: parent
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: panel.implicitHeight + 24
        color: root.colors.overlay

        Column {
            id: panel
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 28
            anchors.rightMargin: 20
            spacing: 8

            Text {
                width: parent.width
                text: String(root.prompt.message || "")
                color: root.colors.text
                elide: Text.ElideRight
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Text {
                width: parent.width
                text: String(root.prompt.detail || root.prompt.origin || "")
                color: root.colors.mutedText
                wrapMode: Text.WrapAnywhere
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            TextField {
                id: answer
                objectName: "browserPromptText"
                visible: root.asksForText
                width: Math.min(520, parent.width)
                color: root.colors.text
                placeholderText: "Response"
            }

            Row {
                visible: root.asksForCredentials
                spacing: 8

                TextField {
                    id: user
                    objectName: "browserPromptUser"
                    width: 220
                    color: root.colors.text
                    placeholderText: "Username"
                }

                TextField {
                    id: password
                    objectName: "browserPromptPassword"
                    width: 220
                    color: root.colors.text
                    placeholderText: "Password"
                    echoMode: TextInput.Password
                }
            }

            Row {
                spacing: 12

                CheckBox {
                    id: stopPrompts
                    visible: root.canStop
                    text: "Stop prompts from this page"
                    palette.windowText: root.colors.text
                }

                CheckBox {
                    id: remember
                    visible: root.canRemember
                    text: "Remember for this origin and scheme"
                    palette.windowText: root.colors.text
                }
            }

            Row {
                spacing: 8

                ActionButton {
                    colors: root.colors
                    label: root.kind === "external-protocol" ? "Open" : (root.kind
                                                                         === "http-authentication"
                                                                         ? "Sign in" : "OK")
                    primary: true
                    onClicked: root.submit(true)
                }

                ActionButton {
                    visible: root.kind !== "javascript-alert"
                    colors: root.colors
                    label: "Cancel"
                    onClicked: root.submit(false)
                }
            }
        }
    }
}
