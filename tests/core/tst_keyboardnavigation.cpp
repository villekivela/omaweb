#include "KeyboardNavigation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <utility>

using omaweb::KeyboardNavigation;

class KeyboardNavigationTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsVersionedBindingsDisabledByDefault();
    void resolvesSitePassthroughForHostsAndSubdomains();
    void rejectsUnsupportedVersionsAndCommands();
    void dropsBindingsThisBuildDoesNotKnowAndKeepsTheRest();
    void readsTheCommandPanelIdAsTheCommandScope();
    void bindsTheScreenshotCommands();
    void bindsSiteInformation();
    void persistsTheEnabledSetting();
    void adoptsNewDefaultsOnceWithoutResurrectingRemovedBindings();
    void replacesRetiredDefaultWithoutChangingCustomBindings();
    void movesTheShippedOpenFileKeyToJumpBack();
    void offersTheShippedJumpForwardKeyOnce();
    void seedsAFileTheOwnerCanWrite();
    void makesAReadOnlyFileWritableAgain();
};

static QString writeConfiguration(const QString &directory, const QByteArray &contents)
{
    const auto path = directory + QStringLiteral("/keybindings.json");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()) {
        return {};
    }
    return path;
}

void KeyboardNavigationTest::loadsVersionedBindingsDisabledByDefault()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": false,
        "bindings": {
            "j": "scroll-down",
            "k": "scroll-up",
            "gg": "scroll-top",
            "G": "scroll-bottom",
            "f": "open-link",
            "Shift+F": "open-link-background"
        },
        "passthrough": {}
    })JSON");

    KeyboardNavigation navigation(path);

    QVERIFY(navigation.valid());
    QVERIFY(!navigation.enabled());
    QCOMPARE(
        navigation.bindings().value(QStringLiteral("gg")).toString(), QStringLiteral("scroll-top"));
    const auto configuration
        = navigation.configurationForUrl(QUrl(QStringLiteral("https://example.com")));
    QCOMPARE(configuration.value(QStringLiteral("version")).toInt(), 1);
    QVERIFY(!configuration.value(QStringLiteral("enabled")).toBool());
}

void KeyboardNavigationTest::resolvesSitePassthroughForHostsAndSubdomains()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down", "k": "scroll-up" },
        "passthrough": {
            "youtube.com": { "keys": ["k"] },
            "editor.example": { "all": true }
        }
    })JSON");
    KeyboardNavigation navigation(path);

    const auto youtube
        = navigation.configurationForUrl(QUrl(QStringLiteral("https://www.youtube.com/watch?v=1")));
    QCOMPARE(youtube.value(QStringLiteral("passthroughKeys")).toStringList(),
        QStringList {QStringLiteral("k")});
    QVERIFY(!youtube.value(QStringLiteral("passthroughAll")).toBool());

    const auto editor
        = navigation.configurationForUrl(QUrl(QStringLiteral("https://editor.example/document")));
    QVERIFY(editor.value(QStringLiteral("passthroughAll")).toBool());

    const auto unrelated
        = navigation.configurationForUrl(QUrl(QStringLiteral("https://notyoutube.com")));
    QVERIFY(unrelated.value(QStringLiteral("passthroughKeys")).toStringList().isEmpty());
}

void KeyboardNavigationTest::rejectsUnsupportedVersionsAndCommands()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 2,
        "enabled": true,
        "bindings": { "x": "run-arbitrary-code" }
    })JSON");
    KeyboardNavigation navigation(path);

    QVERIFY(!navigation.valid());
    QVERIFY(!navigation.enabled());
    QVERIFY(!navigation.errorMessage().isEmpty());

    // A file whose page bindings are all unknown has nothing left to honour,
    // which is the one case that still fails the file outright.
    const auto emptied = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "x": "run-arbitrary-code" }
    })JSON");
    KeyboardNavigation nothingLeft(emptied);
    QVERIFY(!nothingLeft.valid());
    QVERIFY(nothingLeft.errorMessage().contains(QStringLiteral("run-arbitrary-code")));
}

