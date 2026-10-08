#include "AgentActivityLog.h"
#include <QJSValue>
#include "AgentControl.h"
#include "BrowserController.h"
#include "StoredFaviconProvider.h"
#include "ContentBlocker.h"
#include "EngineSuggestions.h"
#include "GlobalPrivacyControl.h"
#include "SecureDns.h"
#include "SettingsFile.h"
#include "WebRtcPolicy.h"
#include "ReleaseWatch.h"
#include "EngineCapabilities.h"
#include "PageImages.h"
#include "FaviconTint.h"
#include "FontSettings.h"
#include "SidebarOpacity.h"
#include "DefaultBrowser.h"
#include "ExternalProtocolHandler.h"
#include "InputMethod.h"
#include "LocaleReport.h"
#include "KeyboardNavigation.h"
#include "KitTheme.h"
#include "MediaAnnouncer.h"
#include "PagePrinter.h"
#include "PaymentCards.h"
#include "ProcessResources.h"
#include "Quickshell.h"
#include "RuntimeSecurity.h"
#include "SavedDownload.h"
#include "SoundingTabs.h"
#include "InputOrigin.h"
#include "PrimaryHold.h"
#include "Scenes.h"
#include "SystemClipboard.h"
#include "SystemMotion.h"
#include "SystemNotifier.h"
#include "ThemeController.h"
#include "Translations.h"
#include "WindowChrome.h"
#include "WindowManager.h"

#include <algorithm>
#include <chrono>
#include <QJSValue>
#include <QQmlComponent>
#include <QAbstractItemModel>
#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QFile>
#include <QFontDatabase>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QTemporaryDir>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <optional>

namespace {

// Stand-in favicons for the lab, which runs no engine and therefore has no
// icon store of its own. The set is deliberately mixed: coloured marks to show
// a chip taking a site's own colour, and a white one to show the neutral chip
// an icon with no colour to give leaves behind.
QVariantList drawMockFavicons(const QString &directory)
{
    static const QList<QColor> marks = {
        QColor(0xe5, 0x4b, 0x4b),
        QColor(0x3f, 0x8f, 0xe8),
        QColor(0x2f, 0xb2, 0x8a),
        QColor(0xe8, 0x9f, 0x2a),
        QColor(0x9c, 0x5c, 0xe0),
        QColor(0xff, 0xff, 0xff),
    };
    QDir().mkpath(directory);
    QVariantList urls;
    for (qsizetype index = 0; index < marks.size(); ++index) {
        QImage icon(32, 32, QImage::Format_ARGB32);
        icon.fill(Qt::transparent);
        QPainter painter(&icon);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(marks.at(index));
        painter.drawRoundedRect(QRectF(4, 4, 24, 24), 7, 7);
        painter.end();
        const auto path = QDir(directory).filePath(QStringLiteral("favicon-%1.png").arg(index));
        if (icon.save(path)) {
            urls.append(QUrl::fromLocalFile(path));
        }
    }
    return urls;
}

// A day's worth of tabs for a Space the lab would otherwise bring up at rest.
// The sidebar's shape is the point — Pinned tiles above an ordinary list — so
// these are named the way a reader's own tabs are named rather than by their
// position in this array.
struct SampleTab {
    const char *url;
    const char *title;
    bool pinned;
};

const QList<SampleTab> &sampleTabs()
{
    static const QList<SampleTab> tabs = {
        {"https://mail.proton.me/u/0/inbox", "Inbox", true},
        {"https://calendar.google.com/calendar/r/week", "Calendar", true},
        {"https://github.com/notifications", "Notifications", true},
        {"https://music.youtube.com/library", "Library", true},
        {"https://github.com/villekivela/omaweb/pull/124", "Generate the website's screenshots",
            false},
        {"https://doc.qt.io/qt-6/qtquick-visualcanvas-scenegraph.html", "Qt Quick Scene Graph",
            false},
        {"https://afterdark.example/night-drive", "Night drive: a photo essay", false},
        {"https://wayland.app/protocols/xdg-shell", "xdg-shell protocol", false},
        {"https://archlinux.org/packages/extra/x86_64/qt6-webengine/", "Arch Linux - qt6-webengine",
            false},
        {"https://omarchy.org/", "Omarchy", false},
    };
    return tabs;
}

// The id of the tab in one row of a tab list.
QString tabIdAt(QAbstractItemModel *tabs, int row)
{
    if (tabs == nullptr || row < 0 || row >= tabs->rowCount()) {
        return {};
    }
    const auto roles = tabs->roleNames();
    const auto role = roles.key(QByteArrayLiteral("tabId"), -1);
    if (role < 0) {
        return {};
    }
    return tabs->data(tabs->index(row, 0), role).toString();
}

// The id of the tab a background open has just appended. A seeded tab is
// addressed by id for its title, its icon and its pin, and the controller
// hands back no id of its own for a background open.
QString lastTabId(QAbstractItemModel *tabs)
{
    return tabs == nullptr ? QString {} : tabIdAt(tabs, tabs->rowCount() - 1);
}

// A security key request's step as the engine reports it, for `--show
// security-key:<step>`.
QVariantMap securityKeyStep(const QString &step)
{
    const auto pin = [](const QString &error, int attemptsLeft) {
        return QVariantMap {{QStringLiteral("state"), QStringLiteral("pin")},
            {QStringLiteral("pin"),
                QVariantMap {{QStringLiteral("purpose"), QStringLiteral("unlock")},
                    {QStringLiteral("error"), error},
                    {QStringLiteral("attemptsLeft"), attemptsLeft},
                    {QStringLiteral("minimumLength"), 4}}}};
    };
    if (step == QLatin1String("pin")) {
        return pin(QString(), 8);
    }
    if (step == QLatin1String("pin-wrong")) {
        return pin(QStringLiteral("wrong"), 7);
    }
    if (step == QLatin1String("accounts")) {
        return {{QStringLiteral("state"), QStringLiteral("accounts")},
            {QStringLiteral("accounts"),
                QVariantList {
                    QVariantMap {{QStringLiteral("name"), QStringLiteral("reader@example.org")}},
                    QVariantMap {{QStringLiteral("name"), QStringLiteral("work@example.org")}}}}};
    }
    if (step == QLatin1String("failed")) {
        return {{QStringLiteral("state"), QStringLiteral("failed")},
            {QStringLiteral("failure"), QStringLiteral("no-key")}};
    }
    return {{QStringLiteral("state"), QStringLiteral("touch")}};
}

// Seeds the Space the lab came up on. The blank tab it came up with is left
// active and taken back at the end, so the viewport still draws the Start page
// with a populated sidebar beside it: a reader opening a new tab on a working
// day, which is the state a screenshot of this browser wants.
// The tab the seeded day ends on: the blank tab unless `onShow` names one of
// the sample addresses, which a capture of the browser in use asks for.
// The sample tab `--browse` ends the seeded day on: a picture-led page, so a
// shot of the browser in use shows a page with something to look at. Its host
// is under `.example`, reserved for this, so the page is nobody's real site.
constexpr const char *browsedTab = "https://afterdark.example/night-drive";

// The two filter lists a first run subscribes to, written into the lab's
// content-blocking settings as the browser would leave them after updating:
// seeded, enabled, current as of now, with a list file on disk. The lab
// builds its blocker without the defaults, since seeding them fetches, and
// a list updated less than a day ago with its file present is one the
// blocker does not fetch.
void writeSampleLists(const QDir &dataRoot)
{
    const auto folder = dataRoot.filePath(QStringLiteral("content-blocking"));
    QDir().mkpath(QDir(folder).filePath(QStringLiteral("lists")));
    const auto now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QJsonArray subscriptions;
    const QList<std::pair<const char *, const char *>> lists = {
        {"easylist", "EasyList"},
        {"easyprivacy", "EasyPrivacy"},
    };
    for (const auto &[id, title] : lists) {
        const auto name = QString::fromUtf8(id);
        subscriptions.append(QJsonObject {
            {QStringLiteral("id"), name},
            {QStringLiteral("title"), QString::fromUtf8(title)},
            {QStringLiteral("source"), QStringLiteral("https://easylist.to/")},
            {QStringLiteral("license"), QStringLiteral("GPLv3 or CC BY-SA 3.0")},
            {QStringLiteral("updateAddress"),
                QStringLiteral("https://easylist.to/easylist/%1.txt").arg(name)},
            {QStringLiteral("updateStatus"), QStringLiteral("current")},
            {QStringLiteral("lastUpdated"), now},
            {QStringLiteral("enabled"), true},
        });
        QFile list(QDir(folder).filePath(QStringLiteral("lists/%1.txt").arg(name)));
        if (list.open(QIODevice::WriteOnly)) {
            list.write("! Sample list for the UI lab\n");
        }
    }
    QFile settings(QDir(folder).filePath(QStringLiteral("settings.json")));
    if (settings.open(QIODevice::WriteOnly)) {
        settings.write(QJsonDocument(QJsonObject {
                                         {QStringLiteral("version"), 1},
                                         {QStringLiteral("seeded"), true},
                                         {QStringLiteral("userRules"), QString()},
                                         {QStringLiteral("disabledSites"), QJsonArray()},
                                         {QStringLiteral("subscriptions"), subscriptions},
                                     })
                .toJson(QJsonDocument::Indented));
    }
}

void seedSampleTabs(
    omaweb::BrowserController &browser, const QVariantList &favicons, const QString &onShow)
{
    auto shownTabId = browser.activeTabId();
    auto *unpinned = browser.unpinnedTabs();
    qsizetype icon = 0;
    for (const auto &sample : sampleTabs()) {
        const QUrl url(QString::fromUtf8(sample.url));
        browser.openInputInBackground(url);
        const auto tabId = lastTabId(unpinned);
        if (tabId.isEmpty()) {
            continue;
        }
        const auto favicon
            = favicons.isEmpty() ? QUrl {} : favicons.at(icon++ % favicons.size()).toUrl();
        browser.reportTabPageState(
            tabId, url, QString::fromUtf8(sample.title), favicon, false, false);
        if (!onShow.isEmpty() && url.toString() == onShow) {
            shownTabId = tabId;
        }
        // A tab that was opened was also visited. Without this History is a
        // page saying the Space has none, which is a state of the empty lab
        // rather than a state of the browser.
        browser.recordVisit(url, QString::fromUtf8(sample.title));
        // Pinning is an operation on the tab on show, so a seeded pin is
        // activated and pinned in turn. Nothing sees the intermediate state:
        // the blank tab is active again before control reaches the event loop.
        if (sample.pinned) {
            browser.activateTab(tabId);
            browser.toggleActivePinned();
        }
    }
    browser.activateTab(shownTabId);
}

// Two more Spaces, each with a page or two of its own, so that switching
// Space has somewhere to go. The first Space is put back on show at the
// end. This runs before the QML loads: a switch with the interface up tears
// the leaving Space's engines down, which a seed has no reason to pay for.
void seedSampleSpaces(omaweb::BrowserController &browser, const QVariantList &favicons)
{
    const auto firstSpaceId = browser.activeSpaceId();
    auto *unpinned = browser.unpinnedTabs();
    qsizetype icon = 0;
    struct SampleSpace {
        const char *name;
        QList<SampleTab> tabs;
    };
    const QList<SampleSpace> spaces = {
        {"Work",
            {{"https://github.com/pulls", "Pull requests", true},
                {"https://doc.qt.io/qt-6/qmlapplications.html", "QML Applications", false},
                {"http://localhost:3000/", "localhost:3000", false}}},
    };
    for (const auto &space : spaces) {
        const auto spaceId = browser.createSpace(QString::fromUtf8(space.name));
        if (spaceId.isEmpty() || !browser.switchSpace(spaceId)) {
            continue;
        }
        const auto restTabId = browser.activeTabId();
        for (const auto &sample : space.tabs) {
            const QUrl url(QString::fromUtf8(sample.url));
            browser.openInputInBackground(url);
            const auto tabId = lastTabId(unpinned);
            if (tabId.isEmpty()) {
                continue;
            }
            const auto favicon
                = favicons.isEmpty() ? QUrl {} : favicons.at(icon++ % favicons.size()).toUrl();
            browser.reportTabPageState(
                tabId, url, QString::fromUtf8(sample.title), favicon, false, false);
            if (sample.pinned) {
                browser.activateTab(tabId);
                browser.toggleActivePinned();
            }
        }
        browser.activateTab(restTabId);
    }
    browser.switchSpace(firstSpaceId);
}

// Two Agents' last hour in two Spaces: one working in an Agent Space and one
// granted the reader's Work Space, with one verb refused. What the Agent
// activity page is reviewed on.
void seedAgentActivity(omaweb::AgentActivityLog &log)
{
    struct SampleLine {
        int minutesAgo;
        const char *agent;
        const char *spaceId;
        const char *space;
        const char *address;
        const char *verb;
        const char *target;
        const char *outcome;
    };
    static const QList<SampleLine> lines = {
        {42, "claude", "lab-research", "Research", "", "space new", "Research", "ok"},
        {41, "claude", "lab-research", "Research", "https://shop.example/login", "open",
            "https://shop.example/login", "ok"},
        {41, "claude", "lab-research", "Research", "https://shop.example/login", "look", "", "ok"},
        {40, "claude", "lab-research", "Research", "https://shop.example/login", "do",
            "fill 7, fill 8, click 9", "ok"},
        {39, "claude", "lab-research", "Research", "https://shop.example/account", "read", "",
            "ok"},
        {24, "deploy-check", "lab-work", "Work", "http://localhost:3000/", "open",
            "http://localhost:3000/", "ok"},
        {23, "deploy-check", "lab-work", "Work", "http://localhost:3000/", "console", "", "ok"},
        {23, "deploy-check", "lab-work", "Work", "http://localhost:3000/", "shot", "full page",
            "ok"},
        {12, "deploy-check", "lab-work", "Work", "https://github.com/pulls", "eval", "", "refused"},
        {3, "claude", "lab-research", "Research", "https://shop.example/account", "do",
            "click 14, wait", "ok"},
    };
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &line : lines) {
        omaweb::AgentActivityLog::Entry entry;
        entry.time = now - line.minutesAgo * 60 * 1000LL;
        entry.agent = QString::fromUtf8(line.agent);
        entry.spaceId = QString::fromUtf8(line.spaceId);
        entry.space = QString::fromUtf8(line.space);
        entry.address = QString::fromUtf8(line.address);
        entry.verb = QString::fromUtf8(line.verb);
        entry.target = QString::fromUtf8(line.target);
        entry.outcome = QString::fromUtf8(line.outcome);
        log.record(std::move(entry));
    }
}

