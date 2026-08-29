import QtQuick

QtObject {
    property string profilePath: ""
    property string downloadDirectory: ""
    property bool acceptDownloads: false
    readonly property var profile: this
}
