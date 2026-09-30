import QtQuick
import QtQuick.Controls
import qs.Commons

Item {
    id: root

    property var colors
    property string iconFontFamily
    property bool open: false
    property var prompt: ({})
    // The page the bar stands over, blurred under its ground so it shows
    // through as colour and shape but cannot be read. Null over no page.
    property Item backdropSource: null
    readonly property string kind: String(prompt.kind || "")
    readonly property bool asksForText: kind === "javascript-prompt"
    readonly property bool asksForCredentials: kind === "http-authentication"
    readonly property bool canStop: kind.startsWith("javascript-")
    readonly property bool canRemember: kind === "external-protocol" && prompt.rememberable
                                        !== false
    // An Agent asking for a Space is the browser's question, not the page's,
    // and it can come while the reader is typing into that page. It waits
    // for a click rather than taking the keyboard, so no keystroke meant for
    // the page answers it.
    readonly property bool takesFocus: kind !== "agent-grant"

    signal answered(bool accepted, string text, string user, string password, bool stopPrompts,
                    bool remember)

    visible: open
    focus: open && takesFocus

    // The bar is hidden between prompts rather than destroyed, so a password
    // typed into it stays resident in a live text input until something writes
    // over the field. Every route out of a prompt clears it: the reader
    // answering, the bar closing, and the prompt being swapped for another
    // tab's while the bar stays up.
    onOpenChanged: root.open ? root.showPrompt() : root.clearFields()
    onPromptChanged: if (root.open)
                         root.showPrompt()

    function clearFields() {
        answer.text = "";
        user.text = "";
        password.text = "";
        stopPrompts.checked = false;
        remember.checked = false;
    }

    function showPrompt() {
        root.clearFields();
        answer.text = String(root.prompt.defaultText || "");
        if (!root.takesFocus)
            return;
        Qt.callLater(function () {
            if (root.asksForCredentials)
                user.forceActiveFocus();
            else if (root.asksForText)
                answer.forceActiveFocus();
            else
                root.forceActiveFocus();
        });
    }

    // Read out and cleared before the answer is emitted: what the emit sets off
    // may leave this bar open on another prompt, and that prompt's fields are
    // its own.
    function submit(accepted) {
        const answerText = answer.text;
        const userText = user.text;
        const passwordText = password.text;
        const stopChecked = stopPrompts.checked;
        const rememberChecked = remember.checked;
        root.clearFields();
        root.answered(accepted, answerText, userText, passwordText, stopChecked, rememberChecked);
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.submit(false);
            event.accepted = true;
        }
    }

    SheetFloor {}

    MouseArea {
        anchors.fill: parent
    }

    PageBackdrop {
        objectName: "pageBarBackdrop"
        anchors.fill: ground
        source: root.backdropSource
    }

    Rectangle {
        id: ground
        objectName: "pagePromptGround"
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
                    objectName: "browserPromptAccept"
                    label: root.kind === "external-protocol" ? "Open" : root.kind
                                                               === "http-authentication"
                                                               ? "Sign in" : root.kind
                                                                 === "agent-grant" ? "Allow" : "OK"
                    primary: true
                    onClicked: root.submit(true)
                }

                ActionButton {
                    visible: root.kind !== "javascript-alert"
                    colors: root.colors
                    objectName: "browserPromptRefuse"
                    label: root.kind === "agent-grant" ? "Deny" : "Cancel"
                    onClicked: root.submit(false)
                }
            }
        }
    }
}