// One configuration is shared by every Omaweb on the machine, so a build meets
// commands it does not have: one another build added, one retired since the file
// was written. Losing the whole keymap to any of them leaves a keyboard-driven
// browser with no keyboard, so the binding goes and the rest stays.
void KeyboardNavigationTest::dropsBindingsThisBuildDoesNotKnowAndKeepsTheRest()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down", "z": "teleport" },
        "browser": {
            "Primary+L": "open-address",
            "Primary+Shift+D": "debug-current-tab",
            "Primary+W": "close-tab"
        }
    })JSON");
    KeyboardNavigation navigation(path);

    QVERIFY(navigation.valid());
    QVERIFY(navigation.enabled());
    QCOMPARE(
        navigation.bindings().value(QStringLiteral("j")).toString(), QStringLiteral("scroll-down"));
    QVERIFY(!navigation.bindings().contains(QStringLiteral("z")));
    QCOMPARE(navigation.browserBindings().size(), 2);
    QCOMPARE(navigation.browserBindings().value(QStringLiteral("Primary+W")).toString(),
        QStringLiteral("close-tab"));
    QVERIFY(!navigation.browserBindings().contains(QStringLiteral("Primary+Shift+D")));

    // What was dropped is named rather than passed over in silence.
    QVERIFY(navigation.errorMessage().contains(QStringLiteral("debug-current-tab")));
    QVERIFY(navigation.errorMessage().contains(QStringLiteral("teleport")));
}

// The command scope was once the command panel, and a reader's file still names it
// that way. The binding they chose keeps working rather than being dropped.
void KeyboardNavigationTest::readsTheCommandPanelIdAsTheCommandScope()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down" },
        "browser": { "Primary+K": "command-panel", ":": "command-scope" }
    })JSON");
    KeyboardNavigation navigation(path);

    QVERIFY(navigation.valid());
    QVERIFY(navigation.errorMessage().isEmpty());
    QCOMPARE(navigation.browserBindings().value(QStringLiteral("Primary+K")).toString(),
        QStringLiteral("command-scope"));
    QCOMPARE(navigation.browserBindings().value(QStringLiteral(":")).toString(),
        QStringLiteral("command-scope"));
}

// The screenshot commands have no keys of their own, and a reader who wants
// them on a key binds them in the file like any other.
void KeyboardNavigationTest::bindsTheScreenshotCommands()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down" },
        "browser": {
            "Primary+Shift+S": "screenshot-page",
            "Primary+Shift+C": "copy-screenshot",
            "Primary+Alt+S": "screenshot-full-page",
            "Primary+Alt+C": "copy-full-page-screenshot"
        }
    })JSON");
    KeyboardNavigation navigation(path);

    QVERIFY(navigation.valid());
    QVERIFY2(navigation.errorMessage().isEmpty(), qPrintable(navigation.errorMessage()));
    const auto browser = navigation.browserBindings();
    QCOMPARE(browser.value(QStringLiteral("Primary+Shift+S")).toString(),
        QStringLiteral("screenshot-page"));
    QCOMPARE(browser.value(QStringLiteral("Primary+Shift+C")).toString(),
        QStringLiteral("copy-screenshot"));
    QCOMPARE(browser.value(QStringLiteral("Primary+Alt+S")).toString(),
        QStringLiteral("screenshot-full-page"));
    QCOMPARE(browser.value(QStringLiteral("Primary+Alt+C")).toString(),
        QStringLiteral("copy-full-page-screenshot"));
}

