#include "BarFeed.h"
#include "BarSocket.h"
#include "BarTabs.h"
#include "StoredFavicons.h"

#include <QDeadlineTimer>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <functional>
#include <memory>

#include <sys/stat.h>

using omaweb::BarFeed;
using omaweb::BarSocket;
using omaweb::BarTabs;

namespace {

// A widget the browser has taken in. The widget's end is connected before the
// browser's has accepted it, so the wait is for the browser's turn too.
std::unique_ptr<QLocalSocket> connectWidget(const QString &path)
{
    auto widget = std::make_unique<QLocalSocket>();
    widget->connectToServer(path);
    QDeadlineTimer deadline(2000);
    while (widget->state() != QLocalSocket::ConnectedState && !deadline.hasExpired()) {
        QTest::qWait(10);
    }
    QTest::qWait(50);
    return widget->state() == QLocalSocket::ConnectedState ? std::move(widget) : nullptr;
}

// The next line the browser sends, or an empty object if none comes. The
// browser's end runs on this thread, so the wait keeps its events running.
QJsonObject nextLine(QLocalSocket &widget)
{
    QDeadlineTimer deadline(2000);
    while (!widget.canReadLine() && !deadline.hasExpired()) {
        QTest::qWait(10);
    }
    return QJsonDocument::fromJson(widget.readLine()).object();
}

void say(QLocalSocket &widget, const QByteArray &line)
{
    widget.write(line + '\n');
    widget.flush();
}

QByteArray png(QColor colour)
{
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(colour);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

// A state of one Space whose tabs show these icons, by the address the window
// draws each from.
QJsonObject showing(const QStringList &icons)
{
    QJsonArray tabs;
    for (int i = 0; i < icons.size(); ++i) {
        tabs.append(QJsonObject {{QStringLiteral("id"), QStringLiteral("t%1").arg(i)},
            {QStringLiteral("icon"), icons.at(i)}});
    }
    return {{QStringLiteral("spaces"),
        QJsonArray {QJsonObject {{QStringLiteral("id"), QStringLiteral("work")},
            {QStringLiteral("pins"), QJsonArray {}}, {QStringLiteral("tabs"), tabs}}}}};
}

QStringList iconsIn(const QJsonObject &state)
{
    QStringList icons;
    const auto tabs = state.value(QStringLiteral("spaces"))
                          .toArray()
                          .first()
                          .toObject()
                          .value(QStringLiteral("tabs"))
                          .toArray();
    for (const auto &tab : tabs) {
        icons.append(tab.toObject().value(QStringLiteral("icon")).toString());
    }
    return icons;
}

QByteArray contentsOf(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The engine's icons, as this process's QML engine would read them.
class FakeIcons final {
public:
    FakeIcons()
    {
        omaweb::storedfavicons::setReader(
            [this](const QUrl &address, QObject *, omaweb::storedfavicons::Answer answer) {
                answer(m_icons.value(address.toString()));
            });
    }
    ~FakeIcons() { omaweb::storedfavicons::setReader({}); }

    void set(const QString &address, const QByteArray &image) { m_icons.insert(address, image); }

private:
    QHash<QString, QByteArray> m_icons;
};

// The icons of the first state sent to the widget that `accepts`, or none if
// no such state comes.
QStringList iconsOnceSent(
    QLocalSocket &widget, const std::function<bool(const QStringList &)> &accepts)
{
    QDeadlineTimer deadline(3000);
    while (!deadline.hasExpired()) {
        QTest::qWait(10);
        while (widget.canReadLine()) {
            const auto icons = iconsIn(QJsonDocument::fromJson(widget.readLine()).object());
            if (accepts(icons)) {
                return icons;
            }
        }
    }
    return {};
}

bool namesAFile(const QStringList &icons) { return icons.value(0).endsWith(u".png"); }

} // namespace

class BarTest final : public QObject {
    Q_OBJECT

private slots:
    void aWidgetThatConnectsIsSentTheStateAsItStands();
    void eachStateReachesTheWidgetOnEveryMonitor();
    void onlyThisUserCanOpenTheSocket();
    void closingTakesTheSocketAway();
    void aWidgetAsksForTheActionsOfTheFixedSet();
    void anActionOutsideTheSetIsDropped();
    void theBarShowsTheTabsFromTheStartUntilNoWidgetHasComeForTheGrace();
    void aWidgetBackWithinTheGraceKeepsTheTabsInTheBar();
    void aTabsIconReachesTheBarAsAFileNamedByItsContents();
    void aBadgeDrawnIntoTheIconArrivesUnderANewNameAndTheOldFileGoes();
    void aBurstOfIconChangesReachesTheBarAFewTimesASecond();
    void clearingTakesEveryIconFileAway();
    void aSpaceWithNoIconStoredLeavesTheBarItsHostTile();
    void theBarIsListenedForOnlyWhileTheReaderHasChosenIt();
    void theWindowsStateReachesTheBarWithItsIconsAsFiles();
    void theBarsActionsReachTheWindow();
};

void BarTest::aWidgetThatConnectsIsSentTheStateAsItStands()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    socket.publish({{QStringLiteral("activeSpace"), QStringLiteral("work")}});

    auto widget = connectWidget(path);
    QVERIFY(widget);
    QCOMPARE(
        nextLine(*widget).value(QStringLiteral("activeSpace")).toString(), QStringLiteral("work"));
}

void BarTest::eachStateReachesTheWidgetOnEveryMonitor()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    auto left = connectWidget(path);
    auto right = connectWidget(path);
    QVERIFY(left && right);
    QTRY_VERIFY(socket.widgetsPresent());
    QTest::qWait(50);

