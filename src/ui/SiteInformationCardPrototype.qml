import QtQuick
import QtQuick.Controls
import qs.Commons

// PROTOTYPE for #541 — THROWAWAY. Nothing here is the feature.
//
// Question: within the decisions fixed in #541 (a ~400 px card from the
// address; the verdict first; compact rows; permission dropdowns; details that
// drill into the card; two site actions), what should Site information look
// like? Four structurally different answers, on fake data, switchable live.
//
// Run: scripts/prototype_site_card.sh   (UI lab; flags below)
//   --site-variant N      1-6, the variant to start on
//   --site-scenario NAME  secure | http | cert | start
//   --site-detail NAME    certificate | blocked | cookies | third
// Keys while the card is open: Left/Right variant, Up/Down row, Enter opens a
// row, Escape steps back and then closes. The strip at the bottom does the
// same with the pointer and also cycles the scenario.
//
// It reads no engine and writes nothing. Wording is English only and is not
// wrapped for the translation gate on purpose: the winner is rewritten, with
// Finnish, when it is folded in.
Item {
    id: root

    required property var colors
    property string iconFontFamily: ""
    property Item addressItem: null
    property bool open: false
    signal closeRequested

    // ---- run-time switches ------------------------------------------------
    function argument(name, fallback) {
        const args = Qt.application.arguments;
        const at = args.indexOf(name);
        return at >= 0 && at + 1 < args.length ? args[at + 1] : fallback;
    }

    readonly property var variantNames: ["Dense list", "Hero verdict", "Hero verdict, 2/3 hero", "Hero verdict, compact band", "Terminal table", "Flush flyout"]
    readonly property var scenarioKeys: ["secure", "http", "cert", "start"]
    property int variant: Math.max(0, Math.min(5, Number(argument("--site-variant", "1")) - 1))
    property string scenario: argument("--site-scenario", "secure")
    property string detail: ""
    property int cursor: 0
    property real anchorX: 16
    property real anchorY: 90
    readonly property real flushX: addressItem ? mapFromItem(addressItem, addressItem.width + 16, 0).x : 320

    // ---- fake data ----------------------------------------------------------
    readonly property var blockedItems: [{
            "k": "doubleclick.example/pixel",
            "v": "advertising"
        }, {
            "k": "metrics.shop.example/collect",
            "v": "analytics",
            "sub": "through trk-7f3.cdn-edge.example"
        }, {
            "k": "fonts.cdn-edge.example/loader.js",
            "v": "fingerprinting"
        }, {
            "k": "stats.social.example/beacon",
            "v": "social"
        }, {
            "k": "ads.newsnet.example/slot/4",
            "v": "advertising"
        }]
    property var permissionValues: ({
                                        "camera": "allow",
                                        "notifications": "block"
                                    })

    readonly property var scenarios: ({
                                          "secure": {
                                              "host": "www.example.org",
                                              "url": "https://www.example.org/articles/42",
                                              "kind": "secure",
                                              "verdict": "Connection is secure",
                                              "sub": "Encrypted with TLS 1.3",
                                              "glyph": "lock",
                                              "issuer": "Let's Encrypt R11",
                                              "blocked": 14,
                                              "cookies": "9 cookies · 2.4 MB",
                                              "third": "2 allowed",
                                              "perms": true
                                          },
                                          "http": {
                                              "host": "old.example.net",
                                              "url": "http://old.example.net/index.html",
                                              "kind": "insecure",
                                              "verdict": "Not secure",
                                              "sub": "Anything you send here can be read on the way",
                                              "glyph": "lock_open",
                                              "issuer": "",
                                              "blocked": 3,
                                              "cookies": "2 cookies · 18 kB",
                                              "third": "None",
                                              "perms": false
                                          },
                                          "cert": {
                                              "host": "expired.example.com",
                                              "url": "https://expired.example.com/",
                                              "kind": "error",
                                              "verdict": "The certificate expired on 12 Sep 2026",
                                              "sub": "Waived for this session",
                                              "glyph": "warning",
                                              "issuer": "Example Internal CA",
                                              "blocked": 0,
                                              "cookies": "None",
                                              "third": "None",
                                              "perms": false
                                          },
                                          "start": {
                                              "host": "Start page",
                                              "url": "omaweb://start",
                                              "kind": "start",
                                              "verdict": "Omaweb's own page",
                                              "sub": "Nothing was loaded from the network",
                                              "glyph": "home",
                                              "issuer": "",
                                              "blocked": 0,
                                              "cookies": "None",
                                              "third": "None",
                                              "perms": false
                                          }
                                      })
    readonly property var site: scenarios[scenario] || scenarios["secure"]
    readonly property bool tls: site.kind === "secure" || site.kind === "error"
    readonly property color verdictColor: site.kind === "secure" ? colors.accent : site.kind
                                                                    === "error" ? colors.urgent : site.kind
                                                                    === "insecure" ? "#d99a2b" : colors.mutedText

    // The rows that can open. `drill` is whether there is a detail behind it.
    readonly property var rows: {
        const list = [];
        if (tls)
            list.push({
                          "key": "certificate",
                          "label": "Certificate",
                          "value": site.issuer,
                          "glyph": "badge",
                          "drill": true
                      });
        list.push({
                      "key": "blocked",
                      "label": "Blocked",
                      "value": site.blocked > 0 ? site.blocked + " requests" : "None",
                      "glyph": "shield",
                      "drill": site.blocked > 0
                  });
        list.push({
                      "key": "cookies",
                      "label": "Cookies and site data",
                      "value": site.cookies,
                      "glyph": "cookie",
                      "drill": site.cookies !== "None"
                  });
        list.push({
                      "key": "third",
                      "label": "Third parties",
                      "value": site.third,
                      "glyph": "hub",
                      "drill": site.third !== "None"
                  });
        return list;
    }
    readonly property var permissionRows: site.perms ? [{
                "key": "camera",
                "label": "Camera",
                "glyph": "videocam",
                "value": permissionValues["camera"]
            }, {
                "key": "notifications",
                "label": "Notifications",
                "glyph": "notifications",
                "value": permissionValues["notifications"]
            }] : []
    readonly property var permissionOptions: [{
            "value": "allow",
            "label": "Allow"
        }, {
            "value": "ask",
            "label": "Ask"
        }, {
            "value": "block",
            "label": "Block"
        }]

    function setPermission(key, value) {
        const copy = Object.assign({}, permissionValues);
        copy[key] = value;
        permissionValues = copy;
    }

    function detailTitle(key) {
        return {
            "certificate": "Certificate",
            "blocked": "Blocked requests",
            "cookies": "Cookies and site data",
            "third": "Third parties"
        }[key] || "";
    }

    function detailLines(key) {
        if (key === "certificate")
            return [{
                        "k": "Issued to",
                        "v": site.host
                    }, {
                        "k": "Issued by",
                        "v": site.issuer
                    }, {
                        "k": "Valid from",
                        "v": site.kind === "error" ? "12 Jun 2026" : "3 Sep 2026"
                    }, {
                        "k": "Valid until",
                        "v": site.kind === "error" ? "12 Sep 2026 (expired)" : "2 Dec 2026"
                    }, {
                        "k": "Key",
                        "v": "ECDSA P-256 · SHA-256 with ECDSA"
                    }, {
                        "k": "Fingerprint",
                        "v": "9F:2A:61:C4:0B:7E:D3:58:…:A1:44"
                    }, {
                        "k": "Chain",
                        "v": site.host + " → " + site.issuer + " → ISRG Root X1"
                    }];
        if (key === "blocked")
            return blockedItems.slice(0, Math.min(5, site.blocked));
        if (key === "cookies")
            return [{
                        "k": "Cookies",
                        "v": site.kind === "http" ? "2 · 3 kB" : "9 · 41 kB"
                    }, {
                        "k": "Local storage",
                        "v": site.kind === "http" ? "—" : "1.4 MB"
                    }, {
                        "k": "Cache",
                        "v": site.kind === "http" ? "15 kB" : "1.0 MB"
                    }, {
                        "k": "Held for this Space",
                        "v": site.cookies
                    }];
        if (key === "third")
            return [{
                        "k": "login.example-id.net",
                        "v": "allowed for authentication"
                    }, {
                        "k": "pay.example-psp.com",
                        "v": "allowed for payment"
                    }];
        return [];
    }

    function openRow(index) {
        const row = rows[index];
        if (row && row.drill)
            detail = row.key;
    }

    function back() {
        if (detail.length > 0) {
            detail = "";
            return true;
        }
        return false;
    }

    function cycleVariant(by) {
        variant = (variant + by + 6) % 6;
    }

    function cycleScenario(by) {
        const at = scenarioKeys.indexOf(scenario);
        scenario = scenarioKeys[(at + by + 4) % 4];
        detail = "";
        cursor = 0;
    }

    onOpenChanged: {
        confirming = "";
        if (!open)
            return;
        detail = argument("--site-detail", "");
        cursor = 0;
        if (addressItem) {
            const point = mapFromItem(addressItem, 0, addressItem.height + 8);
            anchorX = point.x;
            anchorY = point.y;
        }
    }

    // ---- keys --------------------------------------------------------------
    Shortcut {
        sequence: "Escape"
        enabled: root.open
        onActivated: if (!root.back())
                         root.closeRequested()
    }
    Shortcut {
        sequence: "Return"
        enabled: root.open && root.detail === ""
        onActivated: root.openRow(root.cursor)
    }
    Shortcut {
        sequence: "Down"
        enabled: root.open && root.detail === ""
        onActivated: root.cursor = Math.min(root.rows.length - 1, root.cursor + 1)
    }
    Shortcut {
        sequence: "Up"
        enabled: root.open && root.detail === ""
        onActivated: root.cursor = Math.max(0, root.cursor - 1)
    }
    Shortcut {
        sequence: "Left"
        enabled: root.open
        onActivated: root.cycleVariant(-1)
    }
    Shortcut {
        sequence: "Right"
        enabled: root.open
        onActivated: root.cycleVariant(1)
    }

    // ---- shared pieces -----------------------------------------------------
    component T: Text {
        color: root.colors.text
        font.family: Style.font.family
        font.pixelSize: Style.font.body
        elide: Text.ElideRight
    }
    component G: Text {
        property real size: Style.font.iconLarge
        color: root.colors.mutedText
        font.family: root.iconFontFamily
        font.pixelSize: size
    }
    component Hit: MouseArea {
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
    }
    // The destructive pair every variant ends on: each confirms first.
    property string confirming: ""
    onDetailChanged: confirming = ""
    function confirmText(action) {
        return action === "data" ? "Clear all data this site holds in this Space?" : "Forget every permission decided for this site?";
    }
    function confirmAction(action) {
        if (confirming === action) {
            confirming = "";
            return;
        }
        confirming = action;
    }

    // Click outside the card closes it. The real card does the same.
    MouseArea {
        anchors.fill: parent
        visible: root.open
        onClicked: root.closeRequested()
    }

    // ---- A: dense list -----------------------------------------------------
    component VariantA: Rectangle {
        id: a
        property real p: root.detail !== "" ? 1 : 0
        Behavior on p {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
        width: 400
        height: Math.round(listPane.implicitHeight * (1 - p) + detailPane.implicitHeight * p)
        radius: 2
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.accent
        clip: true

        Column {
            id: listPane
            x: -a.p * a.width
            width: a.width
            padding: 12
            spacing: 0
            Row {
                spacing: 10
                width: a.width - 24
                SiteTileMock {
                    host: root.site.host
                    anchors.verticalCenter: parent.verticalCenter
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 42
                    T {
                        text: root.site.host
                        width: parent.width
                        font.pixelSize: Style.font.title
                    }
                    Row {
                        spacing: 5
                        G {
                            text: root.site.glyph
                            color: root.verdictColor
                            size: Style.font.icon
                        }
                        T {
                            text: root.site.verdict
                            color: root.verdictColor
                            width: 330 - 22
                        }
                    }
                }
            }
            Item {
                width: 1
                height: 10
            }
            Repeater {
                model: root.rows
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: a.width - 24
                    height: 30
                    color: ma.containsMouse || root.cursor === index ? Qt.rgba(1, 1, 1, 0.06) : "transparent"
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: root.colors.separator
                        opacity: 0.5
                    }
                    T {
                        x: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: root.colors.mutedText
                    }
                    T {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: chev.left
                        anchors.rightMargin: 6
                        width: 200
                        horizontalAlignment: Text.AlignRight
                        text: modelData.value
                    }
                    G {
                        id: chev
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.drill ? "chevron_right" : ""
                    }
                    Hit {
                        id: ma
                        anchors.fill: parent
                        onClicked: root.openRow(index)
                    }
                }
            }
            Item {
                width: 1
                height: root.permissionRows.length > 0 ? 8 : 0
            }
            Repeater {
                model: root.permissionRows
                delegate: Item {
                    required property var modelData
                    width: a.width - 24
                    height: 32
                    T {
                        x: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: root.colors.mutedText
                    }
                    SettingDropdown {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 110
                        colors: root.colors
                        options: root.permissionOptions
                        value: modelData.value
                        onChanged: function (v) {
                            root.setPermission(modelData.key, v);
                        }
                    }
                }
            }
            Item {
                width: 1
                height: 8
            }
            Row {
                spacing: 16
                x: 4
                T {
                    text: root.confirming === "data" ? "Really clear?" : "Clear site data"
                    color: root.confirming === "data" ? root.colors.urgent : root.colors.accent
                    Hit {
                        anchors.fill: parent
                        onClicked: root.confirmAction("data")
                    }
                }
                T {
                    text: root.confirming === "perms" ? "Really reset?" : "Reset permissions"
                    color: root.confirming === "perms" ? root.colors.urgent : root.colors.accent
                    Hit {
                        anchors.fill: parent
                        onClicked: root.confirmAction("perms")
                    }
                }
            }
        }

        Column {
            id: detailPane
            x: (1 - a.p) * a.width
            width: a.width
            padding: 12
            spacing: 6
            Row {
                spacing: 4
                G {
                    text: "chevron_left"
                    color: root.colors.accent
                }
                T {
                    text: root.site.host
                    color: root.colors.accent
                    Hit {
                        anchors.fill: parent
                        onClicked: root.back()
                    }
                }
            }
            T {
                text: root.detailTitle(root.detail)
                font.pixelSize: Style.font.title
            }
            Repeater {
                model: root.detail !== "" ? root.detailLines(root.detail) : []
                delegate: Column {
                    required property var modelData
                    width: a.width - 24
                    Row {
                        spacing: 8
                        T {
                            text: modelData.k
                            width: 170
                            color: root.colors.mutedText
                        }
                        T {
                            text: modelData.v
                            width: a.width - 24 - 178
                            wrapMode: Text.WrapAnywhere
                            elide: Text.ElideNone
                        }
                    }
                    T {
                        visible: !!modelData.sub
                        text: "  " + (modelData.sub || "")
                        color: root.colors.mutedText
                        font.pixelSize: Style.font.caption
                    }
                }
            }
        }
    }

    // ---- B: hero verdict ---------------------------------------------------
    // B at three hero sizes: 0 full, 1 about two thirds, 2 a compact band.
    component VariantB: Rectangle {
        id: b
        property int mode: 0
        readonly property real heroHeight: [112, 75, 56][mode]
        readonly property real heroShrink: [52, 25, 14][mode]
        readonly property real badgeSize: [64, 44, 26][mode]
        property real p: root.detail !== "" ? 1 : 0
        Behavior on p {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
        width: 400
        height: Math.round(hero.implicitHeight + (b.mode === 2 ? 0 : 24) + (body.implicitHeight * (1 - p) + detailBody.implicitHeight * p))
        radius: 10
        color: b.mode === 2 ? root.colors.overlayOpaque : root.colors.overlay
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.14)
        clip: true

        Column {
            id: hero
            width: b.width
            Rectangle {
                width: parent.width
                height: b.heroHeight - b.p * b.heroShrink
                color: Qt.rgba(root.verdictColor.r, root.verdictColor.g, root.verdictColor.b, 0.14)
                Row {
                    visible: b.mode < 2
                    anchors.fill: parent
                    anchors.margins: b.mode === 0 ? 16 : 12
                    spacing: 14
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: b.badgeSize - b.p * (b.mode === 0 ? 26 : 10)
                        height: width
                        radius: width / 2
                        color: root.verdictColor
                        G {
                            anchors.centerIn: parent
                            text: root.site.glyph
                            color: root.colors.overlay
                            size: parent.width * 0.55
                        }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 90
                        spacing: 3
                        T {
                            text: b.p > 0.5 ? root.detailTitle(root.detail) : root.site.verdict
                            width: parent.width
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                            font.pixelSize: (b.mode === 0 ? 19 : 16) - b.p * 4
                            font.bold: true
                        }
                        T {
                            text: b.p > 0.5 ? "" : root.site.sub
                            visible: b.p < 0.5
                            width: parent.width
                            color: root.colors.mutedText
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                        }
                        Rectangle {
                            visible: b.p > 0.5
                            color: Qt.rgba(1, 1, 1, 0.1)
                            width: backLabel.width + 20
                            height: 24
                            radius: 12
                            T {
                                id: backLabel
                                anchors.centerIn: parent
                                text: "‹ " + root.site.host
                            }
                            Hit {
                                anchors.fill: parent
                                onClicked: root.back()
                            }
                        }
                    }
                }
            }
        }

        // The compact band: badge and verdict on one line, the TLS line under.
        Item {
            visible: b.mode === 2
            x: 12
            y: 0
            width: b.width - 24
            height: b.heroHeight - b.p * b.heroShrink
            Row {
                id: bandRow
                y: 8
                spacing: 8
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: b.badgeSize
                    height: width
                    radius: width / 2
                    color: root.verdictColor
                    G {
                        anchors.centerIn: parent
                        text: root.site.glyph
                        color: root.colors.overlay
                        size: parent.width * 0.62
                    }
                }
                T {
                    anchors.verticalCenter: parent.verticalCenter
                    text: b.p > 0.5 ? root.detailTitle(root.detail) : root.site.verdict
                    width: b.width - 24 - b.badgeSize - 8 - (b.p > 0.5 ? 110 : 0)
                    font.pixelSize: 15
                    font.bold: true
                }
            }
            T {
                visible: b.p < 0.5
                x: b.badgeSize + 8
                y: 8 + b.badgeSize / 2 + 9
                text: root.site.sub
                width: parent.width - x
                color: root.colors.mutedText
                font.pixelSize: Style.font.bodySmall
            }
            Rectangle {
                visible: b.p > 0.5
                anchors.right: parent.right
                y: 8
                color: Qt.rgba(1, 1, 1, 0.1)
                width: bandBack.width + 20
                height: 24
                radius: 12
                T {
                    id: bandBack
                    anchors.centerIn: parent
                    text: "‹ " + root.site.host
                }
                Hit {
                    anchors.fill: parent
                    onClicked: root.back()
                }
            }
        }

        Item {
            y: hero.implicitHeight
            width: b.width
            height: parent.height - hero.implicitHeight
            clip: true
            Column {
                id: body
                x: -b.p * b.width
                width: b.width
                padding: 12
                spacing: 10
                T {
                    text: root.site.host
                    width: b.width - 24
                    color: root.colors.mutedText
                }
                Grid {
                    columns: 2
                    spacing: 8
                    Repeater {
                        model: root.rows
                        delegate: Rectangle {
                            required property int index
                            required property var modelData
                            width: (b.width - 24 - 8) / 2
                            height: 66
                            radius: 8
                            color: ma.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                            border.width: root.cursor === index ? 1 : 0
                            border.color: root.colors.accent
                            G {
                                x: 10
                                y: 8
                                text: modelData.glyph
                                size: Style.font.icon
                            }
                            T {
                                x: 10
                                y: 30
                                width: parent.width - 20
                                text: modelData.value
                                font.pixelSize: Style.font.subtitle
                                font.bold: true
                            }
                            T {
                                x: 10
                                y: 48
                                width: parent.width - 20
                                text: modelData.label
                                color: root.colors.mutedText
                                font.pixelSize: Style.font.caption
                            }
                            G {
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                y: 8
                                text: modelData.drill ? "chevron_right" : ""
                            }
                            Hit {
                                id: ma
                                anchors.fill: parent
                                onClicked: root.openRow(index)
                            }
                        }
                    }
                }
                Flow {
                    visible: root.permissionRows.length > 0
                    width: b.width - 24
                    spacing: 8
                    Repeater {
                        model: root.permissionRows
                        delegate: Rectangle {
                            required property var modelData
                            width: (b.width - 24 - 8) / 2
                            height: 40
                            radius: 8
                            color: Qt.rgba(1, 1, 1, 0.05)
                            G {
                                x: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.glyph
                                size: Style.font.icon
                            }
                            SettingDropdown {
                                visible: b.mode < 2
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 108
                                colors: root.colors
                                options: root.permissionOptions
                                value: modelData.value
                                onChanged: function (v) {
                                    root.setPermission(modelData.key, v);
                                }
                            }
                            PermDrop {
                                visible: b.mode === 2
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                value: modelData.value
                                onPicked: function (v) {
                                    root.setPermission(modelData.key, v);
                                }
                            }
                        }
                    }
                }
                Row {
                    spacing: 8
                    Repeater {
                        model: [{
                                "id": "data",
                                "t": "Clear site data",
                                "c": "Clear? tap again"
                            }, {
                                "id": "perms",
                                "t": "Reset permissions",
                                "c": "Reset? tap again"
                            }]
                        delegate: Rectangle {
                            required property var modelData
                            width: (b.width - 24 - 8) / 2
                            height: 30
                            radius: 15
                            color: root.confirming === modelData.id ? root.colors.urgent : "transparent"
                            border.width: 1
                            border.color: root.confirming === modelData.id ? root.colors.urgent : Qt.rgba(1, 1, 1, 0.25)
                            T {
                                anchors.centerIn: parent
                                text: root.confirming === modelData.id ? modelData.c : modelData.t
                            }
                            Hit {
                                anchors.fill: parent
                                onClicked: root.confirmAction(modelData.id)
                            }
                        }
                    }
                }
            }
            Column {
                id: detailBody
                x: (1 - b.p) * b.width
                width: b.width
                padding: 12
                spacing: 8
                Repeater {
                    model: root.detail !== "" ? root.detailLines(root.detail) : []
                    delegate: Rectangle {
                        required property var modelData
                        width: b.width - 24
                        height: lineCol.implicitHeight + 16
                        radius: 8
                        color: Qt.rgba(1, 1, 1, 0.05)
                        Column {
                            id: lineCol
                            x: 10
                            y: 8
                            width: parent.width - 20
                            T {
                                text: modelData.k
                                width: parent.width
                                color: root.colors.mutedText
                                font.pixelSize: Style.font.caption
                            }
                            T {
                                text: modelData.v + (modelData.sub ? " · " + modelData.sub : "")
                                width: parent.width
                                wrapMode: Text.WrapAnywhere
                                elide: Text.ElideNone
                            }
                        }
                    }
                }
            }
        }
    }

    // ---- C: terminal table -------------------------------------------------
    component VariantC: Rectangle {
        id: c
        readonly property string mono: "Menlo"
        width: 400
        height: col.implicitHeight + 24
        radius: 0
        color: root.colors.overlay
        border.width: 1
        border.color: root.colors.text
        clip: true
        Column {
            id: col
            x: 12
            y: 12
            width: c.width - 24
            spacing: 2
            M {
                text: root.detail === "" ? root.site.host : "‹ " + root.site.host + " / " + root.detailTitle(root.detail).toLowerCase()
                color: root.detail === "" ? root.colors.text : root.colors.accent
                width: parent.width
                font.bold: true
                Hit {
                    anchors.fill: parent
                    onClicked: root.back()
                    enabled: root.detail !== ""
                }
            }
            M {
                text: "────────────────────────────────────────────"
                color: root.colors.separator
                elide: Text.ElideNone
            }
            M {
                visible: root.detail === ""
                text: (root.site.kind === "secure" ? "[ OK ] " : root.site.kind === "error" ? "[FAIL] " : root.site.kind === "insecure" ? "[WARN] " : "[ -- ] ") + root.site.verdict
                color: root.verdictColor
                width: parent.width
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
            M {
                visible: root.detail === ""
                text: "       " + root.site.sub
                color: root.colors.mutedText
                width: parent.width
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
            Item {
                visible: root.detail === ""
                width: 1
                height: 6
            }
            Repeater {
                model: root.detail === "" ? root.rows : []
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    width: col.width
                    height: 22
                    color: root.cursor === index || ma.containsMouse ? root.colors.text : "transparent"
                    M {
                        x: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label.toLowerCase().padEnd(24, " ")
                        color: parent.color == root.colors.text ? root.colors.overlay : root.colors.mutedText
                    }
                    M {
                        x: 4 + 24 * 6.9
                        width: parent.width - x - 20
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.value
                        color: parent.color == root.colors.text ? root.colors.overlay : root.colors.text
                    }
                    M {
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.drill ? "›" : ""
                        color: parent.color == root.colors.text ? root.colors.overlay : root.colors.text
                    }
                    Hit {
                        id: ma
                        anchors.fill: parent
                        onClicked: root.openRow(index)
                    }
                }
            }
            M {
                visible: root.detail === "" && root.permissionRows.length > 0
                text: "── permissions ─────────────────────────────"
                color: root.colors.separator
                elide: Text.ElideNone
            }
            Repeater {
                model: root.detail === "" ? root.permissionRows : []
                delegate: Item {
                    required property var modelData
                    width: col.width
                    height: 28
                    M {
                        x: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label.toLowerCase()
                        color: root.colors.mutedText
                    }
                    SettingDropdown {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 100
                        colors: root.colors
                        options: root.permissionOptions
                        value: modelData.value
                        onChanged: function (v) {
                            root.setPermission(modelData.key, v);
                        }
                    }
                }
            }
            Repeater {
                model: root.detail !== "" ? root.detailLines(root.detail) : []
                delegate: Column {
                    required property var modelData
                    width: col.width
                    M {
                        text: modelData.k.toLowerCase().padEnd(16, " ") + modelData.v
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        elide: Text.ElideNone
                    }
                    M {
                        visible: !!modelData.sub
                        text: "  ↳ " + (modelData.sub || "")
                        color: root.colors.mutedText
                        width: parent.width
                    }
                }
            }
            M {
                text: "────────────────────────────────────────────"
                color: root.colors.separator
                elide: Text.ElideNone
            }
            Row {
                spacing: 12
                M {
                    text: root.confirming === "data" ? "[ sure? y ]" : "[ clear site data ]"
                    color: root.confirming === "data" ? root.colors.urgent : root.colors.accent
                    Hit {
                        anchors.fill: parent
                        onClicked: root.confirmAction("data")
                    }
                }
                M {
                    text: root.confirming === "perms" ? "[ sure? y ]" : "[ reset permissions ]"
                    color: root.confirming === "perms" ? root.colors.urgent : root.colors.accent
                    Hit {
                        anchors.fill: parent
                        onClicked: root.confirmAction("perms")
                    }
                }
            }
        }
    }

    // ---- D: flush flyout with a verdict band -------------------------------
    component VariantD: Rectangle {
        id: d
        property real p: root.detail !== "" ? 1 : 0
        Behavior on p {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }
        width: 400
        height: Math.max(420, band.height + sections.implicitHeight * (1 - p) + detailCol.implicitHeight * p + 24)
        color: root.colors.overlay
        // Flush with the sidebar's edge: no floating shadow, only a hairline.
        Rectangle {
            width: 1
            height: parent.height
            color: root.colors.separator
            anchors.right: parent.right
        }
        Rectangle {
            id: band
            width: parent.width
            height: 76 - d.p * 24
            color: root.verdictColor
            Row {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10
                G {
                    anchors.verticalCenter: parent.verticalCenter
                    text: d.p > 0.5 ? "chevron_left" : root.site.glyph
                    color: root.colors.overlay
                    size: 26
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 40
                    T {
                        text: d.p > 0.5 ? root.site.host + " / " + root.detailTitle(root.detail) : root.site.verdict
                        color: root.colors.overlay
                        font.bold: true
                        font.pixelSize: Style.font.title
                        width: parent.width
                    }
                    T {
                        visible: d.p < 0.5
                        text: root.site.host + " · " + root.site.sub
                        color: root.colors.overlay
                        opacity: 0.85
                        width: parent.width
                        font.pixelSize: Style.font.bodySmall
                    }
                }
            }
            Hit {
                anchors.fill: parent
                enabled: d.p > 0.5
                onClicked: root.back()
            }
        }
        Item {
            y: band.height
            width: d.width
            height: d.height - band.height
            clip: true
            Column {
                id: sections
                x: -d.p * d.width
                width: d.width
                padding: 16
                spacing: 4
                T {
                    text: "ABOUT THIS PAGE"
                    color: root.colors.mutedText
                    font.pixelSize: Style.font.caption
                    font.letterSpacing: 1
                }
                Repeater {
                    model: root.rows
                    delegate: Item {
                        required property int index
                        required property var modelData
                        width: d.width - 32
                        height: 34
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: -2
                            radius: 4
                            color: ma.containsMouse || root.cursor === index ? Qt.rgba(1, 1, 1, 0.07) : "transparent"
                        }
                        G {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.glyph
                            size: Style.font.icon
                        }
                        Column {
                            x: 30
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 60
                            T {
                                text: modelData.value
                                width: parent.width
                            }
                            T {
                                text: modelData.label
                                color: root.colors.mutedText
                                font.pixelSize: Style.font.caption
                                width: parent.width
                            }
                        }
                        G {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.drill ? "chevron_right" : ""
                        }
                        Hit {
                            id: ma
                            anchors.fill: parent
                            onClicked: root.openRow(index)
                        }
                    }
                }
                Item {
                    visible: root.permissionRows.length > 0
                    width: 1
                    height: 10
                }
                T {
                    visible: root.permissionRows.length > 0
                    text: "PERMISSIONS"
                    color: root.colors.mutedText
                    font.pixelSize: Style.font.caption
                    font.letterSpacing: 1
                }
                Repeater {
                    model: root.permissionRows
                    delegate: Item {
                        required property var modelData
                        width: d.width - 32
                        height: 34
                        G {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.glyph
                            size: Style.font.icon
                        }
                        T {
                            x: 30
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                        }
                        SettingDropdown {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 110
                            colors: root.colors
                            options: root.permissionOptions
                            value: modelData.value
                            onChanged: function (v) {
                                root.setPermission(modelData.key, v);
                            }
                        }
                    }
                }
                Item {
                    width: 1
                    height: 12
                }
                Row {
                    spacing: 10
                    Repeater {
                        model: [{
                                "id": "data",
                                "t": "Clear site data"
                            }, {
                                "id": "perms",
                                "t": "Reset permissions"
                            }]
                        delegate: Rectangle {
                            required property var modelData
                            width: (d.width - 32 - 10) / 2
                            height: 28
                            radius: 4
                            color: root.confirming === modelData.id ? root.colors.urgent : Qt.rgba(1, 1, 1, 0.07)
                            T {
                                anchors.centerIn: parent
                                text: root.confirming === modelData.id ? "Confirm" : modelData.t
                                color: root.confirming === modelData.id ? root.colors.overlay : root.colors.text
                            }
                            Hit {
                                anchors.fill: parent
                                onClicked: root.confirmAction(modelData.id)
                            }
                        }
                    }
                }
            }
            Column {
                id: detailCol
                x: (1 - d.p) * d.width
                width: d.width
                padding: 16
                spacing: 8
                Repeater {
                    model: root.detail !== "" ? root.detailLines(root.detail) : []
                    delegate: Column {
                        required property var modelData
                        width: d.width - 32
                        spacing: 1
                        Rectangle {
                            width: parent.width
                            height: 1
                            color: root.colors.separator
                            opacity: 0.5
                        }
                        T {
                            text: modelData.k
                            color: root.colors.mutedText
                            font.pixelSize: Style.font.caption
                            width: parent.width
                        }
                        T {
                            text: modelData.v + (modelData.sub ? "  ·  " + modelData.sub : "")
                            width: parent.width
                            wrapMode: Text.WrapAnywhere
                            elide: Text.ElideNone
                        }
                    }
                }
            }
        }
    }

    component M: Text {
        color: root.colors.text
        font.family: "Menlo"
        font.pixelSize: Style.font.bodySmall
        elide: Text.ElideRight
    }
    // Allow / Ask / Block with the icon font's own arrow. The kit's dropdown
    // draws its arrow from a Nerd Font glyph this Mac does not have.
    component PermDrop: Rectangle {
        id: drop
        property string value: "ask"
        signal picked(string v)
        function labelOf(v) {
            for (const o of root.permissionOptions)
                if (o.value === v)
                    return o.label;
            return v;
        }
        width: 108
        height: 26
        radius: 4
        color: Qt.rgba(1, 1, 1, 0.06)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.25)
        T {
            x: 8
            anchors.verticalCenter: parent.verticalCenter
            text: drop.labelOf(drop.value)
        }
        G {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            text: "expand_more"
        }
        Hit {
            anchors.fill: parent
            onClicked: menu.opened ? menu.close() : menu.open()
        }
        Popup {
            id: menu
            y: drop.height + 2
            width: drop.width
            padding: 2
            background: Rectangle {
                color: root.colors.overlayOpaque
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.25)
                radius: 4
            }
            contentItem: Column {
                Repeater {
                    model: root.permissionOptions
                    delegate: Rectangle {
                        required property var modelData
                        width: drop.width - 4
                        height: 26
                        color: pick.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                        T {
                            x: 6
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                        }
                        Hit {
                            id: pick
                            anchors.fill: parent
                            onClicked: {
                                drop.picked(modelData.value);
                                menu.close();
                            }
                        }
                    }
                }
            }
        }
    }
    component SiteTileMock: Rectangle {
        property string host: ""
        width: 32
        height: 32
        radius: 6
        color: Qt.rgba(1, 1, 1, 0.1)
        T {
            anchors.centerIn: parent
            text: host.substring(0, 2).toUpperCase()
            color: root.colors.accent
            font.bold: true
        }
    }

    // ---- placement ---------------------------------------------------------
    Item {
        id: slot
        visible: root.open
        x: root.variant === 5 ? root.flushX : root.anchorX
        y: root.variant === 5 ? 0 : root.anchorY
        // Keep the card on the window, as the real one has to.
        MouseArea {
            // Swallow clicks on the card so they do not close it.
            anchors.fill: card
            z: -1
        }
        Item {
            id: card
            width: 400
            height: [va.height, vb.height, vb2.height, vb3.height, vc.height, vd.height][root.variant]
            VariantA {
                id: va
                visible: root.variant === 0
            }
            VariantB {
                id: vb
                visible: root.variant === 1
            }
            VariantB {
                id: vb2
                mode: 1
                visible: root.variant === 2
            }
            VariantB {
                id: vb3
                mode: 2
                visible: root.variant === 3
            }
            VariantC {
                id: vc
                visible: root.variant === 4
            }
            VariantD {
                id: vd
                visible: root.variant === 5
                height: root.height
            }
        }
    }

    // ---- the prototype's own chrome ---------------------------------------
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: Math.max(strip.implicitWidth, banner.implicitWidth) + 28
        height: 56
        radius: 28
        color: "#111111"
        border.width: 1
        border.color: "#f5c542"
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                id: banner
                anchors.horizontalCenter: parent.horizontalCenter
                text: "PROTOTYPE #541 — throwaway · fake data · nothing is saved"
                color: "#f5c542"
                font.family: Style.font.family
                font.pixelSize: 9
                font.bold: true
            }
            Row {
                id: strip
                spacing: 14
                anchors.horizontalCenter: parent.horizontalCenter
                Text {
                    text: "‹"
                    color: "white"
                    font.pixelSize: 18
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onClicked: root.cycleVariant(-1)
                    }
                }
                Text {
                    text: ["A", "B", "B2", "B3", "C", "D"][root.variant] + " — " + root.variantNames[root.variant]
                    color: "white"
                    font.family: Style.font.family
                    font.pixelSize: 13
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: "›"
                    color: "white"
                    font.pixelSize: 18
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onClicked: root.cycleVariant(1)
                    }
                }
                Text {
                    text: "· page: " + root.scenario + " ⟳"
                    color: "#bbbbbb"
                    font.family: Style.font.family
                    font.pixelSize: 12
                    anchors.verticalCenter: parent.verticalCenter
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        onClicked: root.cycleScenario(1)
                    }
                }
            }
        }
    }
}
