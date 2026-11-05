import QtQuick
import Omaweb
import qs.Commons
import qs.Ui as Omarchy
import "SiteChip.mjs" as SiteChip

// A Folder's row: a folder glyph, open while the Folder is, and the Folder's name
// in the quiet text colour, with a count of its rows while it is collapsed. The
// glyph alone shows the state. The row has no colour of its own, because only
// Space colours mark Spaces, and it has no page, so it is never on show and never
// marked. It stands in the ordinary list and answers the pointer and the Sidebar
// cursor as a tab's row does.
Item {
    id: root
    objectName: "folder-" + folderRowId

    required property string folderRowId
    required property string folderName
    required property bool folderCollapsed
    required property int folderMemberCount
    readonly property bool isFolder: true
    // Which place this row holds in the list. The list sets it.
    property int placeInSection: -1
    // How far the row's marks reach past it on each side, as a tab's row does.
    property real reachLeft: 0
    property real reachRight: 0
    property var colors
    property string iconFontFamily
    // A row in the hand would land in this Folder if it were let go now.
    property bool dropTarget: false

    property bool reachedByPointer: false
    onActiveFocusChanged: if (!activeFocus)
                              reachedByPointer = false
    readonly property bool cursorShown: activeFocus && !reachedByPointer

    // Lined up with an ordinary tab's row: the glyph takes the chip's place, and
    // the name the title's.
    readonly property int chipSize: SiteChip.side(Style.font.caption)
    readonly property int chipInset: Style.space(4)
    readonly property int titleInset: Style.space(8) + chipSize + Style.space(9)
    readonly property int endGap: Style.space(12)

    signal toggled(string folderId)
    signal dragStarted(string folderId)
    signal dragMoved(string folderId, real sceneX, real sceneY)
    signal dragEnded(string folderId)
    signal menuRequested(string folderId, real anchorX, real anchorY)

    // Held and carried as a tab's row is, by a transform the list sets.
    property bool lifted: false
    property bool landing: false
    property point carry: Qt.point(0, 0)
    readonly property point grabbedAt: hoverArea.grabbedAt
    z: lifted ? 20 : 0
    opacity: lifted ? 0.92 : 1.0
    transform: Translate {
        x: root.carry.x
        y: root.carry.y
    }

    Behavior on carry {
        enabled: !root.lifted && !root.landing && !SystemMotion.reduced
        PropertyAnimation {
            duration: 110
            easing.type: Easing.OutCubic
        }
    }

    height: Style.space(36)
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: root.folderCollapsed ? qsTr("Folder %1, collapsed: %2").arg(
                                                root.folderName).arg(root.folderMemberCount) : qsTr(
                                                "Folder %1, open").arg(root.folderName)
    Accessible.onPressAction: root.toggled(root.folderRowId)

    // Return and `l` climb to the outline, as they do from a tab's row.
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Space) {
            root.toggled(root.folderRowId);
            event.accepted = true;
        }
    }

    function steer() {
        root.reachedByPointer = false;
        root.forceActiveFocus();
    }

    function openMenu(x, y) {
        const point = root.mapToItem(null, x, y);
        root.menuRequested(root.folderRowId, point.x, point.y);
    }

    // The pointer's wash, and the same wash while a row would land here.
    Rectangle {
        objectName: "folderHover-" + root.folderRowId
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
        visible: (hoverArea.containsMouse || root.dropTarget) && !root.cursorShown
        color: Qt.alpha(root.colors.accent, 0.07)
    }

    Omarchy.BorderSurface {
        objectName: "folderCursor-" + root.folderRowId
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
        visible: root.cursorShown
        radius: 0
        color: Qt.alpha(root.colors.accent, 0.07)
        borderSpec: Border.flat(root.colors.accent, 1)
    }

    Text {
        id: glyph
        objectName: "folderGlyph-" + root.folderRowId
        anchors.left: parent.left
        anchors.leftMargin: root.chipInset
        anchors.verticalCenter: parent.verticalCenter
        width: root.chipSize
        horizontalAlignment: Text.AlignHCenter
        text: root.folderCollapsed ? "folder" : "folder_open"
        color: root.colors.mutedText
        font.family: root.iconFontFamily
        font.pixelSize: root.chipSize
    }

    Text {
        objectName: "folderName-" + root.folderRowId
        anchors.left: parent.left
        anchors.leftMargin: root.titleInset
        anchors.right: count.visible ? count.left : parent.right
        anchors.rightMargin: count.visible ? Style.space(6) : root.endGap - root.reachRight
        anchors.verticalCenter: parent.verticalCenter
        text: root.folderName
        color: root.colors.mutedText
        elide: Text.ElideRight
        font.family: Style.font.family
        font.pixelSize: Style.font.body
    }

    Text {
        id: count
        objectName: "folderCount-" + root.folderRowId
        anchors.right: parent.right
        anchors.rightMargin: root.endGap - root.reachRight
        anchors.verticalCenter: parent.verticalCenter
        visible: root.folderCollapsed
        text: String(root.folderMemberCount)
        color: root.colors.mutedText
        font.family: Style.font.family
        font.pixelSize: Style.font.caption
    }

    MouseArea {
        id: hoverArea
        objectName: "folderPointer-" + root.folderRowId
        anchors.fill: parent
        anchors.leftMargin: -root.reachLeft
        anchors.rightMargin: -root.reachRight
        z: 10
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        preventStealing: true
        property point grabbedAt: Qt.point(0, 0)
        property point pressedAt: Qt.point(0, 0)
        readonly property real liftThreshold: 4
        property bool carried: false

        onPressed: function (mouse) {
            hoverArea.carried = false;
            hoverArea.grabbedAt = hoverArea.mapToItem(root, mouse.x, mouse.y);
            hoverArea.pressedAt = hoverArea.mapToItem(null, mouse.x, mouse.y);
        }

        onPositionChanged: function (mouse) {
            if (!(hoverArea.pressedButtons & Qt.LeftButton))
                return;
            const scene = hoverArea.mapToItem(null, mouse.x, mouse.y);
            if (!root.lifted) {
                const travelled = Math.max(Math.abs(scene.x - hoverArea.pressedAt.x), Math.abs(
                                               scene.y - hoverArea.pressedAt.y));
                if (travelled < hoverArea.liftThreshold)
                    return;
                root.dragStarted(root.folderRowId);
            }
            root.dragMoved(root.folderRowId, scene.x, scene.y);
        }

        onReleased: function (mouse) {
            if (!root.lifted)
                return;
            hoverArea.carried = true;
            root.dragEnded(root.folderRowId);
        }

        onCanceled: if (root.lifted)
                        root.dragEnded(root.folderRowId)

        onClicked: function (mouse) {
            root.reachedByPointer = true;
            root.forceActiveFocus();
            if (mouse.button === Qt.RightButton) {
                const at = hoverArea.mapToItem(root, mouse.x, mouse.y);
                root.openMenu(at.x, at.y);
                return;
            }
            // A Folder's row is not marked, and a marking click is no toggle.
            if (root.lifted || hoverArea.carried || (mouse.modifiers & (Qt.ControlModifier
                                                                        | Qt.ShiftModifier)))
                return;
            root.toggled(root.folderRowId);
        }
    }
}
