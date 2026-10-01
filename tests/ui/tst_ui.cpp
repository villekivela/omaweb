#include "AgentActivityLog.h"
#include "AgentControl.h"
#include "BrowserController.h"
#include "ContentBlocker.h"
#include "EngineBuild.h"
#include "EngineCapabilities.h"
#include "PageImages.h"
#include "FaviconTint.h"
#include "StoredFaviconProvider.h"
#include "ExternalProtocolHandler.h"
#include "InputMethod.h"
#include "KeyboardNavigation.h"
#include "HttpsOnly.h"
#include "EngineSuggestions.h"
#include "../core/SuggestServer.h"
#include "SecureDns.h"
#include "FontSettings.h"
#include "KitTheme.h"
#include "MediaAnnouncer.h"
#include "PagePrinter.h"
#include "ProbeClock.h"
#include "ProcessResources.h"
#include "RuntimeSecurity.h"
#include "SavedDownload.h"
#include "SoundingTabs.h"
#include "SystemNotifier.h"
#include "Quickshell.h"
#include "PrimaryHold.h"
#include "SystemClipboard.h"
#include "ThemeController.h"
#include "WindowManager.h"

#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQmlContext>
#include <QDir>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFontDatabase>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QUrl>
#include <QWindow>
#include <QtQuickTest/quicktest.h>

#include <memory>

namespace {

// A search engine's suggest endpoint on loopback, for the Omnibar's Engine
// suggestion rows to be asked for and answered without the network.
class SuggestServerProbe final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE QString suggestUrl() { return server().suggestUrl(); }
    Q_INVOKABLE int requestCount() { return static_cast<int>(server().requests().size()); }
    Q_INVOKABLE QString lastTarget()
    {
        return server().requests().isEmpty()
            ? QString {}
            : QString::fromUtf8(server().requests().constLast().target);
    }
    Q_INVOKABLE void answer(const QString &body) { server().body = body.toUtf8(); }

private:
    // Built on first use rather than with the harness, which exists before
    // the application's event loop does and so could not accept a socket.
    omaweb::test::SuggestServer &server()
    {
        if (!m_server) {
            m_server = std::make_unique<omaweb::test::SuggestServer>();
        }
        return *m_server;
    }

    std::unique_ptr<omaweb::test::SuggestServer> m_server;
};

class SyncLauncherProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *controller READ controller CONSTANT)
    Q_PROPERTY(QString errorMessage READ errorMessage CONSTANT)
    Q_PROPERTY(bool configured READ configured CONSTANT)

public:
    QObject *controller() const { return nullptr; }
    QString errorMessage() const { return {}; }
    bool configured() const { return false; }
    Q_INVOKABLE bool load() { return false; }
};

// The desktop's default-browser setting as a desktop that offers no way to ask
// for it answers. The real one runs `xdg-settings` on the host, which would
// make the settings page under test draw whatever this machine says.
class DefaultBrowserProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)

public:
    bool available() const { return false; }
    bool isDefault() const { return false; }
    Q_INVOKABLE bool makeDefault() { return false; }
    Q_INVOKABLE void refresh() { }

signals:
    void changed();
};

// The engine's side of Secure DNS: whether it took the resolver it was given.
// A test says it did not, which a real engine does for a template Chromium's
// own parser refuses.
class EngineSecureDnsProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool applied MEMBER m_applied NOTIFY appliedChanged)

signals:
    void appliedChanged();

private:
    bool m_applied = true;
};

// A page's icon as a web engine's icon store hands it over: an image provider
// the QML engine already has. The id names the icon's colour.
class ColourIcons final : public QQuickImageProvider {
public:
    ColourIcons()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString &id, QSize *size, const QSize &) override
    {
        QImage icon(16, 16, QImage::Format_ARGB32);
        icon.fill(QColor(id));
        if (size) {
            *size = icon.size();
        }
        return icon;
    }
};

// What a test needs to read an image the browser wrote: its size, one pixel,
// and the files in a directory of the harness's own.
class ImageProbe final : public QObject {
    Q_OBJECT

public:
    explicit ImageProbe(QString root)
        : m_root(std::move(root))
    {
    }

    Q_INVOKABLE QString directory(const QString &name) const
    {
        const QDir root(m_root);
        root.mkpath(name);
        return root.filePath(name);
    }
    Q_INVOKABLE QStringList files(const QString &directory) const
    {
        return QDir(directory).entryList(QDir::Files, QDir::Name);
    }
    Q_INVOKABLE QSize size(const QString &path) const { return QImage(path).size(); }
    Q_INVOKABLE QColor pixel(const QString &path, int x, int y) const
    {
        return QImage(path).pixelColor(x, y);
    }

private:
    QString m_root;
};