// Site information opens from the keyboard as well as from the address, on a
// key the reader can move like any other.
void KeyboardNavigationTest::bindsSiteInformation()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down" },
        "browser": { "Primary+Shift+L": "site-information", "Alt+I": "site-information" }
    })JSON");
    KeyboardNavigation navigation(path);

    QVERIFY(navigation.valid());
    QVERIFY2(navigation.errorMessage().isEmpty(), qPrintable(navigation.errorMessage()));
    const auto browser = navigation.browserBindings();
    QCOMPARE(browser.value(QStringLiteral("Primary+Shift+L")).toString(),
        QStringLiteral("site-information"));
    QCOMPARE(browser.value(QStringLiteral("Alt+I")).toString(), QStringLiteral("site-information"));
}

void KeyboardNavigationTest::persistsTheEnabledSetting()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": false,
        "bindings": { "j": "scroll-down" },
        "passthrough": {}
    })JSON");
    KeyboardNavigation navigation(path);
    QSignalSpy changed(&navigation, &KeyboardNavigation::configurationChanged);

    QVERIFY(navigation.setEnabled(true));
    QCOMPARE(changed.count(), 1);

    KeyboardNavigation restored(path);
    QVERIFY(restored.enabled());
}

static const QByteArray defaultConfiguration = R"JSON({
    "version": 1,
    "enabled": true,
    "bindings": {
        "j": "scroll-down",
        "k": "scroll-up"
    },
    "browser": {
        "Primary+L": "open-address",
        "Primary+B": "toggle-sidebar"
    },
    "passthrough": {}
})JSON";

static QJsonObject readConfiguration(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

void KeyboardNavigationTest::adoptsNewDefaultsOnceWithoutResurrectingRemovedBindings()
{
    QTemporaryDir root;
    const auto defaults = root.path() + QStringLiteral("/default.json");
    {
        QFile file(defaults);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(defaultConfiguration), defaultConfiguration.size());
    }

    // A file written before the "browser" section existed, missing one of the
    // page bindings the shipped defaults carry.
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": {
            "j": "scroll-down"
        },
        "passthrough": {}
    })JSON");

    QVERIFY(KeyboardNavigation::adoptDefaults(path, defaults));
    auto settings = readConfiguration(path);
    // A whole missing section arrives, and so does a binding inside a section
    // the file already had.
    QCOMPARE(settings.value(QStringLiteral("browser")).toObject().size(), 2);
    QCOMPARE(
        settings.value(QStringLiteral("bindings")).toObject().value(QStringLiteral("k")).toString(),
        QStringLiteral("scroll-up"));

    // Nothing left to adopt, so nothing is rewritten.
    QVERIFY(!KeyboardNavigation::adoptDefaults(path, defaults));

    // A binding the user deletes stays deleted: the ledger remembers that this
    // file was already offered it.
    settings = readConfiguration(path);
    auto bindings = settings.value(QStringLiteral("bindings")).toObject();
    bindings.remove(QStringLiteral("k"));
    settings.insert(QStringLiteral("bindings"), bindings);
    auto browser = settings.value(QStringLiteral("browser")).toObject();
    browser.remove(QStringLiteral("Primary+B"));
    settings.insert(QStringLiteral("browser"), browser);
    QVERIFY(writeConfiguration(root.path(), QJsonDocument(settings).toJson(QJsonDocument::Indented))
                .size()
        > 0);

    QVERIFY(!KeyboardNavigation::adoptDefaults(path, defaults));
    settings = readConfiguration(path);
    QVERIFY(!settings.value(QStringLiteral("bindings")).toObject().contains(QStringLiteral("k")));
    QVERIFY(!settings.value(QStringLiteral("browser"))
            .toObject()
            .contains(QStringLiteral("Primary+B")));

    // A binding a later release introduces still arrives.
    auto laterDefaults = readConfiguration(defaults);
    browser = laterDefaults.value(QStringLiteral("browser")).toObject();
    browser.insert(QStringLiteral("Primary+E"), QStringLiteral("focus-sidebar"));
    laterDefaults.insert(QStringLiteral("browser"), browser);
    {
        QFile file(defaults);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument(laterDefaults).toJson(QJsonDocument::Indented));
    }

    QVERIFY(KeyboardNavigation::adoptDefaults(path, defaults));
    settings = readConfiguration(path);
    browser = settings.value(QStringLiteral("browser")).toObject();
    QCOMPARE(
        browser.value(QStringLiteral("Primary+E")).toString(), QStringLiteral("focus-sidebar"));
    QVERIFY(!browser.contains(QStringLiteral("Primary+B")));

    // The file still loads, ledger and all.
    KeyboardNavigation navigation(path);
    QVERIFY(navigation.valid());
}

