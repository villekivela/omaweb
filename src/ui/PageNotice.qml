import QtQuick
import Omaweb
import qs.Commons
import qs.Ui as Omarchy

// A short statement about the page, over the page, that takes itself away. It
// is neither a question — `PageQuestionBar` has actions and takes the keyboard
// — nor a standing warning — `NoticeBox` waits in the settings it is about.
// This one says what just happened: the size the page is now drawn at, the site
// that took the screen, a command this engine cannot carry out.
//
// A command Omaweb cannot carry out has to say so rather than do nothing, and
// this is where it says it.
Item {
    id: root

    property var colors
    property string iconFontFamily
    property string glyph: "info"
    property string message: ""
    property string detail: ""
    // How long the statement stands. A notice about a key the reader has to
    // press is set to wait longer than one about a size they can see.
    property int duration: 2400

    readonly property bool showing: hold.running || fade.opacity > 0

    function show(noticeGlyph, noticeMessage, noticeDetail, noticeDuration) {
        root.glyph = noticeGlyph;
        root.message = noticeMessage;
        root.detail = noticeDetail === undefined ? "" : noticeDetail;
        root.duration = noticeDuration === undefined ? 2400 : noticeDuration;
        hold.restart();
    }

    function dismiss() {
        hold.stop();
        fade.opacity = 0;
    }

    visible: fade.opacity > 0
    implicitHeight: fade.implicitHeight
    implicitWidth: fade.implicitWidth
    height: implicitHeight
    width: implicitWidth

    Timer {
        id: hold
        interval: root.duration
        onTriggered: fade.opacity = 0
        onRunningChanged: if (running)
                              fade.opacity = 1
    }

    Omarchy.BorderSurface {
        id: fade
        objectName: "pageNoticeSurface"
        anchors.fill: parent
        opacity: 0
        radius: Style.cornerRadius
        padding: Style.spacing.lg
        color: surfaceDither.fill
        borderSpec: Border.controlSpec("normal", root.colors.accent, root.colors.accent)

        implicitWidth: contentLeftInset + body.implicitWidth + contentRightInset
        implicitHeight: contentTopInset + body.implicitHeight + contentBottomInset

        Accessible.role: Accessible.AlertMessage
        Accessible.name: root.detail.length > 0 ? qsTr("%1. %2").arg(root.message).arg(root.detail) :
                                                  root.message

        GroundDither {
            id: surfaceDither
            ground: root.colors.overlay
            leftInset: fade.borderLeft
            topInset: fade.borderTop
            rightInset: fade.borderRight
            bottomInset: fade.borderBottom
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
        // The notice comes down from the top edge as it fades in, and goes
        // back up as it fades out: it belongs to the edge, not to the page.
        // Under reduced motion it fades where it stands.
        transform: Translate {
            y: SystemMotion.reduced ? 0 : (fade.opacity - 1) * 8
        }

        Row {
            id: body
            anchors.left: parent.left
            anchors.leftMargin: fade.contentLeftInset
            anchors.verticalCenter: parent.verticalCenter
            spacing: Style.spacing.lg

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.glyph
                color: root.colors.accent
                font.family: root.iconFontFamily
                font.pixelSize: Style.font.icon
                Accessible.ignored: true
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Style.spacing.xs

                Text {
                    text: root.message
                    color: root.colors.text
                    font.family: Style.font.family
                    font.pixelSize: Style.font.body
                }

                Text {
                    visible: root.detail.length > 0
                    text: root.detail
                    color: root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }
            }
        }
    }
}