    socket.publish({{QStringLiteral("activeSpace"), QStringLiteral("reading")}});

    QCOMPARE(
        nextLine(*left).value(QStringLiteral("activeSpace")).toString(), QStringLiteral("reading"));
    QCOMPARE(nextLine(*right).value(QStringLiteral("activeSpace")).toString(),
        QStringLiteral("reading"));
}

void BarTest::onlyThisUserCanOpenTheSocket()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("omaweb/bar.sock"));
    BarSocket socket;
    QVERIFY(socket.listen(path));

    struct stat status {};
    QCOMPARE(::stat(QFile::encodeName(path).constData(), &status), 0);
    QCOMPARE(status.st_mode & 0777, 0600u);
    QCOMPARE(
        ::stat(QFile::encodeName(home.filePath(QStringLiteral("omaweb"))).constData(), &status), 0);
    QCOMPARE(status.st_mode & 0777, 0700u);
}

// The setting off is the socket gone, so nothing finds a browser to talk to.
void BarTest::closingTakesTheSocketAway()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    auto widget = connectWidget(path);
    QVERIFY(widget);

    socket.close();

    QVERIFY(!socket.listening());
    QVERIFY(!QFileInfo::exists(path));
    QTRY_COMPARE(widget->state(), QLocalSocket::UnconnectedState);
    QVERIFY(!socket.widgetsPresent());
}

