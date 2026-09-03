import QtQuick
import qs.Commons

// What Omaweb states about the site on show, for the Space it is on show in.
//
// One origin, one Space, one moment: the connection the engine reported, the
// requests Content blocking refused, the site data the Space holds, the
// origin's Site permissions, and two confirmed ways to take state back. Where
// the engine cannot answer for one of those lines, the line says so — a blank
// reads as a reassurance, and the reader has no way to tell one from an answer.
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
    // The categories the engine said it could not take, from the window.
    property var untouchedDataCategories: []

    // What the panel just did, on its way to the notice that says so. An
    // irreversible action that reports nothing is one the reader cannot tell
    // from a button that does not work.
    signal noticeRequested(string glyph, string message, string detail)

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
    // Which reset the panel is waiting on a confirmation for. Both are
    // irreversible, so neither happens on one press.
    property string resetPending: ""

    readonly property string originLabel: {
        const address = String(root.activeUrl)
        const separator = address.indexOf("://")
        if (separator === -1) return address
        const authority = address.substring(separator + 3).split("/")[0]
        return authority.length > 0 ? authority : address
    }

    readonly property string connectionSentence: {
        switch (root.connectionState) {
        case "secure": return "connection is encrypted"
        case "certificate-error": return "certificate could not be verified"
        case "insecure": return "connection is not encrypted"
        default: return "no page is loaded"
        }
    }

    function permissionDecisionName(decision) {
        switch (Number(decision)) {
        case root.allowedOnce: return "allowed once"
        case root.allowedPersistently: return "always allowed"
        case root.blocked: return "blocked"
        default: return "asked each time"
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
        return root.formatBytes(root.siteDataBytes)
            + " of cookies and cache in this Space, which clearing takes"
    }

    function refreshSiteInformation() {
        if (!root.browser) return
        root.sitePermissionRows = root.browser.sitePermissions(root.activeUrl)
        root.cookieAllowanceRows = root.browser.thirdPartyCookieAllowances()
        root.refusedThirdParties = root.cookiePolicy
            ? root.cookiePolicy.refusedOrigins(root.activeUrl) : []
        root.siteDataBytes = root.siteDataOnDisk
            ? root.browser.siteDataBytes(root.browser.activeSpaceId, root.siteDataEntries)
            : -1
        root.retainedDataBytes = root.siteDataOnDisk
            ? root.browser.siteDataBytes(root.browser.activeSpaceId, root.retainedDataEntries)
            : -1
        root.resetPending = ""
    }

    onOpenChanged: if (root.open) root.refreshSiteInformation()
    onSiteDataGenerationChanged: if (root.open) root.refreshSiteInformation()

    visible: root.open
    height: statusColumn.implicitHeight + 20
    radius: 2
    color: root.colors.overlay
    border.width: 1
    border.color: root.colors.accent

    MouseArea { anchors.fill: parent }

    Column {
        id: statusColumn
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        SectionLabel {
            colors: root.colors
            text: "site information"
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
            text: "· " + root.connectionSentence
                + (root.connectionState === "certificate-error"
                    ? " · waived for this session" : "")
            color: root.connectionState === "certificate-error"
                ? root.colors.urgent : root.colors.mutedText
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
        // machine that has been browsing for a while, and no action here can
        // move it, so it is said plainly instead of being counted above.
        Text {
            objectName: "siteInformationRetainedData"
            width: parent.width
            visible: root.siteDataOnDisk && root.retainedDataBytes > 0
            text: "· " + root.formatBytes(root.retainedDataBytes)
                + " of storage and databases this engine cannot clear"
            color: root.colors.mutedText
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Text {
            objectName: "siteInformationCookies"
            width: parent.width
            text: root.thirdPartyCookieControlAvailable
                ? "· third-party cookies and storage are blocked"
                : "· this engine cannot refuse a third party"
            color: root.thirdPartyCookieControlAvailable
                ? root.colors.mutedText : root.colors.urgent
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        // A flow that needs a third party can be given one, by name, for a
        // reason the reader can read back. Nothing here survives the session,
        // and every one of them is beside its own way out.
        Repeater {
            model: root.thirdPartyCookieControlAvailable ? root.refusedThirdParties : []

            Row {
                required property int index
                required property string modelData

                objectName: "refusedThirdParty" + index
                width: statusColumn.width
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: statusColumn.width - 132
                    text: "· " + modelData
                    color: root.colors.mutedText
                    elide: Text.ElideMiddle
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }

                ActionButton {
                    objectName: "allowSignIn" + index
                    colors: root.colors
                    label: "sign-in"
                    onClicked: {
                        root.browser.allowThirdPartyCookies(modelData, "authentication")
                        root.refreshSiteInformation()
                    }
                }

                ActionButton {
                    objectName: "allowPayment" + index
                    colors: root.colors
                    label: "payment"
                    onClicked: {
                        root.browser.allowThirdPartyCookies(modelData, "payment")
                        root.refreshSiteInformation()
                    }
                }
            }
        }

        Repeater {
            model: root.cookieAllowanceRows

            Row {
                required property int index
                required property var modelData

                objectName: "cookieAllowance" + index
                width: statusColumn.width
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: statusColumn.width - 80
                    text: "· " + modelData.origin + " — allowed for " + modelData.purpose
                    color: root.colors.text
                    elide: Text.ElideMiddle
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }

                ActionButton {
                    objectName: "revokeAllowance" + index
                    colors: root.colors
                    label: "revoke"
                    onClicked: {
                        root.browser.revokeThirdPartyCookieAllowance(modelData.origin)
                        root.refreshSiteInformation()
                    }
                }
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
            text: root.privateWindow
                ? "· nothing decided in this private session"
                : "· nothing decided for this site"
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
                text: "· " + modelData.permission + " — "
                    + root.permissionDecisionName(modelData.decision)
                color: root.colors.mutedText
                wrapMode: Text.WordWrap
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }
        }

        // Both resets are irreversible, so neither happens on one press. The
        // second press is about one named action rather than a general
        // agreement, and the question says which scope is about to go: the
        // engine cannot clear one site's storage on its own, so clearing site
        // data is the Space's and says so.
        Text {
            objectName: "siteInformationResetQuestion"
            width: parent.width
            visible: root.resetPending.length > 0
            text: root.resetPending === "site-data"
                ? "clear the cookies, storage and cache of every site in this Space?"
                : "reset every decision made for " + root.originLabel + "?"
            color: root.colors.text
            wrapMode: Text.WordWrap
            font.family: Style.font.family
            font.pixelSize: Style.font.caption
        }

        Row {
            width: parent.width
            spacing: 6

            ActionButton {
                objectName: "clearSiteData"
                colors: root.colors
                label: root.resetPending === "site-data"
                    ? "confirm clear" : "clear Space data"
                primary: root.resetPending === "site-data"
                enabled: !root.privateWindow && root.siteDataOnDisk
                onClicked: {
                    if (root.resetPending !== "site-data") {
                        root.resetPending = "site-data"
                        return
                    }
                    const cleared = root.browser.clearBrowsingData(
                        ["cookies", "storage", "cache"], 0)
                    root.refreshSiteInformation()
                    // Named for what the engine actually took, and it is the
                    // engine that says what it could not — the difference
                    // between an action that fell short and one that lied.
                    const stayed = root.untouchedDataCategories
                    root.noticeRequested(cleared ? "delete_sweep" : "block",
                        cleared
                            ? "Cleared this Space's cookies and cache"
                            : "Could not clear this Space's site data",
                        cleared && stayed.length > 0
                            ? stayed.join(" and ")
                                + " stayed: this engine has no way to remove them"
                            : "")
                }
            }

            ActionButton {
                objectName: "resetSitePermissions"
                colors: root.colors
                label: root.resetPending === "permissions"
                    ? "confirm reset" : "reset permissions"
                primary: root.resetPending === "permissions"
                enabled: !root.blank
                onClicked: {
                    if (root.resetPending !== "permissions") {
                        root.resetPending = "permissions"
                        return
                    }
                    const reset = root.browser.resetSitePermissions(root.activeUrl)
                    root.refreshSiteInformation()
                    // Reloading does not take a capability off a page: the
                    // engine answers a granted one from a store keyed by the
                    // frame that asked, and a reload reuses that frame. Opening
                    // the site again is a new frame, and asks.
                    root.noticeRequested(reset ? "shield_person" : "block",
                        reset
                            ? "Reset every decision for " + root.originLabel
                            : "Could not reset the decisions for " + root.originLabel,
                        reset
                            ? "a page already holding one keeps it until you open the site again"
                            : "")
                }
            }
        }
    }
}
