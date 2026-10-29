#include "FileChooser.h"

#include <QDBusAbstractAdaptor>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QGuiApplication>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QVariantMap>
#include <QWindow>

using omaweb::FileChooser;

namespace {

const auto kService = QStringLiteral("org.freedesktop.portal.Desktop");
const auto kPath = QStringLiteral("/org/freedesktop/portal/desktop");
const auto kRequestInterface = QStringLiteral("org.freedesktop.portal.Request");

// A filter as the portal's specification writes it, `(sa(us))`: a name, then
// patterns that are globs (0) or MIME types (1). Declared here rather than
// taken from the code under test, so the test reads the wire format itself.
struct Pattern {
    uint kind = 0;
    QString value;
    bool operator==(const Pattern &) const = default;
};
struct Filter {
    QString name;
    QList<Pattern> patterns;
    bool operator==(const Filter &) const = default;
};

QDBusArgument &operator<<(QDBusArgument &argument, const Pattern &pattern)
{
    argument.beginStructure();
    argument << pattern.kind << pattern.value;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, Pattern &pattern)
{
    argument.beginStructure();
    argument >> pattern.kind >> pattern.value;
    argument.endStructure();
    return argument;
}
QDBusArgument &operator<<(QDBusArgument &argument, const Filter &filter)
{
    argument.beginStructure();
    argument << filter.name << filter.patterns;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, Filter &filter)
{
    argument.beginStructure();
    argument >> filter.name >> filter.patterns;
    argument.endStructure();
    return argument;
}

} // namespace

Q_DECLARE_METATYPE(Pattern)
Q_DECLARE_METATYPE(Filter)

namespace {

// The request a dialog belongs to. The portal closes it when the caller takes
// the dialog back, and that is the only call made on it.
class StubRequest final : public QObject {
    Q_OBJECT

public:
    bool closed = false;
};

class StubRequestAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Request")

public:
    explicit StubRequestAdaptor(StubRequest *request)
        : QDBusAbstractAdaptor(request)
    {
    }

public slots:
    void Close() { static_cast<StubRequest *>(parent())->closed = true; }
};

// The desktop's file chooser portal, reduced to what the seam depends on: it
// is asked for a dialog and answers with the request the dialog belongs to.
// The reader's answer arrives later, as `Response` on that request, which the
// test sends itself in the reader's place.
class StubFileChooserPortal final : public QObject, protected QDBusContext {
    Q_OBJECT

public:
    struct Asked {
        QString method;
        QString parentWindow;
        QString title;
        QVariantMap options;
        QString handle;
        StubRequest *request = nullptr;
    };

    QList<Asked> asked;
    QDBusConnection bus = QDBusConnection::sessionBus();

    QDBusObjectPath ask(const QString &method, const QString &parentWindow, const QString &title,
        const QVariantMap &options)
    {
        // The specification's path for a request, from the caller's unique
        // name and its token, which a caller relies on to listen before asking.
        auto sender = message().service().mid(1);
        sender.replace(QLatin1Char('.'), QLatin1Char('_'));
        const auto handle = QStringLiteral("%1/request/%2/%3")
                                .arg(kPath, sender, options.value("handle_token").toString());
        auto *request = new StubRequest;
        new StubRequestAdaptor(request);
        connection().registerObject(handle, request);
        asked.append(Asked {method, parentWindow, title, options, handle, request});
        return QDBusObjectPath(handle);
    }

    void respond(uint response, const QVariantMap &results)
    {
        auto signal = QDBusMessage::createSignal(
            asked.constLast().handle, kRequestInterface, QStringLiteral("Response"));
        signal << response << results;
        bus.send(signal);
    }
};

class FileChooserAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit FileChooserAdaptor(StubFileChooserPortal *portal)
        : QDBusAbstractAdaptor(portal)
    {
    }

    uint version() const { return 4; }