void KeyboardNavigationTest::replacesRetiredDefaultWithoutChangingCustomBindings()
{
    QTemporaryDir root;
    const auto defaults = root.path() + QStringLiteral("/defaults.json");
    QFile defaultsFile(defaults);
    QVERIFY(defaultsFile.open(QIODevice::WriteOnly));
    const QByteArray currentDefaults = R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "u": "scroll-half-page-up" },
        "browser": {
            "X": "reopen-tab",
            "J": "next-tab",
            "K": "previous-tab",
            "Primary+Shift+C": "copy-address"
        },
        "passthrough": {}
    })JSON";
    QCOMPARE(defaultsFile.write(currentDefaults), currentDefaults.size());
    defaultsFile.close();

    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down" },
        "browser": {
            "u": "reopen-tab",
            "gt": "next-tab",
            "gT": "previous-tab",
            "gs": "next-space",
            "gn": "new-space",
            "q": "close-tab",
            "Primary+Shift+C": "inspect-element",
            "Primary+Shift+K": "inspect-element"
        },
        "passthrough": {}
    })JSON");

    QVERIFY(KeyboardNavigation::adoptDefaults(path, defaults));
    const auto settings = readConfiguration(path);
    const auto pageBindings = settings.value(QStringLiteral("bindings")).toObject();
    const auto browserBindings = settings.value(QStringLiteral("browser")).toObject();
    QCOMPARE(
        pageBindings.value(QStringLiteral("u")).toString(), QStringLiteral("scroll-half-page-up"));
    QVERIFY(!browserBindings.contains(QStringLiteral("u")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gt")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gT")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gs")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gn")));
    QCOMPARE(browserBindings.value(QStringLiteral("X")).toString(), QStringLiteral("reopen-tab"));
    QCOMPARE(browserBindings.value(QStringLiteral("q")).toString(), QStringLiteral("close-tab"));
    // A key Omaweb has repurposed follows the new default rather than keeping
    // the command Omaweb itself put there. Adoption cannot do this on its own:
    // it never argues with a key the file already binds.
    QCOMPARE(browserBindings.value(QStringLiteral("Primary+Shift+C")).toString(),
        QStringLiteral("copy-address"));
    // The same command on a key the reader chose is the reader's, and stays.
    QCOMPARE(browserBindings.value(QStringLiteral("Primary+Shift+K")).toString(),
        QStringLiteral("inspect-element"));
}

// Against the keymap Omaweb ships: Primary+O was open-file's, and a file
// still carrying it there follows the key to the Tab jump list. The reader's
// own command on the key is theirs.
void KeyboardNavigationTest::movesTheShippedOpenFileKeyToJumpBack()
{
    const auto defaults = QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH);
    for (const auto &[written, expected] : {
             std::pair {QStringLiteral("open-file"), QStringLiteral("jump-back")},
             std::pair {QStringLiteral("reload"), QStringLiteral("reload")},
         }) {
        QTemporaryDir root;
        const auto path = writeConfiguration(root.path(),
            QStringLiteral(R"JSON({
            "version": 1,
            "enabled": true,
            "bindings": { "j": "scroll-down" },
            "browser": { "Primary+O": "%1" },
            "passthrough": {}
        })JSON")
                .arg(written)
                .toUtf8());

        QVERIFY(KeyboardNavigation::adoptDefaults(path, defaults));
        const auto browser = readConfiguration(path).value(QStringLiteral("browser")).toObject();
        QCOMPARE(browser.value(QStringLiteral("Primary+O")).toString(), expected);
        KeyboardNavigation navigation(path);
        QVERIFY(navigation.valid());
        QCOMPARE(navigation.errorMessage(), QString {});
    }
}

