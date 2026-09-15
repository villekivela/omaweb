#include "MediaAnnouncer.h"

#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QPointer>
#include <QStringList>

namespace omaweb {
namespace {

    const auto kService = QStringLiteral("org.mpris.MediaPlayer2.omaweb");
    const auto kPath = QStringLiteral("/org/mpris/MediaPlayer2");
    const auto kPlayerInterface = QStringLiteral("org.mpris.MediaPlayer2.Player");
    const auto kPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

    // MPRIS gives every track an object path of its own. Omaweb's tracks are
    // tabs, so the tab's identifier is the path, with everything an object path
    // may not hold replaced. A consumer that tells tracks apart by this sees one
    // track per tab, which is what a tab is.
    const auto kNoTrack = QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack");

    // What core last handed over, in the shape `SoundingTabs` assembles. Empty
    // means no tab is making sound.
    QVariantMap announced;

    // The name the player is exported under, which is the plain one unless
    // another Omaweb already answers to it.
    QString exportedName = kService;

    // The subscription is the process's, one per session bus, and it outlives
    // any single announcer. It reports to whichever announcer is current, and to
    // none once that announcer is gone.
    QPointer<MediaAnnouncer> currentAnnouncer;

    QString trackId()
    {
        const auto tabId = announced.value(QStringLiteral("tabId")).toString();
        if (tabId.isEmpty()) {
            return kNoTrack;
        }
        QString path;
        for (const auto character : tabId) {
            path.append(character.isLetterOrNumber() ? character : QLatin1Char('_'));
        }
        return QStringLiteral("/org/omaweb/tab/") + path;
    }

    QVariantMap currentMetadata()
    {
        QVariantMap fields;
        fields.insert(
            QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(trackId())));
        const auto title = announced.value(QStringLiteral("title")).toString();
        if (!title.isEmpty()) {
            fields.insert(QStringLiteral("xesam:title"), title);
        }
        const auto artist = announced.value(QStringLiteral("artist")).toString();
        if (!artist.isEmpty()) {
            fields.insert(QStringLiteral("xesam:artist"), QStringList {artist});
        }
        const auto album = announced.value(QStringLiteral("album")).toString();
        if (!album.isEmpty()) {
            fields.insert(QStringLiteral("xesam:album"), album);
        }
        // The address the page named, passed through. Omaweb neither fetches it
        // nor caches it: a consumer that wants the picture asks for it itself,
        // which keeps an address a page chose off Omaweb's network path.
        const auto artwork = announced.value(QStringLiteral("artwork")).toString();
        if (!artwork.isEmpty()) {
            fields.insert(QStringLiteral("mpris:artUrl"), artwork);
        }
        return fields;
    }

    QString currentPlaybackStatus()
    {
        if (announced.isEmpty()) {
            return QStringLiteral("Stopped");
        }
        return announced.value(QStringLiteral("playing")).toBool() ? QStringLiteral("Playing")
                                                                   : QStringLiteral("Paused");
    }

    // Every player property that follows the sounding tab, in one place. The
    // adaptor's getters answer from here and so does the change signal, so a
    // consumer that reads and one that listens cannot be told different things.
    QVariantMap currentPlayerProperties()
    {
        return {
            {QStringLiteral("PlaybackStatus"), currentPlaybackStatus()},
            {QStringLiteral("Metadata"), currentMetadata()},
            {QStringLiteral("CanGoNext"), announced.value(QStringLiteral("canGoNext")).toBool()},
            {QStringLiteral("CanGoPrevious"),
                announced.value(QStringLiteral("canGoPrevious")).toBool()},
            {QStringLiteral("CanPlay"), !announced.isEmpty()},
            {QStringLiteral("CanPause"), !announced.isEmpty()},
        };
    }

    QVariant playerProperty(const QString &name) { return currentPlayerProperties().value(name); }

    void report(const QString &name)
    {
        if (auto *announcer = currentAnnouncer.data()) {
            emit announcer->commanded(name);
        }
    }

    // The root interface. Omaweb neither quits nor raises on the desktop's
    // request: quitting a browser from a media widget is not something a reader
    // asked for, and the window a sounding tab belongs to is not always a window
    // there is one of.
    class MprisRoot final : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
        Q_PROPERTY(bool CanQuit READ canQuit)
        Q_PROPERTY(bool CanRaise READ canRaise)
        Q_PROPERTY(bool HasTrackList READ hasTrackList)
        Q_PROPERTY(QString Identity READ identity)
        Q_PROPERTY(QString DesktopEntry READ desktopEntry)
        Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
        Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

    public:
        explicit MprisRoot(QObject *parent)
            : QDBusAbstractAdaptor(parent)
        {
        }

        bool canQuit() const { return false; }
        bool canRaise() const { return false; }
        bool hasTrackList() const { return false; }
        QString identity() const { return QStringLiteral("Omaweb"); }
        QString desktopEntry() const { return QStringLiteral("omaweb"); }
        QStringList supportedUriSchemes() const { return {}; }
        QStringList supportedMimeTypes() const { return {}; }