void BarTest::aWidgetAsksForTheActionsOfTheFixedSet()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    QSignalSpy asked(&socket, &BarSocket::actionRequested);
    auto widget = connectWidget(path);
    QVERIFY(widget);

    say(*widget, R"({"do": "activate", "tab": "t1", "space": "work"})");
    say(*widget, R"({"do": "close", "tab": "t2"})");
    say(*widget, R"({"do": "reload", "tab": "ignored"})");
    say(*widget, R"({"do": "mute", "tab": "t3"})");
    say(*widget, R"({"do": "unmute", "tab": "t3"})");
    say(*widget, R"({"do": "omnibar"})");

    QTRY_COMPARE(asked.count(), 6);
    QCOMPARE(asked.at(0).at(0).toString(), QStringLiteral("activate"));
    QCOMPARE(asked.at(0).at(1).toMap(),
        (QVariantMap {{QStringLiteral("tab"), QStringLiteral("t1")},
            {QStringLiteral("space"), QStringLiteral("work")}}));
    QCOMPARE(asked.at(1).at(0).toString(), QStringLiteral("close"));
    QCOMPARE(
        asked.at(1).at(1).toMap(), (QVariantMap {{QStringLiteral("tab"), QStringLiteral("t2")}}));
    QCOMPARE(asked.at(2).at(0).toString(), QStringLiteral("reload"));
    QVERIFY(asked.at(2).at(1).toMap().isEmpty());
    QCOMPARE(asked.at(3).at(0).toString(), QStringLiteral("mute"));
    QCOMPARE(
        asked.at(3).at(1).toMap(), (QVariantMap {{QStringLiteral("tab"), QStringLiteral("t3")}}));
    QCOMPARE(asked.at(4).at(0).toString(), QStringLiteral("unmute"));
    QCOMPARE(asked.at(5).at(0).toString(), QStringLiteral("omnibar"));
}

// The bar reaches the tabs, the page's back, forward and reload, and the
// Location, and nothing else: no page, no address to open, no tab it does
// not name, and none of the sidebar's own tab commands.
void BarTest::anActionOutsideTheSetIsDropped()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    QSignalSpy asked(&socket, &BarSocket::actionRequested);
    auto widget = connectWidget(path);
    QVERIFY(widget);

    say(*widget, R"({"do": "eval", "script": "document.cookie"})");
    say(*widget, R"({"do": "open", "url": "https://example.com"})");
    say(*widget, R"({"do": "close"})");
    say(*widget, R"({"do": "pin", "tab": "t2"})");
    say(*widget, R"({"do": "duplicate", "tab": "t2"})");
    say(*widget, R"({"do": "popout", "tab": "t2"})");
    say(*widget, R"({"do": "move", "tab": "t2", "index": 3})");
    say(*widget, R"({"do": "cycle", "step": 1})");
    say(*widget, R"({"do": "space", "space": "reading"})");
    say(*widget, "not json");
    say(*widget, R"({"do": "back"})");

    QTRY_COMPARE(asked.count(), 1);
    QTest::qWait(50);
    QCOMPARE(asked.count(), 1);
    QCOMPARE(asked.at(0).at(0).toString(), QStringLiteral("back"));
}

void BarTest::theBarShowsTheTabsFromTheStartUntilNoWidgetHasComeForTheGrace()
{
    QTemporaryDir home;
    BarSocket socket;
    socket.setAbsenceGrace(100);
    QVERIFY(!socket.widgetsPresent());

    QVERIFY(socket.listen(home.filePath(QStringLiteral("bar.sock"))));

    QVERIFY(socket.widgetsPresent());
    QTRY_VERIFY(!socket.widgetsPresent());
}

void BarTest::aWidgetBackWithinTheGraceKeepsTheTabsInTheBar()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarSocket socket;
    socket.setAbsenceGrace(300);
    QVERIFY(socket.listen(path));
    auto widget = connectWidget(path);
    QVERIFY(widget);
    QSignalSpy changed(&socket, &BarSocket::widgetsPresentChanged);

    // A shell reload: the widget goes and its successor arrives.
    widget->disconnectFromServer();
    QTest::qWait(100);
    auto successor = connectWidget(path);
    QVERIFY(successor);
    QTest::qWait(400);

    QVERIFY(socket.widgetsPresent());
    QCOMPARE(changed.count(), 0);

    successor->disconnectFromServer();
    QTRY_VERIFY(!socket.widgetsPresent());
}

