import QtQuick

// A page-owned mark that reports work without pretending the engine knows how
// much of a navigation remains. It has no input or accessible role: changing
// progress must not interrupt the reader or stand between them and the page.
Rectangle {
    id: root
    objectName: "pageLoadingIndicator"

    required property var colors
    property bool active: false
    property bool allowed: true
    property bool motionEnabled: true
    readonly property bool moving: sweepMotion.running

    width: 80
    height: 5
    radius: height / 2
    color: Qt.alpha(root.colors.accent, 0.45)
    enabled: false
    Accessible.ignored: true
    visible: allowed && (active || opacity > 0)
    opacity: active && allowed ? 1 : 0
    scale: active && allowed ? 1 : 0.92

    Behavior on opacity {
        enabled: root.motionEnabled && root.allowed
        NumberAnimation {
            duration: root.active ? 100 : 110
            easing.type: Easing.OutCubic
        }
    }

    Behavior on scale {
        enabled: root.motionEnabled && root.allowed
        NumberAnimation {
            duration: root.active ? 150 : 110
            easing.type: root.active ? Easing.OutBack : Easing.InBack
            easing.overshoot: 0.8
        }
    }

    Rectangle {
        id: sweep

        property real position: -length
        readonly property real length: 24

        // Item clipping is rectangular. Keep the rounded sweep inside the track instead.
        x: Math.max(0, position)
        width: Math.max(0, Math.min(root.width, position + length) - x)
        height: Math.min(root.height, width)
        anchors.verticalCenter: parent.verticalCenter
        radius: height / 2
        color: root.colors.accent
        visible: root.motionEnabled

        SequentialAnimation on position {
            id: sweepMotion
            running: root.visible && root.motionEnabled
            loops: Animation.Infinite

            NumberAnimation {
                from: -sweep.length
                to: root.width
                duration: 900
                easing.type: Easing.InOutCubic
            }

            PauseAnimation {
                duration: 120
            }
        }
    }
}
