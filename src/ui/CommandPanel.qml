import QtQuick
import QtQuick.Controls

Item {
    id: root
    objectName: "commandPanel"

    property var colors
    property var typography
    property var commands
    property bool open: false
    property bool commandMode: false
    property bool newTabIntent: false
    property string presetText: ""
    property var suggestions: []

    property var results: []
    property int selected: 0

    signal dismissed()
    signal committed(string text)
    signal queryChanged(string text)

    visible: open

    function beginAddress(preset, forNewTab) {
        commandMode = false
        newTabIntent = forNewTab
        presetText = preset
    }

    function beginCommand() {
        commandMode = true
        newTabIntent = false
    }

    onOpenChanged: {
        if (!open) {
            return
        }
        input.text = commandMode ? "" : presetText
        refresh()
        Qt.callLater(function() {
            input.forceActiveFocus()
            input.selectAll()
        })
    }

    function refresh() {
        results = commandMode ? commands.search(input.text) : []
        selected = 0
    }

    function step(delta) {
        if (results.length === 0) {
            return
        }
        selected = (selected + delta + results.length) % results.length
    }

    function accept() {
        if (!commandMode) {
            if (input.text.trim().length > 0) {
                root.committed(input.text)
            }
            return
        }
        if (results.length === 0) {
            return
        }
        const action = results[selected]
        if (!action.enabled) {
            return
        }
        root.dismissed()
        commands.invoke(action)
    }

    Rectangle {
        anchors.fill: parent
        color: "#99000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.dismissed()
        }
    }

    Rectangle {
        id: panel
        width: Math.min(660, parent.width - 96)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Math.max(80, parent.height * 0.14)
        height: header.height + body.height + footer.height
        radius: 3
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        clip: true

        Item {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 62

            Text {
                id: prompt
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                text: root.commandMode ? ":" : (root.newTabIntent ? "+" : ">")
                color: root.colors.accent
                font.family: root.typography.family
                font.pixelSize: 18
            }

            TextField {
                id: input
                objectName: "omnibarInput"
                anchors.left: prompt.right
                anchors.right: modeLabel.left
                anchors.leftMargin: 8
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                height: 40
                background: null
                color: root.colors.text
                placeholderText: root.commandMode
                    ? "search every action"
                    : (root.newTabIntent ? "address or search — opens in a new tab" : "address or search")
                placeholderTextColor: root.colors.mutedText
                font.family: root.typography.family
                font.pixelSize: 17
                selectByMouse: true
                Accessible.name: placeholderText

                onTextChanged: {
                    root.refresh()
                    root.queryChanged(text)
                }

                onAccepted: root.accept()

                Keys.onEscapePressed: function(event) {
                    root.dismissed()
                    event.accepted = true
                }

                Keys.onDownPressed: function(event) {
                    root.step(1)
                    event.accepted = true
                }

                Keys.onUpPressed: function(event) {
                    root.step(-1)
                    event.accepted = true
                }
            }

            SectionLabel {
                id: modeLabel
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                typography: root.typography
                text: root.commandMode ? "command" : (root.newTabIntent ? "new tab" : "this tab")
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: root.colors.border
            }
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: header.bottom
            height: root.commandMode
                ? Math.min(commandList.contentHeight, 336) + 8
                : (root.suggestions.length > 0 ? Math.min(suggestionList.contentHeight, 200) + 8 : 0)

            ListView {
                id: commandList
                objectName: "commandList"
                anchors.fill: parent
                anchors.topMargin: 4
                visible: root.commandMode
                clip: true
                model: root.results
                currentIndex: root.selected
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    required property int index
                    required property var modelData

                    width: commandList.width
                    height: 30

                    readonly property bool startsGroup: index === 0
                        || commandList.model[index - 1].group !== modelData.group

                    Rectangle {
                        anchors.fill: parent
                        color: index === root.selected ? root.colors.surface : "transparent"
                    }

                    Rectangle {
                        width: 2
                        height: parent.height
                        anchors.left: parent.left
                        color: index === root.selected ? root.colors.accent : "transparent"
                    }

                    SectionLabel {
                        id: groupLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        width: 92
                        colors: root.colors
                        typography: root.typography
                        text: parent.startsGroup ? modelData.group : ""
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: groupLabel.right
                        anchors.leftMargin: 10
                        anchors.right: keys.left
                        anchors.rightMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.commands.highlight(modelData.title, input.text)
                        textFormat: Text.StyledText
                        color: modelData.enabled ? root.colors.text : root.colors.mutedText
                        opacity: modelData.enabled ? 1 : 0.6
                        elide: Text.ElideRight
                        font.family: root.typography.family
                        font.pixelSize: root.typography.size
                    }

                    Text {
                        id: keys
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.keys
                        color: root.colors.mutedText
                        opacity: 0.85
                        font.family: root.typography.family
                        font.pixelSize: root.typography.smallSize
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: root.selected = index
                        onClicked: root.accept()
                    }
                }
            }

            ListView {
                id: suggestionList
                objectName: "historySuggestionList"
                anchors.fill: parent
                anchors.topMargin: 4
                visible: !root.commandMode && root.suggestions.length > 0
                clip: true
                model: root.suggestions
                boundsBehavior: Flickable.StopAtBounds

                delegate: Item {
                    required property int index
                    required property var modelData

                    width: suggestionList.width
                    height: 28
                    Accessible.role: Accessible.Button
                    Accessible.name: "Open history result " + modelData.title

                    Rectangle {
                        anchors.fill: parent
                        color: suggestionMouse.containsMouse ? root.colors.surface : "transparent"
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.title + "  ·  " + modelData.url
                        color: root.colors.mutedText
                        elide: Text.ElideMiddle
                        font.family: root.typography.family
                        font.pixelSize: root.typography.size
                    }

                    MouseArea {
                        id: suggestionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.committed(modelData.url.toString())
                    }
                }
            }
        }

        Item {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 26

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: root.colors.border
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 14

                KeyHint {
                    colors: root.colors
                    typography: root.typography
                    text: "↑↓ SELECT"
                }

                KeyHint {
                    colors: root.colors
                    typography: root.typography
                    text: "⏎ RUN"
                }

                KeyHint {
                    colors: root.colors
                    typography: root.typography
                    text: "ESC CLOSE"
                }
            }

            KeyHint {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                colors: root.colors
                typography: root.typography
                visible: root.commandMode
                text: root.results.length + " ACTIONS"
            }
        }
    }
}