public slots:
    QDBusObjectPath OpenFile(
        const QString &parentWindow, const QString &title, const QVariantMap &options)
    {
        return portal()->ask(QStringLiteral("OpenFile"), parentWindow, title, options);
    }
    QDBusObjectPath SaveFile(
        const QString &parentWindow, const QString &title, const QVariantMap &options)
    {
        return portal()->ask(QStringLiteral("SaveFile"), parentWindow, title, options);
    }

private:
    StubFileChooserPortal *portal() const { return static_cast<StubFileChooserPortal *>(parent()); }
};

template <typename T> T unmarshalled(const QVariant &value)
{
    T result;
    qvariant_cast<QDBusArgument>(value) >> result;
    return result;
}

} // namespace

class FileChooserPortalTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void opensOneFileThroughThePortal();
    void opensSeveralFiles();
    void opensAFolder();
    void savesWhereTheSaveStarts();
    void offersAPagesAcceptedTypesFirst();
    void offersQtsNameFilters();
    void reportsACancelledDialog();
    void takesTheDialogBackUnanswered();
    void namesNoWindowWhenNoneAsked();
    void hearsAPortalThatIsSlowToAnswer();
    void namesTheWindowTheDialogBelongsTo();
    void asksNothingWithoutAPortal();

private:
    StubFileChooserPortal *m_portal = nullptr;
};

void FileChooserPortalTest::initTestCase()
{
    qDBusRegisterMetaType<Pattern>();
    qDBusRegisterMetaType<QList<Pattern>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<QList<Filter>>();

    auto bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "the test needs a session bus, run it under dbus-run-session");

    m_portal = new StubFileChooserPortal;
    new FileChooserAdaptor(m_portal);
    QVERIFY(bus.registerObject(kPath, m_portal));
    QVERIFY(bus.registerService(kService));
}

// One file, chosen in the desktop's own dialog: the reader's choice comes back
// as the address the portal gave for it.
void FileChooserPortalTest::opensOneFileThroughThePortal()
{
    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);
    QSignalSpy cancelled(&chooser, &FileChooser::cancelled);

    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"}}));
    QCOMPARE(m_portal->asked.size(), 1);
    const auto asked = m_portal->asked.constLast();
    QCOMPARE(asked.method, QStringLiteral("OpenFile"));
    QCOMPARE(asked.title, QStringLiteral("Choose file"));
    QCOMPARE(asked.options.value("multiple").toBool(), false);
    QCOMPARE(asked.options.value("directory").toBool(), false);
    QVERIFY(chooser.isOpen());

    m_portal->respond(0, {{"uris", QStringList {"file:///home/reader/one.png"}}});
    QVERIFY(chosen.wait());
    QCOMPARE(chosen.constFirst().constFirst().value<QList<QUrl>>(),
        QList<QUrl> {QUrl("file:///home/reader/one.png")});
    QCOMPARE(cancelled.size(), 0);
    QVERIFY(!chooser.isOpen());
}

// Several files at once, each coming back, in the order the reader gave them.
void FileChooserPortalTest::opensSeveralFiles()
{
    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);

    QVERIFY(chooser.ask({{"mode", "open-multiple"}, {"title", "Choose file"}}));
    const auto asked = m_portal->asked.constLast();
    QCOMPARE(asked.method, QStringLiteral("OpenFile"));
    QCOMPARE(asked.options.value("multiple").toBool(), true);
    QCOMPARE(asked.options.value("directory").toBool(), false);

    m_portal->respond(
        0, {{"uris", QStringList {"file:///home/reader/one.png", "file:///home/reader/two.png"}}});
    QVERIFY(chosen.wait());
    QCOMPARE(chosen.constFirst().constFirst().value<QList<QUrl>>(),
        (QList<QUrl> {QUrl("file:///home/reader/one.png"), QUrl("file:///home/reader/two.png")}));
}