// The shipped defaults are a Qt resource, and a copy out of one is read-only.
// A source set read-only stands in for it.
void KeyboardNavigationTest::seedsAFileTheOwnerCanWrite()
{
    QTemporaryDir root;
    const auto defaults = writeConfiguration(root.path(), R"JSON({"version": 1})JSON");
    QVERIFY(QFile::setPermissions(
        defaults, QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther));
    const auto path = QDir(root.path()).filePath(QStringLiteral("seeded.json"));

    QVERIFY(KeyboardNavigation::seedDefaults(path, defaults));

    const auto permissions = QFileInfo(path).permissions();
    QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
    QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
    QSaveFile rewrite(path);
    QVERIFY(rewrite.open(QIODevice::WriteOnly));
}

// Releases that seeded from the resource left the file read-only.
void KeyboardNavigationTest::makesAReadOnlyFileWritableAgain()
{
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({"version": 1})JSON");
    QVERIFY(QFile::setPermissions(
        path, QFileDevice::ReadOwner | QFileDevice::ReadGroup | QFileDevice::ReadOther));

    KeyboardNavigation::restoreOwnerWrite(path);

    QVERIFY(QFileInfo(path).permissions().testFlag(QFileDevice::WriteOwner));
    QFile contents(path);
    QVERIFY(contents.open(QIODevice::ReadOnly));
    QCOMPARE(contents.readAll(), QByteArray(R"JSON({"version": 1})JSON"));
}

// A file an earlier release wrote, with a ledger that never offered
// Primary+I, gains it once. Deleted after that, it stays deleted.
void KeyboardNavigationTest::offersTheShippedJumpForwardKeyOnce()
{
    const auto defaults = QStringLiteral(OMAWEB_DEFAULT_KEYBINDINGS_PATH);
    QTemporaryDir root;
    const auto path = writeConfiguration(root.path(), R"JSON({
        "version": 1,
        "enabled": true,
        "bindings": { "j": "scroll-down" },
        "browser": { "Primary+L": "open-address" },
        "passthrough": {},
        "adoptedDefaults": {
            "bindings": ["j"],
            "browser": ["Primary+L", "Primary+O"]
        }
    })JSON");

    QVERIFY(KeyboardNavigation::adoptDefaults(path, defaults));
    auto settings = readConfiguration(path);
    auto browser = settings.value(QStringLiteral("browser")).toObject();
    QCOMPARE(browser.value(QStringLiteral("Primary+I")).toString(), QStringLiteral("jump-forward"));
    // Primary+O was offered before and the reader had dropped it.
    QVERIFY(!browser.contains(QStringLiteral("Primary+O")));
    KeyboardNavigation navigation(path);
    QVERIFY(navigation.valid());
    QCOMPARE(navigation.errorMessage(), QString {});
    QCOMPARE(navigation.browserBindings().value(QStringLiteral("Primary+I")).toString(),
        QStringLiteral("jump-forward"));

    browser.remove(QStringLiteral("Primary+I"));
    settings.insert(QStringLiteral("browser"), browser);
    QVERIFY(!writeConfiguration(root.path(), QJsonDocument(settings).toJson()).isEmpty());
    QVERIFY(!KeyboardNavigation::adoptDefaults(path, defaults));
    QVERIFY(!readConfiguration(path)
            .value(QStringLiteral("browser"))
            .toObject()
            .contains(QStringLiteral("Primary+I")));
}

QTEST_GUILESS_MAIN(KeyboardNavigationTest)

#include "tst_keyboardnavigation.moc"
