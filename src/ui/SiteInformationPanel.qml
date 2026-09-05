import QtQuick
import qs.Commons

// What Omaweb states about the site on show, for the Space it is on show in.
//
// One origin, one Space, one moment: the connection the engine reported, the
// requests Content blocking refused, the site data the Space holds, and the
// origin's Site permissions. Where the engine cannot answer for one of those
// lines, the line says so — a blank reads as a reassurance, and the reader has
// no way to tell one from an answer.
//
// This panel states; it does not ask. Every question it leads to is asked in
// the centred dialog every other question about the browser is asked in, which
// has the room to say what is about to happen and answers to the keyboard. A
// confirmation squeezed in beside the thing it is about has neither.
Rectangle {
    id: root

    required property var colors
    required property var browser
    // The engine's third-party filter, which is the only thing that knows which
    // embedded origins this page has actually had refused. Null where the
    // engine has no filter at all.
    property var cookiePolicy: null
    property url activeUrl
    property bool blank: false
    property bool privateWindow: false
    property string connectionState: "internal"
    property bool certificateDecisionsAvailable: false
    property bool thirdPartyCookieControlAvailable: false
    property bool siteDataOnDisk: false
    property bool insecureContentBlocked: true
    property int blockedRequestCount: 0
    property bool open: false
    // The files and directories the engine says its site data lives in, and a
    // count the window bumps when the engine reports it has finished clearing.
    // Chromium clears asynchronously, so a size read straight after the press
    // has not moved yet and reads as an action that did nothing.
    property var siteDataEntries: []
    // What the engine holds that its clearing cannot take. Reported on a line
    // of its own rather than folded into the size above: a byte the action
    // cannot move must not be counted as one it can.
    property var retainedDataEntries: []
    property int siteDataGeneration: 0

    // What the panel just did, on its way to the notice that says so. An
    // irreversible action that reports nothing is one the reader cannot tell
    // from a button that does not work.
    // What the reader asked for, by name, for the window to ask about properly.
    signal actionRequested(string action)

    // How many refused third parties are worth listing. A page has as many as
    // it has embedded services, most of them asking for storage they do not
    // need; the reader acts on one of these only when a flow is failing, and
    // the dialog is where the whole list is.
    readonly property int listedThirdParties: 3

    // The decisions the core stores, named here so nothing in this file
    // compares against a bare number.
    readonly property int allowedOnce: 1
    readonly property int allowedPersistently: 2
    readonly property int blocked: 3

    // Read when the panel opens rather than kept live: these are answers about
    // one origin at one moment, and a panel nobody has open has nothing to say.
    property var sitePermissionRows: []
    property var cookieAllowanceRows: []
    property var refusedThirdParties: []
    property real siteDataBytes: -1
    property real retainedDataBytes: -1
    readonly property string originLabel: {
        const address = String(root.activeUrl)
        const separator = address.indexOf("://")
        if (separator === -1)
            return address
        const authority = address.substring(separator + 3).split("/")[0]
        return authority.length > 0 ? authority : address
    }

    readonly property string connectionSentence: {
        switch (root.connectionState) {
        case "secure":
            return "connection is encrypted"
        case "certificate-error":
            return "certificate could not be verified"
        case "insecure":
            return "connection is not encrypted"
        default:
            return "no page is loaded"
        }
    }

    function permissionDecisionName(decision) {
        switch (Number(decision)) {
        case root.allowedOnce:
            return "allowed once"
        case root.allowedPersistently:
            return "always allowed"
        case root.blocked:
            return "blocked"
        default:
            return "asked each time"
        }
    }

    // Cookies, storage and cache together, because that is what the engine can
    // measure and what the clearing action takes. Naming the three is the
    // difference between a number the reader can act on and one they have to
    // guess the scope of.
    function formatBytes(bytes) {
        const units = ["B", "kB", "MB", "GB"]
        let size = bytes
        let unit = 0
        while (size >= 1024 && unit < units.length - 1) {
            size = size / 1024
            unit += 1
        }
        return (unit === 0 ? Math.round(size) : Math.round(size * 10) / 10) + " " + units[unit]
    }

    function siteDataSentence() {
        if (!root.siteDataOnDisk)
            return "this engine keeps no site data on disk"
        if (root.siteDataBytes < 0)
            return "the site data in this Space could not be measured"
        return root.formatBytes(root.siteDataBytes) + " of cookies and cache in this Space"
    }

    function refreshSiteInformation() {
        if (!root.browser)
            return
        root.sitePermissionRows = root.browser.sitePermissions(root.activeUrl)
        root.cookieAllowanceRows = root.browser.thirdPartyCookieAllowances()
        root.refusedThirdParties = root.cookiePolicy ? root.cookiePolicy.refusedOrigins(
                                                           root.activeUrl) : []
        root.siteDataBytes = root.siteDataOnDisk ? root.browser.siteDataBytes(
                                                       root.browser.activeSpaceId,
                                                       root.siteDataEntries) : -1
        root.retainedDataBytes = root.siteDataOnDisk ? root.browser.siteDataBytes(
                                                           root.browser.activeSpaceId,
                                                           root.retainedDataEntries) : -1
    }

    onOpenChanged: if (root.open)
                       root.refreshSiteInformation()
    onSiteDataGenerationChanged: if (root.open)
                                     root.refreshSiteInformation()

    visible: root.open
    height: statusColumn.implicitHeight + 20
    radius: 2
    color: root.colors.overlay
    border.width: 1
    border.color: root.colors.accent

    MouseArea {
        anchors.fill: parent
    }

    Column {
        id: statusColumn
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        SectionLabel {
            objectName: "siteInformationName"
            colors: root.colors
            text: "site information"
            // The first thing in the panel, so there is nothing above it to
            // lean away from and the lean would read as dead space under the
            // border.
            topPadding: overshoot
        }

        // The origin first: every line under it is about this site inside this
        // Space, and nothing else.
        Text {
            objectName: "siteInformationOrigin"
            width: parent.width
            text: root.blank ? "no site" : root.originLabel
            color: root.colors.text
            elide: Text.ElideMiddle
            font.family: Style.font.family
            font.pixelSize: Style.font.body
        }

        Text {
            objectName: "siteInformationConnection"
            width: parent.width
            text: "· " + root.connectionSentence + (root.connectionState === "certificate-error"
                                                    ? " · waived for this session" : "")
            color: root.connectionState === "certificate-error" ? root.colors.urgent :
                                                                  root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        // An engine that cannot report a certificate failure cannot be blocking
        // on one either, so the connection line above is worth less than it
        // looks and the reader is told so.
        Text {
            objectName: "siteInformationCertificates"
            width: parent.width
            visible: !root.certificateDecisionsAvailable
            text: "· this engine cannot report a certificate failure"
            color: root.colors.urgent
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Text {
            width: parent.width
            visible: !root.insecureContentBlocked
            text: "· this engine is not blocking insecure content"
            color: root.colors.urgent
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Text {
            objectName: "siteInformationBlocked"
            width: parent.width
            text: "· " + root.blockedRequestCount + " requests blocked in this window"
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Text {
            objectName: "siteInformationSiteData"
            width: parent.width
            text: "· " + root.siteDataSentence()
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        // The rest of what the Space is holding. It is the larger half on a
        // machine that has been browsing for a while, and the Space-wide
        // clearing cannot take any of it — only a page can empty its own — so
        // it is said plainly rather than counted into the size above.
        Text {
            objectName: "siteInformationRetainedData"
            width: parent.width
            visible: root.siteDataOnDisk && root.retainedDataBytes > 0
            text: "· " + root.formatBytes(root.retainedDataBytes) + " of storage and databases"
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Text {
            objectName: "siteInformationCookies"
            width: parent.width
            text: root.thirdPartyCookieControlAvailable
                  ? "· third-party cookies and storage are blocked" :
                    "· this engine cannot refuse a third party"
            color: root.thirdPartyCookieControlAvailable ? root.colors.mutedText :
                                                           root.colors.urgent
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        // What this page had refused, and what it has been allowed. Stated
        // here and acted on in the dialog: a reader looking at an embedded
        // asset host has no way to judge it from its name alone, and a row of
        // buttons beside each one invites a decision nobody can make.
        Repeater {
            model: root.thirdPartyCookieControlAvailable ? root.refusedThirdParties.slice(0,
                                                                                          root.listedThirdParties) :
                                                           []

            Text {
                required property int index
                required property string modelData

                objectName: "refusedThirdParty" + index
                width: statusColumn.width
                text: "· " + modelData + " — refused"
                color: root.colors.mutedText
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }

        Text {
            objectName: "refusedThirdPartyOverflow"
            width: parent.width
            visible: root.thirdPartyCookieControlAvailable && root.refusedThirdParties.length
                     > root.listedThirdParties
            text: "· and " + (root.refusedThirdParties.length - root.listedThirdParties)
                  + " more, listed under third parties"
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Repeater {
            model: root.cookieAllowanceRows

            Text {
                required property int index
                required property var modelData

                objectName: "cookieAllowance" + index
                width: statusColumn.width
                text: "· " + modelData.origin + " — allowed for " + modelData.purpose
                color: root.colors.text
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }

        SectionLabel {
            colors: root.colors
            text: "permissions in this space"
        }

        Text {
            objectName: "siteInformationNoPermissions"
            width: parent.width
            visible: root.sitePermissionRows.length === 0
            text: root.privateWindow ? "· nothing decided in this private session" :
                                       "· nothing decided for this site"
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Repeater {
            model: root.sitePermissionRows

            Text {
                required property int index
                required property var modelData

                objectName: "sitePermission" + index
                width: statusColumn.width
                text: "· " + modelData.permission + " — " + root.permissionDecisionName(
                          modelData.decision)
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }

        // Triggers, not answers. Each one opens the window's own dialog, where
        // there is room to say which scope is about to go and the keyboard can
        // answer it.
        Flow {
            width: parent.width
            spacing: 6

            ActionButton {
                objectName: "clearSiteStorage"
                colors: root.colors
                label: "clear this site"
                enabled: !root.blank
                onClicked: root.actionRequested("site-storage")
            }

            ActionButton {
                objectName: "clearSiteData"
                colors: root.colors
                label: "clear Space data"
                enabled: !root.privateWindow && root.siteDataOnDisk
                onClicked: root.actionRequested("space-data")
            }

            ActionButton {
                objectName: "resetSitePermissions"
                colors: root.colors
                label: "reset permissions"
                enabled: !root.blank
                onClicked: root.actionRequested("reset-permissions")
            }

            ActionButton {
                objectName: "manageThirdParties"
                colors: root.colors
                label: "third parties"
                enabled: root.thirdPartyCookieControlAvailable && (root.refusedThirdParties.length
                                                                   > 0 || root.cookieAllowanceRows.length
                                                                   > 0)
                onClicked: root.actionRequested("third-party")
            }
        }
    }
}
