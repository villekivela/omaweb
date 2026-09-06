// A benchmark harness for issue #100: it drives the real Qt engine view over
// deterministic local fixtures and records what one navigation costs. It is
// measurement only. The behaviour under test lives in EngineView.qml, the
// content matcher, and the Rust blocker, and this file must never stand in for
// any of them.
//
// The same source is copied into a worktree for each compared revision, so
// every fixture, rule set, and metric definition is identical on both sides.

#include "ContentMatcher.h"
#include "ExternalProtocolHandler.h"
#include "QtContentBlocker.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include <ctime>
#include <memory>
#include <unistd.h>

using omaweb::ContentMatcher;

static const QStringList fixtureNames {
    QStringLiteral("site-css"), QStringLiteral("scriptlets"), QStringLiteral("large-dom")};

namespace {

// Chromium stamps its trace events with base::TimeTicks, which is
// CLOCK_MONOTONIC in microseconds on Linux. Reading the same clock here is what
// lets the trace pass attribute style recalculations to one navigation.
qint64 monotonicMicroseconds()
{
    timespec now {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return qint64(now.tv_sec) * 1000000 + now.tv_nsec / 1000;
}

// CPU consumed by this process and every descendant: the browser process that
// runs QML and the matcher, and the Chromium renderer, GPU, and utility
// processes it starts. Wall-clock time is never a substitute, so this reads
// utime and stime out of /proc rather than timing the navigation.
struct ProcessTreeCpu {
    double milliseconds = 0;
    int processes = 0;
};

ProcessTreeCpu processTreeCpu()
{
    const long ticksPerSecond = sysconf(_SC_CLK_TCK);
    struct Entry {
        int parent = 0;
        double milliseconds = 0;
    };
    QHash<int, Entry> entries;
    const auto names
        = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::NoSort);
    for (const auto &name : names) {
        bool numeric = false;
        const int pid = name.toInt(&numeric);
        if (!numeric) {
            continue;
        }
        QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
        if (!stat.open(QIODevice::ReadOnly)) {
            continue;
        }
        const auto line = QString::fromUtf8(stat.readAll());
        // The second field is the executable name in parentheses and may itself
        // contain spaces, so the fields are counted from after it.
        const auto close = line.lastIndexOf(u')');
        if (close < 0) {
            continue;
        }
        const auto fields = QStringView(line).mid(close + 2).split(u' ', Qt::SkipEmptyParts);
        // Counting from the state field as field 3: ppid is 4, utime 14, stime 15.
        if (fields.size() < 13) {
            continue;
        }
        Entry entry;
        entry.parent = fields.at(1).toInt();
        entry.milliseconds = double(fields.at(11).toLongLong() + fields.at(12).toLongLong())
            * 1000.0 / double(ticksPerSecond);
        entries.insert(pid, entry);
    }

    ProcessTreeCpu total;
    const int self = int(getpid());
    for (auto it = entries.cbegin(); it != entries.cend(); ++it) {
        int walker = it.key();
        for (int depth = 0; depth < 64 && walker > 1; ++depth) {
            if (walker == self) {
                total.milliseconds += it.value().milliseconds;
                total.processes += 1;
                break;
            }
            const auto parent = entries.constFind(walker);
            if (parent == entries.cend()) {
                break;
            }
            walker = parent.value().parent;
        }
    }
    return total;
}

// One page, its host, and the rules written against that host. Each fixture
// answers on a loopback address of its own, so its site rules are written
// against a host the other fixtures never load and no name resolution is
// involved in reaching it.
struct Fixture {
    QString name;
    QString host;
    QByteArray body;
    QString rules;
    int elements = 0;
    int classNames = 0;
    int identifiers = 0;
};

// The generic selectors the survey has to answer for come out of the pinned
// lists themselves, in file order, so the fixture stays valid when the pins
// move and the page is identical for both compared revisions.
struct GenericSelectors {
    QStringList classNames;
    QStringList identifiers;
};

GenericSelectors genericSelectors(const QString &lists, int wanted)
{
    GenericSelectors selectors;
    static const QRegularExpression classRule(QStringLiteral("^##\\.([A-Za-z][A-Za-z0-9_-]*)$"));
    static const QRegularExpression identifierRule(QStringLiteral("^###([A-Za-z][A-Za-z0-9_-]*)$"));
    const auto lines = QStringView(lists).split(u'\n');
    for (const auto &line : lines) {
        if (selectors.classNames.size() >= wanted && selectors.identifiers.size() >= wanted) {
            break;
        }
        const auto text = line.trimmed().toString();
        const auto asClass = classRule.match(text);
        if (asClass.hasMatch() && selectors.classNames.size() < wanted) {
            selectors.classNames.append(asClass.captured(1));
            continue;
        }
        const auto asIdentifier = identifierRule.match(text);
        if (asIdentifier.hasMatch() && selectors.identifiers.size() < wanted) {
            selectors.identifiers.append(asIdentifier.captured(1));
        }
    }
    return selectors;
}

// The script every fixture carries. It watches the injected site stylesheet for
// the rewrite issue #100 is about, and reports the page's own paint timings.
// It publishes into document.title because the harness reads the view through
// the same contract the shell does.
QByteArray measurementScript(int settleMilliseconds)
{
    return QByteArray(R"JS(
<script>
(() => {
    const state = {
        siteStyleWatched: false,
        siteStyleMutations: 0,
        siteStyleBytes: 0,
        genericStyleBytes: 0,
        hiddenSample: 0,
        firstPaint: null,
        firstContentfulPaint: null,
        largestContentfulPaint: null,
        settled: false,
    };
    const publish = () => { document.title = JSON.stringify(state); };
    const styleNode = id => document.getElementById(id);
    // The document-creation script appends the site stylesheet as soon as the
    // first element exists, which may be after this script runs, so the watch
    // waits for the node rather than assuming it.
    const watch = () => {
        const style = styleNode("__omaweb_content_blocking");
        if (!style) return false;
        state.siteStyleWatched = true;
        new MutationObserver(records => {
            state.siteStyleMutations += records.length;
            publish();
        }).observe(style, { childList: true, characterData: true, subtree: true });
        return true;
    };
    if (!watch()) {
        const waiting = new MutationObserver(() => { if (watch()) waiting.disconnect(); });
        waiting.observe(document, { childList: true, subtree: true });
    }
    new PerformanceObserver(list => {
        for (const entry of list.getEntries()) {
            if (entry.name === "first-paint") state.firstPaint = entry.startTime;
            if (entry.name === "first-contentful-paint")
                state.firstContentfulPaint = entry.startTime;
        }
        publish();
    }).observe({ type: "paint", buffered: true });
    new PerformanceObserver(list => {
        for (const entry of list.getEntries())
            state.largestContentfulPaint = entry.startTime;
        publish();
    }).observe({ type: "largest-contentful-paint", buffered: true });
    setTimeout(() => {
        // What the page ended up hiding, so two builds can be compared on the
        // blocking they actually delivered and not only on what it cost.
        const site = styleNode("__omaweb_content_blocking");
        const generic = styleNode("__omaweb_content_blocking_generic");
        state.siteStyleBytes = site ? site.textContent.length : 0;
        state.genericStyleBytes = generic ? generic.textContent.length : 0;
        const sample = Array.from(document.querySelectorAll("div")).slice(0, 200);
        state.hiddenSample =
            sample.filter(node => getComputedStyle(node).display === "none").length;
        state.settled = true;
        publish();
    }, SETTLE);
    publish();
})();
</script>
)JS")
        .replace("SETTLE", QByteArray::number(settleMilliseconds));
}

Fixture buildFixture(const QString &name, const QString &lists, int settleMilliseconds)
{
    Fixture fixture;
    fixture.name = name;
    // Chromium resolves any name under .localhost to the loopback address
    // itself, so each fixture gets a hostname of its own without touching name
    // resolution. A bare loopback address would not do: EasyList exempts those
    // from generic hiding, and the generic survey is half of what is measured.
    fixture.host = name + QStringLiteral(".localhost");

    // Site rules the fixture owns, so a site-specific stylesheet and its
    // scriptlets exist for a host the pinned lists have never heard of.
    QStringList rules;
    QStringList body;
    const auto append = [&body](const QString &markup) { body.append(markup); };

    const int siteSelectors = 40;
    for (int index = 0; index < siteSelectors; ++index) {
        rules.append(QStringLiteral("%1##.fixture-site-ad-%2").arg(fixture.host).arg(index));
        rules.append(QStringLiteral("%1###fixture-site-box-%2").arg(fixture.host).arg(index));
    }

    // Scriptlets out of the vendored uBlock Origin library, named the way a
    // list names them. The scriptlet fixture carries many; the others carry one
    // so that document-creation injection is exercised everywhere.
    static const QStringList scriptletRules = {
        QStringLiteral("##+js(set-constant, __benchmarkConstant, true)"),
        QStringLiteral("##+js(abort-on-property-read, __benchmarkAbortRead)"),
        QStringLiteral("##+js(abort-on-property-write, __benchmarkAbortWrite)"),
        QStringLiteral("##+js(abort-current-script, __benchmarkAbortScript, benchmark)"),
        QStringLiteral("##+js(prevent-setTimeout, __benchmarkTimeout)"),
        QStringLiteral("##+js(prevent-setInterval, __benchmarkInterval)"),
        QStringLiteral("##+js(prevent-requestAnimationFrame, __benchmarkFrame)"),
        QStringLiteral("##+js(prevent-addEventListener, __benchmarkEvent)"),
        QStringLiteral("##+js(prevent-window-open, __benchmarkOpen)"),
        QStringLiteral("##+js(prevent-fetch, __benchmarkFetch)"),
        QStringLiteral("##+js(prevent-xhr, __benchmarkXhr)"),
        QStringLiteral("##+js(prevent-dialog)"),
        QStringLiteral("##+js(prevent-refresh)"),
        QStringLiteral("##+js(prevent-canvas)"),
        QStringLiteral("##+js(nowebrtc)"),
        QStringLiteral("##+js(alert-buster)"),
        QStringLiteral("##+js(overlay-buster)"),
        QStringLiteral("##+js(disable-newtab-links)"),
        QStringLiteral("##+js(window.name-defuser)"),
        QStringLiteral("##+js(remove-attr, data-benchmark, .fixture-attr)"),
        QStringLiteral("##+js(remove-class, fixture-removed, .fixture-removed)"),
        QStringLiteral("##+js(set-attr, .fixture-attr, data-benchmark, 1)"),
        QStringLiteral("##+js(json-prune, benchmark)"),
        QStringLiteral("##+js(href-sanitizer, a[href], ?url)"),
        QStringLiteral("##+js(adjust-setTimeout, __benchmarkAdjust, *, 0.1)"),
        QStringLiteral("##+js(adjust-setInterval, __benchmarkAdjust, *, 0.1)"),
        QStringLiteral("##+js(noeval-if, __benchmarkNoEval)"),
        QStringLiteral("##+js(set-local-storage-item, __benchmarkStore, 1)"),
        QStringLiteral("##+js(set-session-storage-item, __benchmarkSession, 1)"),
        QStringLiteral("##+js(remove-node-text, span, fixture)"),
        QStringLiteral("##+js(spoof-css, .fixture-column, visibility, visible)"),
        QStringLiteral("##+js(prevent-innerHTML, __benchmarkMarkup)"),
        QStringLiteral("##+js(call-nothrow, JSON.parse)"),
        QStringLiteral("##+js(remove-cookie, __benchmarkCookie)"),
        QStringLiteral("##+js(json-prune-fetch-response, benchmark)"),
        QStringLiteral("##+js(json-prune-xhr-response, benchmark)"),
        QStringLiteral("##+js(xml-prune, benchmark)"),
        QStringLiteral("##+js(m3u-prune, benchmark)"),
    };
    const int scriptlets = name == QStringLiteral("scriptlets") ? scriptletRules.size() : 1;
    for (int index = 0; index < scriptlets; ++index) {
        rules.append(fixture.host + scriptletRules.at(index));
    }

    const int elements = name == QStringLiteral("large-dom") ? 6000 : 400;
    const auto selectors = genericSelectors(lists, elements);
    QSet<QString> classNames;
    QSet<QString> identifiers;
    for (int index = 0; index < elements; ++index) {
        QStringList tokens;
        // A third of the page carries a class the pinned lists hide generically,
        // a third carries the fixture's own site rules, and a third carries
        // neither, so the survey has real work and a real answer.
        const int role = index % 3;
        if (role == 0 && !selectors.classNames.isEmpty()) {
            tokens.append(selectors.classNames.at(index % selectors.classNames.size()));
        } else if (role == 1) {
            tokens.append(QStringLiteral("fixture-site-ad-%1").arg(index % siteSelectors));
        }
        tokens.append(QStringLiteral("fixture-content-%1").arg(index));
        tokens.append(QStringLiteral("fixture-column"));
        const auto identifier = role == 2 && !selectors.identifiers.isEmpty()
            ? selectors.identifiers.at(index % selectors.identifiers.size())
            : QStringLiteral("fixture-node-%1").arg(index);
        for (const auto &token : std::as_const(tokens)) {
            classNames.insert(token);
        }
        identifiers.insert(identifier);
        append(QStringLiteral("<div id=\"%1\" class=\"%2\"><span class=\"fixture-text\">"
                              "fixture %3</span></div>")
                .arg(identifier, tokens.join(u' '))
                .arg(index));
    }

    fixture.elements = elements * 2;
    fixture.classNames = int(classNames.size());
    fixture.identifiers = int(identifiers.size());
    fixture.rules = rules.join(u'\n');
    fixture.body = QByteArrayLiteral("<!doctype html><html><head><meta charset=\"utf-8\">")
        + measurementScript(settleMilliseconds)
        + QByteArrayLiteral("</head><body><h1>omaweb cosmetic benchmark</h1>")
        + body.join(QString()).toUtf8() + QByteArrayLiteral("</body></html>");
    return fixture;
}

// The fixture and a blank page, served from this process so that nothing
// outside it decides when a navigation starts.
class FixtureServer final : public QTcpServer {
public:
    explicit FixtureServer(QByteArray fixture)
        : m_fixture(std::move(fixture))
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                const auto request = socket->readAll();
                const auto fields = request.split(' ');
                m_requests += 1;
                const auto &body = fields.value(1).startsWith("/blank") ? m_blank : m_fixture;
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n"
                              "Cache-Control: no-store\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->flush();
                socket->disconnectFromHost();
            });
        });
    }

    int requestCount() const { return m_requests; }

