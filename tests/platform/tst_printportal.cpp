#include "PagePrinter.h"

#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QGuiApplication>
#include <QTest>
#include <QWindow>
#include <QVariantMap>

using omaweb::PagePrinter;

namespace {

const auto kService = QStringLiteral("org.freedesktop.portal.Desktop");
const auto kPath = QStringLiteral("/org/freedesktop/portal/desktop");

// The desktop's print portal, reduced to the exchange the seam depends on: it
// is handed a document and answers with the request the dialog belongs to. The
// real portal draws that dialog and waits for a person, neither of which a test
// can do.
class StubPrintPortal final : public QObject {
    Q_OBJECT

public:
    struct Request {
        QString parentWindow;
        QString title;
        QByteArray document;
        QVariantMap options;
    };

    QList<Request> requests;

    QString Print(const QString &parentWindow, const QString &title,
        const QDBusUnixFileDescriptor &descriptor, const QVariantMap &options)
    {
        // Read what actually arrived rather than trusting the path it was sent
        // from: the seam takes the spooled file away while the portal still has
        // it open, and this is what proves the document survives that.
        QFile document;
        QByteArray contents;
        if (document.open(descriptor.fileDescriptor(), QIODevice::ReadOnly)) {
            contents = document.readAll();
            document.close();
        }
        requests.append(Request {parentWindow, title, contents, options});
        return QStringLiteral("/org/freedesktop/portal/desktop/request/1/omaweb");
    }
};

class PrintPortalAdaptor final : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.portal.Print")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit PrintPortalAdaptor(StubPrintPortal *portal)
        : QDBusAbstractAdaptor(portal)
    {
    }

    uint version() const { return 4; }

public slots:
    QDBusObjectPath Print(const QString &parentWindow, const QString &title,
        const QDBusUnixFileDescriptor &descriptor, const QVariantMap &options)
    {
        auto *portal = static_cast<StubPrintPortal *>(parent());
        return QDBusObjectPath(portal->Print(parentWindow, title, descriptor, options));
    }
};

} // namespace

class PrintPortalTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void handsTheRenderedDocumentToThePortal();
    void asksForADialogRatherThanAStraightPrint();
    void takesTheSpooledCopyAwayOnceThePortalHasIt();
    void refusesToPrintADocumentThatWasNeverRendered();
    void printsUnparentedWhenNoWindowAsked();
    void namesTheWindowTheDialogBelongsTo();

private:
    StubPrintPortal *m_portal = nullptr;
    PagePrinter *m_printer = nullptr;

    QString spoolARenderedPage(const QByteArray &contents);
};

void PrintPortalTest::initTestCase()
{
    auto bus = QDBusConnection::sessionBus();
    QVERIFY2(bus.isConnected(), "the test needs a session bus, run it under dbus-run-session");

    m_portal = new StubPrintPortal;
    new PrintPortalAdaptor(m_portal);
    QVERIFY(bus.registerObject(kPath, m_portal));
    QVERIFY(bus.registerService(kService));

    m_printer = new PagePrinter(this);
    QVERIFY(m_printer->available());
}

QString PrintPortalTest::spoolARenderedPage(const QByteArray &contents)
{
    const auto path = m_printer->reserveDestination(QStringLiteral("Some page title"));
    QFile document(path);
    if (!document.open(QIODevice::WriteOnly)) {
        return {};
    }
    document.write(contents);
    document.close();
    return path;
}

// What the engine adapter rendered is what the portal is given, by descriptor
// rather than by name, and the job the reader sees named is the page's.
void PrintPortalTest::handsTheRenderedDocumentToThePortal()
{
    const QByteArray rendered = "%PDF-1.4\nrendered page\n%%EOF\n";
    const auto path = spoolARenderedPage(rendered);
    QVERIFY(!path.isEmpty());

    QVERIFY(m_printer->present(path, QStringLiteral("Some page title")));
    QCOMPARE(m_portal->requests.size(), 1);

    const auto request = m_portal->requests.constLast();
    QCOMPARE(request.title, QStringLiteral("Some page title"));
    QCOMPARE(request.document, rendered);
}

// A print token is what tells the portal the settings have already been
// answered. Sending none is what makes it show the dialog, which is the whole
// point of the command.
void PrintPortalTest::asksForADialogRatherThanAStraightPrint()
{
    const auto path = spoolARenderedPage("%PDF-1.4\n%%EOF\n");
    QVERIFY(!path.isEmpty());
    QVERIFY(m_printer->present(path, QString()));

    const auto request = m_portal->requests.constLast();
    QVERIFY(!request.options.contains(QStringLiteral("token")));
    // A job still needs a name, and an untitled page is the browser's.
    QCOMPARE(request.title, QStringLiteral("Omaweb"));
}

// The spool is a staging area rather than a place documents accumulate. The
// portal holds the descriptor, so the copy can go the moment it is handed over.
void PrintPortalTest::takesTheSpooledCopyAwayOnceThePortalHasIt()
{
    const QByteArray rendered = "%PDF-1.4\nstill readable\n%%EOF\n";
    const auto path = spoolARenderedPage(rendered);
    QVERIFY(!path.isEmpty());
    QVERIFY(QFile::exists(path));

    QVERIFY(m_printer->present(path, QStringLiteral("Job")));
    QVERIFY(!QFile::exists(path));
    // Taken away by name, and still whole through the descriptor.
    QCOMPARE(m_portal->requests.constLast().document, rendered);
}

// Nothing to present, nothing to keep. A render that never arrived is not a
// dialog the reader should be shown.
void PrintPortalTest::refusesToPrintADocumentThatWasNeverRendered()
{
    const auto asked = m_portal->requests.size();
    QVERIFY(!m_printer->present(QString(), QStringLiteral("Job")));

    const auto missing = m_printer->reserveDestination(QStringLiteral("never rendered"));
    QVERIFY(!missing.isEmpty());
    QVERIFY(!QFile::exists(missing));
    QVERIFY(!m_printer->present(missing, QStringLiteral("Job")));
    QCOMPARE(m_portal->requests.size(), asked);
}

// A print that names no window still prints. The portal places the dialog
// itself then, which is what every print did before a window was named.
void PrintPortalTest::printsUnparentedWhenNoWindowAsked()
{
    const auto path = spoolARenderedPage("%PDF-1.4\n%%EOF\n");
    QVERIFY(!path.isEmpty());
    QVERIFY(m_printer->present(path, QStringLiteral("Job"), nullptr));

    QCOMPARE(m_portal->requests.constLast().parentWindow, QString());
}

// The name is the window system's to give, so this is only an answer on a
// session that has one. Under the offscreen platform the suite otherwise runs
// on there is no surface to export and nothing to check.
void PrintPortalTest::namesTheWindowTheDialogBelongsTo()
{
    if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
        QSKIP("the portal's name for a window comes from the window system");
    }

    QWindow window;
    window.resize(320, 240);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const auto path = spoolARenderedPage("%PDF-1.4\n%%EOF\n");
    QVERIFY(!path.isEmpty());
    QVERIFY(m_printer->present(path, QStringLiteral("Job"), &window));

    // `wayland:<handle>` is a surface exported through `zxdg_exporter_v2`, and
    // the handle beyond the prefix is the compositor's to make up.
    const auto parent = m_portal->requests.constLast().parentWindow;
    QVERIFY2(parent.startsWith(QStringLiteral("wayland:")), qPrintable(parent));
    QVERIFY(parent.size() > QStringLiteral("wayland:").size());
}

QTEST_MAIN(PrintPortalTest)

#include "tst_printportal.moc"