// A folder is an open dialog that chooses directories, which is how the portal
// asks for one. A name with a space in it arrives percent-encoded and goes on
// as the same address Qt's dialog would give.
void FileChooserPortalTest::opensAFolder()
{
    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);

    QVERIFY(chooser.ask({{"mode", "folder"}, {"title", "Choose folder"}}));
    const auto asked = m_portal->asked.constLast();
    QCOMPARE(asked.method, QStringLiteral("OpenFile"));
    QCOMPARE(asked.title, QStringLiteral("Choose folder"));
    QCOMPARE(asked.options.value("directory").toBool(), true);
    QCOMPARE(asked.options.value("multiple").toBool(), false);

    m_portal->respond(0, {{"uris", QStringList {"file:///home/reader/My%20Photos"}}});
    QVERIFY(chosen.wait());
    const auto urls = chosen.constFirst().constFirst().value<QList<QUrl>>();
    QCOMPARE(urls.size(), 1);
    QCOMPARE(urls.constFirst().toLocalFile(), QStringLiteral("/home/reader/My Photos"));
}

// A save starts in the folder it was given, under the name it was given, and
// the portal takes the folder as a NUL-terminated byte string.
void FileChooserPortalTest::savesWhereTheSaveStarts()
{
    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);

    QVERIFY(chooser.ask({{"mode", "save"}, {"title", "Save as"},
        {"currentFile", QUrl("file:///home/reader/Downloads/archive.zip")}}));
    const auto asked = m_portal->asked.constLast();
    QCOMPARE(asked.method, QStringLiteral("SaveFile"));
    QCOMPARE(asked.title, QStringLiteral("Save as"));
    QCOMPARE(asked.options.value("current_name").toString(), QStringLiteral("archive.zip"));
    QCOMPARE(asked.options.value("current_folder").toByteArray(),
        QByteArray("/home/reader/Downloads\0", 23));
    QVERIFY(!asked.options.contains("multiple"));
    QVERIFY(!asked.options.contains("directory"));

    m_portal->respond(0, {{"uris", QStringList {"file:///home/reader/Downloads/kept.zip"}}});
    QVERIFY(chosen.wait());
    QCOMPARE(chosen.constFirst().constFirst().value<QList<QUrl>>(),
        QList<QUrl> {QUrl("file:///home/reader/Downloads/kept.zip")});
}

// What a page's `accept` names is the filter the dialog opens on: its MIME
// types as MIME types and its suffixes as globs, which match either case, as
// Chrome's do, so `REPORT.PDF` is offered for `.pdf`. The reader can still widen it
// to the filters after it.
void FileChooserPortalTest::offersAPagesAcceptedTypesFirst()
{
    FileChooser chooser;
    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"},
        {"acceptedTypes", QStringList {"image/*", ".pdf", "text/plain"}},
        {"acceptedTypesName", "Files the page takes"},
        {"nameFilters", QStringList {"All files (*)"}}}));

    const auto asked = m_portal->asked.constLast();
    const auto filters = unmarshalled<QList<Filter>>(asked.options.value("filters"));
    const QList<Filter> expected {
        {"Files the page takes", {{1, "image/*"}, {0, "*.[pP][dD][fF]"}, {1, "text/plain"}}},
        {"All files", {{0, "*"}}},
    };
    QCOMPARE(filters, expected);
    QCOMPARE(unmarshalled<Filter>(asked.options.value("current_filter")), expected.constFirst());
    chooser.close();
}

// Omaweb's own dialogs name their filters the way Qt's dialog takes them, and
// the portal is given the same filters.
void FileChooserPortalTest::offersQtsNameFilters()
{
    FileChooser chooser;
    QVERIFY(chooser.ask({{"mode", "save"}, {"title", "Save Sync recovery key"},
        {"nameFilters", QStringList {"Text files (*.txt)", "Pages (*.html *.htm)"}}}));

    const auto filters
        = unmarshalled<QList<Filter>>(m_portal->asked.constLast().options.value("filters"));
    const QList<Filter> expected {
        {"Text files", {{0, "*.txt"}}},
        {"Pages", {{0, "*.html"}, {0, "*.htm"}}},
    };
    QCOMPARE(filters, expected);
    chooser.close();
}

// The reader closing the dialog is an answer of its own, and the page that
// asked is told it got nothing.
void FileChooserPortalTest::reportsACancelledDialog()
{
    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);
    QSignalSpy cancelled(&chooser, &FileChooser::cancelled);

    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"}}));
    m_portal->respond(1, {});
    QVERIFY(cancelled.wait());
    QCOMPARE(chosen.size(), 0);
    QVERIFY(!chooser.isOpen());
}