void BarTest::aTabsIconReachesTheBarAsAFileNamedByItsContents()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    const auto github = png(Qt::white);
    icons.set(QStringLiteral("image://favicon/https://github.com/favicon.svg"), github);
    BarSocket socket;
    QVERIFY(socket.listen(path));
    BarFeed feed(&socket, home.filePath(QStringLiteral("icons")));
    auto widget = connectWidget(path);
    QVERIFY(widget);

    feed.publish(
        showing({QStringLiteral("image://favicon/https://github.com/favicon.svg"), QString()}));

    const auto shown = iconsOnceSent(*widget, namesAFile);
    QVERIFY(namesAFile(shown));
    QCOMPARE(shown.value(1), QString());
    QCOMPARE(QFileInfo(shown.at(0)).absolutePath(), home.filePath(QStringLiteral("icons")));
    QCOMPARE(contentsOf(shown.at(0)), github);
    QCOMPARE(QFileInfo(shown.at(0)).fileName(),
        QString::fromLatin1(
            QCryptographicHash::hash(github, QCryptographicHash::Sha256).toHex().left(32))
            + u".png");
}

// The widget's Image caches by path, so a picture with a new count in it has to
// arrive under a new path, and the one before it is nobody's any more.
void BarTest::aBadgeDrawnIntoTheIconArrivesUnderANewNameAndTheOldFileGoes()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    icons.set(QStringLiteral("image://favicon/data:quiet"), png(Qt::green));
    icons.set(QStringLiteral("image://favicon/data:three-unread"), png(Qt::red));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    BarFeed feed(&socket, home.filePath(QStringLiteral("icons")));
    feed.setInterval(0);
    auto widget = connectWidget(path);
    QVERIFY(widget);
    feed.publish(showing({QStringLiteral("image://favicon/data:quiet")}));
    const auto quiet = iconsOnceSent(*widget, namesAFile).value(0);
    QVERIFY(!quiet.isEmpty());

    feed.publish(showing({QStringLiteral("image://favicon/data:three-unread")}));

    const auto badged = iconsOnceSent(*widget, [&quiet](const QStringList &icons) {
        return namesAFile(icons) && icons.at(0) != quiet;
    }).value(0);
    QVERIFY(!badged.isEmpty());
    QCOMPARE(contentsOf(badged), png(Qt::red));
    QVERIFY(!QFileInfo::exists(quiet));
}

// A blinking icon is a new picture every few hundred milliseconds. The bar
// takes the first at once and the latest of what follows, not every one.
void BarTest::aBurstOfIconChangesReachesTheBarAFewTimesASecond()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    BarSocket socket;
    QVERIFY(socket.listen(path));
    BarFeed feed(&socket, home.filePath(QStringLiteral("icons")));
    feed.setInterval(200);
    auto widget = connectWidget(path);
    QVERIFY(widget);
    for (int i = 0; i < 10; ++i) {
        const auto address = QStringLiteral("image://favicon/data:frame-%1").arg(i);
        icons.set(address, png(QColor::fromHsv(i * 30, 255, 255)));
        feed.publish(showing({address}));
    }

    // The first at once, the latest after an interval, and that one again
    // once its icon has been read, an interval later.
    QTest::qWait(700);
    QList<QJsonObject> sent;
    while (widget->canReadLine()) {
        sent.append(QJsonDocument::fromJson(widget->readLine()).object());
    }

    QVERIFY(!sent.isEmpty());
    QVERIFY2(sent.size() <= 3, qPrintable(QString::number(sent.size())));
    QCOMPARE(contentsOf(iconsIn(sent.last()).value(0)), png(QColor::fromHsv(9 * 30, 255, 255)));
}

void BarTest::clearingTakesEveryIconFileAway()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    icons.set(QStringLiteral("image://favicon/a"), png(Qt::blue));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    BarFeed feed(&socket, home.filePath(QStringLiteral("icons")));
    feed.setInterval(0);
    auto widget = connectWidget(path);
    QVERIFY(widget);
    feed.publish(showing({QStringLiteral("image://favicon/a")}));
    const auto file = iconsOnceSent(*widget, namesAFile).value(0);
    QVERIFY(!file.isEmpty());

    feed.clear();

    QVERIFY(!QFileInfo::exists(file));
    QVERIFY(QDir(home.filePath(QStringLiteral("icons"))).isEmpty());
}

