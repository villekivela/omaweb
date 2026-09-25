import QtQuick
import QtTest
import Omaweb
import Omaweb as Core
import qs.Commons
import "../../src/ui" as Omaweb

// The settings page's layout, which is derived rather than written down. Every
// margin, gap and measure comes from the kit's spacing scale or from the face
// the text is actually drawn in, so it is asserted on both axes the theme can
// move — the type tokens and the spacing scale — one at a time, so neither can
// stand in for the other. The type size is the axis a pixel count breaks
// silently: a count that is right at one theme font size clips or crowds at
// the next.
TestCase {
    id: testCase
    name: "SettingsPageLayout"
    when: windowShown
    // A TestCase is invisible by default, and this case asks what the page
    // draws: an item under an invisible ancestor reports `visible` false
    // whatever it says itself, so a walk over the tree would skip the section
    // it came to look at.
    visible: true
    width: 1200
    height: 900

    readonly property var colorsFixture: ({
                                              text: "#f3f1fa",
                                              mutedText: "#8d88a3",
                                              accent: "#9b87ff",
                                              privateAccent: "#ffb37a",
                                              urgent: "#ff6f7d",
                                              separator: "#4a4658",
                                              border: "#4a4658",
                                              surface: "#26232f",
                                              sidebar: "#26232fcc",
                                              sheet: "#26232f99",
                                              overlay: "#1f1d27f2",
                                              windowOpaque: "#16151d"
                                          })

    // The content-blocking section only offers the default lists back when
    // there is a blocker to ask, so the stub records the ask rather than
    // standing in for the whole service.
    QtObject {
        id: blockerStub

        property int restoreCount: 0
        property var compilationReport: ({})
        property var subscribed: []

        function restoreDefaultSubscriptions() {
            restoreCount += 1;
        }

        function subscribeKnownList(id) {
            subscribed = subscribed.concat([id]);
        }
    }

    QtObject {
        id: syncControllerStub

        property bool enabled: false
        property bool connecting: true
        property bool awaitingRepositoryCreation: false
        property bool awaitingInstallation: false
        property bool pending: false
        property string provider: "Forge"
        property string login: ""
        property string status: "Waiting for Forge authorization"
        property string errorMessage: ""
        property string userCode: "ABCD-EFGH"
        property url verificationUrl: ""
        property string avatarPath: ""
        property string recoveryKey: ""
        property date lastSuccessfulSync
        property int continueCount: 0
        property var providerText: ({
                                        "name": "Forge",
                                        "connectAction": "Connect Forge",
                                        "authorizationAction": "Open Forge authorization",
                                        "failureTitle": "Forge Sync failed",
                                        "codeCopiedNotice": "Forge code copied",
                                        "codePrompt": "Enter ABCD-EFGH on Forge",
                                        "authorizationNote":
                                        "The Forge login becomes your Sync identity.",
                                        "repositoryTitle": "Create the private repository on Forge",
                                        "repositoryNote":
                                        "Keep the prefilled name and Private visibility.",
                                        "installationTitle": "Grant Omaweb Sync access on Forge",
                                        "installationNote":
                                        "Select the Sync repository and approve.",
                                        "observationNote":
                                        "Forge can still observe repository size."
                                    })

        function continueConnection() {
            continueCount += 1;
            return true;
        }

        signal consentPageRequested(url url)
        signal stateChanged
    }

    QtObject {
        id: syncLauncherStub

        property var controller: syncControllerStub
        property string errorMessage: ""
        property bool configured: false

        function load() {
            return true;
        }
    }

    // The Spaces the page lists, and the one command it submits about them.
    // The stub reorders itself the way the core command does, so a row that
    // has moved is a thing this test reads off the page rather than off the
    // click it sent.
    ListModel {
        id: spacesFixture

        ListElement {
            spaceId: "personal"
            spaceName: "Personal"
            spaceColor: "#9b87ff"
            active: true
        }

        ListElement {
            spaceId: "work"
            spaceName: "Work"
            spaceColor: "#7ad3ff"
            active: false
        }

        ListElement {
            spaceId: "reading"
            spaceName: "Reading"
            spaceColor: "#ffb37a"
            active: false
        }
    }

    QtObject {
        id: browserStub

        property bool privateBrowsing: false
        property url activeUrl: "https://one.example/"
        property string activeSpaceName: "Personal"
        property string downloadDirectory: "/home/reader/Downloads"
        property var spaces: spacesFixture

        function moveSpaceBy(spaceId, offset) {
            for (let row = 0; row < spacesFixture.count; ++row) {
                if (spacesFixture.get(row).spaceId !== spaceId)
                    continue;
                const destination = row + offset;
                if (offset === 0 || destination < 0 || destination >= spacesFixture.count)
                    return false;
                spacesFixture.move(row, destination, 1);
                return true;
            }
            return false;
        }

        function preference(key, fallback) {
            return fallback;
        }

        function setPreference(key, value) {
        }
    }

    SignalSpy {
        id: settingsClosedSpy
        signalName: "closed"
    }

    SignalSpy {
        id: syncCodeCopiedSpy
        signalName: "syncCodeCopied"
    }

    SignalSpy {
        id: syncConnectionFailedSpy
        signalName: "syncConnectionFailed"
    }

    SignalSpy {
        id: syncConsentRequestedSpy
        signalName: "syncConsentRequested"
    }

    // Two of everything the page lists, so a gap between one row and the next
    // is a thing this test can measure rather than a thing it has to imagine.
    readonly property var retainedTabsFixture: [
        {
            tabId: "kept-one",
            title: "First kept tab",
            url: "https://one.example",
            spaceName: "Personal",
            inspected: false,
            running: true,
            residentBytes: 120 * 1024 * 1024
        },
        {
            tabId: "kept-two",
            title: "Second kept tab",
            url: "https://two.example",
            spaceName: "Work",
            inspected: true,
            running: false,
            residentBytes: 0
        }
    ]

    // The page takes the window's download list, which is a model with roles,
    // so the fixture is one too rather than an array of maps.
    readonly property int downloadsFixtureCount: 2

    ListModel {
        id: downloadsFixture

        ListElement {
            recordId: "first"
            runtimeId: ""
            path: "/home/reader/Downloads/first.zip"
            state: "completed"
            error: ""
            receivedBytes: 10
            totalBytes: 10
        }

        ListElement {
            recordId: "second"
            runtimeId: ""
            path: "/home/reader/Downloads/second.zip"
            state: "interrupted"
            error: "the connection was lost"
            receivedBytes: 5
            totalBytes: 100
        }
    }

    Component {
        id: pageComponent

        Omaweb.SettingsPage {
            colors: testCase.colorsFixture
            iconFontFamily: ""
            open: true
            retainedTabs: testCase.retainedTabsFixture
            downloads: downloadsFixture
            width: 1200
            height: 900
        }
    }

    // Both axes the theme can move the page along, shared with the other pages
    // that ask the same question of their own layouts.
    ThemeAxis {
        id: theme
    }

    property var livePage: null

    function initTestCase() {
        theme.remember();
    }

    // A failing verify() throws, so nothing after it in the test body runs. The
    // page and the singleton it moved are put back here instead, where one
    // test's failure cannot leave the next one reading a shell it did not set.
    function cleanup() {
        settingsClosedSpy.target = null;
        settingsClosedSpy.clear();
        syncCodeCopiedSpy.target = null;
        syncCodeCopiedSpy.clear();
        syncConnectionFailedSpy.target = null;
        syncConnectionFailedSpy.clear();
        syncConsentRequestedSpy.target = null;
        syncConsentRequestedSpy.clear();
        syncControllerStub.errorMessage = "";
        syncControllerStub.awaitingRepositoryCreation = false;
        syncControllerStub.continueCount = 0;
        syncControllerStub.enabled = false;
        syncControllerStub.login = "";
        syncControllerStub.status = "Waiting for Forge authorization";
        browserStub.privateBrowsing = false;
        resetSpacesFixture();
        theme.restore();
        fontSettings.resetInterfaceFontSize();
        if (livePage !== null) {
            livePage.destroy();
            livePage = null;
        }
    }

    function makePage() {
        livePage = pageComponent.createObject(testCase);
        verify(livePage !== null);
        return livePage;
    }

    // The page reads the Spaces off the browser it is given rather than a
    // fixture of its own, so a test that reorders them puts the order back.
    function resetSpacesFixture() {
        spacesFixture.clear();
        spacesFixture.append({
                                 spaceId: "personal",
                                 spaceName: "Personal",
                                 spaceColor: "#9b87ff",
                                 active: true
                             });
        spacesFixture.append({
                                 spaceId: "work",
                                 spaceName: "Work",
                                 spaceColor: "#7ad3ff",
                                 active: false
                             });
        spacesFixture.append({
                                 spaceId: "reading",
                                 spaceName: "Reading",
                                 spaceColor: "#ffb37a",
                                 active: false
                             });
    }

    // The page with a browser behind it, opened on the Spaces section.
    function makeSpacesPage() {
        const page = makePage();
        page.browser = browserStub;
        page.section = page.sections.indexOf("spaces");
        return page;
    }

    // A button the positioner has finished placing. One read before its row is
    // laid out sits on top of its neighbours, so a press meant for it lands
    // elsewhere. A width alone is not enough: the Flow gives every button its
    // width before it moves any of them off the left edge, so this waits for a
    // place that has stopped changing.
    function settleAction(action) {
        verify(action !== null);
        let previous = -1;
        let steady = 0;
        tryVerify(function () {
            const placed = action.mapToItem(testCase, 0, 0).x;
            steady = action.width > 0 && placed === previous ? steady + 1 : 0;
            previous = placed;
            return steady >= 2;
        });
        return action;
    }

    // The Spaces as the page draws them, top to bottom, so the order under
    // test is the one the reader sees rather than the one the model holds.
    function spaceOrder(page) {
        const rows = [];
        for (const spaceId of ["personal", "work", "reading"]) {
            const row = findChild(page, "settingsSpace-" + spaceId);
            verify(row !== null);
            rows.push(row);
        }
        rows.sort(function (left, right) {
            return left.mapToItem(page, 0, 0).y - right.mapToItem(page, 0, 0).y;
        });
        return rows.map(function (row) {
            return row.title;
        });
    }

    // The room between where one item stops being drawn and the next starts,
    // in the page's own coordinates, so a gap is read off what is drawn rather
    // than off the container that spaced it.
    function gapBetween(page, above, below) {
        return Math.round(below.mapToItem(page, 0, 0).y - (above.mapToItem(page, 0, 0).y
                                                           + above.height));
    }

    function fieldWithPlaceholder(item, placeholder) {
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index];
            if (child.placeholderText === placeholder)
                return child;
            const deeper = fieldWithPlaceholder(child, placeholder);
            if (deeper !== null)
                return deeper;
        }
        return null;
    }

    // The order Spaces are listed in is the reader's, and Settings is where
    // they set it. A move is one step, so the row at either end has nowhere to
    // go that way and says so before the click rather than after it.
    function test_aSpaceMovesOneStepAndTheEndsSayTheyCannot() {
        const page = makeSpacesPage();
        compare(spaceOrder(page), ["Personal", "Work", "Reading"]);

        const firstUp = settleAction(findChild(page, "moveSpaceUp-personal"));
        const lastDown = settleAction(findChild(page, "moveSpaceDown-reading"));
        verify(!firstUp.enabled);
        verify(!lastDown.enabled);
        verify(settleAction(findChild(page, "moveSpaceDown-personal")).enabled);
        verify(settleAction(findChild(page, "moveSpaceUp-reading")).enabled);

        const readingUp = settleAction(findChild(page, "moveSpaceUp-reading"));
        mouseClick(readingUp, readingUp.width / 2, readingUp.height / 2);
        tryVerify(function () {
            return spaceOrder(page)[1] === "Reading";
        });
        compare(spaceOrder(page), ["Personal", "Reading", "Work"]);

        // The ends are where the moved rows now are, so the disabled edge
        // followed the Space rather than staying with the row it started in.
        tryVerify(function () {
            return !findChild(page, "moveSpaceDown-work").enabled;
        });
        verify(findChild(page, "moveSpaceUp-work").enabled);
        verify(findChild(page, "moveSpaceDown-reading").enabled);

        const workUp = settleAction(findChild(page, "moveSpaceUp-work"));
        mouseClick(workUp, workUp.width / 2, workUp.height / 2);
        tryVerify(function () {
            return spaceOrder(page)[1] === "Work";
        });
        compare(spaceOrder(page), ["Personal", "Work", "Reading"]);
    }

    // Four answers where a row carried two. The row keeps the pane's measure:
    // the answers wrap within their own share of it rather than crowding the
    // Space's name out or running off the edge.
    function test_theArrowsEndTheSpaceRowWithoutCrowdingItsName() {
        const page = makeSpacesPage();
        const row = findChild(page, "settingsSpace-work");
        const up = settleAction(findChild(page, "moveSpaceUp-work"));
        const down = findChild(page, "moveSpaceDown-work");
        const actions = up.parent;
        const leftEdge = function (item) {
            return item.mapToItem(row, 0, 0).x;
        };

        verify(leftEdge(findChild(page, "renameSpace-work")) < leftEdge(up));
        verify(leftEdge(findChild(page, "deleteSpace-work")) < leftEdge(up));
        verify(leftEdge(up) < leftEdge(down));
        compare(Math.round(leftEdge(down) + down.width), row.width);

        verify(actions.width <= row.width * 0.7);
        verify(leftEdge(actions) > 0);
        compare(row.height, Math.max(row.implicitHeight, actions.implicitHeight
                                     + row.verticalPadding * 2));
    }

    // Both arrows are keyboard answers, and each says which Space it moves and
    // which way: an arrow draws a direction and names nothing at all, so the
    // name a screen reader reads has to carry the Space.
    function test_movingASpaceIsAKeyboardAnswerThatNamesIt() {
        const page = makeSpacesPage();
        const workUp = settleAction(findChild(page, "moveSpaceUp-work"));
        const workDown = settleAction(findChild(page, "moveSpaceDown-work"));
        compare(workUp.Accessible.name, "Move Work up");
        compare(workDown.Accessible.name, "Move Work down");
        verify(workUp.activeFocusOnTab);
        verify(workDown.activeFocusOnTab);

        workUp.forceActiveFocus();
        verify(workUp.activeFocus);
        keyClick(Qt.Key_Space);
        tryVerify(function () {
            return spaceOrder(page)[0] === "Work";
        });
        compare(spaceOrder(page), ["Work", "Personal", "Reading"]);
    }

    // Space management belongs to a regular window, so a Private window is
    // offered none of it, the new answers included.
    function test_aPrivateWindowIsOfferedNoSpaceActions() {
        browserStub.privateBrowsing = true;
        const page = makeSpacesPage();
        for (const spaceId of ["personal", "work", "reading"]) {
            compare(findChild(page, "moveSpaceUp-" + spaceId), null);
            compare(findChild(page, "moveSpaceDown-" + spaceId), null);
            compare(findChild(page, "settingsSpace-" + spaceId), null);
        }
        compare(findChild(page, "newSpaceButton").visible, false);
    }

    function test_aLetterSelectsAndFocusesItsSection() {
        const page = makePage();
        const privacy = findChild(page, "settingsSection" + page.sections.indexOf("privacy"));
        const spaces = findChild(page, "settingsSection" + page.sections.indexOf("spaces"));
        verify(privacy !== null);
        verify(spaces !== null);

        page.forceActiveFocus();
        keyClick(Qt.Key_P);

        compare(page.sections[page.section], "privacy");
        verify(privacy.activeFocus);

        keyClick(Qt.Key_Space);
        compare(page.sections[page.section], "privacy");
        keyClick(Qt.Key_Tab);
        verify(spaces.activeFocus);
    }

    function test_aRepeatedLetterWrapsAndAnUnknownLetterDoesNothing() {
        const page = makePage();
        const downloads = findChild(page, "settingsSection" + page.sections.indexOf("downloads"));
        verify(downloads !== null);

        page.forceActiveFocus();
        keyClick(Qt.Key_D, Qt.ShiftModifier);
        compare(page.sections[page.section], "downloads");
        verify(downloads.activeFocus);

        keyClick(Qt.Key_D);
        compare(page.sections[page.section], "downloads");
        verify(downloads.activeFocus);

        keyClick(Qt.Key_Z);
        compare(page.sections[page.section], "downloads");
        verify(downloads.activeFocus);
    }

    QtObject {
        id: privacyControlStub

        property bool enabled: true
    }

    // The signal is one switch for the whole browser, shown under privacy and
    // bound to the browser's answer rather than to a state of its own: the
    // switch flips the setting, and reads back what the setting says.
    function test_globalPrivacyControlIsOneSwitchUnderPrivacy() {
        const page = makePage();
        page.section = page.sections.indexOf("privacy");
        const toggle = findChild(page, "globalPrivacyControl");
        verify(toggle !== null);
        verify(!toggle.visible);

        privacyControlStub.enabled = true;
        page.globalPrivacyControl = privacyControlStub;
        verify(toggle.visible);
        verify(toggle.checked);
        settleAction(toggle);
        mouseClick(toggle, toggle.width / 2, toggle.height / 2);
        tryVerify(function () {
            return !privacyControlStub.enabled;
        });
        verify(!toggle.checked);
        mouseClick(toggle, toggle.width / 2, toggle.height / 2);
        tryVerify(function () {
            return privacyControlStub.enabled;
        });
        verify(toggle.checked);
    }

    QtObject {
        id: secureDnsStub

        property string resolver: ""
        property string customTemplate: ""
        readonly property string quad9: "https://dns.quad9.net/dns-query"
        readonly property string serverTemplate: {
            if (resolver === "custom")
                return customTemplate;
            return resolver === "quad9" ? quad9 : "";
        }
        readonly property string resolverTitle: resolver === "quad9" ? "Quad9" : serverTemplate
        readonly property var resolvers: [
            {
                "id": "quad9",
                "title": "Quad9",
                "template": "https://dns.quad9.net/dns-query"
            }
        ]

        function useResolver(id) {
            resolver = id;
            return true;
        }
        function useCustom(address) {
            if (address.indexOf("https://") !== 0)
                return false;
            customTemplate = address;
            resolver = "custom";
            return true;
        }
        function turnOff() {
            resolver = "";
        }
    }

    // Secure DNS under privacy: off by default, a named resolver or an address
    // the reader types, and a line that says whose resolver looks names up.
    function test_secureDnsSaysWhoseResolverLooksNamesUp() {
        const page = makePage();
        page.section = page.sections.indexOf("privacy");
        secureDnsStub.resolver = "";
        page.secureDns = secureDnsStub;
        const choice = findChild(page, "secureDnsResolver");
        const inUse = findChild(page, "secureDnsInUse");
        const address = findChild(page, "secureDnsAddress");
        verify(choice !== null && inUse !== null && address !== null);
        compare(choice.value, "");
        compare(inUse.text, "Names are looked up by your system's resolver.");
        verify(!address.visible);

        choice.changed("quad9");
        compare(secureDnsStub.resolver, "quad9");
        compare(inUse.text, "Names are looked up by Quad9, over an encrypted connection.");

        choice.changed("custom");
        verify(address.visible);
        address.text = "http://dns.example/dns-query";
        address.accepted();
        verify(findChild(page, "secureDnsAddressRefused").visible);
        compare(secureDnsStub.resolver, "quad9");
        address.text = "https://dns.example/dns-query";
        address.accepted();
        verify(!findChild(page, "secureDnsAddressRefused").visible);
        compare(secureDnsStub.resolver, "custom");
        compare(inUse.text, "Names are looked up by https://dns.example/dns-query, "
                + "over an encrypted connection.");

        choice.changed("");
        compare(secureDnsStub.resolver, "");
        page.secureDns = null;
    }

    QtObject {
        id: webRtcPolicyStub

        property bool publicInterfacesOnly: true
    }

    QtObject {
        id: engineWebRtcPolicyStub

        property bool available: true
    }

    // One switch under privacy for what a page's call may learn about the
    // reader's network, bound to the policy rather than to a state of its own.
    // A build whose engine adapter cannot reach the profile's settings shows
    // the fact in the switch's place.
    function test_webRtcAddressPolicyIsOneSwitchUnderPrivacy() {
        const page = makePage();
        page.section = page.sections.indexOf("privacy");
        const toggle = findChild(page, "webRtcPublicInterfacesOnly");
        const notice = findChild(page, "webRtcPolicyNotice");
        verify(toggle !== null);
        verify(notice !== null);
        verify(!toggle.visible);
        verify(!notice.visible);

        webRtcPolicyStub.publicInterfacesOnly = true;
        engineWebRtcPolicyStub.available = true;
        page.webRtcPolicy = webRtcPolicyStub;
        page.engineWebRtcPolicy = engineWebRtcPolicyStub;
        verify(toggle.visible);
        verify(!notice.visible);
        verify(toggle.checked);
        settleAction(toggle);
        mouseClick(toggle, toggle.width / 2, toggle.height / 2);
        tryVerify(function () {
            return !webRtcPolicyStub.publicInterfacesOnly;
        });
        verify(!toggle.checked);
        mouseClick(toggle, toggle.width / 2, toggle.height / 2);
        tryVerify(function () {
            return webRtcPolicyStub.publicInterfacesOnly;
        });
        verify(toggle.checked);

        engineWebRtcPolicyStub.available = false;
        verify(!toggle.visible);
        verify(notice.visible);
    }

    // Two Known extensions as the controller reports them: one whose package
    // is here and on, one Omaweb names but has not downloaded. The second is
    // the state the section exists to explain, so it is in the fixture rather
    // than assumed away.
    readonly property var extensionsFixture: [
        {
            "key": "bitwarden",
            "name": "Bitwarden Password Manager",
            "publisher": "Bitwarden, Inc.",
            "licence": "GPL-3.0-or-later",
            "summary": "Fills and saves passwords.",
            "installed": true,
            "enabled": true
        },
        {
            "key": "onepassword",
            "name": "1Password",
            "publisher": "AgileBits Inc.",
            "licence": "Proprietary",
            "summary": "Fills and saves passwords.",
            "installed": false,
            "enabled": false
        }
    ]

    SignalSpy {
        id: extensionToggleSpy
        signalName: "knownExtensionToggled"
    }

    // What the lists may say and this build does not act on is named, and CNAME
    // uncloaking is named among it only on an engine that cannot resolve a
    // host for the interceptor (ADR 0050).
    function test_contentBlockingNamesCnameUncloakingOnlyWhereTheEngineLacksIt() {
        const page = makePage();
        page.section = page.sections.indexOf("content blocking");
        const support = findChild(page, "contentBlockingSupport");
        verify(support !== null);
        verify(support.text.indexOf("CNAME uncloaking") >= 0);

        page.cnameUncloakingAvailable = true;
        verify(support.text.indexOf("CNAME uncloaking") < 0);
        verify(support.text.indexOf("$cname") >= 0);
    }

    // A build running the engine the system supplies cannot host an extension,
    // and says so where the list would be rather than drawing switches that
    // would reach nothing.
    function test_aBuildThatCannotHostAnExtensionSaysSoRatherThanListing() {
        const page = makePage();
        page.knownExtensions = testCase.extensionsFixture;
        page.section = page.sections.indexOf("extensions");
        const notice = findChild(page, "extensionsUnavailableNotice");
        verify(notice !== null);
        verify(notice.visible);
        const toggle = findChild(page, "knownExtension-bitwarden");
        verify(toggle !== null);
        verify(!toggle.visible);

        page.knownExtensionsAvailable = true;
        verify(!notice.visible);
        verify(toggle.visible);
    }

    // A Private window hosts no extension, and the capability that draws the
    // switches belongs to the build rather than the window, so without this
    // the section offers a reader a switch that cannot do anything here.
    function test_aPrivateWindowSaysWhyItListsNoExtension() {
        const page = makePage();
        page.knownExtensionsAvailable = true;
        page.knownExtensions = testCase.extensionsFixture;
        page.section = page.sections.indexOf("extensions");

        const toggle = findChild(page, "knownExtension-bitwarden");
        verify(toggle !== null);
        verify(toggle.visible);

        page.privateWindow = true;
        const notice = findChild(page, "extensionsPrivateNotice");
        verify(notice !== null);
        verify(notice.visible);
        // The switch goes rather than greying out: a reader cannot act on it
        // in this window at all, and a dimmed switch invites the try.
        verify(!toggle.visible);

        page.privateWindow = false;
        verify(!notice.visible);
        verify(toggle.visible);
    }

    // One switch per Known extension, carrying what the reader needs to judge
    // it. A package that has not arrived leaves a switch that says why it
    // cannot be turned on, because a name with no explanation reads as a
    // browser that has lost it.
    function test_eachKnownExtensionIsOneSwitchThatSaysWhoPublishesIt() {
        const page = makePage();
        page.knownExtensionsAvailable = true;
        page.knownExtensions = testCase.extensionsFixture;
        page.section = page.sections.indexOf("extensions");

        const present = findChild(page, "knownExtension-bitwarden");
        verify(present !== null);
        verify(present.enabled);
        verify(present.checked);
        verify(present.note.indexOf("Bitwarden, Inc.") >= 0);
        verify(present.note.indexOf("GPL-3.0-or-later") >= 0);
        verify(present.note.indexOf("not downloaded yet") < 0);

        // Not downloaded is not a reason to refuse the switch: turning it on
        // is what fetches it. A download already running is the only reason.
        const absent = findChild(page, "knownExtension-onepassword");
        verify(absent !== null);
        verify(absent.enabled);
        verify(!absent.checked);
        verify(absent.note.indexOf("not downloaded yet") >= 0);

        // The switch reports the key and the answer; what that answer costs is
        // the controller's, and the page never holds a state of its own.
        extensionToggleSpy.target = page;
        extensionToggleSpy.clear();
        settleAction(present);
        mouseClick(present, present.width / 2, present.height / 2);
        tryCompare(extensionToggleSpy, "count", 1);
        compare(extensionToggleSpy.signalArguments[0][0], "bitwarden");
        compare(extensionToggleSpy.signalArguments[0][1], false);
        extensionToggleSpy.target = null;
    }

    // A download already running is the one reason the switch refuses: pressing
    // it again would ask for the same folder twice while the first answer is
    // still being written into it.
    function test_anExtensionBeingDownloadedSaysSoAndCannotBePressed() {
        const page = makePage();
        page.knownExtensionsAvailable = true;
        const arriving = testCase.extensionsFixture.map(function (entry) {
            return Object.assign({}, entry, {
                                     "fetching": entry.key === "onepassword"
                                 });
        });
        page.knownExtensions = arriving;
        page.section = page.sections.indexOf("extensions");

        const downloading = findChild(page, "knownExtension-onepassword");
        verify(downloading !== null);
        verify(!downloading.enabled);
        verify(downloading.note.indexOf("downloading") >= 0);
        verify(downloading.note.indexOf("not downloaded yet") < 0);

        // The one that is not being fetched is untouched by another's download.
        verify(findChild(page, "knownExtension-bitwarden").enabled);
    }

    // The reader's type, stubbed the way the page reads it: the size on show,
    // whether it is theirs, the theme's underneath, and a page's fonts each
    // beside the engine's own. The real object is driven by the layout test
    // below, through the kit, and by tst_omarchy_kit.
    QtObject {
        id: fontSettingsStub

        property int themeFontSize: 12
        property int override: 0
        readonly property int interfaceFontSize: override > 0 ? override : themeFontSize
        readonly property bool interfaceFontSizeOverridden: override > 0
        readonly property int minimumInterfaceFontSize: 8
        readonly property int maximumInterfaceFontSize: 24
        readonly property int minimumPageFontSize: 9
        readonly property int maximumPageFontSize: 72
        readonly property int maximumPageMinimumFontSize: 24
        property var pageFontsMap: ({})
        property var familyCalls: []
        property var sizeCalls: []

        function reset() {
            override = 0;
            familyCalls = [];
            sizeCalls = [];
            pageFontsMap = {
                "standardFamily": {
                    "value": "Adwaita Sans",
                    "overridden": false,
                    "engine": "Adwaita Sans"
                },
                "fixedFamily": {
                    "value": "Adwaita Mono",
                    "overridden": false,
                    "engine": "Adwaita Mono"
                },
                "fontSize": {
                    "value": 16,
                    "overridden": false,
                    "engine": 16
                },
                "minimumFontSize": {
                    "value": 0,
                    "overridden": false,
                    "engine": 0
                }
            };
        }

        function increaseInterfaceFontSize() {
            override = interfaceFontSize + 1;
        }
        function decreaseInterfaceFontSize() {
            override = interfaceFontSize - 1;
        }
        function resetInterfaceFontSize() {
            override = 0;
        }
        function installedFamilies() {
            return ["Adwaita Sans", "Noto Serif", Style.font.family];
        }
        function setPageFamily(which, family) {
            familyCalls = familyCalls.concat([[which, family]]);
        }
        function setPageSize(which, size) {
            sizeCalls = sizeCalls.concat([[which, size]]);
        }
    }

    QtObject {
        id: pageFontsStub

        property bool available: true
    }

    function makeInterfacePage() {
        fontSettingsStub.reset();
        const page = makePage();
        page.fontSettings = fontSettingsStub;
        page.pageFonts = pageFontsStub;
        page.section = page.sections.indexOf("interface");
        return page;
    }

    function click(control) {
        settleAction(control);
        mouseClick(control, control.width / 2, control.height / 2);
    }

    // The interface size is stepped from the size on show and reset to the
    // theme's, and the control names its value and its default to a screen
    // reader along with each action (#170).
    function test_theInterfaceSizeStepsFromTheThemesAndResetsToIt() {
        const page = makeInterfacePage();
        const stepper = findChild(page, "interfaceFontSize");
        verify(stepper !== null);
        const value = findChild(stepper, "value");
        const reset = findChild(stepper, "reset");
        compare(value.text, "12px");
        verify(!reset.visible);
        compare(stepper.Accessible.role, Accessible.SpinBox);
        compare(stepper.Accessible.name, "Interface font size");
        compare(stepper.Accessible.description, "12 pixels, the theme's");

        click(findChild(stepper, "increase"));
        compare(fontSettingsStub.override, 13);
        compare(value.text, "13px");
        compare(stepper.Accessible.description, "13 pixels");
        verify(reset.visible);
        compare(findChild(stepper, "increase").Accessible.name,
                "Increase Interface font size, now 13 pixels");

        click(findChild(stepper, "decrease"));
        click(findChild(stepper, "decrease"));
        compare(fontSettingsStub.override, 11);
        click(reset);
        compare(fontSettingsStub.override, 0);
        compare(value.text, "12px");
        verify(!reset.visible);

        // The ends of the range are ends.
        fontSettingsStub.override = 24;
        verify(!findChild(stepper, "increase").enabled);
        verify(findChild(stepper, "decrease").enabled);
        fontSettingsStub.override = 8;
        verify(findChild(stepper, "increase").enabled);
        verify(!findChild(stepper, "decrease").enabled);
    }

    // A page's families are chosen from what the host has, with the engine's
    // own answer leading the list and, for the fixed-width slot, the family
    // the interface is drawn in offered next (#293). Choosing the engine's
    // answer again is an empty family, which is how the setting says reset.
    function test_pageFamiliesAreChosenFromTheHostBehindTheEngineDefault() {
        const page = makeInterfacePage();
        const standard = findChild(page, "pageStandardFamily");
        const fixed = findChild(page, "pageFixedFamily");
        verify(standard !== null);
        verify(fixed !== null);
        compare(standard.options[0].value, "");
        compare(standard.options[0].label, "Engine default (Adwaita Sans)");
        compare(standard.options.length, 4);
        compare(standard.value, "");
        compare(fixed.options[0].label, "Engine default (Adwaita Mono)");
        compare(fixed.options[1].value, Style.font.family);
        compare(fixed.options[1].label, "Interface's (" + Style.font.family + ")");
        compare(fixed.options.length, 4);
        compare(standard.Accessible.name, "Standard font for pages");
        compare(fixed.Accessible.name, "Fixed-width font for pages");

        standard.changed("Noto Serif");
        fixed.changed(Style.font.family);
        fixed.changed("");
        compare(fontSettingsStub.familyCalls, [[Core.FontSettings.Standard, "Noto Serif"],
                                               [Core.FontSettings.Fixed, Style.font.family],
                                               [Core.FontSettings.Fixed, ""]]);

        // A family the reader chose is what the list shows chosen.
        fontSettingsStub.pageFontsMap = Object.assign({}, fontSettingsStub.pageFontsMap, {
                                                          "standardFamily": {
                                                              "value": "Noto Serif",
                                                              "overridden": true,
                                                              "engine": "Adwaita Sans"
                                                          }
                                                      });
        compare(standard.value, "Noto Serif");
    }

    // A page's sizes step from the engine's own, a floor of none reads as
    // none, and reset is a zero, which is how the setting says the engine's.
    function test_pageSizesStepFromTheEnginesAndResetToThem() {
        const page = makeInterfacePage();
        const size = findChild(page, "pageFontSize");
        const minimum = findChild(page, "pageMinimumFontSize");
        verify(size !== null);
        verify(minimum !== null);
        compare(findChild(size, "value").text, "16px");
        compare(size.Accessible.description, "16 pixels, the engine's");
        verify(!findChild(size, "reset").visible);
        compare(findChild(minimum, "value").text, "none");
        compare(minimum.Accessible.description, "none, the engine's");
        verify(!findChild(minimum, "decrease").enabled);

        click(findChild(size, "increase"));
        click(findChild(minimum, "increase"));
        compare(fontSettingsStub.sizeCalls, [[Core.FontSettings.Default, 17],
                                             [Core.FontSettings.Minimum, 1]]);

        fontSettingsStub.pageFontsMap = Object.assign({}, fontSettingsStub.pageFontsMap, {
                                                          "fontSize": {
                                                              "value": 20,
                                                              "overridden": true,
                                                              "engine": 16
                                                          },
                                                          "minimumFontSize": {
                                                              "value": 12,
                                                              "overridden": true,
                                                              "engine": 0
                                                          }
                                                      });
        compare(findChild(size, "value").text, "20px");
        compare(findChild(minimum, "value").text, "12px");
        verify(findChild(size, "reset").visible);
        click(findChild(size, "reset"));
        click(findChild(minimum, "decrease"));
        click(findChild(minimum, "reset"));
        compare(fontSettingsStub.sizeCalls.slice(2), [[Core.FontSettings.Default, 0],
                                                      [Core.FontSettings.Minimum, 11],
                                                      [Core.FontSettings.Minimum, 0]]);
    }

    // On a Qt this build was not compiled against the engine's fonts are out
    // of reach (ADR 0047), and the group says so in place of its controls.
    function test_pageFontsSayWhenTheEngineIsOutOfReach() {
        const page = makeInterfacePage();
        verify(findChild(page, "pageFontsGroup").visible);
        verify(!findChild(page, "pageFontsNotice").visible);
        pageFontsStub.available = false;
        verify(!findChild(page, "pageFontsGroup").visible);
        verify(findChild(page, "pageFontsNotice").visible);
        pageFontsStub.available = true;
    }

    // Without the reader's settings, as in a window that was handed none,
    // the interface section is the three switches it was.
    function test_theTypeGroupsWaitForTheReadersSettings() {
        const page = makePage();
        page.section = page.sections.indexOf("interface");
        verify(!findChild(page, "interfaceFontSizeRow").visible);
        verify(!findChild(page, "pageFontsGroup").visible);
        verify(!findChild(page, "pageFontsNotice").visible);
    }

    // The reader's size reaches the kit's base size, which every measure on
    // this page is derived from, so the whole page is asked the overflow
    // question at both ends of the supported range, through the real setting
    // rather than the theme axis (#170).
    function describe(item) {
        if (item === null)
            return "nothing";
        return (item.objectName || String(item)) + " " + item.width + "x" + item.height + " at "
                + item.mapToItem(null, 0, 0).x + " text=" + (item.text || "");
    }

    function test_noSectionOverflowsThePaneAtTheSmallestAndLargestInterfaceSize() {
        const page = makePage();
        page.fontSettings = fontSettings;
        page.pageFonts = pageFontsStub;
        const pane = findChild(page, "settingsPane");
        verify(pane !== null);
        for (const size of [fontSettings.minimumInterfaceFontSize,
                            fontSettings.maximumInterfaceFontSize]) {
            fontSettings.setInterfaceFontSize(size);
            tryCompare(Style.font, "baseSize", size);
            for (let section = 0; section < page.sections.length; ++section) {
                page.section = section;
                tryVerify(function () {
                    return testCase.overflowingItem(pane, pane) === null;
                }, 5000, "section " + page.sections[section] + " overflows the pane at " + size
                + "px: " + describe(testCase.overflowingItem(pane, pane)));
                tryVerify(function () {
                    return pane.height > 0;
                });
            }
        }
        fontSettings.resetInterfaceFontSize();
    }

    function test_syncHasAnExplicitPrivacyBoundary() {
        const page = makePage();
        compare(page.sections.indexOf("sync") >= 0, true);
        page.section = page.sections.indexOf("sync");

        const boundary = findChild(page, "syncPrivacyBoundary");
        verify(boundary !== null);
        verify(boundary.text.indexOf("Passwords") >= 0);
        verify(boundary.text.indexOf("Private") >= 0);
        verify(boundary.text.indexOf("history") >= 0);
    }

    function test_syncShowsTheProviderAccountProminently() {
        const page = makePage();
        page.syncLauncher = syncLauncherStub;
        page.section = page.sections.indexOf("sync");
        syncControllerStub.enabled = true;
        syncControllerStub.login = "octocat";
        syncControllerStub.status = "Sync is on";

        const card = findChild(page, "syncAccountCard");
        const avatar = findChild(page, "syncAccountAvatar");
        const monogram = findChild(page, "syncAccountMonogram");
        const login = findChild(page, "syncAccountLogin");
        const provider = findChild(page, "syncAccountProvider");
        const state = findChild(page, "syncAccountState");
        verify(card !== null);
        verify(card.visible);
        verify(avatar !== null);
        compare(avatar.width, 64);
        compare(avatar.height, avatar.width);
        compare(avatar.radius, avatar.width / 2);
        compare(monogram.text, "O");
        verify(monogram.visible);
        compare(login.text, "octocat");
        compare(provider.text, "Forge account");
        compare(state.text, "Sync is on");

        syncControllerStub.enabled = false;
        syncControllerStub.status = "Sync is paused";
        compare(state.text, "Sync is paused");
        compare(String(state.color), String(page.colors.mutedText));
    }

    function test_syncConsentRoutesAfterSettingsIsClosed() {
        const page = makePage();
        page.syncLauncher = syncLauncherStub;
        page.open = false;
        settingsClosedSpy.target = page;
        syncCodeCopiedSpy.target = page;
        syncConsentRequestedSpy.target = page;
        SystemClipboard.copyText("stale clipboard");

        syncControllerStub.consentPageRequested(syncControllerStub.verificationUrl);

        compare(SystemClipboard.text(), syncControllerStub.userCode);
        compare(settingsClosedSpy.count, 1);
        compare(syncCodeCopiedSpy.count, 1);
        compare(syncConsentRequestedSpy.count, 1);
        verify(findChild(page, "copySyncCodeButton") !== null);
    }

    function test_syncFailureIsVisibleAfterSettingsCloses() {
        const page = makePage();
        page.syncLauncher = syncLauncherStub;
        page.section = page.sections.indexOf("sync");
        syncConnectionFailedSpy.target = page;

        syncControllerStub.errorMessage = "Forge refused repository creation";
        syncControllerStub.stateChanged();

        compare(syncConnectionFailedSpy.count, 1);
        compare(syncConnectionFailedSpy.signalArguments[0][0], "Forge Sync failed");
        compare(syncConnectionFailedSpy.signalArguments[0][1], "Forge refused repository creation");
        const notice = findChild(page, "syncErrorNotice");
        verify(notice.visible);
        compare(notice.detail, "Forge refused repository creation");
    }

    function test_syncRepositoryCreationHasAnExplicitContinuation() {
        const page = makePage();
        page.syncLauncher = syncLauncherStub;
        page.section = page.sections.indexOf("sync");
        syncControllerStub.awaitingRepositoryCreation = true;

        const button = findChild(page, "continueSyncSetupButton");
        verify(button.visible);
        button.clicked();
        compare(syncControllerStub.continueCount, 1);
    }

    function test_aFieldKeepsTheLettersItAccepts() {
        const page = makePage();
        page.section = page.sections.indexOf("content blocking");
        const field = fieldWithPlaceholder(page, page.subscriptionPlaceholders.title);
        verify(field !== null);

        field.focusInput();
        keyClick(Qt.Key_P);

        compare(field.text, "p");
        compare(page.sections[page.section], "content blocking");
        verify(field.activeFocus);
    }

    function test_aDialogKeepsUnhandledLettersFromTheSettingsPage() {
        const page = makePage();
        page.section = page.sections.indexOf("downloads");
        page.clearDataOpen = true;
        const firstCategory = findChild(page, "clearCategory-cookies");
        verify(firstCategory !== null);
        tryVerify(function () {
            return firstCategory.activeFocus;
        });

        keyClick(Qt.Key_P);

        compare(page.sections[page.section], "downloads");
        verify(firstCategory.activeFocus);
    }

    // The page's own frame is the kit's rhythm rather than a pixel count, so a
    // theme that makes the shell denser or roomier moves it with everything
    // else.
    function test_theFrameFollowsTheThemeSpacingScale() {
        const page = makePage();
        const frame = ["sideMargin", "topInset", "headerGap", "bottomInset", "railGap",
                       "closeSize"];
        const before = ({});
        for (let index = 0; index < frame.length; ++index) {
            verify(page[frame[index]] > 0);
            before[frame[index]] = page[frame[index]];
        }

        theme.useSpacingScale(2);
        for (let index = 0; index < frame.length; ++index)
            verify(page[frame[index]] > before[frame[index]]);
    }

    // And the frame is what the page is actually drawn on, rather than a set of
    // properties beside a layout that still carries its own numbers.
    function test_theHeaderAndTheBodyAreDrawnOnThatFrame() {
        const page = makePage();
        const eyebrow = findChild(page, "settingsEyebrow");
        const close = findChild(page, "closeSettingsButton");
        verify(eyebrow !== null);
        verify(close !== null);

        compare(eyebrow.mapToItem(page, 0, 0).x, page.sideMargin);
        compare(eyebrow.mapToItem(page, 0, 0).y, page.topInset);
        compare(close.width, page.closeSize);
        compare(close.height, page.closeSize);
        compare(Math.round(close.mapToItem(page, close.width, 0).x), page.width - page.sideMargin);

        theme.useSpacingScale(2);
        compare(eyebrow.mapToItem(page, 0, 0).x, page.sideMargin);
        compare(eyebrow.mapToItem(page, 0, 0).y, page.topInset);
        compare(close.width, page.closeSize);
    }

    // The rail's rhythm is the theme's too: the room each name takes around
    // itself and the gap between one name and the next both move with the
    // spacing scale, measured from where the names are drawn rather than read
    // off the container that spaced them.
    function test_theRailRhythmFollowsTheThemeSpacing() {
        const page = makePage();
        const first = findChild(page, "settingsSection0");
        const second = findChild(page, "settingsSection1");
        verify(first !== null);
        verify(second !== null);

        // A name leans on nothing: it belongs to the rail, not to what follows.
        compare(first.topPadding, first.bottomPadding);
        verify(first.topPadding > 0);

        function railGapBetweenNames() {
            return testCase.gapBetween(page, first, second);
        }

        const padding = first.topPadding;
        tryVerify(function () {
            return railGapBetweenNames() > 0;
        });
        const gap = railGapBetweenNames();

        theme.useSpacingScale(3);
        verify(first.topPadding > padding);
        // A Column repositions on the next polish, so the gap is read back
        // rather than sampled the instant the singleton moved.
        tryVerify(function () {
            return railGapBetweenNames() > gap;
        });
    }

    // The gap between the blocks a section is made of belongs to the pane, so
    // adding a row to one block cannot change the rhythm of the block under it.
    // Inside a block the rows abut on purpose: each draws the rule that divides
    // it from the row above, and a gap there would break the list into cards.
    function test_thePaneOwnsTheGapBetweenTheBlocksOfASection() {
        const page = makePage();
        const pane = findChild(page, "settingsPane");
        verify(pane !== null);
        verify(pane.spacing > 0);

        // Two cards, which want the pane's gap between them.
        const favicons = findChild(page, "useFavicons");
        const tint = findChild(page, "tintFavicons");
        // Two rows of a ruled list, which do not.
        const firstKept = findChild(page, "retainedTab-kept-one");
        const secondKept = findChild(page, "retainedTab-kept-two");
        verify(favicons !== null);
        verify(tint !== null);
        verify(firstKept !== null);
        verify(secondKept !== null);

        function cardGap() {
            return testCase.gapBetween(page, favicons, tint);
        }
        function ruledGap() {
            return testCase.gapBetween(page, firstKept, secondKept);
        }

        tryVerify(function () {
            return cardGap() === pane.spacing;
        });
        tryVerify(function () {
            return ruledGap() === 0;
        });

        const spacing = pane.spacing;
        theme.useSpacingScale(3);
        verify(pane.spacing > spacing);
        tryVerify(function () {
            return cardGap() === pane.spacing;
        });
        tryVerify(function () {
            return ruledGap() === 0;
        });
    }

    // And a ruled list is a ruled list wherever it is, not a rule the tabs
    // section happens to keep: the downloads a Space recorded abut the row that
    // names the directory they went to.
    function test_aRuledListAbutsInEverySectionThatHasOne() {
        const page = makePage();
        page.section = page.sections.indexOf("downloads");
        const rows = [];
        for (let index = 0; index < testCase.downloadsFixtureCount; ++index)
            rows.push(findChild(page, "recordedDownload-" + index));
        for (let index = 0; index < rows.length; ++index)
            verify(rows[index] !== null);

        tryVerify(function () {
            return testCase.gapBetween(page, rows[0], rows[1]) === 0;
        });
    }

    // A section label leans toward the list it heads rather than sitting evenly
    // between two, because it belongs to what follows it. The exception is a
    // label that opens its section: leaning away from nothing is dead space, so
    // it takes only the sliver a tall glyph paints above its own box.
    function test_aSectionLabelLeansTowardWhatItHeadsUnlessItOpensTheSection() {
        const page = makePage();
        const keptActive = findLabel(page, "kept active");
        verify(keptActive !== null);
        verify(keptActive.topPadding > keptActive.bottomPadding);
        verify(keptActive.topPadding > keptActive.overshoot);

        page.section = page.sections.indexOf("privacy");
        const privacy = findLabel(page, "privacy");
        verify(privacy !== null);
        compare(privacy.topPadding, privacy.overshoot);
    }

    // The kit's section header carries no objectName of its own, so it is found
    // by the one thing that identifies it: the words it draws.
    function findLabel(item, text) {
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index];
            if (child.text === text && child.overshoot !== undefined)
                return child;
            const deeper = findLabel(child, text);
            if (deeper !== null)
                return deeper;
        }
        return null;
    }

    // Two form fields share a row while a pair still fits the words they ask
    // for. A pane too narrow for that stacks them, rather than shaving each one
    // past its own placeholder — which is what a fixed two columns did, out of
    // sight of any width the page reports.
    function test_pairedFieldsStackRatherThanNarrowPastTheirWords() {
        const page = makePage();
        page.section = page.sections.indexOf("content blocking");
        const fields = findChild(page, "subscriptionFields");
        verify(fields !== null);
        verify(page.fieldFloor > 0);

        tryVerify(function () {
            return fields.columns === 2;
        });
        verify(fields.fieldWidth >= page.fieldFloor);

        const roomy = page.width;
        page.width = page.sideMargin * 2 + page.railWidth + page.railGap + page.fieldFloor * 2
                - Style.spacing.huge;
        tryVerify(function () {
            return fields.columns === 1;
        });
        verify(fields.fieldWidth >= page.fieldFloor);

        // And back, by the same arithmetic rather than by a remembered state.
        page.width = roomy;
        tryVerify(function () {
            return fields.columns === 2;
        });
    }

    // A line of explanation beside a row is a measure, not a pixel count: the
    // box grows with the face it is drawn in, so it still wraps to about as
    // many lines when the theme doubles the type. A fixed 260 pixels wrapped
    // the same words into twice the lines and pushed the row it explains off
    // the pane.
    function test_theExplanationIsMeasuredInTheFaceItIsDrawnIn() {
        const page = makePage();
        // Network, which is where the longest of these explanations sits.
        page.section = page.sections.indexOf("network");
        const status = findChild(page, "automaticRequestsStatus");
        verify(status !== null);
        verify(page.noteMeasure > 0);

        tryVerify(function () {
            return status.lineCount > 1;
        });
        const measure = page.noteMeasure;
        const width = status.width;
        const lines = status.lineCount;

        theme.useTypeTokens(2);
        verify(page.noteMeasure > measure);
        tryVerify(function () {
            return status.width > width;
        });
        // The words did not gain a paragraph's worth of lines on the way.
        tryVerify(function () {
            return status.lineCount <= lines + 1;
        });
    }

    // No filter lists says so. Left to the Repeater alone the section drew
    // blank page between two headings, which reads as a section that failed to
    // load rather than as a browser blocking nothing (#43), and the default
    // lists are offered back beside the sentence saying they are gone.
    function test_anEmptyListOfSubscriptionsReadsAsEmpty() {
        const page = makePage();
        page.blocker = blockerStub;
        page.section = page.sections.indexOf("content blocking");
        const notice = findChild(page, "noSubscriptionsNotice");
        const restore = findChild(page, "restoreDefaultListsButton");
        verify(notice !== null);
        verify(restore !== null);

        // The fixture page carries no subscriptions, which is the state.
        compare(page.subscriptions.length, 0);
        tryVerify(function () {
            return notice.visible && notice.height > 0;
        });
        verify(findChild(page, "noSubscriptionsText").text.length > 0);

        // The offer reaches the blocker rather than only reading as one.
        const asked = blockerStub.restoreCount;
        restore.clicked();
        compare(blockerStub.restoreCount, asked + 1);

        // And the sentence goes as soon as there is a list to show.
        page.subscriptions = [
                    {
                        id: "easylist",
                        title: "EasyList",
                        source: "https://easylist.to/",
                        license: "GPLv3 or CC BY-SA 3.0",
                        updateAddress: "https://easylist.to/easylist/easylist.txt",
                        updateStatus: "not updated",
                        enabled: true
                    }
                ];
        tryVerify(function () {
            return !notice.visible;
        });
    }

    // A list the browser knows but has not subscribed is offered by name with
    // its source, whatever else is subscribed, and one action subscribes it.
    // The offer goes once the list is a subscription, because a list that is
    // both offered and subscribed reads as two lists (#291).
    function test_aKnownListIsOfferedByNameAndSubscribedInOneAction() {
        const page = makePage();
        page.blocker = blockerStub;
        page.section = page.sections.indexOf("content blocking");
        page.knownLists = [
                    {
                        id: "easylist-cookie",
                        title: "EasyList Cookie",
                        source: "https://easylist.to/",
                        license: "CC BY 3.0",
                        updateAddress: "https://secure.fanboy.co.nz/fanboy-cookiemonster.txt"
                    }
                ];
        const offers = findChild(page, "knownListOffers");
        verify(offers !== null);
        tryVerify(function () {
            return offers.visible && offers.height > 0;
        });
        const offer = findChild(offers, "knownListOffer");
        verify(offer !== null);
        compare(offer.title, "EasyList Cookie");
        verify(offer.note.indexOf("https://easylist.to/") >= 0);

        const subscribe = findChild(offer, "subscribeKnownListButton");
        verify(subscribe !== null);
        subscribe.clicked();
        compare(blockerStub.subscribed, ["easylist-cookie"]);

        page.knownLists = [];
        tryVerify(function () {
            return !offers.visible;
        });
    }

    // Nothing is offered before there is a blocker to ask: the page is built
    // ahead of the service it reads, and a button that cannot act is worse
    // than no offer at all.
    function test_theOfferWaitsForABlockerToAsk() {
        const page = makePage();
        page.section = page.sections.indexOf("content blocking");
        verify(!page.blocker);
        const notice = findChild(page, "noSubscriptionsNotice");
        verify(notice !== null);
        verify(!notice.visible);
    }

    // The first item under the pane that runs past its right edge, or that had
    // to elide its own text to fit — crowded counts as well as clipped — or
    // null when there is none.
    function overflowingItem(pane, item) {
        for (let index = 0; index < item.children.length; ++index) {
            const child = item.children[index];
            if (child.visible === false)
                continue;
            if (child.width > 0 && child.mapToItem(pane, child.width, 0).x > pane.width + 1)
                return child;
            // A row that had to elide its own text is crowded even though it
            // fits, which is the other half of what a fixed count costs.
            if (child.truncated === true)
                return child;
            const deeper = overflowingItem(pane, child);
            if (deeper !== null)
                return deeper;
        }
        return null;
    }

    // The whole-page form of the same question the properties above ask one at
    // a time, and it is asked of every section the rail names: a pixel count
    // left in any one of them shows up here and nowhere else.
    function test_noSectionOverflowsThePaneAtALargerType() {
        const page = makePage();
        const pane = findChild(page, "settingsPane");
        verify(pane !== null);
        compare(page.sections.length, 12);

        theme.useTypeTokens(2);
        for (let section = 0; section < page.sections.length; ++section) {
            page.section = section;
            tryVerify(function () {
                return testCase.overflowingItem(pane, pane) === null;
            }, 5000, "section " + page.sections[section] + " overflows the pane");
            // And the section put something in the pane to begin with, so a
            // section that drew nothing cannot pass by being empty.
            tryVerify(function () {
                return pane.height > 0;
            });
        }
    }
}