// A page that goes away while its dialog is open takes the dialog with it:
// the portal is told to close the request, and nothing is reported, since
// there is no page left to report to.
void FileChooserPortalTest::takesTheDialogBackUnanswered()
{
    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);
    QSignalSpy cancelled(&chooser, &FileChooser::cancelled);

    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"}}));
    auto *request = m_portal->asked.constLast().request;
    chooser.close();
    QVERIFY(!chooser.isOpen());
    QTRY_VERIFY(request->closed);

    // A late answer to a request already taken back is not the reader's.
    m_portal->respond(0, {{"uris", QStringList {"file:///home/reader/late.png"}}});
    QTest::qWait(100);
    QCOMPARE(chosen.size(), 0);
    QCOMPARE(cancelled.size(), 0);
}

void FileChooserPortalTest::namesNoWindowWhenNoneAsked()
{
    FileChooser chooser;
    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"}}, nullptr));
    QCOMPARE(m_portal->asked.constLast().parentWindow, QString());
    chooser.close();
}

// A portal that has not answered in time may still show its dialog, so the
// caller is told one is open rather than opening Qt's beside it, and the
// reader's answer is heard on the request's path.
void FileChooserPortalTest::hearsAPortalThatIsSlowToAnswer()
{
    // The portal on a connection of its own, served by the same thread the
    // caller blocks while it waits, so it answers only after the wait is over.
    const auto name = QStringLiteral("slow-portal");
    auto slow = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
    StubFileChooserPortal portal;
    portal.bus = slow;
    new FileChooserAdaptor(&portal);
    QVERIFY(QDBusConnection::sessionBus().unregisterService(kService));
    const auto restore = qScopeGuard([&] {
        slow.unregisterService(kService);
        slow.unregisterObject(kPath);
        QDBusConnection::disconnectFromBus(name);
        QDBusConnection::sessionBus().registerService(kService);
    });
    QVERIFY(slow.registerObject(kPath, &portal));
    QVERIFY(slow.registerService(kService));

    FileChooser chooser;
    QSignalSpy chosen(&chooser, &FileChooser::chosen);
    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"}}));
    QVERIFY(chooser.isOpen());

    QTRY_COMPARE(portal.asked.size(), 1);
    portal.respond(0, {{"uris", QStringList {"file:///home/reader/slow.png"}}});
    QVERIFY(chosen.wait());
    QCOMPARE(chosen.constFirst().constFirst().value<QList<QUrl>>(),
        QList<QUrl> {QUrl("file:///home/reader/slow.png")});
}

// The name is the window system's to give, so this is only an answer on a
// session that has one. Under the offscreen platform the suite otherwise runs
// on there is no surface to export.
void FileChooserPortalTest::namesTheWindowTheDialogBelongsTo()
{
    if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
        QSKIP("the portal's name for a window comes from the window system");
    }

    QWindow window;
    window.resize(320, 240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FileChooser chooser;
    QVERIFY(chooser.ask({{"mode", "open"}, {"title", "Choose file"}}, &window));
    const auto parent = m_portal->asked.constLast().parentWindow;
    QVERIFY2(parent.startsWith(QStringLiteral("wayland:")), qPrintable(parent));
    QVERIFY(parent.size() > QStringLiteral("wayland:").size());
    chooser.close();
}

// Without a portal nothing is shown and the caller is told so, which is its
// cue to open Qt's own dialog. This runs last: it takes the portal away.
void FileChooserPortalTest::asksNothingWithoutAPortal()
{
    QVERIFY(QDBusConnection::sessionBus().unregisterService(kService));

    FileChooser chooser;
    QVERIFY(!chooser.ask({{"mode", "open"}, {"title", "Choose file"}}));
    QVERIFY(!chooser.isOpen());
}

QTEST_MAIN(FileChooserPortalTest)

#include "tst_filechooserportal.moc"