    public slots:
        void Raise() { }
        void Quit() { }
    };

    // The player. Seeking and position are not offered: what is playing belongs
    // to a page, and a page reports neither through the media session it
    // declares.
    class MprisPlayer final : public QDBusAbstractAdaptor {
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
        Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
        Q_PROPERTY(QVariantMap Metadata READ metadata)
        Q_PROPERTY(bool CanGoNext READ canGoNext)
        Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
        Q_PROPERTY(bool CanPlay READ canPlay)
        Q_PROPERTY(bool CanPause READ canPause)
        Q_PROPERTY(bool CanSeek READ canSeek)
        Q_PROPERTY(bool CanControl READ canControl)
        Q_PROPERTY(double Rate READ rate)
        Q_PROPERTY(double MinimumRate READ rate)
        Q_PROPERTY(double MaximumRate READ rate)
        Q_PROPERTY(double Volume READ volume)
        Q_PROPERTY(qlonglong Position READ position)

    public:
        explicit MprisPlayer(QObject *parent)
            : QDBusAbstractAdaptor(parent)
        {
        }

        QString playbackStatus() const
        {
            return playerProperty(QStringLiteral("PlaybackStatus")).toString();
        }
        QVariantMap metadata() const { return playerProperty(QStringLiteral("Metadata")).toMap(); }
        bool canGoNext() const { return playerProperty(QStringLiteral("CanGoNext")).toBool(); }
        bool canGoPrevious() const
        {
            return playerProperty(QStringLiteral("CanGoPrevious")).toBool();
        }
        bool canPlay() const { return playerProperty(QStringLiteral("CanPlay")).toBool(); }
        bool canPause() const { return playerProperty(QStringLiteral("CanPause")).toBool(); }
        bool canSeek() const { return false; }
        bool canControl() const { return true; }
        double rate() const { return 1.0; }
        // Per-tab volume is the desktop's own mixer's, which already has one
        // slider per process. A second one here would be a second answer.
        double volume() const { return 1.0; }
        qlonglong position() const { return 0; }

    public slots:
        void Play() { report(QStringLiteral("play")); }
        void Pause() { report(QStringLiteral("pause")); }
        void PlayPause() { report(QStringLiteral("playpause")); }
        void Stop() { report(QStringLiteral("stop")); }
        void Next() { report(QStringLiteral("next")); }
        void Previous() { report(QStringLiteral("previous")); }

    signals:
        void Seeked(qlonglong position);
    };

    // The object the two adaptors hang off. It is the process's, built once and
    // registered on the bus for as long as a tab is making sound.
    QObject *playerObject()
    {
        static QObject *shared = [] {
            auto *created = new QObject;
            new MprisRoot(created);
            new MprisPlayer(created);
            return created;
        }();
        return shared;
    }

    // Qt does not emit `PropertiesChanged` for an adaptor's properties, and a
    // consumer that polls instead of listening is a consumer that shows the
    // previous track. The properties that follow the sounding tab are sent by
    // hand whenever it changes.
    void reportPropertiesChanged()
    {
        auto signal = QDBusMessage::createSignal(
            kPath, kPropertiesInterface, QStringLiteral("PropertiesChanged"));
        signal << kPlayerInterface << currentPlayerProperties() << QStringList {};
        QDBusConnection::sessionBus().send(signal);
    }

} // namespace

MediaAnnouncer::MediaAnnouncer(QObject *parent)
    : QObject(parent)
{
    currentAnnouncer = this;
}

MediaAnnouncer::~MediaAnnouncer()
{
    if (currentAnnouncer == this) {
        currentAnnouncer.clear();
        announce({});
    }
}

bool MediaAnnouncer::available() const { return QDBusConnection::sessionBus().isConnected(); }

void MediaAnnouncer::announce(const QVariantMap &announcement)
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        return;
    }
    if (announcement == announced) {
        return;
    }
    const auto wasExported = !announced.isEmpty();
    announced = announcement;

    if (announced.isEmpty()) {
        if (wasExported) {
            bus.unregisterObject(kPath);
            bus.unregisterService(exportedName);
            // The plain name is asked for again next time. Holding an instance
            // name for the rest of the process would keep Omaweb a second-class
            // player on a desktop where the first Omaweb has since gone.
            exportedName = kService;
        }
        return;
    }
    if (!wasExported) {
        // The object goes on the bus before the name does, so the first thing a
        // consumer does after seeing the name arrive finds a player to read.
        bus.registerObject(kPath, playerObject(), QDBusConnection::ExportAdaptors);
        if (!bus.registerService(exportedName)) {
            // A second Omaweb is already holding the plain name. MPRIS names
            // the rest by instance rather than having them go unannounced.
            exportedName = kService + QStringLiteral(".instance")
                + QString::number(QCoreApplication::applicationPid());
            bus.registerService(exportedName);
        }
        return;
    }
    reportPropertiesChanged();
}

} // namespace omaweb

#include "LinuxMediaAnnouncer.moc"
