import QtQuick
import Omaweb
import qs.Commons

// The certificate a site presented, as a browser shows one: who it names,
// who vouches for it, when it holds, its fingerprint, the names it is good
// for, and the chain above it up to the trust anchor.
//
// Every value is there to be compared against something else — a fingerprint
// a colleague read out, the names a development server was started with — so
// each one is copied whole with its own button, or with Return on the row the
// arrows are on. The chain runs left to right, the site's own certificate
// first, so Left and Right walk it and Up and Down walk the fields.
DialogPanel {
    id: root

    // One map per certificate, in the shape the engine adapter reports: the
    // site's own first, the trust anchor last.
    property var chain: []
    // The site that presented it, and whether the engine accepted it. A chain
    // shown from the certificate interstitial is one the engine refused.
    property string origin: ""
    property bool verified: true

    property int selectedCertificate: 0
    property int selectedField: 0
    // The field last copied, named on its own button until another is.
    property string copiedField: ""

    readonly property var certificate: root.chain.length > root.selectedCertificate
                                       ? root.chain[root.selectedCertificate] : ({})
    readonly property var fields: [
        {
            "key": "subject",
            "label": "Subject"
        },
        {
            "key": "issuer",
            "label": "Issuer"
        },
        {
            "key": "notBefore",
            "label": "Valid from"
        },
        {
            "key": "notAfter",
            "label": "Valid until"
        },
        {
            "key": "sha256",
            "label": "SHA-256"
        },
        {
            "key": "subjectAlternativeNames",
            "label": "Names"
        }
    ]

    panelObjectName: "certificatePanel"
    label: "certificate"
    cancelHint: "esc close"
    confirmHint: "←→ chain · ↑↓ field · ⏎ copy"

    onOpenChanged: {
        if (!open)
            return;
        root.selectedCertificate = 0;
        root.selectedField = 0;
        root.copiedField = "";
        // Deferred as the command dialog's is: `visible` follows `open`
        // through a binding that may not have run yet, and an item that is not
        // visible cannot take the keyboard.
        Qt.callLater(function () {
            if (root.open)
                root.forceActiveFocus();
        });
    }
    onSelectedCertificateChanged: root.copiedField = ""

    // What the row shows and what the button copies are the same text, so
    // what lands on the clipboard is what the reader saw.
    function fieldValue(key) {
        const value = root.certificate[key];
        if (value === undefined || value === null)
            return "";
        if (key === "subjectAlternativeNames")
            return value.length > 0 ? value.join(", ") : "none";
        return String(value);
    }

    function copyField(index) {
        const field = root.fields[index];
        if (!field)
            return;
        root.selectedField = index;
        if (SystemClipboard.copyText(root.fieldValue(field.key)))
            root.copiedField = field.key;
    }

    // Where the certificate stands in the chain, in the words a reader
    // checks it by. One certificate that signed itself is its own chain.
    function roleOf(index) {
        const entry = root.chain[index];
        if (root.chain.length === 1)
            return entry.selfSigned ? "self-signed" : "the only certificate sent";
        if (index === 0)
            return "the site's own";
        if (index === root.chain.length - 1)
            return entry.selfSigned ? "trust anchor" : "last sent";
        return "intermediate";
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            root.dismissed();
        } else if (event.key === Qt.Key_Left) {
            root.selectedCertificate = Math.max(0, root.selectedCertificate - 1);
        } else if (event.key === Qt.Key_Right) {
            root.selectedCertificate = Math.min(root.chain.length - 1, root.selectedCertificate
                                                + 1);
        } else if (event.key === Qt.Key_Up) {
            root.selectedField = (root.selectedField + root.fields.length - 1) % root.fields.length;
        } else if (event.key === Qt.Key_Down) {
            root.selectedField = (root.selectedField + 1) % root.fields.length;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.matches(
                       StandardKey.Copy)) {
            root.copyField(root.selectedField);
        } else {
            return;
        }
        event.accepted = true;
    }

    Column {
        width: parent.width
        topPadding: 14
        bottomPadding: 8
        spacing: 10

        Text {
            objectName: "certificateOrigin"
            x: 16
            width: parent.width - 32
            text: (root.origin.length > 0 ? root.origin : "this site") + (root.verified
                                                                          ? " · verified by the engine" :
                                                                            " · could not be verified")
            color: root.verified ? root.colors.text : root.colors.urgent
            elide: Text.ElideMiddle
            font.family: Style.font.family
            font.pixelSize: Style.font.body
        }

        // The chain, the site's own certificate first. Each entry is a button
        // so a pointer picks one as the arrows do.
        Flow {
            x: 16
            width: parent.width - 32
            spacing: 6

            Repeater {
                model: root.chain

                ActionButton {
                    required property int index
                    required property var modelData

                    objectName: "certificateChainEntry" + index
                    colors: root.colors
                    label: (index > 0 ? "› " : "") + modelData.name
                    accessibleName: modelData.name + ", " + root.roleOf(index)
                    primary: index === root.selectedCertificate
                    // The buttons are the pointer's. A button holding the
                    // keyboard would take Return for itself, and the arrows
                    // and Return would stop meaning what the foot says.
                    focusable: false
                    onClicked: root.selectedCertificate = index
                }
            }
        }

        Text {
            objectName: "certificateRole"
            x: 16
            width: parent.width - 32
            visible: root.chain.length > 0
            text: root.chain.length > 0 ? root.roleOf(root.selectedCertificate) : ""
            color: root.colors.mutedText
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Column {
            width: parent.width

            Repeater {
                model: root.fields

                Rectangle {
                    id: row

                    required property int index
                    required property var modelData

                    readonly property bool current: index === root.selectedField

                    objectName: "certificateField_" + modelData.key
                    width: parent.width
                    height: Math.max(value.implicitHeight, copy.implicitHeight) + 12
                    color: row.current ? root.colors.surface : "transparent"

                    Rectangle {
                        width: 2
                        height: parent.height
                        color: row.current ? root.edge : "transparent"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.selectedField = row.index
                    }

                    Text {
                        id: name
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        width: 88
                        text: row.modelData.label
                        color: root.colors.mutedText
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    Text {
                        id: value
                        objectName: "certificateValue_" + row.modelData.key
                        anchors.left: name.right
                        anchors.leftMargin: 8
                        anchors.right: copy.left
                        anchors.rightMargin: 8
                        anchors.top: parent.top
                        anchors.topMargin: 6
                        text: root.fieldValue(row.modelData.key)
                        color: root.colors.text
                        // A fingerprint and a distinguished name have no
                        // spaces worth breaking at, and cut short they
                        // cannot be compared.
                        wrapMode: Text.WrapAnywhere
                        font.family: Style.font.family
                        font.pixelSize: Style.font.caption
                    }

                    ActionButton {
                        id: copy
                        objectName: "copyCertificateField_" + row.modelData.key
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 4
                        colors: root.colors
                        label: root.copiedField === row.modelData.key ? "copied" : "copy"
                        accessibleName: "Copy " + row.modelData.label
                        focusable: false
                        onClicked: root.copyField(row.index)
                    }
                }
            }
        }
    }
}
