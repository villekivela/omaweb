#include "MediaAnnouncer.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QSignalSpy>
#include <QTest>

using omaweb::MediaAnnouncer;

namespace {

const auto kService = QStringLiteral("org.mpris.MediaPlayer2.omaweb");
const auto kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const auto kPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const auto kRootInterface = QStringLiteral("org.mpris.MediaPlayer2");
const auto kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

// Reads a property the way a bar's media widget does, over the bus rather than
// off the object, so what the test sees is what a consumer would see.
QVariant readProperty(const QString &interface, const QString &name)
{
    auto call = QDBusMessage::createMethodCall(
        kService, kPath, kPropertiesInterface, QStringLiteral("Get"));
    call << interface << name;
    const auto reply = QDBusConnection::sessionBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        return {};
    }
    return reply.arguments().value(0).value<QDBusVariant>().variant();
}

QVariantMap readMetadata()
{
    QVariantMap fields;
    const auto answer = readProperty(kPlayerInterface, QStringLiteral("Metadata"));
    const auto argument = answer.value<QDBusArgument>();
    argument >> fields;
    return fields;
}

bool serviceRegistered()
{
    auto *bus = QDBusConnection::sessionBus().interface();
    return bus && bus->isServiceRegistered(kService).value();
}

QVariantMap playing(const QString &tabId, const QString &title)
{
    QVariantMap announcement;
    announcement.insert(QStringLiteral("tabId"), tabId);
    announcement.insert(QStringLiteral("playing"), true);
    if (!title.isEmpty()) {
        announcement.insert(QStringLiteral("title"), title);
    }
    return announcement;
}

} // namespace