// A keyring that takes its time, as one does while the desktop asks the reader
// to unlock it, so a capture finds Settings still reading. The lab waits out
// the read when it closes.
class SlowKeyring final : public omaweb::PaymentCardKeyring {
public:
    explicit SlowKeyring(std::unique_ptr<omaweb::PaymentCardKeyring> keyring)
        : m_keyring(std::move(keyring))
    {
    }

    bool available() override { return m_keyring->available(); }
    std::expected<QList<omaweb::KeyringItem>, omaweb::KeyringFailure> items() override
    {
        QThread::sleep(std::chrono::seconds(20));
        return m_keyring->items();
    }
    bool store(const QString &id, const QByteArray &secret) override
    {
        return m_keyring->store(id, secret);
    }
    bool remove(const QString &id) override { return m_keyring->remove(id); }

private:
    std::unique_ptr<omaweb::PaymentCardKeyring> m_keyring;
};

} // namespace

int main(int argc, char *argv[])
{
    // Startup is measured from here: what the process pays before `main` is
    // the loader's and the same for every build, and what comes after is what
    // an Omaweb change can slow down.
    QElapsedTimer startup;
    startup.start();
    QGuiApplication application(argc, argv);
    const auto captureFontFile = qEnvironmentVariable("OMAWEB_CAPTURE_FONT_FILE");
    if (!captureFontFile.isEmpty() && QFontDatabase::addApplicationFont(captureFontFile) < 0) {
        qCritical("Could not load capture font: %s", qPrintable(captureFontFile));
        return 1;
    }
    omaweb::installWindowChrome(&application);
    // `--locale fi` shows the chrome as that locale's reader sees it, so a
    // mock can be checked in Finnish without changing the shell's environment.
    const auto localeArguments = application.arguments();
    const auto localeIndex = localeArguments.indexOf(QStringLiteral("--locale"));
    const auto localeChoice = localeIndex >= 0 && localeIndex + 1 < localeArguments.size()
        ? omaweb::LocaleChoice {QLocale(localeArguments.at(localeIndex + 1)), {}}
        : omaweb::localeChoice();
    const auto catalogueDirectories
        = omaweb::catalogueDirectories(QStringLiteral(OMAWEB_TRANSLATIONS_DIRECTORY));
    omaweb::installCatalogue(&application, localeChoice.locale, catalogueDirectories);
    static omaweb::LocaleReport localeReport {
        localeChoice, omaweb::shippedLanguages(catalogueDirectories)};
    QCoreApplication::setOrganizationName(QStringLiteral("Omaweb"));
    QCoreApplication::setApplicationName(QStringLiteral("Omaweb UI Lab"));
    // The settings page reads Qt.application.version for its about section, so
    // the lab has to carry the same version the browser does.
    QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));

    // The lab keeps nothing between runs, so its data root is temporary unless
    // a caller hands it one: the restore probe seeds a Space on disk first and
    // then measures the lab bringing it back.
    const auto arguments = application.arguments();
    std::optional<QTemporaryDir> temporaryRoot;
    QString dataRootPath;
    const auto dataRootIndex = arguments.indexOf(QStringLiteral("--data-root"));
    if (dataRootIndex >= 0 && dataRootIndex + 1 < arguments.size()) {
        dataRootPath = arguments.at(dataRootIndex + 1);
    } else {
        temporaryRoot.emplace();
        if (!temporaryRoot->isValid()) {
            qCritical("Could not create temporary UI-lab data directory.");
            return 1;
        }
        dataRootPath = temporaryRoot->path();
    }
    const QDir dataRoot(dataRootPath);
    if (arguments.contains(QStringLiteral("--sample-lists"))) {
        writeSampleLists(dataRoot);
    }

    // Off until it is turned on in the lab's Settings, where it is written
    // under the lab's own data root. Turned on, the lab asks the real engines,
    // which is how the rows are reviewed against what an engine answers.
    omaweb::EngineSuggestions engineSuggestions(dataRootPath);
    // `--suggest-url <url>` answers Engine suggestions from that address
    // instead, so a capture lists the same rows on every run and asks no real
    // engine: scripts/build_website_themes.py serves a fixed answer there. An
    // engine is saved under a configuration root, which the lab otherwise has
    // none of, so the flag gives it the data root.
    const auto suggestIndex = arguments.indexOf(QStringLiteral("--suggest-url"));
    const auto suggestUrl = suggestIndex >= 0 && suggestIndex + 1 < arguments.size()
        ? arguments.at(suggestIndex + 1)
        : QString();
    omaweb::BrowserController browser(omaweb::SpaceStorage(dataRootPath, QStringLiteral("mock")),
        suggestUrl.isEmpty() ? QString() : dataRootPath);
    browser.setEngineSuggestions(&engineSuggestions);
    // A keyring in memory: the lab never reaches the desktop's.
    // `--sample-cards` puts two cards in it, so the Settings section, the
    // suggestion list and Site information have rows to tell apart.
    auto keyring = std::make_shared<omaweb::MemoryPaymentCardKeyring::Contents>();
    if (arguments.contains(QStringLiteral("--sample-cards"))) {
        keyring->items = {
            {.id = QStringLiteral("sample-everyday"),
                .secret = QByteArrayLiteral(
                    R"({"number":"4242424242424242","name":"Meri Laine","expiryMonth":8,"expiryYear":2029,"nickname":"Everyday","added":1})")},
            {.id = QStringLiteral("sample-travel"),
                .secret = QByteArrayLiteral(
                    R"({"number":"5555555555554444","name":"Meri Laine","expiryMonth":11,"expiryYear":2030,"nickname":"","added":2})")},
        };
    }
    // `--keyring <state>` has the keyring stand where Settings is to be
    // reviewed: `unavailable`, `unreachable`, `locked`, `failed`, or `reading`,
    // which answers after 20 seconds.
    const auto keyringIndex = arguments.indexOf(QStringLiteral("--keyring"));
    const auto keyringState = keyringIndex >= 0 ? arguments.value(keyringIndex + 1) : QString();
    keyring->available = keyringState != QLatin1String("unavailable");
    keyring->locked = keyringState == QLatin1String("locked");
    if (keyringState == QLatin1String("unreachable")) {
        keyring->failure = omaweb::KeyringFailure::Unreachable;
    } else if (keyringState == QLatin1String("failed")) {
        keyring->failure = omaweb::KeyringFailure::Failed;
    }
    std::unique_ptr<omaweb::PaymentCardKeyring> labKeyring
        = std::make_unique<omaweb::MemoryPaymentCardKeyring>(keyring);
    if (keyringState == QLatin1String("reading")) {
        labKeyring = std::make_unique<SlowKeyring>(std::move(labKeyring));
    }
    omaweb::PaymentCards paymentCards(std::move(labKeyring));
    browser.setPaymentCards(&paymentCards);
    // `--sample-addresses` saves two addresses for one reader, so the Settings
    // section and the suggestion list have rows to tell apart.
    if (arguments.contains(QStringLiteral("--sample-addresses"))) {
        browser.saveAddress({{QStringLiteral("name"), QStringLiteral("Meri Laine")},
            {QStringLiteral("street"), QStringLiteral("Rantakatu 4 B 12")},
            {QStringLiteral("postalCode"), QStringLiteral("90100")},
            {QStringLiteral("city"), QStringLiteral("Oulu")},
            {QStringLiteral("country"), QStringLiteral("Finland")},
            {QStringLiteral("phone"), QStringLiteral("+358 40 123 4567")},
            {QStringLiteral("email"), QStringLiteral("meri@kotisivu.example")}});
        browser.saveAddress({{QStringLiteral("name"), QStringLiteral("Meri Laine")},
            {QStringLiteral("street"), QStringLiteral("Tehtaankatu 5")},
            {QStringLiteral("postalCode"), QStringLiteral("00140")},
            {QStringLiteral("city"), QStringLiteral("Helsinki")},
            {QStringLiteral("country"), QStringLiteral("Finland")},
            {QStringLiteral("email"), QStringLiteral("meri@work.example")}});
    }
    omaweb::ContentBlocker contentBlocker(dataRootPath, omaweb::ContentBlocker::DefaultLists::None);
    const auto keybindingsPath = dataRoot.filePath(QStringLiteral("keybindings.json"));
    omaweb::KeyboardNavigation::seedDefaults(
        keybindingsPath, QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH));
    omaweb::KeyboardNavigation keyboardNavigation(
        keybindingsPath, QStringLiteral(OMAWEB_KEYBOARD_NAVIGATION_SCRIPT_PATH));
    // The lab reviews chrome, and chrome is drawn in a palette, so it honours
    // the same override the browser does: `OMAWEB_THEME_FILE=<path>` reviews a
    // theme without installing it. Nothing else in the lab's search order —
    // the config directory, the desktop's theme — applies: a lab that read the
    // machine's theme would review a different palette on every machine.
    const auto themeOverride = qEnvironmentVariable("OMAWEB_THEME_FILE");
    omaweb::ThemeController theme(
        themeOverride.isEmpty() ? QStringLiteral(OMAWEB_THEME_PATH) : themeOverride);
    omaweb::WindowManager windowManager;
    // The size control is reviewed here; what it sets is written under the
    // lab's own data root rather than the reader's configuration.
    omaweb::FontSettings fontSettings(dataRootPath, QFontDatabase::families());
    omaweb::SidebarOpacity sidebarOpacity(dataRootPath);
    theme.followSidebarOpacity(&sidebarOpacity);
    // No engine runs here to report its own fonts, so the group is reviewed
    // over the values a Linux engine reports.
    fontSettings.setEngineFonts(
        {QStringLiteral("DejaVu Sans"), QStringLiteral("DejaVu Sans Mono"), 16, 0});

    omaweb::registerBrowserController();
    omaweb::registerDownloads();
    omaweb::registerFaviconTint();
    omaweb::registerFontSettings();
    omaweb::registerEngineCapabilities();
    omaweb::registerPageImages();
    omaweb::registerDefaultBrowser();
    omaweb::registerSystemClipboard();
    omaweb::registerInputOrigin();
    omaweb::registerPrimaryHold();
    omaweb::registerScenes();
    omaweb::registerExternalProtocolHandler();
    omaweb::registerPagePrinter();
    omaweb::registerSystemMotion();
    omaweb::registerSystemNotifier();
    omaweb::registerSoundingTabs();
    omaweb::registerMediaAnnouncer();
    omaweb::registerProcessResources();
    omaweb::registerSavedDownload();
    static omaweb::RuntimeSecurity runtimeSecurity({}, {});
    omaweb::registerRuntimeSecurity(&runtimeSecurity);
    // The lab reviews chrome rather than the desktop it runs on, so the report
    // it draws is of a desktop that asked for no input method.
    static omaweb::InputMethodReport inputMethod {omaweb::InputMethodHost {}};
    omaweb::registerInputMethodReport(&inputMethod);
    omaweb::registerLocaleReport(&localeReport);
    QQmlApplicationEngine engine;
    omaweb::quickshell::installShim(engine);
    omaweb::installStoredFavicons(engine);
    engine.rootContext()->setContextProperty(QStringLiteral("browser"), &browser);
    // `--agent-activity` seeds what the Agent activity page lists.
    omaweb::AgentActivityLog agentActivity(dataRoot.filePath(QStringLiteral("agent-activity")));
    if (arguments.contains(QStringLiteral("--agent-activity"))) {
        seedAgentActivity(agentActivity);
    }
    engine.rootContext()->setContextProperty(QStringLiteral("agentActivity"), &agentActivity);
    engine.rootContext()->setContextProperty(QStringLiteral("contentBlocker"), &contentBlocker);
    engine.rootContext()->setContextProperty(
        QStringLiteral("keyboardNavigation"), &keyboardNavigation);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
    // The lab runs no engine, so there is no third-party filter to attach.
    // Site information reads the gap off the adapter's capabilities.
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineCookiePolicy"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineHeldDownloads"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("fontSettings"), &fontSettings);
    engine.rootContext()->setContextProperty(QStringLiteral("sidebarOpacity"), &sidebarOpacity);
    // The file the lab's own settings are written to, under its data root, so
    // Settings names a file and says what is wrong with it as it would.
    omaweb::SettingsFile settingsFile(dataRootPath);
    engine.rootContext()->setContextProperty(QStringLiteral("settingsFile"), &settingsFile);
    engine.rootContext()->setContextProperty(
        QStringLiteral("pageFonts"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("windowManager"), &windowManager);
    engine.rootContext()->setContextProperty(
        QStringLiteral("syncLauncher"), static_cast<QObject *>(nullptr));
    // A release the lab is behind, so the Release mark is there to be reviewed.
    // The lab asks GitHub nothing: what is being drawn is the mark, and the
    // answer it would get depends on when somebody last cut a release.
    static omaweb::ReleaseWatch releaseWatch(
        QStringLiteral("0.0.1"), omaweb::ReleaseWatch::Ask::Never);
    releaseWatch.showRelease(QStringLiteral("v9.9.9"));
    engine.rootContext()->setContextProperty(QStringLiteral("releaseWatch"), &releaseWatch);
    // The switch is reviewed here; what it flips is written under the lab's
    // own data root rather than the reader's configuration.
    static omaweb::GlobalPrivacyControl globalPrivacyControl(dataRootPath);
    engine.rootContext()->setContextProperty(
        QStringLiteral("globalPrivacyControl"), &globalPrivacyControl);
    static omaweb::WebRtcPolicy webRtcPolicy(dataRootPath);
    static omaweb::SecureDns secureDns(dataRootPath);
    engine.rootContext()->setContextProperty(QStringLiteral("secureDns"), &secureDns);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineSecureDns"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(QStringLiteral("webRtcPolicy"), &webRtcPolicy);
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineSuggestions"), &engineSuggestions);
    // The lab runs no engine, so there is nothing to report unreachable.
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineWebRtcPolicy"), QVariant::fromValue<QObject *>(nullptr));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_VIEW_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("engineProfileSource"), QUrl(QStringLiteral(OMAWEB_ENGINE_PROFILE_URL)));
    engine.rootContext()->setContextProperty(
        QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
    const auto mockFavicons = drawMockFavicons(dataRoot.filePath(QStringLiteral("favicons")));
    engine.rootContext()->setContextProperty(QStringLiteral("mockFaviconUrls"), mockFavicons);
    // `--browse` shows the browser in use: the seeded day ends on a page, and
    // the stand-in view draws a sample one where it would otherwise say no
    // engine is running.
    const auto browse = arguments.contains(QStringLiteral("--browse"));
    engine.rootContext()->setContextProperty(QStringLiteral("labSamplePages"), browse);
    engine.addImportPath(QStringLiteral(OMAWEB_UI_DIRECTORY));
    // The vendored Omarchy component kit: qs.Ui and qs.Commons.
    engine.addImportPath(QStringLiteral(OMAWEB_OMARCHY_IMPORT_PATH));
    // The kit's own colour and type come from an Omarchy theme on disk. Omaweb's
    // palette is the source of truth, so it is pushed into the kit's singletons
    // once the engine can resolve them.
    omaweb::KitTheme kitTheme(&engine, &theme, &fontSettings);

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    if (!suggestUrl.isEmpty()) {
        // The default engine keeps DuckDuckGo's name and query address, so
        // the rows read as a reader's would. The preset already holds the id
        // `duckduckgo`, so the engine added under the same name is given
        // `duckduckgo-2`, and the preset goes.
        browser.addSearchEngine(QStringLiteral("DuckDuckGo"),
            QStringLiteral("https://duckduckgo.com/?q={query}"), {}, suggestUrl);
        browser.setDefaultSearchEngine(QStringLiteral("duckduckgo-2"));
        browser.deleteSearchEngine(QStringLiteral("duckduckgo"));
        engineSuggestions.setEnabled(true);
    }
    // `--many-spaces` seeds two more of the reader's Spaces, each taking a
    // colour of its own, and with `--agents` six more Agent Spaces, so the
    // footer runs out of room and counts the rest.
    const auto manySpaces = arguments.contains(QStringLiteral("--many-spaces"));
    if (manySpaces) {
        for (const auto *name : {"Home", "Travel"}) {
            browser.createSpace(QString::fromUtf8(name));
        }
    }
    // `--spaces` seeds the Spaces to switch between; see seedSampleSpaces.
    if (arguments.contains(QStringLiteral("--spaces"))) {
        seedSampleSpaces(browser, mockFavicons);
    }
    // `--projects` makes two Spaces projects' (`omaweb dev`), so their rows in
    // Settings can be reviewed: one whose folder is here, with an agent command
    // of its own, and one recorded inside a container, whose folder is not.
    if (arguments.contains(QStringLiteral("--projects"))) {
        const auto shop = dataRoot.filePath(QStringLiteral("code/shop"));
        QDir().mkpath(shop);
        browser.createProjectSpace({.directory = shop,
            .address = QStringLiteral("http://localhost:5173"),
            .agentCommand = QStringLiteral("incus exec dev --cwd {dir} -- claude")});
        browser.createProjectSpace({.directory = QStringLiteral("/workspace/blog"),
            .address = QStringLiteral("http://localhost:4321"),
            .agentCommand = {}});
    }
    // `--agents` has an Agent at work, so its marks can be reviewed: an Agent
    // Space it made with a tab it is driving, on show, and a second Agent Space
    // no Agent is using. `--agents-away` leaves the reader's first Space on
    // show instead, with the Agent's Space marked in the footer.
    // `--agents-window` has the Agent's page open an Auxiliary window, and the
    // capture is of that window. `--agents-grant` has the Agent ask for the
    // reader's page on show, so the grant prompt stands over it. The Agent is
    // the real Agent rules answered by the stand-in page.
    const auto agentsGrant = arguments.contains(QStringLiteral("--agents-grant"));
    const auto agentsAway = agentsGrant || arguments.contains(QStringLiteral("--agents-away"));
    const auto agentsWindow = arguments.contains(QStringLiteral("--agents-window"));
    const auto agentsTakenOver = arguments.contains(QStringLiteral("--agents-taken-over"));
    const auto agents = agentsAway || agentsWindow || agentsTakenOver
        || arguments.contains(QStringLiteral("--agents"));
    std::optional<omaweb::AgentControl> agentControl;
    const auto agentName = QStringLiteral("claude-code");
    QString agentTabId;
    QString agentSpaceId;
    if (agents) {
        agentControl.emplace(&browser, dataRoot.filePath(QStringLiteral("config")));
        agentControl->setAllowAgents(true);
        const auto made = agentControl->answer({
            {QStringLiteral("verb"), QStringLiteral("space new")},
            {QStringLiteral("name"), agentName},
            {QStringLiteral("space"), QStringLiteral("Review")},
        });
        agentSpaceId
            = made.value(QStringLiteral("space")).toObject().value(QStringLiteral("id")).toString();
        const auto opened = agentControl->answer({
            {QStringLiteral("verb"), QStringLiteral("open")},
            {QStringLiteral("name"), agentName},
            {QStringLiteral("url"), QStringLiteral("https://forge.example/omaweb/pull/384")},
        });
        agentTabId
            = opened.value(QStringLiteral("tab")).toObject().value(QStringLiteral("id")).toString();
        // Once Review is the reader's, the Agent is still at work in a Space
        // of its own, so both an attached Agent Space and a Space of the
        // reader's wearing the mark in its own colour are in the footer.
        if (agentsTakenOver) {
            agentControl->answer({
                {QStringLiteral("verb"), QStringLiteral("space new")},
                {QStringLiteral("name"), agentName},
                {QStringLiteral("space"), QStringLiteral("Crawl")},
            });
            agentControl->answer({
                {QStringLiteral("verb"), QStringLiteral("open")},
                {QStringLiteral("name"), agentName},
                {QStringLiteral("url"), QStringLiteral("https://docs.example/crawl")},
            });
        }
        browser.createAgentSpace(QStringLiteral("Scratch"), agentName);
        if (manySpaces) {
            for (const auto *name : {"Signup flow", "Pricing check", "Docs crawl", "Changelog",
                     "Benchmarks", "Triage"}) {
                browser.createAgentSpace(QString::fromUtf8(name), agentName);
            }
        }
        engine.rootContext()->setContextProperty(
            QStringLiteral("agentControl"), &agentControl.value());
    }
    engine.load(QUrl(QStringLiteral(OMAWEB_MAIN_QML_URL)));
    // A click, once the page is up, so the frame's label has an act to name.
    // The stand-in page reports the name a step carries as the element's. Not
    // in a Space the reader has taken over, where it would ask for a grant.
    if (agentControl && !agentTabId.isEmpty() && !agentsTakenOver) {
        QTimer::singleShot(200, &application, [&agentControl, agentName, agentTabId] {
            agentControl->handle(
                {
                    {QStringLiteral("verb"), QStringLiteral("do")},
                    {QStringLiteral("name"), agentName},
                    {QStringLiteral("tab"), agentTabId},
                    {QStringLiteral("steps"),
                        QJsonArray {QJsonObject {
                            {QStringLiteral("action"), QStringLiteral("click")},
                            {QStringLiteral("target"), QStringLiteral("12")},
                            {QStringLiteral("name"), QStringLiteral("Files changed")},
                        }}},
                },
                [](const QJsonObject &) { });
        });
    }
    if (agentControl && agentsGrant) {
        QTimer::singleShot(300, &application, [&agentControl, &browser, agentName] {
            agentControl->handle(
                {
                    {QStringLiteral("verb"), QStringLiteral("look")},
                    {QStringLiteral("name"), agentName},
                    {QStringLiteral("tab"), browser.activeTabId()},
                },
                [](const QJsonObject &) { });
        });
    }
    if (agentsWindow && !engine.rootObjects().isEmpty()) {
        auto *root = engine.rootObjects().constFirst();
        QTimer::singleShot(400, &application, [root] {
            auto *host = root->findChild<QObject *>(QStringLiteral("engineLoader"));
            auto *page = host ? host->property("item").value<QObject *>() : nullptr;
            if (page == nullptr) {
                qCritical("No Agent page to open a window from for --agents-window");
                return;
            }
            QMetaObject::invokeMethod(page, "simulateNewWindowRequest",
                Q_ARG(QVariant, QStringLiteral("https://forge.example/login/oauth")),
                Q_ARG(QVariant, true));
        });
    }

    // What the Start page says under the reader's locale, for a test to read
    // the catalogue back from the screen's own text.
    if (arguments.contains(QStringLiteral("--report-start-page"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *word = engine.rootObjects().constFirst()->findChild<QObject *>(
            QStringLiteral("startPageHintWord"));
        if (word == nullptr) {
            qCritical("The Start page has no hint");
            return 1;
        }
        printf("start_page_hint=%s\n", qPrintable(word->property("text").toString()));
        fflush(stdout);
        return 0;
    }

    // One prompt and one context-menu entry as the window words them under the
    // reader's locale, for a test to read the catalogue back from the chrome's
    // own functions rather than from a copy of its strings.
    if (arguments.contains(QStringLiteral("--report-chrome"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *window = engine.rootObjects().constFirst();
        QVariantMap request;
        request.insert(QStringLiteral("name"), QStringLiteral("Forge"));
        request.insert(QStringLiteral("spaceName"), QStringLiteral("Work"));
        QVariant prompt;
        QMetaObject::invokeMethod(
            window, "grantPrompt", Q_RETURN_ARG(QVariant, prompt), Q_ARG(QVariant, request));
        QVariantMap context;
        context.insert(QStringLiteral("linkUrl"), QStringLiteral("https://example.test/"));
        QVariant menu;
        QMetaObject::invokeMethod(
            window, "pageMenuFor", Q_RETURN_ARG(QVariant, menu), Q_ARG(QVariant, context));
        // A JavaScript object or array comes back as a `QJSValue` or already plain.
        const auto plain = [](const QVariant &value) {
            return value.metaType() == QMetaType::fromType<QJSValue>()
                ? value.value<QJSValue>().toVariant()
                : value;
        };
        const auto rows = plain(menu).toList();
        QString entry;
        for (const auto &row : rows) {
            if (row.toMap().value(QStringLiteral("run")) == QStringLiteral("copy-link")) {
                entry = row.toMap().value(QStringLiteral("label")).toString();
            }
        }
        printf("prompt_message=%s\n",
            qPrintable(plain(prompt).toMap().value(QStringLiteral("message")).toString()));
        printf("menu_entry=%s\n", qPrintable(entry));
        fflush(stdout);
        return 0;
    }

    // What Settings and Site information say under the reader's locale: the Language row, the
    // Settings heading, and the verdict Site information gives for a window
    // with no page loaded.
    if (arguments.contains(QStringLiteral("--report-settings"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *root = engine.rootObjects().constFirst();
        // Built after the first frame otherwise, which a report does not wait for.
        QMetaObject::invokeMethod(root, "buildSettings");
        auto *heading = root->findChild<QObject *>(QStringLiteral("settingsHeading"));
        auto *connection = root->findChild<QObject *>(QStringLiteral("siteInformationVerdict"));
        auto *language = root->findChild<QObject *>(QStringLiteral("languageRow"));
        if (heading == nullptr || connection == nullptr || language == nullptr) {
            qCritical("Settings or Site information is missing");
            return 1;
        }
        printf("language_title=%s\n", qPrintable(language->property("title").toString()));
        printf("language_note=%s\n", qPrintable(language->property("note").toString()));
        printf("settings_heading=%s\n", qPrintable(heading->property("text").toString()));
        printf("site_information_state=%s\n", qPrintable(connection->property("text").toString()));
        fflush(stdout);
        return 0;
    }

    // Flips the Floating controls toggle as a reader does and reports what
    // the browser stored, so a test can check that a setting changed under a
    // translated chrome is written under its English key and value.
    if (arguments.contains(QStringLiteral("--report-setting-change"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *root = engine.rootObjects().constFirst();
        QMetaObject::invokeMethod(root, "buildSettings");
        auto *heading = root->findChild<QObject *>(QStringLiteral("settingsHeading"));
        auto *toggle = root->findChild<QObject *>(QStringLiteral("floatingControls"));
        if (heading == nullptr || toggle == nullptr) {
            qCritical("Settings has no Floating controls toggle");
            return 1;
        }
        QMetaObject::invokeMethod(toggle, "clicked");
        printf("settings_heading=%s\n", qPrintable(heading->property("text").toString()));
        printf("stored_floating_controls=%s\n",
            qPrintable(browser.preference(QStringLiteral("floating-controls"))));
        fflush(stdout);
        return 0;
    }

    // The row the command panel lists for one command: the title the Omnibar,
    // the Start page and the shortcut sheet show, and the identifier and keys
    // Sync projects, which stay untranslated. A test asks by identifier.
    const auto commandIndex = arguments.indexOf(QStringLiteral("--report-command-title"));
    if (commandIndex >= 0 && commandIndex + 1 < arguments.size()) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *commands = engine.rootObjects().constFirst()->findChild<QObject *>(
            QStringLiteral("browserCommands"));
        if (commands == nullptr) {
            qCritical("The window has no command registry");
            return 1;
        }
        QVariant listed;
        QMetaObject::invokeMethod(commands, "actions", Q_RETURN_ARG(QVariant, listed));
        const auto wanted = arguments.at(commandIndex + 1);
        const auto entries = listed.canConvert<QJSValue>()
            ? listed.value<QJSValue>().toVariant().toList()
            : listed.toList();
        for (const auto &entry : entries) {
            const auto action = entry.toMap();
            if (action.value(QStringLiteral("command")).toString() == wanted) {
                for (const auto &field :
                    {QStringLiteral("title"), QStringLiteral("command"), QStringLiteral("keys")}) {
                    printf("command_%s=%s\n", qPrintable(field),
                        qPrintable(action.value(field).toString()));
                }
                fflush(stdout);
                return 0;
            }
        }
        qCritical("The command panel does not list %s", qPrintable(wanted));
        return 1;
    }

    // The two startup numbers the tests keep, in milliseconds since `main`:
    // the first frame the window drew, and the first frame with the visible
    // Space's page in it. A Space at rest has no page to draw, so the report
    // ends at the first frame; anything else waits for the page.
    if (arguments.contains(QStringLiteral("--report-startup"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (window == nullptr) {
            qCritical("The root object is not a window");
            return 1;
        }
        auto *engineLoader = window->findChild<QObject *>(QStringLiteral("engineLoader"));
        QObject::connect(window, &QQuickWindow::frameSwapped, window,
            [engineLoader, &startup, &browser, firstFrame = true, done = false]() mutable {
                if (done) {
                    return;
                }
                const auto elapsed = startup.nsecsElapsed() / 1e6;
                if (firstFrame) {
                    firstFrame = false;
                    printf("first_frame_milliseconds=%.1f\n", elapsed);
                    if (browser.atRest()) {
                        done = true;
                        fflush(stdout);
                        QCoreApplication::quit();
                        return;
                    }
                }
                if (engineLoader == nullptr
                    || engineLoader->property("item").value<QObject *>() == nullptr) {
                    return;
                }
                printf("page_frame_milliseconds=%.1f\n", elapsed);
                done = true;
                fflush(stdout);
                QCoreApplication::quit();
            });
    }

    // The lab comes up on a Space at rest, which draws neither the Pinned
    // section nor the tab list, so the sidebar that distinguishes this browser
    // is the one thing a capture of it cannot show. `--tabs` seeds a day.
    if (arguments.contains(QStringLiteral("--tabs"))) {
        seedSampleTabs(browser, mockFavicons, browse ? QString::fromUtf8(browsedTab) : QString());
    }
    // `--put-away` has the sample day's ordinary tabs last shown one by one
    // over the past few days, then lets the check put away those past twelve
    // hours, as a first start after a long weekend would. The History sheet
    // lists them and the notice says so.
    if (arguments.contains(QStringLiteral("--put-away"))) {
        const auto shown = browser.activeTabId();
        auto *unpinned = browser.unpinnedTabs();
        const auto role = unpinned->roleNames().key(QByteArrayLiteral("tabId"), -1);
        QStringList ordinary;
        for (int row = 0; row < unpinned->rowCount(); ++row) {
            ordinary.append(unpinned->data(unpinned->index(row, 0), role).toString());
        }
        constexpr qint64 hour = 60 * 60 * 1000;
        const auto now = QDateTime::currentMSecsSinceEpoch();
        for (qsizetype index = 0; index < ordinary.size(); ++index) {
            browser.setNowForTests(now - (ordinary.size() - index) * 9 * hour);
            browser.activateTab(ordinary.at(index));
        }
        browser.setNowForTests(now - hour);
        browser.activateTab(shown);
        browser.setNowForTests(0);
        browser.putAwayUnusedTabs();
    }
    // After the sample day, which is seeded into the Space on show.
    if (agents && !agentsAway && browser.switchSpace(agentSpaceId)) {
        browser.activateTab(agentTabId);
    }
    // `--agents-taken-over` has the reader take the Agent's Space over while
    // the Agent is still attached, so a Space of the reader's wears the mark.
    if (agentsTakenOver) {
        browser.takeOverSpace(agentSpaceId);
    }
    // `--narrow` puts the sidebar at its minimum width.
    if (arguments.contains(QStringLiteral("--narrow")) && !engine.rootObjects().isEmpty()) {
        auto *window = engine.rootObjects().constFirst();
        window->setProperty("sidebarWidth", window->property("sidebarMinimumWidth"));
    }
    // `--sidebar-right` stands the sidebar against the window's right edge, as
    // Settings' interface section does.
    if (arguments.contains(QStringLiteral("--sidebar-right")) && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().constFirst()->setProperty("sidebarSide", QStringLiteral("right"));
    }
    // `--tint-favicons` turns site colour on, as Settings' interface section
    // does, so the active pin wears its site's colour.
    if (arguments.contains(QStringLiteral("--tint-favicons")) && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().constFirst()->setProperty("tintFavicons", true);
    }
    // `--active-pin` puts the first pin on show, so its mark can be seen.
    if (arguments.contains(QStringLiteral("--active-pin"))) {
        if (const auto pin = tabIdAt(browser.pinnedTabs(), 0); !pin.isEmpty()) {
            browser.activateTab(pin);
        }
    }
    // `--scene <id>` stands the Start page on that Scene, as Settings'
    // interface section does: `crt-road`, `night-sky`, `game-of-life`,
    // `vector-terrain`, `radar`, `hyperspace` or `none`.
    const auto sceneIndex = arguments.indexOf(QStringLiteral("--scene"));
    if (sceneIndex >= 0 && sceneIndex + 1 < arguments.size() && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().constFirst()->setProperty(
            "startPageScene", arguments.at(sceneIndex + 1));
    }
    // `--scene-time <seconds>` holds the Start page's Scene still at that time
    // on its clock, so a capture shows a moment that comes and goes, such as a
    // comet catching the Omnibar's rim. The clock is stepped there a frame at
    // a time once the `--show` state is in place, so whatever the Scene eases,
    // such as a drive, has eased as far as it would have.
    const auto sceneTimeIndex = arguments.indexOf(QStringLiteral("--scene-time"));
    if (sceneTimeIndex >= 0 && sceneTimeIndex + 1 < arguments.size()
        && !engine.rootObjects().isEmpty()) {
        auto *window = engine.rootObjects().constFirst();
        const auto until = arguments.at(sceneTimeIndex + 1).toDouble();
        QTimer::singleShot(0, window, [window, until] {
            auto *host = window->findChild<QObject *>(QStringLiteral("startPageScene"));
            if (host == nullptr) {
                return;
            }
            // An assignment in QML, which lets go of the binding that would
            // start the clock again; a write from here would leave it.
            QQmlExpression(qmlContext(host), host, QStringLiteral("running = false")).evaluate();
            constexpr auto frame = 1.0 / 30;
            for (auto time = 0.0; time < until; time += frame) {
                host->setProperty("time", std::min(time + frame, until));
            }
        });
    }
    // `--space-overflow` opens the menu of the Spaces the footer left out, which
    // is a click on its count. Late enough that a compositor has given the
    // window its size: the menu hangs from where the count stands then.
    if (arguments.contains(QStringLiteral("--space-overflow")) && !engine.rootObjects().isEmpty()) {
        auto *window = engine.rootObjects().constFirst();
        QTimer::singleShot(1500, window, [window] {
            if (auto *sidebar = window->findChild<QObject *>(QStringLiteral("sidebar"))) {
                QMetaObject::invokeMethod(sidebar, "openHiddenSpaces");
            }
        });
    }
    // Private chrome is a whole palette of its own, and the lab is where it is
    // reviewed. Nothing else about the window changes.
    if (arguments.contains(QStringLiteral("--private")) && !engine.rootObjects().isEmpty()) {
        engine.rootObjects().constFirst()->setProperty("privateWindow", true);
    }
    // The chromeless state and the two places — settings and history — are
    // reached by a key or a click in the browser, which a capture cannot
    // press. Naming the state is how the lab reviews them.
    //
    // A state is a list of properties rather than one flag, because settings is
    // many sections deep and a section can have a dialog standing over it. An
    // empty object name means the window; anything else is found by the name
    // the surface already carries, so naming a state costs the browser nothing.
    // "settings" alone still opens the page on whatever section it was left on.
    struct ShowProperty {
        const char *object;
        const char *property;
        QVariant value;
    };
    const auto showIndex = arguments.indexOf(QStringLiteral("--show"));
    if (showIndex >= 0 && showIndex + 1 < arguments.size() && !engine.rootObjects().isEmpty()) {
        static const QHash<QString, QList<ShowProperty>> states = {
            {QStringLiteral("collapsed"),
                {{"", "sidebarCollapsed", true}, {"", "sidebarPeeked", false}}},
            {QStringLiteral("peek"),
                {{"", "sidebarCollapsed", true}, {"", "sidebarPeeked", true},
                    {"", "floatingControls", false}}},
            {QStringLiteral("settings"), {{"", "settingsOpen", true}}},
            {QStringLiteral("settings:clear"),
                {{"", "settingsOpen", true}, {"settingsSurface", "clearDataOpen", true}}},
            {QStringLiteral("history"), {{"", "historyOpen", true}}},
            {QStringLiteral("shortcuts"), {{"", "shortcutsOpen", true}}},
            // The Start page's road without its CRT glass, as the Settings
            // interface section leaves it.
            {QStringLiteral("plain-road"), {{"", "startPageGlass", false}}},
            // The Start page's Scene as a commit leaves it until the page
            // paints: the road speeding up, the sky's stars streaking.
            {QStringLiteral("drive"), {{"", "startPageDriving", true}}},
            // Opens the Agent activity page in a new tab, as its command does.
            // `--agent-filter` picks one Agent's lines.
            {QStringLiteral("agent-activity"), {}},
            // The last seeded tab's page asks for notifications, so the
            // question bar stands over it. `--private` shows the Private
            // window's wording.
            {QStringLiteral("permission"), {}},
            // The same page asks a JavaScript question, so the prompt bar
            // stands over it.
            {QStringLiteral("prompt"), {}},
            // The same page asks for a security key, at each step the prompt
            // has: the touch, the PIN and a wrong one, the account chooser and
            // a failure. `--private` shows the Private window's wording.
            {QStringLiteral("security-key:touch"), {}},
            {QStringLiteral("security-key:pin"), {}},
            {QStringLiteral("security-key:pin-wrong"), {}},
            {QStringLiteral("security-key:accounts"), {}},
            {QStringLiteral("security-key:failed"), {}},
            // The last seeded tab's page has an email field focused with "me"
            // typed into it, over values the Space remembers for that field,
            // so form history's suggestion list stands under it with its
            // first row highlighted.
            {QStringLiteral("form-suggestions"), {}},
            // The same page's email field is marked as part of an address and
            // focused empty, so the saved addresses (`--sample-addresses`)
            // stand above the field's form history.
            {QStringLiteral("address-suggestions"), {}},
            // The Settings addresses section with an address's fields open.
            {QStringLiteral("settings:add-address"), {{"settingsSurface", "addressEditing", true}}},
            // The same page's card number field, pressed and empty, with the
            // saved cards (`--sample-cards`) under it.
            {QStringLiteral("card-suggestions"), {}},
            // The same page has just had a card typed and submitted, and the
            // bar offers to save it.
            {QStringLiteral("card-save-offer"), {}},
            // The Settings payment cards section with a card's fields open.
            {QStringLiteral("settings:add-card"), {{"settingsSurface", "cardEditing", true}}},
            // `:ask` with Allow agents off: the question that offers to turn
            // it on stands over the last seeded tab's page.
            {QStringLiteral("ask"), {{"", "agentQuestionOpen", true}}},
            // The last two seeded tabs side by side, the last one active.
            {QStringLiteral("split"), {{"", "sidebarPeeked", false}}},
            // Steps to the next Space shortly before a capture, so the frame
            // is taken part way through the list's arrival.
            {QStringLiteral("space-step"), {}},
            {QStringLiteral("space-settled"), {}},
            {QStringLiteral("omnibar-step"), {}},
            {QStringLiteral("omnibar-settled"), {}},
            {QStringLiteral("tab-step"), {}},
            {QStringLiteral("tab-settled"), {}},
            {QStringLiteral("settings-step"), {}},
            {QStringLiteral("settings-settled"), {}},
        };
        const auto requested = arguments.at(showIndex + 1);
        auto *root = engine.rootObjects().constFirst();

        // A visible page gives the peek capture detail whose blur can be
        // reviewed. The other seeded captures keep the blank tab active.
        if (requested == QLatin1String("peek") || requested == QLatin1String("ask")) {
            const auto tabId = lastTabId(browser.unpinnedTabs());
            if (!tabId.isEmpty()) {
                browser.activateTab(tabId);
            }
        }
        // Both are asked by the page on show rather than set on the window.
        const auto securityKey = requested.startsWith(QLatin1String("security-key:"));
        const auto pageAsks = requested == QLatin1String("permission")
            || requested == QLatin1String("prompt")
            || requested == QLatin1String("form-suggestions")
            || requested == QLatin1String("address-suggestions")
            || requested == QLatin1String("card-suggestions")
            || requested == QLatin1String("card-save-offer") || securityKey;
        if (pageAsks) {
            const auto tabId = lastTabId(browser.unpinnedTabs());
            if (tabId.isEmpty()) {
                qCritical("--show %s needs a page; pass --tabs", qPrintable(requested));
                return 1;
            }
            browser.activateTab(tabId);
            // The page's engine is built once the tab is on show, so the
            // question waits for it.
            QTimer::singleShot(300, root, [root, requested, securityKey, &browser] {
                auto *host = root->findChild<QObject *>(QStringLiteral("engineLoader"));
                auto *view = host ? host->property("item").value<QObject *>() : nullptr;
                if (view == nullptr) {
                    qCritical("No page to ask from for --show %s", qPrintable(requested));
                    return;
                }
                const auto origin
                    = view->property("currentUrl")
                          .toUrl()
                          .adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);
                if (securityKey) {
                    QMetaObject::invokeMethod(view, "simulateSecurityKey",
                        Q_ARG(QVariant, securityKeyStep(requested.section(QLatin1Char(':'), 1))));
                    return;
                }
                if (requested == QLatin1String("address-suggestions")) {
                    const auto spaceId = view->property("spaceId").toString();
                    for (const auto &value :
                        {QStringLiteral("someone@elsewhere.example"),
                            QStringLiteral("me@work.example"), QStringLiteral("me@home.example")}) {
                        browser.rememberFormFields(spaceId,
                            {QVariantMap {{QStringLiteral("name"), QStringLiteral("email")},
                                {QStringLiteral("value"), value}}});
                    }
                    QMetaObject::invokeMethod(view, "simulateFormFieldFocus",
                        Q_ARG(QVariant, QStringLiteral("email")), Q_ARG(QVariant, QString()),
                        Q_ARG(QVariant, 240), Q_ARG(QVariant, 180), Q_ARG(QVariant, 320),
                        Q_ARG(QVariant, 34), Q_ARG(QVariant, QStringLiteral("email")));
                    return;
                }
                if (requested == QLatin1String("card-suggestions")) {
                    QMetaObject::invokeMethod(view, "simulateCardFieldFocus",
                        Q_ARG(QVariant, QStringLiteral("cc-number")), Q_ARG(QVariant, 240),
                        Q_ARG(QVariant, 180), Q_ARG(QVariant, 320), Q_ARG(QVariant, 34));
                    return;
                }
                if (requested == QLatin1String("card-save-offer")) {
                    QMetaObject::invokeMethod(view, "simulatePaymentCardSubmit",
                        Q_ARG(QVariant,
                            QVariantMap(
                                {{QStringLiteral("number"), QStringLiteral("4000056655665556")},
                                    {QStringLiteral("name"), QStringLiteral("Meri Laine")},
                                    {QStringLiteral("expiryMonth"), 3},
                                    {QStringLiteral("expiryYear"), 2031},
                                    {QStringLiteral("origin"), origin.toString()}})));
                    return;
                }
                if (requested == QLatin1String("form-suggestions")) {
                    const auto spaceId = view->property("spaceId").toString();
                    for (const auto &value :
                        {QStringLiteral("someone@elsewhere.example"),
                            QStringLiteral("meri@kotisivu.example"),
                            QStringLiteral("me@work.example"), QStringLiteral("me@home.example")}) {
                        browser.rememberFormFields(spaceId,
                            {QVariantMap {{QStringLiteral("name"), QStringLiteral("email")},
                                {QStringLiteral("value"), value}}});
                    }
                    QMetaObject::invokeMethod(view, "simulateFormFieldFocus",
                        Q_ARG(QVariant, QStringLiteral("email")),
                        Q_ARG(QVariant, QStringLiteral("me")), Q_ARG(QVariant, 240),
                        Q_ARG(QVariant, 180), Q_ARG(QVariant, 320), Q_ARG(QVariant, 34));
                    QTimer::singleShot(100, view, [view] {
                        QMetaObject::invokeMethod(
                            view, "simulateFormKey", Q_ARG(QVariant, QStringLiteral("down")));
                    });
                    return;
                }
                if (requested == QLatin1String("prompt")) {
                    QMetaObject::invokeMethod(view, "simulateJavaScriptPrompt",
                        Q_ARG(QVariant, QStringLiteral("confirm")),
                        Q_ARG(QVariant, origin.toString()),
                        Q_ARG(QVariant,
                            QStringLiteral("Leave this page? Changes you made may not be saved.")),
                        Q_ARG(QVariant, QString()));
                    return;
                }
                QMetaObject::invokeMethod(view, "simulateSitePermission",
                    Q_ARG(QVariant, origin.toString()),
                    Q_ARG(QVariant, QStringLiteral("notifications")));
            });
        }
        if (requested == QLatin1String("split")) {
            auto *unpinned = browser.unpinnedTabs();
            const auto rows = unpinned->rowCount();
            if (rows >= 2) {
                browser.activateTab(lastTabId(unpinned));
                browser.addSplit(
                    unpinned->data(unpinned->index(rows - 2, 0), Qt::UserRole + 1).toString());
                // `--split-partner` puts the other half on show.
                if (arguments.contains(QStringLiteral("--split-partner"))) {
                    browser.focusSplitPartner();
                }
            }
        }

        // Settings is built after the window's first frame, and a state that
        // stands on it has it built at once, as a reader asking for it does.
        if (requested.startsWith(QLatin1String("settings"))) {
            QMetaObject::invokeMethod(root, "buildSettings");
        }
        // Settings has a section for each part of the browser, and a review of
        // its layout wants a capture of each. The rail's own list is what names them, so the
        // section is looked up there rather than written down again here:
        // adding a section, or moving one, cannot leave the lab pointing at
        // the wrong page. `settings:privacy:clear` still stands the dialog on
        // the section it names.
        auto state = states.value(requested);
        auto parts = requested.split(QLatin1Char(':'));
        if (state.isEmpty() && parts.size() >= 2 && parts.first() == QLatin1String("settings")) {
            auto *surface = root->findChild<QObject *>(QStringLiteral("settingsSurface"));
            if (surface == nullptr) {
                qCritical("No settingsSurface to show %s on", qPrintable(requested));
                return 1;
            }
            const auto sections = surface->property("sections").toStringList();
            // The rail draws "content blocking"; a command line spells it
            // "content-blocking".
            auto wanted = parts.at(1);
            wanted.replace(QLatin1Char('-'), QLatin1Char(' '));
            const auto section = sections.indexOf(wanted);
            if (section < 0) {
                qCritical("No settings section named %s", qPrintable(parts.at(1)));
                return 1;
            }
            state = {{"", "settingsOpen", true}, {"settingsSurface", "section", section}};
            if (parts.size() > 2) {
                const auto rest = states.value(QStringLiteral("settings:") + parts.at(2));
                if (rest.isEmpty()) {
                    qCritical("Unknown --show state %s", qPrintable(requested));
                    return 1;
                }
                state.append(rest);
            }
        }
        // `--settings-scroll-end` scrolls the section on show to its foot, so a
        // capture shows a form that sits below the first screen of a long
        // section.
        if (state.size() > 1 && arguments.contains(QStringLiteral("--settings-scroll-end"))) {
            QTimer::singleShot(400, root, [root] {
                auto *pane = root->findChild<QObject *>(QStringLiteral("settingsPane"));
                // The visual parents, which reach the ScrollView's Flickable;
                // the object parents skip it.
                for (auto *item = pane; item != nullptr;
                    item = item->property("parent").value<QObject *>()) {
                    if (item->property("contentHeight").isValid()
                        && item->property("contentY").isValid()) {
                        const auto end = item->property("contentHeight").toReal()
                            - item->property("height").toReal();
                        item->setProperty("contentY", std::max(0.0, end));
                        return;
                    }
                }
            });
        }
        if (requested.endsWith(QLatin1String("-step"))
            || requested.endsWith(QLatin1String("-settled"))) {
            const auto delay = requested.endsWith(QLatin1String("-step")) ? 620 : 300;
            const auto what = requested.section(QLatin1Char('-'), 0, 0);
            // `--omnibar-query` types into the Omnibar once it is open, so a
            // capture shows the matches a query finds rather than the address
            // it opens on.
            const auto queryIndex = arguments.indexOf(QStringLiteral("--omnibar-query"));
            const auto query = queryIndex >= 0 && queryIndex + 1 < arguments.size()
                ? arguments.at(queryIndex + 1)
                : QString();
            QTimer::singleShot(delay, root, [root, what, query] {
                if (what == QLatin1String("space")) {
                    QMetaObject::invokeMethod(root, "stepSpace", Q_ARG(QVariant, 1));
                } else if (what == QLatin1String("tab")) {
                    QMetaObject::invokeMethod(root, "stepTab", Q_ARG(QVariant, 1));
                } else if (what == QLatin1String("settings")) {
                    QMetaObject::invokeMethod(root, "requestSettings");
                } else {
                    QMetaObject::invokeMethod(root, "openOmnibar", Q_ARG(QVariant, false));
                    if (query.isEmpty()) {
                        return;
                    }
                    // The panel fills its input with the address as it opens,
                    // so the query goes in once it has.
                    QTimer::singleShot(80, root, [root, query] {
                        auto *input = root->findChild<QObject *>(QStringLiteral("omnibarInput"));
                        if (input != nullptr) {
                            input->setProperty("text", query);
                        }
                    });
                }
            });
        } else if (requested == QLatin1String("agent-activity")) {
            browser.openAgentActivity();
            const auto filterIndex = arguments.indexOf(QStringLiteral("--agent-filter"));
            if (filterIndex >= 0 && filterIndex + 1 < arguments.size()) {
                const auto agent = arguments.at(filterIndex + 1);
                QTimer::singleShot(100, root, [root, agent] {
                    auto *filter
                        = root->findChild<QObject *>(QStringLiteral("agentActivityAgentFilter"));
                    if (filter != nullptr) {
                        filter->setProperty("value", agent);
                    }
                });
            }
        } else if (requested == QLatin1String("site")
            || requested.startsWith(QLatin1String("site:"))) {
            // Site information, opened as the lock opens it, or at a detail as
            // `site:certificate`, `site:blocked`, `site:cookies` or
            // `site:third-parties`. `--site-page` picks the page it is about:
            // `secure` (the default), `http`, `cert` for a certificate error
            // the reader waived, or `start` for the Start page. `--site-sample`
            // gives it what the lab has no engine for: refused requests, two
            // decided permissions, an allowed and two refused third parties,
            // and the site's cookies.
            const auto detail = requested.section(QLatin1Char(':'), 1);
            const auto pageIndex = arguments.indexOf(QStringLiteral("--site-page"));
            const auto page = pageIndex >= 0 && pageIndex + 1 < arguments.size()
                ? arguments.at(pageIndex + 1)
                : QStringLiteral("secure");
            const auto sample = arguments.contains(QStringLiteral("--site-sample"));
            // `--site-card-fills` has two saved cards filled into the page,
            // one into the page and one into a payment processor's frame.
            const auto cardFills = arguments.contains(QStringLiteral("--site-card-fills"));
            static const QHash<QString, QString> addresses = {
                {QStringLiteral("secure"), QStringLiteral("https://www.example.org/articles/42")},
                {QStringLiteral("http"), QStringLiteral("http://old.example.net/index.html")},
                {QStringLiteral("cert"), QStringLiteral("https://localhost:8443/app")},
            };
            if (page != QLatin1String("start") && !addresses.contains(page)) {
                qCritical("Unknown --site-page %s", qPrintable(page));
                return 1;
            }
            const auto address = addresses.value(page);
            if (!address.isEmpty()) {
                browser.openInput(address, false);
            }
            if (sample && !address.isEmpty()) {
                browser.setPermissionDecision(QUrl(address), QStringLiteral("camera"),
                    omaweb::BrowserController::AllowPersistently);
                browser.setPermissionDecision(QUrl(address), QStringLiteral("notifications"),
                    omaweb::BrowserController::Block);
                browser.allowThirdPartyCookies(QUrl(QStringLiteral("https://login.example-id.net")),
                    QStringLiteral("authentication"));
                QQmlComponent stub(&engine);
                stub.setData(R"QML(import QtQml
QtObject {
    property int refusalTallyGeneration: 0
    readonly property var sample: [
        { "address": "https://doubleclick.example/pixel", "canonicalName": "" },
        { "address": "https://metrics.shop.example/collect?id=7",
          "canonicalName": "trk-7f3.cdn-edge.example" },
        { "address": "https://fonts.cdn-edge.example/loader.js", "canonicalName": "" },
        { "address": "https://stats.social.example/beacon", "canonicalName": "" }
    ]
    function refusalTally(spaceId, pageAddress) { return 14; }
    function refusedRequests(spaceId, pageAddress) { return sample; }
    function siteEnabled(url) { return true; }
    function setSiteEnabled(url, enabled) {}
}
)QML",
                    QUrl());
                auto *refusals = stub.create();
                if (refusals == nullptr) {
                    qCritical("%s", qPrintable(stub.errorString()));
                    return 1;
                }
                refusals->setParent(root);
                for (const auto *name : {"sidebar", "siteInformationCard"}) {
                    auto *target = root->findChild<QObject *>(QString::fromLatin1(name));
                    if (target != nullptr) {
                        target->setProperty("blocker", QVariant::fromValue(refusals));
                    }
                }
            }
            QTimer::singleShot(400, root, [root, page, detail, sample, cardFills, address] {
                if (cardFills) {
                    auto *host = root->findChild<QObject *>(QStringLiteral("engineLoader"));
                    auto *view = host ? host->property("item").value<QObject *>() : nullptr;
                    const auto pageOrigin = QUrl(address).adjusted(
                        QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment);
                    if (view != nullptr) {
                        view->setProperty("paymentCardFills",
                            QVariantList {
                                QVariantMap {{QStringLiteral("last4"), QStringLiteral("4242")},
                                    {QStringLiteral("brand"), QStringLiteral("Visa")},
                                    {QStringLiteral("nickname"), QStringLiteral("Everyday")},
                                    {QStringLiteral("origin"), pageOrigin.toString()}},
                                QVariantMap {{QStringLiteral("last4"), QStringLiteral("4444")},
                                    {QStringLiteral("brand"), QStringLiteral("Mastercard")},
                                    {QStringLiteral("nickname"), QString()},
                                    {QStringLiteral("origin"),
                                        QStringLiteral("https://js.pay.example-psp.com")}}});
                    }
                }
                if (page == QLatin1String("cert")) {
                    auto *host = root->findChild<QObject *>(QStringLiteral("engineLoader"));
                    auto *view = host ? host->property("item").value<QObject *>() : nullptr;
                    if (view != nullptr) {
                        QMetaObject::invokeMethod(
                            view, "simulateCertificateError", Q_ARG(QVariant, QVariantMap()));
                        QMetaObject::invokeMethod(
                            root, "respondToCertificateError", Q_ARG(QVariant, true));
                    }
                }
                QMetaObject::invokeMethod(root, "openSiteInformation", Q_ARG(QVariant, detail));
                if (!sample) {
                    return;
                }
                auto *card = root->findChild<QObject *>(QStringLiteral("siteInformationCard"));
                if (card != nullptr) {
                    card->setProperty("refusedThirdParties",
                        QStringList {QStringLiteral("https://pay.example-psp.com"),
                            QStringLiteral("https://cdn.example-video.net")});
                    card->setProperty("cookieCount", 9);
                }
            });
        } else if (state.isEmpty() && !pageAsks) {
            qCritical("Unknown --show state %s", qPrintable(requested));
            return 1;
        }
        for (const auto &property : state) {
            auto *target = *property.object == '\0'
                ? root
                : root->findChild<QObject *>(QString::fromLatin1(property.object));
            if (target == nullptr) {
                qCritical("No %s to show %s on", property.object, qPrintable(requested));
                return 1;
            }
            target->setProperty(property.property, property.value);
        }
        // The hidden sidebar answers a pointer the lab does not have. The real
        // peek stays up because the pointer moves from the reveal edge into
        // it, and the browser's leave timer puts it away when it has not. The
        // offscreen platform parks its cursor at device pixel (10, 10), which
        // at a scale factor of two is inside the six-pixel reveal edge, so the
        // browser's hold timer summons the sidebar over a `collapsed` capture.
        // Repeat the named state until capture, so the shot is the state asked
        // for rather than the one the browser's timers arrive at.
        if (requested == QLatin1String("collapsed") || requested == QLatin1String("peek")) {
            const auto peeked = requested == QLatin1String("peek");
            auto *sidebarKeeper = new QTimer(root);
            sidebarKeeper->setInterval(16);
            QObject::connect(sidebarKeeper, &QTimer::timeout, root, [root, peeked] {
                root->setProperty("sidebarCollapsed", true);
                root->setProperty("sidebarPeeked", peeked);
                if (!peeked) {
                    return;
                }
                root->setProperty("floatingControls", false);
                const auto views = root->findChildren<QObject *>(QStringLiteral("mockEngineView"));
                for (auto *view : views) {
                    view->setProperty("blurReviewPattern", true);
                }
            });
            sidebarKeeper->start();
        }
    }
    // The palette the browser resolves, rather than the template it was
    // rendered from. A theme file names a handful of colours; Omaweb derives
    // every role it draws from them, floors the quiet ones against the ground
    // they sit on, and tints the private grounds. Anything drawing Omaweb's
    // colours outside Omaweb — the website's own palettes — has to read the
    // resolved roles or it is approximating them a second time.
    const auto paletteIndex = arguments.indexOf(QStringLiteral("--dump-palette"));
    if (paletteIndex >= 0 && paletteIndex + 1 < arguments.size()) {
        QFile out(arguments.at(paletteIndex + 1));
        if (!out.open(QIODevice::WriteOnly)) {
            qCritical("Could not write %s", qPrintable(arguments.at(paletteIndex + 1)));
            return 1;
        }
        out.write(QJsonDocument(QJsonObject::fromVariantMap(theme.palette())).toJson());
        out.close();
    }

    // `--sidebar-cursor <rows>` hands the sidebar the keyboard, which puts the
    // Sidebar cursor on the tab on show, and steps it that many rows down, or
    // up for a negative count. The seeded rows are built after the QML loads,
    // so the cursor is placed a moment later.
    const auto cursorIndex = arguments.indexOf(QStringLiteral("--sidebar-cursor"));
    if (cursorIndex >= 0 && cursorIndex + 1 < arguments.size() && !engine.rootObjects().isEmpty()) {
        const auto rows = arguments.at(cursorIndex + 1).toInt();
        auto *root = engine.rootObjects().constFirst();
        QTimer::singleShot(200, root, [root, rows] {
            QMetaObject::invokeMethod(root, "focusSidebar");
            if (auto *sidebar = root->findChild<QObject *>(QStringLiteral("sidebar"));
                sidebar != nullptr && rows != 0) {
                QMetaObject::invokeMethod(sidebar, "stepCursor", Q_ARG(QVariant, rows));
            }
        });
    }

    const auto captureIndex = arguments.indexOf(QStringLiteral("--capture"));
    if (captureIndex >= 0 && captureIndex + 1 < arguments.size()) {
        const auto capturePath = arguments.at(captureIndex + 1);
        // `--capture-delay` waits longer, for a state that takes a moment to
        // arrive.
        const auto delayIndex = arguments.indexOf(QStringLiteral("--capture-delay"));
        const auto delay = delayIndex >= 0 && delayIndex + 1 < arguments.size()
            ? arguments.at(delayIndex + 1).toInt()
            : 700;
        QTimer::singleShot(delay, &application, [&engine, capturePath, agentsWindow] {
            if (engine.rootObjects().isEmpty()) {
                QCoreApplication::exit(1);
                return;
            }
            auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
            if (agentsWindow && window != nullptr) {
                window = window->findChild<QQuickWindow *>(QStringLiteral("auxiliaryWindow"));
            }
            if (!window || !window->grabWindow().save(capturePath)) {
                qCritical("Could not capture %s", qPrintable(capturePath));
                QCoreApplication::exit(1);
                return;
            }
            QCoreApplication::quit();
        });
    }

    if (application.arguments().contains(QStringLiteral("--validate-qml"))) {
        if (engine.rootObjects().isEmpty()) {
            return 1;
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }

    return application.exec();
}