private:
    int m_requests = 0;
    QByteArray m_fixture;
    QByteArray m_blank = "<!doctype html><html><head><title>blank</title></head><body></body>"
                         "</html>";
};

// The matcher behind the QML contract, plus a counter on every cosmetic
// question the view asks. Counting here rather than inside EngineView.qml keeps
// the boundary count independent of how the view chooses to reach it.
class BenchmarkBlocker final : public QObject {
    Q_OBJECT

public:
    explicit BenchmarkBlocker(std::shared_ptr<const ContentMatcher> matcher)
        : m_matcher(std::move(matcher))
    {
    }

    Q_INVOKABLE int blockedRequestCount(const QUrl &) const { return 0; }

    Q_INVOKABLE QString cosmeticStyleSheet(const QUrl &url)
    {
        m_cosmeticStyleSheetCalls += 1;
        return m_matcher->cosmeticStyleSheet(url);
    }

    Q_INVOKABLE QString scriptletSource(const QUrl &url)
    {
        m_scriptletSourceCalls += 1;
        return m_matcher->scriptletSource(url);
    }

    Q_INVOKABLE bool cosmeticSurveyWanted(const QUrl &url)
    {
        m_cosmeticSurveyWantedCalls += 1;
        return m_matcher->cosmeticSurveyWanted(url);
    }