class MediaAnnouncerTest final : public QObject {
    Q_OBJECT

signals:
    // The bus signal, relayed onto a signal a spy can watch. Connecting by name
    // is what a consumer that only listens does.
    void propertiesChanged(
        const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private slots:
    void cleanup();
    void exportsNoPlayerUntilATabMakesSound();
    void carriesWhatTheTabDeclares();
    void answersTheWholeInterfaceAWidgetAsksFor();
    void tellsConsumersWhenTheTrackChanges();
    void announcesPlaybackWithoutATitleForAPrivateTab();
    void reportsTheCommandsTheDesktopSends();
    void withdrawsThePlayerWhenTheSoundStops();
};

void MediaAnnouncerTest::cleanup()
{
    MediaAnnouncer announcer;
    announcer.announce({});
}

void MediaAnnouncerTest::exportsNoPlayerUntilATabMakesSound()
{
    MediaAnnouncer announcer;
    QVERIFY(announcer.available());
    QVERIFY(!serviceRegistered());

    announcer.announce(playing(QStringLiteral("tab-a"), QStringLiteral("Radio Helsinki")));
    QVERIFY(serviceRegistered());
    QCOMPARE(readProperty(kRootInterface, QStringLiteral("Identity")).toString(),
        QStringLiteral("Omaweb"));
    QCOMPARE(readProperty(kPlayerInterface, QStringLiteral("PlaybackStatus")).toString(),
        QStringLiteral("Playing"));
}

void MediaAnnouncerTest::carriesWhatTheTabDeclares()
{
    MediaAnnouncer announcer;
    auto announcement = playing(QStringLiteral("tab-a"), QStringLiteral("Declared title"));
    announcement.insert(QStringLiteral("artist"), QStringLiteral("Declared artist"));
    announcement.insert(QStringLiteral("album"), QStringLiteral("Declared album"));
    announcement.insert(QStringLiteral("artwork"), QStringLiteral("https://example.test/art.png"));
    announcement.insert(QStringLiteral("canGoNext"), true);
    announcer.announce(announcement);

    const auto fields = readMetadata();
    QCOMPARE(
        fields.value(QStringLiteral("xesam:title")).toString(), QStringLiteral("Declared title"));
    QCOMPARE(fields.value(QStringLiteral("xesam:artist")).toStringList(),
        QStringList {QStringLiteral("Declared artist")});
    QCOMPARE(
        fields.value(QStringLiteral("xesam:album")).toString(), QStringLiteral("Declared album"));
    QCOMPARE(fields.value(QStringLiteral("mpris:artUrl")).toString(),
        QStringLiteral("https://example.test/art.png"));
    // The track is the tab, named by a path a consumer can tell from the next
    // tab's.
    QCOMPARE(fields.value(QStringLiteral("mpris:trackid")).value<QDBusObjectPath>().path(),
        QStringLiteral("/org/omaweb/tab/tab_a"));
    QVERIFY(readProperty(kPlayerInterface, QStringLiteral("CanGoNext")).toBool());
    QVERIFY(!readProperty(kPlayerInterface, QStringLiteral("CanGoPrevious")).toBool());
}

// What a bar's media widget does before it draws anything: find out which
// interfaces are there, then read the player's properties in one call. A
// property whose type cannot go on the bus fails here rather than in the widget.
void MediaAnnouncerTest::answersTheWholeInterfaceAWidgetAsksFor()
{
    MediaAnnouncer announcer;
    announcer.announce(playing(QStringLiteral("tab-a"), QStringLiteral("Track")));

    auto introspect = QDBusMessage::createMethodCall(kService, kPath,
        QStringLiteral("org.freedesktop.DBus.Introspectable"), QStringLiteral("Introspect"));
    const auto described = QDBusConnection::sessionBus().call(introspect);
    QCOMPARE(described.type(), QDBusMessage::ReplyMessage);
    const auto interfaces = described.arguments().value(0).toString();
    QVERIFY(interfaces.contains(kRootInterface));
    QVERIFY(interfaces.contains(kPlayerInterface));

    auto all = QDBusMessage::createMethodCall(
        kService, kPath, kPropertiesInterface, QStringLiteral("GetAll"));
    all << kPlayerInterface;
    const auto answer = QDBusConnection::sessionBus().call(all);
    QCOMPARE(answer.type(), QDBusMessage::ReplyMessage);
    QVariantMap properties;
    answer.arguments().value(0).value<QDBusArgument>() >> properties;
    for (const auto &name : {QStringLiteral("PlaybackStatus"), QStringLiteral("Metadata"),
             QStringLiteral("CanControl"), QStringLiteral("CanPlay"), QStringLiteral("CanPause"),
             QStringLiteral("CanGoNext"), QStringLiteral("CanGoPrevious"),
             QStringLiteral("CanSeek"), QStringLiteral("Position"), QStringLiteral("Volume")}) {
        QVERIFY2(properties.contains(name), qPrintable(name));
    }
}

void MediaAnnouncerTest::tellsConsumersWhenTheTrackChanges()
{
    MediaAnnouncer announcer;
    announcer.announce(playing(QStringLiteral("tab-a"), QStringLiteral("First track")));

    // A widget that only listens has to hear the change, because Qt does not
    // send this for an adaptor's properties by itself.
    QDBusConnection::sessionBus().connect(QString {}, kPath, kPropertiesInterface,
        QStringLiteral("PropertiesChanged"), this,
        SIGNAL(propertiesChanged(QString, QVariantMap, QStringList)));
    QSignalSpy changed(this, SIGNAL(propertiesChanged(QString, QVariantMap, QStringList)));

    announcer.announce(playing(QStringLiteral("tab-a"), QStringLiteral("Second track")));
    QVERIFY(changed.wait(1000));
    QCOMPARE(changed.first().at(0).toString(), kPlayerInterface);
    const auto fields = changed.first().at(1).toMap();
    QCOMPARE(fields.value(QStringLiteral("PlaybackStatus")).toString(), QStringLiteral("Playing"));
    QCOMPARE(readMetadata().value(QStringLiteral("xesam:title")).toString(),
        QStringLiteral("Second track"));
}

void MediaAnnouncerTest::announcesPlaybackWithoutATitleForAPrivateTab()
{
    MediaAnnouncer announcer;
    QVariantMap announcement;
    announcement.insert(QStringLiteral("tabId"), QStringLiteral("tab-p"));
    announcement.insert(QStringLiteral("playing"), false);
    announcer.announce(announcement);

    QCOMPARE(readProperty(kPlayerInterface, QStringLiteral("PlaybackStatus")).toString(),
        QStringLiteral("Paused"));
    QVERIFY(readProperty(kPlayerInterface, QStringLiteral("CanPlay")).toBool());
    const auto fields = readMetadata();
    QVERIFY(!fields.contains(QStringLiteral("xesam:title")));
    QVERIFY(!fields.contains(QStringLiteral("xesam:artist")));
    QVERIFY(!fields.contains(QStringLiteral("mpris:artUrl")));
}

void MediaAnnouncerTest::reportsTheCommandsTheDesktopSends()
{
    MediaAnnouncer announcer;
    announcer.announce(playing(QStringLiteral("tab-a"), QStringLiteral("Track")));
    QSignalSpy commanded(&announcer, &MediaAnnouncer::commanded);

    QDBusInterface player(kService, kPath, kPlayerInterface, QDBusConnection::sessionBus());
    QVERIFY(player.isValid());
    for (const auto &method : {QStringLiteral("PlayPause"), QStringLiteral("Next"),
             QStringLiteral("Previous"), QStringLiteral("Pause")}) {
        QVERIFY2(player.call(method).type() == QDBusMessage::ReplyMessage, qPrintable(method));
    }

    QCOMPARE(commanded.count(), 4);
    QCOMPARE(commanded.at(0).at(0).toString(), QStringLiteral("playpause"));
    QCOMPARE(commanded.at(1).at(0).toString(), QStringLiteral("next"));
    QCOMPARE(commanded.at(2).at(0).toString(), QStringLiteral("previous"));
    QCOMPARE(commanded.at(3).at(0).toString(), QStringLiteral("pause"));
}

void MediaAnnouncerTest::withdrawsThePlayerWhenTheSoundStops()
{
    MediaAnnouncer announcer;
    announcer.announce(playing(QStringLiteral("tab-a"), QStringLiteral("Track")));
    QVERIFY(serviceRegistered());

    announcer.announce({});
    QVERIFY(!serviceRegistered());
}

QTEST_GUILESS_MAIN(MediaAnnouncerTest)

#include "tst_mediaannouncer.moc"
