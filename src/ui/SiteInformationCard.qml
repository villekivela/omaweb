import QtQuick
import Omaweb
import qs.Commons
import qs.Ui as Omarchy

// What Omaweb states about the site on show, for the Space it is on show in.
//
// A card that floats from the address over the page edge. It leads with who
// and how: a band in the verdict's colour, then the site's host. Under them
// four tiles, each a value and a label, each opening its detail inside the
// card: the certificate, the requests Content blocking refused, the cookies
// and site data the Space holds, and the third parties this page was refused
// or allowed. Then the permissions this site asked for or the reader decided,
// and the two actions that are this site's alone.
//
// What this build's engine cannot do is not said here. A caveat about the
// engine is not a fact about the site, and Settings is where it is stated.
//
// The card states and decides permissions; it does not ask. Clearing and
// resetting are confirmed in the centred dialog every other question about the
// browser is asked in, which has the room to say what is about to happen.
Omarchy.BorderSurface {
    id: root

    required property var colors
    required property var browser
    property string iconFontFamily: ""
    // The engine's third-party filter, which knows which embedded origins this
    // page has had refused and how many cookies the site holds. Null where the
    // engine has no filter at all.
    property var cookiePolicy: null
    // Content blocking, which owns the Refusal tally the card states.
    property var blocker: null
    property url activeUrl
    property bool blank: false
    property bool privateWindow: false
    property string connectionState: "internal"
    // Whether the page arrived over HTTPS because HTTPS-only mode sent its
    // plain address there.
    property bool upgradedByHttpsOnly: false
    // The Secure DNS resolver that could not find the page's name, or empty.
    // The engine's own error page says only that the name was not found, and
    // the reader's system resolver might have found it, so whose answer that
    // was is said here.
    property string lookupFailedBy: ""
    // The chain the certificate detail shows: the one the page arrived over,
    // or the one the certificate question is about, for the site it names.
    property var certificateChain: []
    property string certificateOrigin: ""
    property bool certificateVerified: true
    property bool thirdPartyCookieControlAvailable: false
    property bool siteDataOnDisk: false
    // The files and directories the engine says its site data lives in, and a
    // count the window bumps when the engine reports it has finished clearing.
    property var siteDataEntries: []
    property var retainedDataEntries: []
    property int siteDataGeneration: 0
    // A permission the page on show is asking for right now. It has a row
    // before it has a decision.
    property string askedPermission: ""
    property bool open: false
    // The detail on show, or empty for the card's top: "certificate",
    // "blocked", "cookies" or "third-parties".
    property string detail: ""
    // The tile the keyboard is on.
    property int cursor: 0

    // Read when the card opens rather than kept live: these are answers about
    // one origin at one moment, and a card nobody has open has nothing to say.
    property var sitePermissionRows: []
    property var cookieAllowanceRows: []
    property var refusedThirdParties: []
    property int cookieCount: 0
    property real siteDataBytes: -1
    property real retainedDataBytes: -1

    readonly property int refusalTally: refusals.count
    property RefusalTally refusals: RefusalTally {
        blocker: root.blocker
        browser: root.browser
        pageAddress: root.activeUrl
    }

    // The detail still being drawn while the card slides back to its top.
    property string shownDetail: ""
    // How far the detail has slid in, from the top (0) to the detail (1).
    property real slide: 0

    // What the reader asked for, by name, for the window to ask about.
    signal actionRequested(string action)
    signal closeRequested

    // How many refused requests the blocked detail lists. The tile counts them
    // all; the page's own network log has the rest.
    readonly property int listedRefusals: 12
    readonly property real gutter: 12
    readonly property real tileGap: 8
    readonly property real bandHeight: root.detail.length > 0 ? 42 : 56
    // Half the card's inner width, beside a neighbour: a tile, a permission
    // row, an action.
    readonly property real halfWidth: (root.width - root.borderLeft - root.borderRight - 2
                                       * root.gutter - root.tileGap) / 2

    readonly property bool overTls: root.connectionState === "secure" || root.connectionState
                                    === "certificate-error"
    readonly property string originLabel: {
        const address = String(root.activeUrl);
        const separator = address.indexOf("://");
        if (separator === -1)
            return address;
        const authority = address.substring(separator + 3).split("/")[0];
        return authority.length > 0 ? authority : address;
    }
    readonly property string hostLabel: root.blank ? qsTr("Start page") : root.originLabel

    // The verdict for each connection the engine reports: what the band says,
    // the line under it, its glyph and its colour. Anything that is not a
    // connection the engine made is Omaweb's own page.
    readonly property var verdicts: ({
                                         "secure": {
                                             "text": qsTr("Connection is secure"),
                                             "detail": root.upgradedByHttpsOnly ? qsTr(
                                                                                      "Encrypted · upgraded from HTTP by HTTPS-only mode") :
                                                                                  qsTr("Encrypted",
                                                                                       "the connection"),
                                             "glyph": "lock",
                                             "color": root.colors.accent
                                         },
                                         "certificate-error": {
                                             "text": qsTr("Certificate could not be verified"),
                                             "detail": qsTr("Waived for this session"),
                                             "glyph": "warning",
                                             "color": root.colors.urgent
                                         },
                                         "insecure": {
                                             "text": qsTr("Not secure"),
                                             "detail": qsTr(
                                                           "Anything sent here can be read on the way"),
                                             "glyph": "lock_open",
                                             "color": root.colors.spaces
                                                      && root.colors.spaces.yellow
                                                      ? root.colors.spaces.yellow :
                                                        root.colors.urgent
                                         },
                                         "internal": {
                                             "text": qsTr("Omaweb's own page"),
                                             "detail": qsTr("Nothing was loaded from the network"),
                                             "glyph": "home",
                                             "color": root.colors.mutedText
                                         }
                                     })
    readonly property var shownVerdict: root.verdicts[root.connectionState]
                                        || root.verdicts.internal
    readonly property string verdict: root.shownVerdict.text
    // The engine reports no protocol version, so none is named. A name the
    // chosen Secure DNS resolver could not find says whose answer that was:
    // the engine's own error page says only that the name was not found.
    readonly property string verdictDetail: root.lookupFailedBy.length > 0 ? qsTr(
                                                                                 "%1 could not find this site, over Secure DNS").arg(
                                                                                 root.lookupFailedBy) :
                                                                             root.shownVerdict.detail
    readonly property color verdictColor: root.shownVerdict.color

    // Who vouches for the site: the certificate above its own in the chain,
    // or, for one sent alone, the name it was issued by.
    function issuerName(chain) {
        if (chain.length === 0)
            return qsTr("Not reported", "a certificate the engine has not reported");
        if (chain.length > 1)
            return String(chain[1].name);
        if (chain[0].selfSigned)
            return qsTr("Self-signed");
        const match = /CN=([^,]+)/.exec(String(chain[0].issuer));
        return match ? match[1] : String(chain[0].issuer);
    }

    function countLabel(count, none, one, many) {
        if (count <= 0)
            return none;
        return count === 1 ? one : many.arg(count);
    }

    // Each detail's title, which is also its tile's label.
    readonly property var titles: ({
                                       "certificate": qsTr("Certificate"),
                                       "blocked": qsTr("Blocked",
                                                       "requests Content blocking refused"),
                                       "cookies": qsTr("Cookies and site data"),
                                       "third-parties": qsTr("Third parties")
                                   })

    readonly property var tiles: {
        const list = [];
        if (root.overTls) {
            list.push({
                          "key": "certificate",
                          "glyph": "badge",
                          "value": root.issuerName(root.certificateChain),
                          "drills": root.certificateChain.length > 0
                      });
        }
        list.push({
                      "key": "blocked",
                      "glyph": "shield",
                      "value": root.countLabel(root.refusalTally, qsTr("None",
                                                                       "no requests were blocked"),
                                               qsTr("1 request"), qsTr("%1 requests")),
                      "drills": root.refusalTally > 0
                  });
        list.push({
                      "key": "cookies",
                      "glyph": "cookie",
                      "value": root.blank ? qsTr("None", "Omaweb's own page holds no site data") :
                                            root.countLabel(root.cookieCount, qsTr("No cookies"),
                                                            qsTr("1 cookie"), qsTr("%1 cookies")),
                      "drills": !root.blank
                  });
        list.push({
                      "key": "third-parties",
                      "glyph": "hub",
                      "value": root.countLabel(root.cookieAllowanceRows.length, qsTr("None allowed",
                                                                                     "third parties"),
                                               qsTr("1 allowed"), qsTr("%1 allowed")),
                      "drills": root.thirdPartyCookieControlAvailable
                                && root.cookieAllowanceRows.length
                                + root.refusedThirdParties.length > 0
                  });
        return list.map(function (tile) {
            return Object.assign(tile, {
                                     "label": root.titles[tile.key]
                                 });
        });
    }

    // The permissions a site can hold a standing answer for, in the order the
    // card lists them, each with its name and glyph. A row the store keeps for
    // something else, such as the plain-HTTP choice HTTPS-only mode remembers,
    // has no Allow, Ask or Block.
    readonly property var permissionKinds: [
        {
            "permission": "camera",
            "label": qsTr("Camera"),
            "glyph": "videocam"
        },
        {
            "permission": "microphone",
            "label": qsTr("Microphone"),
            "glyph": "mic"
        },
        {
            "permission": "camera-and-microphone",
            "label": qsTr("Camera and microphone"),
            "glyph": "perm_camera_mic"
        },
        {
            "permission": "geolocation",
            "label": qsTr("Location"),
            "glyph": "location_on"
        },
        {
            "permission": "notifications",
            "label": qsTr("Notifications"),
            "glyph": "notifications"
        },
        {
            "permission": "automatic-downloads",
            "label": qsTr("Automatic downloads"),
            "glyph": "download"
        }
    ]

    function permissionKind(permission) {
        return root.permissionKinds.find(function (kind) {
            return kind.permission === permission;
        });
    }

    readonly property var permissionRows: {
        const choices = {};
        for (const row of root.sitePermissionRows)
            choices[row.permission] = root.choiceFor(row.decision);
        if (root.askedPermission.length > 0 && choices[root.askedPermission] === undefined)
            choices[root.askedPermission] = "ask";
        return root.permissionKinds.filter(function (kind) {
            return choices[kind.permission] !== undefined;
        }).map(function (kind) {
            return Object.assign({
                                     "value": choices[kind.permission]
                                 }, kind);
        });
    }

    function choiceFor(decision) {
        switch (Number(decision)) {
        case BrowserController.AllowOnce:
        case BrowserController.AllowPersistently:
            return "allow";
        case BrowserController.Block:
            return "block";
        default:
            return "ask";
        }
    }

    function decisionFor(choice) {
        if (choice === "allow")
            return BrowserController.AllowPersistently;
        return choice === "block" ? BrowserController.Block : BrowserController.Ask;
    }

    // The dropdown is the decision: it is stored for this Space straight away.
    // Returns whether it was, so the dropdown shows only a stored answer.
    function decidePermission(permission, choice) {
        return !!root.browser && root.browser.decideSitePermission(root.activeUrl, permission,
                                                                   root.decisionFor(choice));
    }

    // Cookies, storage and cache, measured as the engine can measure them.
    function formatBytes(bytes) {
        const units = ["B", "kB", "MB", "GB"];
        let size = bytes;
        let unit = 0;
        while (size >= 1024 && unit < units.length - 1) {
            size = size / 1024;
            unit += 1;
        }
        return (unit === 0 ? Math.round(size) : Math.round(size * 10) / 10) + " " + units[unit];
    }

    // A refused address as a reader recognises it: host and path, without the
    // scheme, the query or the fragment.
    function requestLabel(address) {
        return String(address).replace(/^[a-z][a-z0-9+.-]*:\/\//i, "").replace(/[?#].*$/, "");
    }

    function openTile(index) {
        const tile = root.tiles[index];
        if (!tile || !tile.drills)
            return;
        root.cursor = index;
        root.detail = tile.key;
        root.forceActiveFocus();
    }

    function back() {
        root.detail = "";
        root.forceActiveFocus();
    }

    function refreshSiteInformation() {
        if (!root.browser)
            return;
        root.sitePermissionRows = root.browser.sitePermissions(root.activeUrl);
        root.cookieAllowanceRows = root.browser.thirdPartyCookieAllowances();
        root.refusedThirdParties = root.cookiePolicy ? root.cookiePolicy.refusedOrigins(
                                                           root.activeUrl) : [];
        root.cookieCount = root.cookiePolicy && !root.blank ? root.cookiePolicy.siteCookieCount(
                                                                  root.browser.sessionSpaceId,
                                                                  root.activeUrl) : 0;
        root.siteDataBytes = root.siteDataOnDisk ? root.browser.siteDataBytes(
                                                       root.browser.activeSpaceId,
                                                       root.siteDataEntries) : -1;
        root.retainedDataBytes = root.siteDataOnDisk ? root.browser.siteDataBytes(
                                                           root.browser.activeSpaceId,
                                                           root.retainedDataEntries) : -1;
    }

    onOpenChanged: {
        if (!root.open)
            return;
        root.cursor = 0;
        root.refreshSiteInformation();
        // A card opens where it was asked to, whatever an earlier one was
        // doing when it was put away.
        slideAnimation.stop();
        root.slide = root.detail.length > 0 ? 1 : 0;
    }
    onSiteDataGenerationChanged: if (root.open)
                                     root.refreshSiteInformation()
    // Only a step inside an open card slides; a card opened at a detail is
    // there at once.
    onDetailChanged: {
        if (root.detail.length > 0)
            root.shownDetail = root.detail;
        slideAnimation.stop();
        const target = root.detail.length > 0 ? 1 : 0;
        if (root.open && root.visible) {
            slideAnimation.to = target;
            slideAnimation.start();
        } else {
            root.slide = target;
        }
    }

    NumberAnimation {
        id: slideAnimation
        target: root
        property: "slide"
        duration: 180
        easing.type: Easing.OutCubic
    }

    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Escape) {
            if (root.detail.length > 0)
                root.back();
            else
                root.closeRequested();
            event.accepted = true;
            return;
        }
        if (root.detail === "certificate") {
            event.accepted = certificateDetail.handleKey(event);
            return;
        }
        if (root.detail.length > 0)
            return;
        const last = root.tiles.length - 1;
        if (event.key === Qt.Key_Right) {
            root.cursor = Math.min(last, root.cursor + 1);
        } else if (event.key === Qt.Key_Left) {
            root.cursor = Math.max(0, root.cursor - 1);
        } else if (event.key === Qt.Key_Down) {
            root.cursor = Math.min(last, root.cursor + 2);
        } else if (event.key === Qt.Key_Up) {
            root.cursor = Math.max(0, root.cursor - 2);
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.openTile(root.cursor);
        } else {
            return;
        }
        event.accepted = true;
    }

    objectName: "siteInformationCard"
    width: 400
    height: root.borderTop + band.height + body.height + root.borderBottom
    radius: Style.cornerRadius
    // Opaque: nothing of the page reads through what the card says about it.
    color: root.colors.overlayOpaque
    borderSpec: Border.controlSpec("normal", root.colors.text, root.colors.accent)
    clip: true
    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Site information")

    Behavior on height {
        enabled: root.open && root.visible
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutCubic
        }
    }

    // Clicks and the wheel on the card stay on it, rather than reaching the
    // outline or the page under it; a click anywhere else puts it away.
    MouseArea {
        anchors.fill: parent
        onClicked: root.forceActiveFocus()
        onWheel: function (wheel) {
            wheel.accepted = true;
        }
    }

    // The verdict's band. At a detail it names the detail and keeps the way
    // back to the top beside it.
    Rectangle {
        id: band
        objectName: "siteInformationBand"
        x: root.borderLeft
        y: root.borderTop
        width: root.width - root.borderLeft - root.borderRight
        height: root.bandHeight
        color: Qt.rgba(root.verdictColor.r, root.verdictColor.g, root.verdictColor.b, 0.14)

        Behavior on height {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }

        // The badge is centred on the text beside it, the verdict and the line
        // under it together, or the verdict alone when there is no second line.
        Row {
            anchors.left: parent.left
            anchors.leftMargin: root.gutter
            anchors.right: backButton.visible ? backButton.left : parent.right
            anchors.rightMargin: root.gutter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Rectangle {
                id: badge
                objectName: "siteInformationBadge"
                anchors.verticalCenter: parent.verticalCenter
                width: 26
                height: 26
                radius: Style.cornerRadius
                color: root.verdictColor

                Text {
                    anchors.centerIn: parent
                    text: root.shownVerdict.glyph
                    color: root.colors.overlayOpaque
                    font.family: root.iconFontFamily
                    font.pixelSize: 16
                    Accessible.ignored: true
                }
            }

            Column {
                id: verdictBlock
                objectName: "siteInformationVerdictBlock"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - badge.width - parent.spacing

                Text {
                    objectName: "siteInformationVerdict"
                    width: parent.width
                    text: root.detail === "blocked" ? qsTr("Blocked requests") : root.detail.length
                                                      > 0 ? root.titles[root.detail] : root.verdict
                    color: root.colors.text
                    elide: Text.ElideRight
                    font.family: Style.font.family
                    font.pixelSize: Style.font.subtitle
                    font.bold: true
                }

                Text {
                    objectName: "siteInformationVerdictDetail"
                    width: parent.width
                    visible: root.detail.length === 0 && text.length > 0
                    text: root.verdictDetail
                    color: root.colors.mutedText
                    elide: Text.ElideRight
                    font.family: Style.font.family
                    font.pixelSize: Style.font.bodySmall
                }
            }
        }

        ActionButton {
            id: backButton
            objectName: "siteInformationBack"
            anchors.right: parent.right
            anchors.rightMargin: root.gutter
            anchors.verticalCenter: parent.verticalCenter
            visible: root.detail.length > 0
            colors: root.colors
            label: "‹ " + root.hostLabel
            accessibleName: qsTr("Back to %1").arg(root.hostLabel)
            focusable: false
            onClicked: root.back()
        }
    }

    // The top and the detail side by side, one slid in for the other. The card
    // keeps its width and its place; only its height follows what is shown.
    Item {
        id: body
        x: root.borderLeft
        y: band.y + band.height
        width: band.width
        height: Math.ceil(overview.implicitHeight * (1 - root.slide) + detailPane.implicitHeight
                          * root.slide)
        clip: true

        Column {
            id: overview
            x: -root.slide * body.width
            width: body.width
            visible: root.slide < 1
            padding: root.gutter
            spacing: 10

            Text {
                objectName: "siteInformationHost"
                width: parent.width - 2 * root.gutter
                text: root.hostLabel
                color: root.colors.mutedText
                elide: Text.ElideMiddle
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }

            Grid {
                columns: 2
                spacing: root.tileGap

                Repeater {
                    model: root.tiles

                    Rectangle {
                        id: tile

                        required property int index
                        required property var modelData

                        objectName: "siteInformationTile_" + modelData.key
                        width: root.halfWidth
                        height: 66
                        radius: Style.cornerRadius
                        color: tileMouse.containsMouse && tile.modelData.drills
                               ? root.colors.surfaceHover : root.colors.surface
                        border.width: root.activeFocus && root.cursor === tile.index ? 1 : 0
                        border.color: root.colors.accent
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("%1: %2").arg(tile.modelData.label).arg(
                                             tile.modelData.value)

                        Text {
                            x: 10
                            y: 8
                            text: tile.modelData.glyph
                            color: root.colors.mutedText
                            font.family: root.iconFontFamily
                            font.pixelSize: Style.font.icon
                            Accessible.ignored: true
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            y: 6
                            visible: tile.modelData.drills
                            text: "›"
                            color: root.colors.mutedText
                            font.family: Style.font.family
                            font.pixelSize: Style.font.title
                            Accessible.ignored: true
                        }

                        Text {
                            objectName: "siteInformationTileValue_" + tile.modelData.key
                            x: 10
                            y: 28
                            width: parent.width - 20
                            text: tile.modelData.value
                            color: root.colors.text
                            elide: Text.ElideRight
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                            font.bold: true
                        }

                        Text {
                            x: 10
                            y: 46
                            width: parent.width - 20
                            text: tile.modelData.label
                            color: root.colors.mutedText
                            elide: Text.ElideRight
                            font.family: Style.font.family
                            font.pixelSize: Style.font.caption
                        }

                        MouseArea {
                            id: tileMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: tile.modelData.drills ? Qt.PointingHandCursor :
                                                                 Qt.ArrowCursor
                            onClicked: root.openTile(tile.index)
                        }
                    }
                }
            }

            // Only what this site asked for or the reader decided in this
            // Space. A site that never asked has no rows at all.
            Flow {
                width: parent.width - 2 * root.gutter
                visible: root.permissionRows.length > 0
                spacing: root.tileGap

                Repeater {
                    model: root.permissionRows

                    Rectangle {
                        id: permissionRow

                        required property var modelData

                        objectName: "sitePermission_" + modelData.permission
                        width: root.halfWidth
                        height: 40
                        radius: Style.cornerRadius
                        color: root.colors.surface

                        Text {
                            x: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: permissionRow.modelData.glyph
                            color: root.colors.mutedText
                            font.family: root.iconFontFamily
                            font.pixelSize: Style.font.icon
                            Accessible.ignored: true
                        }

                        PermissionDropdown {
                            id: choice
                            objectName: "sitePermissionChoice_" + permissionRow.modelData.permission
                            anchors.right: parent.right
                            anchors.rightMargin: 7
                            anchors.verticalCenter: parent.verticalCenter
                            colors: root.colors
                            iconFontFamily: root.iconFontFamily
                            value: permissionRow.modelData.value
                            accessibleName: permissionRow.modelData.label
                            onChosen: function (value) {
                                if (root.decidePermission(permissionRow.modelData.permission,
                                                          value))
                                    choice.value = value;
                            }
                        }
                    }
                }
            }

            Row {
                spacing: root.tileGap

                ActionButton {
                    objectName: "clearSiteStorage"
                    width: root.halfWidth
                    colors: root.colors
                    label: qsTr("Clear site data")
                    enabled: !root.blank
                    onClicked: root.actionRequested("site-storage")
                }

                ActionButton {
                    objectName: "resetSitePermissions"
                    width: root.halfWidth
                    colors: root.colors
                    label: qsTr("Reset permissions")
                    enabled: !root.blank
                    onClicked: root.actionRequested("reset-permissions")
                }
            }
        }

        Column {
            id: detailPane
            x: (1 - root.slide) * body.width
            width: body.width
            visible: root.slide > 0
            padding: root.gutter
            spacing: 8

            CertificateDetail {
                id: certificateDetail
                objectName: "certificateDetail"
                width: parent.width - 2 * root.gutter
                visible: root.shownDetail === "certificate"
                colors: root.colors
                chain: root.certificateChain
                origin: root.certificateOrigin
                verified: root.certificateVerified
            }

            // The requests behind the tally, by host and path: the query of a
            // tracker's address is its payload, and it says nothing a reader
            // decides by. One refused through its host's CNAME chain names the
            // canonical name it matched, on a line of its own under it.
            Column {
                width: parent.width - 2 * root.gutter
                visible: root.shownDetail === "blocked"
                spacing: 4

                Repeater {
                    model: root.shownDetail === "blocked" ? root.refusals.requests.slice(0,
                                                                                         root.listedRefusals) :
                                                            []

                    Column {
                        id: refusal

                        required property int index
                        required property var modelData

                        width: parent.width

                        Text {
                            objectName: "refusedRequest" + refusal.index
                            width: parent.width
                            text: root.requestLabel(refusal.modelData.address)
                            color: root.colors.text
                            elide: Text.ElideMiddle
                            font.family: Style.font.family
                            font.pixelSize: Style.font.body
                        }

                        // A long name keeps its end, the registrable domain a
                        // rule names.
                        Text {
                            objectName: "refusedRequestThrough" + refusal.index
                            width: parent.width
                            visible: !!refusal.modelData.canonicalName
                            text: qsTr("through %1").arg(refusal.modelData.canonicalName || "")
                            color: root.colors.mutedText
                            elide: Text.ElideLeft
                            font.family: Style.font.family
                            font.pixelSize: Style.font.caption
                        }
                    }
                }

                Text {
                    objectName: "refusedRequestOverflow"
                    width: parent.width
                    visible: root.refusals.requests.length > root.listedRefusals
                    text: qsTr("and %n more", "", root.refusals.requests.length
                               - root.listedRefusals)
                    color: root.colors.mutedText
                    font.family: Style.font.family
                    font.pixelSize: Style.font.caption
                }
            }

            // What the site holds in this Space. The engine counts one site's
            // cookies; its size it measures only for the whole Space, so the
            // sizes are named as the Space's.
            Column {
                width: parent.width - 2 * root.gutter
                visible: root.shownDetail === "cookies"
                spacing: 8

                Repeater {
                    model: {
                        const lines = [
                                  {
                                      "name": "siteInformationCookieCount",
                                      "label": qsTr("Cookies this site set"),
                                      "value": root.countLabel(root.cookieCount, qsTr("None",
                                                                                      "no cookies"),
                                                               qsTr("1 cookie"), qsTr("%1 cookies"))
                                  }
                              ];
                        if (root.siteDataOnDisk && root.siteDataBytes >= 0) {
                            lines.push({
                                           "name": "siteInformationSiteData",
                                           "label": qsTr("Cookies and cache in this Space"),
                                           "value": root.formatBytes(root.siteDataBytes)
                                       });
                        }
                        if (root.siteDataOnDisk && root.retainedDataBytes > 0) {
                            lines.push({
                                           "name": "siteInformationRetainedData",
                                           "label": qsTr("Storage and databases in this Space"),
                                           "value": root.formatBytes(root.retainedDataBytes)
                                       });
                        }
                        return lines;
                    }

                    DetailLine {
                        required property var modelData

                        objectName: modelData.name
                        label: modelData.label
                        value: modelData.value
                    }
                }
            }

            // What this page was refused, and what has been allowed. Stated here
            // and answered in the dialog: a reader looking at an embedded asset
            // host has no way to judge it from its name alone.
            Column {
                width: parent.width - 2 * root.gutter
                visible: root.shownDetail === "third-parties"
                spacing: 8

                Repeater {
                    model: root.cookieAllowanceRows

                    DetailLine {
                        required property int index
                        required property var modelData

                        objectName: "cookieAllowance" + index
                        label: modelData.purpose === "payment" ? qsTr("Allowed for a payment") :
                                                                 qsTr("Allowed for a sign-in")
                        value: modelData.origin
                    }
                }

                Repeater {
                    model: root.refusedThirdParties

                    DetailLine {
                        required property int index
                        required property string modelData

                        objectName: "refusedThirdParty" + index
                        label: qsTr("Refused", "a third party refused cookies and storage")
                        value: modelData
                    }
                }

                ActionButton {
                    objectName: "manageThirdParties"
                    colors: root.colors
                    label: qsTr("Allow or stop allowing…")
                    onClicked: root.actionRequested("third-party")
                }
            }
        }
    }

    // A caption over a value, on the tile ground.
    component DetailLine: Rectangle {
        id: line

        property string label: ""
        property string value: ""

        width: parent ? parent.width : 0
        height: lineColumn.implicitHeight + 16
        radius: Style.cornerRadius
        color: root.colors.surface

        Column {
            id: lineColumn
            x: 10
            y: 8
            width: parent.width - 20

            Text {
                width: parent.width
                text: line.label
                color: root.colors.mutedText
                font.family: Style.font.family
                font.pixelSize: Style.font.caption
            }

            Text {
                objectName: "detailValue"
                width: parent.width
                text: line.value
                color: root.colors.text
                wrapMode: Text.WrapAnywhere
                font.family: Style.font.family
                font.pixelSize: Style.font.body
            }
        }
    }
}
