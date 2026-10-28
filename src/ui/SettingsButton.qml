import QtQuick
import QtQuick.Effects
import qs.Commons

// The outline footer's Settings control. Its face is the gear, or the Sync
// avatar once Sync is signed in, and one dot serves both: urgent while
// Settings needs attention or Sync has an error, accent while Sync runs, none
// otherwise. A paused Sync greys the avatar and leaves the dot out.
Item {
    id: root

    property var colors
    property string iconFontFamily
    property var sync: null
    property bool authenticatedSync: false
    property bool activeSync: false
    property bool settingsAttention: false
    property real pixelRatio: 1

    readonly property bool syncFailing: !!root.sync && root.sync.errorMessage.length > 0
    readonly property bool urgent: root.settingsAttention || root.syncFailing
    readonly property bool running: root.authenticatedSync && root.activeSync && !root.urgent

    signal clicked

    width: 28
    height: 26

    ChromeButton {
        id: avatarButton
        objectName: "syncAvatarButton"
        anchors.fill: parent
        visible: root.authenticatedSync
        // Evaluated while the avatar is hidden too, when there is no Sync to name.
        accessibleName: !root.authenticatedSync ? qsTr("Settings") : root.settingsAttention ? qsTr(
                                                                                                  "Settings for %1 — needs attention").arg(
                                                                                                  root.sync.login) :
                                                                                              !root.activeSync
                                                                                              ? qsTr("Settings for %1 — Sync paused").arg(
                                                                                                    root.sync.login) :
                                                                                                qsTr("Settings for %1").arg(
                                                                                                    root.sync.login)
        foreground: root.syncFailing ? root.colors.urgent : avatarButton.restingForeground

        accent: root.colors.accent
        onClicked: root.clicked()

        Rectangle {
            id: avatarClip
            objectName: "syncAvatarClip"
            anchors.centerIn: parent
            width: parent.height - 4
            height: width
            radius: width / 2
            color: root.colors.separator
            clip: true

            Text {
                objectName: "syncAvatarMonogram"
                anchors.centerIn: parent
                visible: avatarImage.status !== Image.Ready
                text: root.sync && root.sync.login.length > 0 ? root.sync.login.charAt(0).toUpperCase() :
                                                                ""
                color: root.activeSync ? root.colors.text : root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.body
                font.bold: true
            }

            Image {
                id: avatarImage
                anchors.fill: parent
                source: root.sync && root.sync.avatarPath.length > 0 ? "file://"
                                                                       + root.sync.avatarPath : ""
                fillMode: Image.PreserveAspectCrop
                visible: false
                smooth: true
            }

            // A Canvas, not a MultiEffect mask: the software renderer
            // draws no effect, so a mask there showed no photo to test
            // (#836). The circle is clipped at the display's own pixels.
            Canvas {
                id: avatarCanvas
                objectName: "syncAvatarCanvas"
                visible: avatarImage.status === Image.Ready
                // Sized in the display's pixels and scaled back, so the
                // edge of the circle is drawn at a fractional scale too.
                width: Math.ceil(parent.width * root.pixelRatio)
                height: width
                scale: parent.width / width
                transformOrigin: Item.TopLeft
                onWidthChanged: requestPaint()
                onAvailableChanged: requestPaint()

                Connections {
                    target: avatarImage

                    function onStatusChanged() {
                        avatarCanvas.load();
                    }
                }

                // A Canvas draws only an image it loaded itself.
                function load() {
                    if (avatarImage.status === Image.Ready) {
                        loadImage(avatarImage.source);
                    }
                    requestPaint();
                }

                onImageLoaded: requestPaint()

                onPaint: {
                    if (!available) {
                        return;
                    }
                    const ctx = getContext("2d");
                    const w = width;
                    const h = height;
                    ctx.reset();
                    if (!isImageLoaded(avatarImage.source)) {
                        return;
                    }
                    const side = Math.min(avatarImage.implicitWidth, avatarImage.implicitHeight);
                    const sx = (avatarImage.implicitWidth - side) / 2;
                    const sy = (avatarImage.implicitHeight - side) / 2;
                    ctx.save();
                    ctx.beginPath();
                    ctx.ellipse(0, 0, w, h);
                    ctx.clip();
                    ctx.drawImage(avatarImage.source, sx, sy, side, side, 0, 0, w, h);
                    ctx.restore();
                }
            }

            // Greyscale for a paused Sync. It draws on a GPU renderer only, as
            // every effect does, and covers the circle the Canvas drew.
            MultiEffect {
                objectName: "syncAvatarEffect"
                anchors.fill: parent
                source: avatarCanvas
                visible: avatarImage.status === Image.Ready && !root.activeSync
                saturation: -1
            }
        }
    }

    ChromeButton {
        id: settingsButton
        objectName: "settingsButton"
        anchors.fill: parent
        visible: !root.authenticatedSync
        icon: "settings"
        accessibleName: root.settingsAttention ? qsTr(
                                                     "Browsing settings and downloads — needs attention") :
                                                 qsTr("Browsing settings and downloads")
        fontFamily: root.iconFontFamily
        accent: root.colors.accent
        onClicked: root.clicked()
    }

    // The colour the notice inside Settings is drawn in, so the mark out here
    // and the thing it stands for read as one. It sits at the gear's top
    // corner and at the avatar's bottom corner.
    Rectangle {
        objectName: "settingsDot"
        anchors.right: parent.right
        anchors.top: root.authenticatedSync ? undefined : parent.top
        anchors.bottom: root.authenticatedSync ? parent.bottom : undefined
        anchors.rightMargin: root.authenticatedSync ? 0 : 3
        anchors.topMargin: 3
        width: 6
        height: 6
        radius: 3
        visible: root.urgent || root.running
        color: root.running ? root.colors.accent : root.colors.urgent

        Accessible.ignored: true
    }
}