// A Space that has stored no icon for a page answers one transparent pixel,
// which the sidebar reads as no artwork and draws the host code for. The bar
// is told there is no icon, so it draws its own.
void BarTest::aSpaceWithNoIconStoredLeavesTheBarItsHostTile()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    QImage pixel(1, 1, QImage::Format_ARGB32);
    pixel.fill(Qt::transparent);
    QByteArray nothing;
    QBuffer buffer(&nothing);
    buffer.open(QIODevice::WriteOnly);
    pixel.save(&buffer, "PNG");
    icons.set(QStringLiteral("image://omaweb-favicon/window/space/page"), nothing);
    icons.set(QStringLiteral("image://favicon/a"), png(Qt::blue));
    BarSocket socket;
    QVERIFY(socket.listen(path));
    BarFeed feed(&socket, home.filePath(QStringLiteral("icons")));
    feed.setInterval(0);
    auto widget = connectWidget(path);
    QVERIFY(widget);

    feed.publish(showing({QStringLiteral("image://omaweb-favicon/window/space/page"),
        QStringLiteral("image://favicon/a")}));

    const auto shown = iconsOnceSent(
        *widget, [](const QStringList &icons) { return icons.value(1).endsWith(u".png"); });
    QCOMPARE(shown.value(0), QString());
    QCOMPARE(QDir(home.filePath(QStringLiteral("icons"))).entryList(QDir::Files).size(), 1);
}

void BarTest::theBarIsListenedForOnlyWhileTheReaderHasChosenIt()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    icons.set(QStringLiteral("image://favicon/a"), png(Qt::blue));
    BarTabs tabs(path, home.filePath(QStringLiteral("icons")));
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(!tabs.showsTabs());

    tabs.setOn(true);
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(tabs.showsTabs());
    auto widget = connectWidget(path);
    QVERIFY(widget);
    tabs.publish(showing({QStringLiteral("image://favicon/a")}).toVariantMap());
    QVERIFY(namesAFile(iconsOnceSent(*widget, namesAFile)));

    tabs.setOn(false);
    QVERIFY(!QFileInfo::exists(path));
    QVERIFY(!tabs.showsTabs());
    QVERIFY(QDir(home.filePath(QStringLiteral("icons"))).isEmpty());
}

void BarTest::theWindowsStateReachesTheBarWithItsIconsAsFiles()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    FakeIcons icons;
    icons.set(QStringLiteral("image://favicon/a"), png(Qt::blue));
    BarTabs tabs(path, home.filePath(QStringLiteral("icons")));
    tabs.setOn(true);
    auto widget = connectWidget(path);
    QVERIFY(widget);

    tabs.publish(showing({QStringLiteral("image://favicon/a")}).toVariantMap());

    QCOMPARE(contentsOf(iconsOnceSent(*widget, namesAFile).value(0)), png(Qt::blue));
}

void BarTest::theBarsActionsReachTheWindow()
{
    QTemporaryDir home;
    const auto path = home.filePath(QStringLiteral("bar.sock"));
    BarTabs tabs(path, home.filePath(QStringLiteral("icons")));
    tabs.setOn(true);
    QSignalSpy asked(&tabs, &BarTabs::actionRequested);
    auto widget = connectWidget(path);
    QVERIFY(widget);

    say(*widget, R"({"do": "activate", "tab": "t1"})");

    QTRY_COMPARE(asked.count(), 1);
    QCOMPARE(asked.at(0).at(0).toString(), QStringLiteral("activate"));
}

QTEST_GUILESS_MAIN(BarTest)

#include "tst_bar.moc"