    Q_INVOKABLE QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids)
    {
        m_genericCosmeticStyleSheetCalls += 1;
        return m_matcher->genericCosmeticStyleSheet(url, classes, ids);
    }

    Q_INVOKABLE bool shouldBlockPopup(const QUrl &, const QUrl &) const { return false; }

    const ContentMatcher &matcher() const { return *m_matcher; }

    void resetCalls()
    {
        m_cosmeticStyleSheetCalls = 0;
        m_scriptletSourceCalls = 0;
        m_cosmeticSurveyWantedCalls = 0;
        m_genericCosmeticStyleSheetCalls = 0;
    }

    int callTotal() const
    {
        return m_cosmeticStyleSheetCalls + m_scriptletSourceCalls + m_cosmeticSurveyWantedCalls
            + m_genericCosmeticStyleSheetCalls;
    }

    QJsonObject calls() const
    {
        return QJsonObject {
            {QStringLiteral("cosmeticStyleSheet"), m_cosmeticStyleSheetCalls},
            {QStringLiteral("scriptletSource"), m_scriptletSourceCalls},
            {QStringLiteral("cosmeticSurveyWanted"), m_cosmeticSurveyWantedCalls},
            {QStringLiteral("genericCosmeticStyleSheet"), m_genericCosmeticStyleSheetCalls},
        };
    }

