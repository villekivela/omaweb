import QtQuick
import QtQuick.Dialogs as Dialogs
import Omaweb

// A file or folder dialog. It is the desktop's own file chooser, asked
// through the portal, and Qt's dialog only where no portal answers
// (ADR 0070): the GTK theme Omarchy sets for Qt apps would otherwise open
// GTK's dialog in process, never the one the reader set for the desktop.
//
// `accepted` carries the chosen addresses, one for a single file, a save or a
// folder. A dialog taken back with `close` answers neither way.
Item {
    id: root

    property string title
    // "open", "open-multiple", "folder" or "save".
    property string mode: "open"
    // Qt's filter strings, such as "Text files (*.txt)".
    property var nameFilters: []
    // A page's `accept` list, offered first and chosen by default.
    property var acceptedTypes: []
    // Where a save starts: its folder and its name.
    property url currentFile

    readonly property bool showing: portal.open || fileDialog.visible || folderDialog.visible

    signal accepted(var urls)
    signal rejected

    visible: false

    function open() {
        root.close();
        const pageFilters = root.acceptedTypes.length > 0 ? [qsTr("All files (*)")] : [];
        const question = {
            "mode": root.mode,
            "title": root.title,
            "nameFilters": root.nameFilters.concat(pageFilters),
            "acceptedTypes": root.acceptedTypes,
            "acceptedTypesName": qsTr("Files the page takes"),
            "currentFile": root.currentFile
        };
        if (portal.ask(question, root.Window.window))
            return;
        if (root.mode === "folder") {
            folderDialog.open();
            return;
        }
        fileDialog.fileMode = root.mode === "open-multiple" ? Dialogs.FileDialog.OpenFiles : (root.mode
                                                                                              === "save"
                                                                                              ? Dialogs.FileDialog.SaveFile :
                                                                                                Dialogs.FileDialog.OpenFile);
        if (root.mode === "save")
            fileDialog.currentFile = root.currentFile;
        fileDialog.open();
    }

    // Starts a save in `folder` under `name`, the part after its last slash,
    // since a page's suggestion is the page's to make up. Each part of the
    // path is escaped, so a `#`, `?` or `%` in a name stays in the name rather
    // than ending the path. No name, `.` or `..` is no start.
    function startAt(folder, name) {
        const base = String(name || "").split("/").pop();
        if (base === "" || base === "." || base === "..") {
            root.currentFile = "";
            return;
        }
        const parts = String(folder).split("/").concat([base]);
        root.currentFile = "file://" + parts.map(encodeURIComponent).join("/");
    }

    function close() {
        portal.close();
        if (fileDialog.visible)
            fileDialog.close();
        if (folderDialog.visible)
            folderDialog.close();
    }

    FileChooser {
        id: portal

        onChosen: function (urls) {
            root.accepted(urls);
        }
        onCancelled: root.rejected()
    }

    Dialogs.FileDialog {
        id: fileDialog
        objectName: "qtFileDialog"

        title: root.title
        nameFilters: root.nameFilters
        onAccepted: {
            if (fileMode !== Dialogs.FileDialog.OpenFiles) {
                root.accepted([selectedFile]);
                return;
            }
            const urls = [];
            for (let index = 0; index < selectedFiles.length; ++index)
                urls.push(selectedFiles[index]);
            root.accepted(urls);
        }
        onRejected: root.rejected()
    }

    Dialogs.FolderDialog {
        id: folderDialog
        objectName: "qtFolderDialog"

        title: root.title
        onAccepted: root.accepted([selectedFolder])
        onRejected: root.rejected()
    }
}