// The shape a window asks the compositor for. QML reads an item's cursorShape
// but not which item's cursor won, and that is the window's.
class CursorProbe final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE int shape(QWindow *window) const { return window->cursor().shape(); }
};

// What the desktop is asked to open. The offscreen platform has no desktop to
// ask, so a local file or folder handed to it is kept here instead.
class DesktopProbe final : public QObject {
    Q_OBJECT

public:
    DesktopProbe() { QDesktopServices::setUrlHandler(QStringLiteral("file"), this, "open"); }
    ~DesktopProbe() override { QDesktopServices::unsetUrlHandler(QStringLiteral("file")); }
    Q_INVOKABLE QStringList opened() const { return m_opened; }

public slots:
    void open(const QUrl &url) { m_opened.append(url.toLocalFile()); }

private:
    QStringList m_opened;
};

// The window losing the keyboard to another one. The offscreen platform has no
// other window to hand it to, so the event the compositor's would cause is sent.
class WindowFocusProbe final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void deactivate(QWindow *window) const
    {
        QEvent deactivate(QEvent::WindowDeactivate);
        QCoreApplication::sendEvent(window, &deactivate);
    }
};

// A browser key the reader moves in their keybindings file, and the keymap
// reading the file again as it does after a Sync.
class KeymapProbe final : public QObject {
    Q_OBJECT

public:
    KeymapProbe(omaweb::KeyboardNavigation *keymap, QString path)
        : m_keymap(keymap)
        , m_path(std::move(path))
    {
    }
    Q_INVOKABLE bool rebind(const QString &from, const QString &to)
    {
        QFile file(m_path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        auto configuration = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        auto browser = configuration.value(QStringLiteral("browser")).toObject();
        if (!browser.contains(from)) {
            return false;
        }
        browser.insert(to, browser.take(from));
        configuration.insert(QStringLiteral("browser"), browser);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        file.write(QJsonDocument(configuration).toJson());
        file.close();
        return m_keymap->reload();
    }

private:
    omaweb::KeyboardNavigation *m_keymap;
    QString m_path;
};

// The application's release watch, which these tests never let ask GitHub. It
// announces no release, and counts the windows that ask it for the notes of an
// upgrade. Where they open, and that they open once, is tst_releasewatch's.
class ReleaseWatchProbe final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checkEnabled READ checkEnabled WRITE setCheckEnabled NOTIFY changed)
    Q_PROPERTY(bool announcing READ announcing CONSTANT)
    Q_PROPERTY(QString release READ release CONSTANT)
    Q_PROPERTY(QString instruction READ instruction CONSTANT)
    Q_PROPERTY(QUrl notes READ notes CONSTANT)
    Q_PROPERTY(int asked READ asked)

public:
    bool checkEnabled() const { return false; }
    void setCheckEnabled(bool) { }
    bool announcing() const { return false; }
    QString release() const { return {}; }
    QString instruction() const { return {}; }
    QUrl notes() const { return {}; }
    int asked() const { return m_asked; }
    Q_INVOKABLE void dismiss() { }
    Q_INVOKABLE QUrl openUpgradeNotes()
    {
        ++m_asked;
        return {};
    }

signals:
    void changed();

private:
    int m_asked = 0;
};

// An Agent Space, which only the Agent socket makes in the browser.
class AgentSpaceProbe final : public QObject {
    Q_OBJECT

public:
    explicit AgentSpaceProbe(omaweb::BrowserController *browser)
        : m_browser(browser)
    {
    }
    Q_INVOKABLE QString create(const QString &name, const QString &creator, bool temporary)
    {
        return m_browser->createAgentSpace(name, creator, temporary);
    }

private:
    omaweb::BrowserController *m_browser;
};

// A request as the Agent socket hands it to the core, so a test reaches the
// window the way `omaweb run` does and reads the answer the CLI would.
class AgentSocketProbe final : public QObject {
    Q_OBJECT
    // The answers `send` has had so far, in the order they came.
    Q_PROPERTY(QVariantList replies READ replies NOTIFY repliesChanged)

public:
    explicit AgentSocketProbe(omaweb::AgentControl &control)
        : m_control(control)
    {
    }

    Q_INVOKABLE QVariantMap ask(const QVariantMap &request)
    {
        return m_control.answer(QJsonObject::fromVariantMap(request)).toVariantMap();
    }

