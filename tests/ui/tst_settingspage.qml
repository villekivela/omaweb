import QtQuick
import QtTest
import Omaweb
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

        function restoreDefaultSubscriptions() {
            restoreCount += 1;
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
        theme.restore();
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
        compare(page.sections.length, 11);

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