signals:
    void rulesChanged();
    void configurationChanged();
    void blockedRequestCountChanged(const QUrl &siteUrl);

private:
    std::shared_ptr<const ContentMatcher> m_matcher;
    int m_cosmeticStyleSheetCalls = 0;
    int m_scriptletSourceCalls = 0;
    int m_cosmeticSurveyWantedCalls = 0;
    int m_genericCosmeticStyleSheetCalls = 0;
};

// Run the event loop until the condition holds or the deadline passes. The
// loop sleeps between polls: a spinning wait would put the harness's own busy
// work into the CPU figure it is here to measure.
template <typename Condition> bool waitFor(Condition condition, int milliseconds)
{
    if (condition()) {
        return true;
    }
    QEventLoop loop;
    QTimer poll;
    poll.setInterval(20);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&condition, &loop] {
        if (condition()) {
            loop.quit();
        }
    });
    QTimer::singleShot(milliseconds, &loop, [&loop] { loop.quit(); });
    poll.start();
    loop.exec();
    return condition();
}

// Waits until the view has stopped asking the blocker anything and stopped
// sending scripts into the page for one settling interval. A navigation is over
// when the survey it started has been answered, and only the view can say when.
void waitForQuiet(QObject *view, const BenchmarkBlocker &blocker, int budget)
{
    constexpr int quietMilliseconds = 300;
    QElapsedTimer overall;
    overall.start();
    int scripts = -1;
    int calls = -1;
    while (overall.elapsed() < budget) {
        const int seenScripts = view->property("cosmeticScriptCalls").toInt();
        const int seenCalls = blocker.callTotal();
        if (seenScripts == scripts && seenCalls == calls) {
            return;
        }
        scripts = seenScripts;
        calls = seenCalls;
        QEventLoop loop;
        QTimer::singleShot(quietMilliseconds, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

QJsonObject pageState(QObject *view)
{
    const auto title = view->property("pageTitle").toString();
    if (!title.startsWith(u'{')) {
        return {};
    }
    return QJsonDocument::fromJson(title.toUtf8()).object();
}

QString fileDigest(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

} // namespace

int main(int argc, char *argv[])
{
    QElapsedTimer uptime;
    uptime.start();
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Measures what one navigation costs the cosmetic blocking path."));
    parser.addHelpOption();
    const QCommandLineOption fixtureOption(QStringLiteral("fixture"),
        QStringLiteral("site-css, scriptlets, or large-dom"), QStringLiteral("name"),
        QStringLiteral("site-css"));
    const QCommandLineOption listsOption(QStringLiteral("lists"),
        QStringLiteral("Directory holding easylist.txt and easyprivacy.txt"),
        QStringLiteral("directory"));
    const QCommandLineOption navigationsOption(QStringLiteral("navigations"),
        QStringLiteral("Measured navigations"), QStringLiteral("count"), QStringLiteral("20"));
    const QCommandLineOption warmupOption(QStringLiteral("warmup"),
        QStringLiteral("Navigations discarded before measuring"), QStringLiteral("count"),
        QStringLiteral("3"));
    const QCommandLineOption settleOption(QStringLiteral("settle"),
        QStringLiteral("Milliseconds a page stays open before it is measured"),
        QStringLiteral("milliseconds"), QStringLiteral("1200"));
    const QCommandLineOption outputOption(QStringLiteral("output"),
        QStringLiteral("Where to write the samples"), QStringLiteral("path"));
    const QCommandLineOption labelOption(QStringLiteral("label"),
        QStringLiteral("How the samples name this run"), QStringLiteral("text"),
        QStringLiteral("unlabelled"));
    const QCommandLineOption conditionOption(QStringLiteral("condition"),
        QStringLiteral("cold or warm"), QStringLiteral("name"), QStringLiteral("warm"));
    // Chromium finalises a startup trace when its own timer runs out, so a
    // traced run has to stay up at least that long or the file is never written.
    const QCommandLineOption holdOption(QStringLiteral("hold"),
        QStringLiteral("Seconds from start before the process may exit"), QStringLiteral("seconds"),
        QStringLiteral("0"));
    parser.addOptions({fixtureOption, listsOption, navigationsOption, warmupOption, settleOption,
        outputOption, labelOption, conditionOption, holdOption});
    // The parser needs the arguments before QGuiApplication exists, because the
    // fixture's port decides the Chromium flags and those are read at
    // initialisation.
    QStringList arguments;
    for (int index = 0; index < argc; ++index) {
        arguments.append(QString::fromLocal8Bit(argv[index]));
    }
    parser.parse(arguments);
    if (parser.isSet(QStringLiteral("help"))) {
        printf("%s", qPrintable(parser.helpText()));
        return 0;
    }

    const auto listsDirectory = parser.value(listsOption);
    QFile easylist(QDir(listsDirectory).filePath(QStringLiteral("easylist.txt")));
    QFile easyprivacy(QDir(listsDirectory).filePath(QStringLiteral("easyprivacy.txt")));
    if (!easylist.open(QIODevice::ReadOnly) || !easyprivacy.open(QIODevice::ReadOnly)) {
        fprintf(stderr, "Both easylist.txt and easyprivacy.txt must be in --lists\n");
        return 2;
    }
    const auto lists = QString::fromUtf8(easylist.readAll() + '\n' + easyprivacy.readAll());

    const auto fixture
        = buildFixture(parser.value(fixtureOption), lists, parser.value(settleOption).toInt());
    if (!fixtureNames.contains(parser.value(fixtureOption))) {
        fprintf(stderr, "Unknown fixture %s\n", qPrintable(parser.value(fixtureOption)));
        return 2;
    }

    // The same registrations the shell makes, because EngineView.qml imports
    // the Omaweb module and reaches the external protocol handler through it.
    omaweb::QtContentBlocker::registerSubstituteScheme();
    QtWebEngineQuick::initialize();
    QGuiApplication application(argc, argv);
    omaweb::registerExternalProtocolHandler();

    // The server needs the application's event dispatcher before it can accept
    // a connection, so it comes up after it rather than beside the fixture.
    FixtureServer server(fixture.body);
    if (!server.listen(QHostAddress::LocalHost)) {
        fprintf(stderr, "The fixture server could not listen on the loopback address\n");
        return 2;
    }

    const auto compilation = ContentMatcher::compile(lists + u'\n' + fixture.rules);
    if (!compilation.matcher) {
        fprintf(stderr, "The pinned lists did not compile\n");
        return 2;
    }
    BenchmarkBlocker blocker(compilation.matcher);

    QTemporaryDir root;
    QQmlEngine engine;
    QQmlComponent component(
        &engine, QUrl::fromLocalFile(QStringLiteral(OMAWEB_QT_ENGINE_VIEW_PATH)));
    const std::unique_ptr<QObject> view(component.createWithInitialProperties({
        {QStringLiteral("profilePath"), root.filePath(QStringLiteral("profile"))},
        {QStringLiteral("contentBlocker"), QVariant::fromValue<QObject *>(&blocker)},
    }));
    if (!view) {
        fprintf(stderr, "%s\n", qPrintable(component.errorString()));
        return 2;
    }
    QQuickWindow window;
    window.resize(1280, 800);
    auto *item = qobject_cast<QQuickItem *>(view.get());
    item->setParentItem(window.contentItem());
    // A view with no size gives Chromium nothing to paint into, and the paint
    // timings this measures would never arrive.
    item->setWidth(window.width());
    item->setHeight(window.height());
    window.show();

    const auto address = [&fixture, &server](const QString &path) {
        return QUrl(
            QStringLiteral("http://%1:%2%3").arg(fixture.host).arg(server.serverPort()).arg(path));
    };
    const QUrl page = address(QStringLiteral("/page.html"));
    const QUrl blank = address(QStringLiteral("/blank.html"));

    const bool cold = parser.value(conditionOption) == QStringLiteral("cold");
    const int warmup = cold ? 0 : parser.value(warmupOption).toInt();
    const int navigations = cold ? 1 : parser.value(navigationsOption).toInt();
    const int settle = parser.value(settleOption).toInt();
    const int budget = settle + 20000;

    QJsonArray samples;
    for (int index = 0; index < warmup + navigations; ++index) {
        // A blank page between navigations replaces the document, so every
        // measured load starts from the same place. Its own cost stays outside
        // the measured window, and the window cannot open until its cosmetic
        // work has finished: the blank page's generic survey is answered
        // asynchronously and would otherwise be counted against the fixture.
        if (index > 0) {
            view->setProperty("currentUrl", blank);
            waitFor(
                [&view] {
                    return view->property("pageTitle").toString() == QStringLiteral("blank")
                        && !view->property("loading").toBool();
                },
                budget);
            waitForQuiet(view.get(), blocker, budget);
        }

        // Every counter is zeroed here, at the edge of the measured window, so
        // that whatever the blank page cost falls outside it.
        view->setProperty("cosmeticScriptCalls", 0);
        view->setProperty("cosmeticSurveyCallbacks", 0);
        blocker.resetCalls();
        const auto lookupsBefore = blocker.matcher().cosmeticLookupCount();
        const auto cpuBefore = processTreeCpu();
        const auto startedAt = monotonicMicroseconds();
        QElapsedTimer wall;
        wall.start();

        view->setProperty("currentUrl", page);
        const bool surveyed = waitFor(
            [&view] { return view->property("genericCosmeticRulesInjected").toBool(); }, budget);
        const bool settled = waitFor(
            [&view] { return pageState(view.get()).value(QStringLiteral("settled")).toBool(); },
            budget);

        const auto finishedAt = monotonicMicroseconds();
        const auto cpuAfter = processTreeCpu();
        const auto state = pageState(view.get());

        QJsonObject sample {
            {QStringLiteral("index"), index},
            {QStringLiteral("warmup"), index < warmup},
            {QStringLiteral("surveyed"), surveyed},
            {QStringLiteral("settled"), settled},
            {QStringLiteral("lookups"),
                double(blocker.matcher().cosmeticLookupCount() - lookupsBefore)},
            {QStringLiteral("blockerCalls"), blocker.calls()},
            {QStringLiteral("cosmeticScriptCalls"), view->property("cosmeticScriptCalls").toInt()},
            {QStringLiteral("surveyCallbacks"), view->property("cosmeticSurveyCallbacks").toInt()},
            {QStringLiteral("siteStyleWatched"),
                state.value(QStringLiteral("siteStyleWatched")).toBool()},
            {QStringLiteral("siteStyleMutations"),
                state.value(QStringLiteral("siteStyleMutations")).toInt()},
            {QStringLiteral("siteStyleBytes"), state.value(QStringLiteral("siteStyleBytes"))},
            {QStringLiteral("genericStyleBytes"), state.value(QStringLiteral("genericStyleBytes"))},
            {QStringLiteral("hiddenSample"), state.value(QStringLiteral("hiddenSample"))},
            {QStringLiteral("cpuMilliseconds"), cpuAfter.milliseconds - cpuBefore.milliseconds},
            {QStringLiteral("cpuProcesses"), cpuAfter.processes},
            {QStringLiteral("wallMilliseconds"), double(wall.nsecsElapsed()) / 1000000.0},
            {QStringLiteral("firstPaintMilliseconds"), state.value(QStringLiteral("firstPaint"))},
            {QStringLiteral("firstContentfulPaintMilliseconds"),
                state.value(QStringLiteral("firstContentfulPaint"))},
            {QStringLiteral("largestContentfulPaintMilliseconds"),
                state.value(QStringLiteral("largestContentfulPaint"))},
            {QStringLiteral("startedAtMicroseconds"), double(startedAt)},
            {QStringLiteral("finishedAtMicroseconds"), double(finishedAt)},
        };
        fprintf(stderr,
            "navigation %d: surveyed=%d settled=%d watched=%d lookups=%d mutations=%d "
            "site=%d generic=%d hidden=%d\n",
            index, int(surveyed), int(settled),
            int(state.value(QStringLiteral("siteStyleWatched")).toBool()),
            int(sample.value(QStringLiteral("lookups")).toDouble()),
            sample.value(QStringLiteral("siteStyleMutations")).toInt(),
            state.value(QStringLiteral("siteStyleBytes")).toInt(),
            state.value(QStringLiteral("genericStyleBytes")).toInt(),
            state.value(QStringLiteral("hiddenSample")).toInt());
        samples.append(sample);
    }

    const auto siteCss = compilation.matcher->cosmeticStyleSheet(page);
    const auto scriptletSource = compilation.matcher->scriptletSource(page);
    QJsonObject report {
        {QStringLiteral("label"), parser.value(labelOption)},
        {QStringLiteral("fixture"), fixture.name},
        {QStringLiteral("host"), fixture.host},
        {QStringLiteral("condition"), parser.value(conditionOption)},
        {QStringLiteral("settleMilliseconds"), settle},
        {QStringLiteral("warmup"), warmup},
        {QStringLiteral("lists"),
            QJsonObject {
                {QStringLiteral("easylistSha256"), fileDigest(easylist.fileName())},
                {QStringLiteral("easyprivacySha256"), fileDigest(easyprivacy.fileName())},
            }},
        {QStringLiteral("fixtureSize"),
            QJsonObject {
                {QStringLiteral("bytes"), fixture.body.size()},
                {QStringLiteral("elements"), fixture.elements},
                {QStringLiteral("classNames"), fixture.classNames},
                {QStringLiteral("identifiers"), fixture.identifiers},
                {QStringLiteral("ruleLines"), int(fixture.rules.count(u'\n')) + 1},
                {QStringLiteral("siteStyleSheetBytes"), int(siteCss.toUtf8().size())},
                {QStringLiteral("scriptletSourceBytes"), int(scriptletSource.toUtf8().size())},
            }},
        {QStringLiteral("samples"), samples},
    };

    const auto outputPath = parser.value(outputOption);
    if (outputPath.isEmpty()) {
        printf("%s\n", QJsonDocument(report).toJson(QJsonDocument::Indented).constData());
        return 0;
    }
    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fprintf(stderr, "Could not write %s\n", qPrintable(outputPath));
        return 2;
    }
    output.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    output.close();

    // The samples are on disk before this wait, because the wait is the risky
    // part: Chromium finalises a startup trace on its own timer, and shutting
    // the engine down under tracing sometimes deadlocks. A caller that gives up
    // on a stuck process still finds both the samples and the trace.
    const qint64 hold = parser.value(holdOption).toInt() * 1000;
    const qint64 remaining = hold - uptime.elapsed();
    if (remaining > 0) {
        QEventLoop loop;
        QTimer::singleShot(int(remaining), &loop, &QEventLoop::quit);
        loop.exec();
    }
    return 0;
}

#include "cosmetic_benchmark.moc"