    // A request answered later, by the page or by the reader, as a page verb
    // is.
    Q_INVOKABLE void send(const QVariantMap &request)
    {
        m_control.handle(QJsonObject::fromVariantMap(request), [this](const QJsonObject &answer) {
            m_replies.append(answer.toVariantMap());
            emit repliesChanged();
        });
    }

    QVariantList replies() const { return m_replies; }

signals:
    void repliesChanged();

private:
    omaweb::AgentControl &m_control;
    QVariantList m_replies;
};

// A favicon on disk for the tests that check what colour a site's chip takes.
// A mark on a transparent plate is the shape a real favicon has.
QUrl writeFavicon(const QString &path, const QColor &mark)
{
    QImage icon(32, 32, QImage::Format_ARGB32);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setPen(Qt::NoPen);
    painter.setBrush(mark);
    painter.drawRect(6, 6, 20, 20);
    painter.end();
    return icon.save(path) ? QUrl::fromLocalFile(path) : QUrl {};
}

} // namespace

class UiTestSetup final : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        // The settings page's about section reads Qt.application.version, so the
        // test host has to carry the version the browser does.
        QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));
        omaweb::quickshell::installShim(*engine);
        omaweb::installStoredFavicons(*engine);
        engine->addImageProvider(QStringLiteral("omawebtesticon"), new ColourIcons);
        omaweb::registerBrowserController();
        omaweb::registerDownloads();
        omaweb::registerFaviconTint();
        omaweb::registerFontSettings();
        omaweb::registerEngineCapabilities();
        omaweb::registerEngineBuild();
        omaweb::registerPageImages();
        omaweb::registerSystemClipboard();
        omaweb::registerPrimaryHold();
        omaweb::registerExternalProtocolHandler();
        omaweb::registerPagePrinter();
        omaweb::registerSystemNotifier();
        omaweb::registerSoundingTabs();
        omaweb::registerMediaAnnouncer();
        omaweb::registerProcessResources();
        omaweb::registerSavedDownload();
        qmlRegisterSingletonType<DefaultBrowserProbe>("Omaweb", 1, 0, "DefaultBrowser",
            [](QQmlEngine *, QJSEngine *) -> QObject * { return new DefaultBrowserProbe; });
        m_runtimeSecurity = std::make_unique<omaweb::RuntimeSecurity>(
            omaweb::SandboxHost {}, omaweb::RuntimeSecurity::EngineBuild {});
        omaweb::registerRuntimeSecurity(m_runtimeSecurity.get());
        // A desktop that asked for no input method, so the chrome under test is
        // the ordinary one rather than one carrying a notice about this host.
        m_inputMethod = std::make_unique<omaweb::InputMethodReport>(omaweb::InputMethodHost {});
        omaweb::registerInputMethodReport(m_inputMethod.get());
        m_dataRoot = std::make_unique<QTemporaryDir>();
        m_engineSuggestions = std::make_unique<omaweb::EngineSuggestions>(
            m_dataRoot->filePath(QStringLiteral("config")));
        // A config root of its own, so a test can save a search engine.
        m_browser = std::make_unique<omaweb::BrowserController>(
            omaweb::SpaceStorage(m_dataRoot->path(), QStringLiteral("mock")),
            m_dataRoot->filePath(QStringLiteral("config")));
        m_browser->setEngineSuggestions(m_engineSuggestions.get());
        m_contentBlocker = std::make_unique<omaweb::ContentBlocker>(m_dataRoot->path());
        m_imageProbe = std::make_unique<ImageProbe>(m_dataRoot->filePath(QStringLiteral("images")));
        const auto keybindingsPath = m_dataRoot->filePath(QStringLiteral("keybindings.json"));
        QFile::copy(QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH), keybindingsPath);
        QFile::setPermissions(keybindingsPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        m_keyboardNavigation = std::make_unique<omaweb::KeyboardNavigation>(keybindingsPath);
        m_keymapProbe = std::make_unique<KeymapProbe>(m_keyboardNavigation.get(), keybindingsPath);
        m_theme = std::make_unique<omaweb::ThemeController>(QStringLiteral(OMAWEB_THEME_PATH));
        // A config root of its own, so a test can set a size and the reader's
        // configuration never learns of it.
        m_fontSettings = std::make_unique<omaweb::FontSettings>(
            m_dataRoot->filePath(QStringLiteral("config")), QFontDatabase::families());
        m_windowManager = std::make_unique<omaweb::WindowManager>();
        // A real choice under the throwaway config root, so a test can name a
        // resolver and read what the chrome says about it.
        m_secureDns
            = std::make_unique<omaweb::SecureDns>(m_dataRoot->filePath(QStringLiteral("config")));
        m_httpsOnly
            = std::make_unique<omaweb::HttpsOnly>(m_dataRoot->filePath(QStringLiteral("config")));
        // Two Agents' lines, a minute apart, for the Agent activity page to
        // list and filter.
        m_agentActivity = std::make_unique<omaweb::AgentActivityLog>(
            m_dataRoot->filePath(QStringLiteral("agent-activity")));
        const auto now = QDateTime::currentMSecsSinceEpoch();
        m_agentActivity->record({.time = now - 60000,
            .agent = QStringLiteral("claude"),
            .spaceId = QStringLiteral("research-space"),
            .space = QStringLiteral("Research"),
            .tabId = QStringLiteral("research-tab"),
            .address = QStringLiteral("https://shop.example/login"),
            .verb = QStringLiteral("do"),
            .target = QStringLiteral("fill 7, click 9"),
            .outcome = QStringLiteral("ok")});
        m_agentActivity->record({.time = now,
            .agent = QStringLiteral("script"),
            .spaceId = QStringLiteral("errands-space"),
            .space = QStringLiteral("Errands"),
            .tabId = {},
            .address = {},
            .verb = QStringLiteral("open"),
            .target = QStringLiteral("https://errands.example/"),
            .outcome = QStringLiteral("refused")});
        engine->rootContext()->setContextProperty(QStringLiteral("browser"), m_browser.get());
        m_agentSpaceProbe = std::make_unique<AgentSpaceProbe>(m_browser.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("agentSpaceProbe"), m_agentSpaceProbe.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("agentActivity"), m_agentActivity.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("contentBlocker"), m_contentBlocker.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("keyboardNavigation"), m_keyboardNavigation.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineContentBlocker"), QVariant::fromValue<QObject *>(nullptr));
        // The lab runs no engine, so there is no third-party filter to attach.
        // Site information reads the gap off the adapter's capabilities.
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineCookiePolicy"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineHeldDownloads"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(QStringLiteral("theme"), m_theme.get());
        engine->rootContext()->setContextProperty(QStringLiteral("imageProbe"), m_imageProbe.get());
        engine->rootContext()->setContextProperty(QStringLiteral("cursorProbe"), &m_cursorProbe);
        engine->rootContext()->setContextProperty(QStringLiteral("desktopProbe"), &m_desktopProbe);
        engine->rootContext()->setContextProperty(
            QStringLiteral("windowFocusProbe"), &m_windowFocusProbe);
        engine->rootContext()->setContextProperty(
            QStringLiteral("keymapProbe"), m_keymapProbe.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("fontSettings"), m_fontSettings.get());
        // These tests run no engine, so there is nothing to draw a page's
        // fonts with; the page shows the controls and they reach nothing.
        engine->rootContext()->setContextProperty(
            QStringLiteral("pageFonts"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(QStringLiteral("syncLauncher"), &m_syncLauncher);
        // The tests draw the chrome without asking GitHub anything, so the
        // watch announces no release and the Release mark is not shown.
        engine->rootContext()->setContextProperty(QStringLiteral("releaseWatch"), &m_releaseWatch);
        engine->rootContext()->setContextProperty(
            QStringLiteral("globalPrivacyControl"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(
            QStringLiteral("webRtcPolicy"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(QStringLiteral("secureDns"), m_secureDns.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineSecureDns"), &m_engineSecureDns);
        engine->rootContext()->setContextProperty(QStringLiteral("httpsOnly"), m_httpsOnly.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineSuggestions"), m_engineSuggestions.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("suggestServer"), &m_suggestServer);
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineWebRtcPolicy"), QVariant::fromValue<QObject *>(nullptr));
        engine->rootContext()->setContextProperty(
            QStringLiteral("windowManager"), m_windowManager.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("engineViewSource"), QUrl(QStringLiteral(OMAWEB_MOCK_ENGINE_VIEW_URL)));
        engine->rootContext()->setContextProperty(QStringLiteral("engineProfileSource"),
            QUrl(QStringLiteral(OMAWEB_MOCK_ENGINE_PROFILE_URL)));
        engine->rootContext()->setContextProperty(
            QStringLiteral("iconFontSource"), QUrl(QStringLiteral(OMAWEB_ICON_FONT_URL)));
        // The mock engine reports no icon of its own here; the tests that care
        // about artwork set one themselves.
        engine->rootContext()->setContextProperty(
            QStringLiteral("mockFaviconUrls"), QVariantList {});
        engine->rootContext()->setContextProperty(QStringLiteral("colouredFaviconUrl"),
            writeFavicon(
                m_dataRoot->filePath(QStringLiteral("coloured.png")), QColor(0x2f, 0x5c, 0xe6)));
        engine->rootContext()->setContextProperty(QStringLiteral("colourlessFaviconUrl"),
            writeFavicon(m_dataRoot->filePath(QStringLiteral("colourless.png")), Qt::white));
        // The shim picks the Qt Quick Controls style the vendored kit needs; a
        // native style refuses the kit's replaced `background` and paints its
        // own. Reading the resolved name back is the only way QML can tell.
        engine->rootContext()->setContextProperty(
            QStringLiteral("controlsStyle"), QQuickStyle::name());
        m_probeClock = std::make_unique<omaweb::test::ProbeClock>();
        engine->rootContext()->setContextProperty(QStringLiteral("probeClock"), m_probeClock.get());
        // The core the browser answers the Agent socket with, with no config
        // root, so Allow agents is off as it is until the reader turns it on.
        m_agentControl = std::make_unique<omaweb::AgentControl>(m_browser.get(), QString());
        m_agentSocket = std::make_unique<AgentSocketProbe>(*m_agentControl);
        engine->rootContext()->setContextProperty(
            QStringLiteral("agentControl"), m_agentControl.get());
        engine->rootContext()->setContextProperty(
            QStringLiteral("agentSocket"), m_agentSocket.get());
        engine->addImportPath(QStringLiteral(OMAWEB_UI_DIRECTORY));
        engine->addImportPath(QStringLiteral(OMAWEB_OMARCHY_IMPORT_PATH));
        m_kitTheme
            = std::make_unique<omaweb::KitTheme>(engine, m_theme.get(), m_fontSettings.get());
    }

    void cleanupTestCase()
    {
        m_agentSocket.reset();
        m_agentControl.reset();
        m_probeClock.reset();
        m_kitTheme.reset();
        m_runtimeSecurity.reset();
        m_fontSettings.reset();
        m_theme.reset();
        m_windowManager.reset();
        m_agentSpaceProbe.reset();
        m_browser.reset();
        m_engineSuggestions.reset();
        m_agentActivity.reset();
        m_contentBlocker.reset();
        m_secureDns.reset();
        m_httpsOnly.reset();
        m_keymapProbe.reset();
        m_keyboardNavigation.reset();
        m_imageProbe.reset();
        m_dataRoot.reset();
    }

private:
    SyncLauncherProbe m_syncLauncher;
    CursorProbe m_cursorProbe;
    DesktopProbe m_desktopProbe;
    WindowFocusProbe m_windowFocusProbe;
    ReleaseWatchProbe m_releaseWatch;
    std::unique_ptr<QTemporaryDir> m_dataRoot;
    SuggestServerProbe m_suggestServer;
    std::unique_ptr<omaweb::EngineSuggestions> m_engineSuggestions;
    std::unique_ptr<omaweb::BrowserController> m_browser;
    std::unique_ptr<AgentSpaceProbe> m_agentSpaceProbe;
    std::unique_ptr<omaweb::AgentActivityLog> m_agentActivity;
    std::unique_ptr<omaweb::ContentBlocker> m_contentBlocker;
    std::unique_ptr<ImageProbe> m_imageProbe;
    std::unique_ptr<omaweb::SecureDns> m_secureDns;
    EngineSecureDnsProbe m_engineSecureDns;
    std::unique_ptr<omaweb::HttpsOnly> m_httpsOnly;
    std::unique_ptr<omaweb::KeyboardNavigation> m_keyboardNavigation;
    std::unique_ptr<KeymapProbe> m_keymapProbe;
    std::unique_ptr<omaweb::ThemeController> m_theme;
    std::unique_ptr<omaweb::FontSettings> m_fontSettings;
    std::unique_ptr<omaweb::KitTheme> m_kitTheme;
    std::unique_ptr<omaweb::WindowManager> m_windowManager;
    std::unique_ptr<omaweb::RuntimeSecurity> m_runtimeSecurity;
    std::unique_ptr<omaweb::InputMethodReport> m_inputMethod;
    std::unique_ptr<omaweb::test::ProbeClock> m_probeClock;
    std::unique_ptr<omaweb::AgentControl> m_agentControl;
    std::unique_ptr<AgentSocketProbe> m_agentSocket;
};

QUICK_TEST_MAIN_WITH_SETUP(omaweb_ui, UiTestSetup)

#include "tst_ui.moc"
