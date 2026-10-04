import QtQuick
import qs.Commons

// A security key's request, on the permission prompt's surface and in its
// place. The engine moves one request through several steps, and the bar
// changes in place as it does rather than stacking a prompt for each, so the
// reader never answers a step that has already passed. Every step can be
// declined, with its last action or with Escape.
PageQuestionBar {
    id: root

    // The step the engine is on, in the words the engine contract gives it.
    property var step: ({})
    // The Space the request was made in, or the Private window's word.
    property string place: ""
    // The authenticators the engine can reach: "usb", "hybrid" (a phone) and
    // "platform" (a passkey stored on this computer).
    property var transports: []

    readonly property string stage: String(root.step.state || "")
    readonly property var pin: root.step.pin || ({})
    readonly property int attemptsLeft: root.pin.attemptsLeft === undefined ? -1 : Number(
                                                                                  root.pin.attemptsLeft)
    // What the bar has to say beyond its question: why the last PIN was not
    // taken, and how many tries the key has left where the engine says.
    readonly property string note: root.stage === "pin" ? root.pinNote() : root.stage === "touch"
                                                          || root.stage === "failed"
                                                          ? root.transportNote() : ""
    readonly property var accounts: root.stage === "accounts" ? (root.step.accounts || []) : []
    property int currentAccount: 0

    signal answered(var answer)

    // What the engine cannot reach is said rather than left out, as ADR 0030
    // asks: without it a reader whose passkey is on their phone waits for a
    // prompt that never comes.
    function transportNote() {
        const reaches = function (transport) {
            return (root.transports || []).indexOf(transport) >= 0;
        };
        if (!reaches("hybrid") && !reaches("platform"))
            return qsTr(
                        "Only a USB security key works here. A phone, or a passkey stored on this computer, cannot be used.");
        if (!reaches("hybrid"))
            return qsTr("A phone cannot be used here.");
        if (!reaches("platform"))
            return qsTr("A passkey stored on this computer cannot be used here.");
        return "";
    }

    function pinNote() {
        const error = String(root.pin.error || "");
        const attempts = root.attemptsLeft;
        if (error === "wrong") {
            if (attempts === 1)
                return qsTr("Wrong PIN. 1 attempt left.");
            if (attempts >= 0)
                return qsTr("Wrong PIN. %1 attempts left.").arg(attempts);
            return qsTr("Wrong PIN.");
        }
        if (error === "too-short")
            return qsTr("The PIN needs at least %1 characters.").arg(Number(root.pin.minimumLength
                                                                            || 4));
        if (error === "invalid-characters")
            return qsTr("The PIN has characters the key does not accept.");
        if (error === "same-as-current")
            return qsTr("The new PIN has to differ from the current one.");
        if (error === "built-in-check-locked")
            return qsTr(
                        "The key's own check is locked after too many tries. Enter its PIN instead.");
        if (attempts === 1)
            return qsTr("1 attempt left.");
        if (attempts >= 0)
            return qsTr("%1 attempts left.").arg(attempts);
        return "";
    }

    function failureMessage(failure) {
        switch (failure) {
        case "no-key":
            return qsTr("This security key has no sign-in for this site");
        case "already-registered":
            return qsTr("This security key is already registered with this site");
        case "too-many-attempts":
            return qsTr("Too many wrong PINs. Remove the key and insert it again.");
        case "locked":
            return qsTr("Too many wrong PINs. The key is locked until it is reset.");
        case "timed-out":
            return qsTr("The security key was not used in time");
        case "not-supported":
            return qsTr("This security key cannot do what the site asks");
        case "key-removed":
            return qsTr("The security key was removed");
        case "key-full":
            return qsTr("This security key has no room for another sign-in");
        default:
            return qsTr("The security key could not be used");
        }
    }

    function chooseAccount(index) {
        const account = root.accounts[index];
        if (account)
            root.answered({
                              "action": "account",
                              "name": String(account.name)
                          });
    }

    Keys.onPressed: function (event) {
        if (root.accounts.length === 0)
            return;
        if (event.key === Qt.Key_Down)
            root.currentAccount = Math.min(root.currentAccount + 1, root.accounts.length - 1);
        else if (event.key === Qt.Key_Up)
            root.currentAccount = Math.max(root.currentAccount - 1, 0);
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
            root.chooseAccount(root.currentAccount);
        else
            return;
        event.accepted = true;
    }

    function submitPin() {
        const typed = pinField.text;
        pinField.text = "";
        if (typed.length > 0)
            root.answered({
                              "action": "pin",
                              "pin": typed
                          });
    }

    // A PIN is not left in a hidden field: every step and every close starts
    // the field empty, and the step that asks for one hands it the keyboard.
    function takeStep() {
        pinField.text = "";
        root.currentAccount = 0;
        if (!root.open)
            return;
        Qt.callLater(function () {
            if (!root.open)
                return;
            if (root.stage === "pin")
                pinField.focusInput();
            else
                root.forceActiveFocus();
        });
    }
    onStepChanged: root.takeStep()
    onOpenChanged: root.takeStep()

    glyph: "passkey"
    message: {
        if (root.stage === "pin") {
            if (root.pin.purpose === "set")
                return qsTr("Choose a PIN for your security key");
            if (root.pin.purpose === "change")
                return qsTr("Choose a new PIN for your security key");
            return qsTr("Enter your security key's PIN");
        }
        if (root.stage === "accounts")
            return qsTr("Choose an account");
        if (root.stage === "failed")
            return root.failureMessage(String(root.step.failure || ""));
        return qsTr("Touch your security key");
    }
    detail: qsTr("%1 · %2").arg(String(root.step.site || "")).arg(root.place)
    actions: root.stage === "pin" ? [
                                        {
                                            "label": qsTr("Continue"),
                                            "action": "pin",
                                            "enabled": pinField.text.length > 0
                                        },
                                        {
                                            "label": qsTr("Cancel"),
                                            "action": "cancel"
                                        }
                                    ] : root.stage === "failed" ? [
                                                                      {
                                                                          "label": qsTr("Close"),
                                                                          "action": "cancel"
                                                                      }
                                                                  ] : [
                                                                      {
                                                                          "label": qsTr("Cancel"),
                                                                          "action": "cancel"
                                                                      }
                                                                  ]

    onActionTriggered: function (index) {
        if (root.actions[index].action === "pin") {
            root.submitPin();
            return;
        }
        pinField.text = "";
        root.answered({
                          "action": "cancel"
                      });
    }

    content: [
        Text {
            width: parent.width
            visible: root.note.length > 0
            text: root.note
            color: root.pin.error ? root.colors.urgent : root.colors.mutedText
            wrapMode: Text.Wrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        },
        SettingField {
            id: pinField
            objectName: "securityKeyPin"
            visible: root.stage === "pin"
            width: Math.min(260, parent.width)
            colors: root.colors
            password: true
            placeholder: qsTr("PIN")
            accessibleName: root.message
            onAccepted: root.submitPin()
        },
        // The engine names each account by the name the site stored on the
        // key, and by a display name where it reports one.
        Column {
            width: Math.min(420, parent.width)
            visible: root.accounts.length > 0

            Repeater {
                model: root.accounts

                Rectangle {
                    required property int index
                    required property var modelData

                    readonly property string name: String(modelData.name || "")
                    readonly property string displayName: String(modelData.displayName || "")
                    readonly property bool current: index === root.currentAccount

                    objectName: "securityKeyAccount" + index
                    width: parent.width
                    height: 30
                    radius: 3
                    color: current ? root.colors.surface : "transparent"

                    Accessible.role: Accessible.ListItem
                    Accessible.name: displayName.length > 0 ? qsTr("%1, %2").arg(displayName).arg(
                                                                  name) : name

                    Row {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        Text {
                            visible: text.length > 0
                            text: parent.parent.displayName
                            color: root.colors.text
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }

                        Text {
                            text: parent.parent.name
                            color: parent.parent.displayName.length > 0 ? root.colors.mutedText :
                                                                          root.colors.text
                            elide: Text.ElideRight
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.chooseAccount(parent.index)
                    }
                }
            }
        }
    ]
}
