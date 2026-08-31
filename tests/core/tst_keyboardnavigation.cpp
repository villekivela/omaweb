#include "KeyboardNavigation.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using tanto::KeyboardNavigation;

class KeyboardNavigationTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsVersionedBindingsDisabledByDefault();
    void resolvesSitePassthroughForHostsAndSubdomains();
    void rejectsUnsupportedVersionsAndCommands();
    void persistsTheEnabledSetting();
    void adoptsNewDefaultsOnceWithoutResurrectingRemovedBindings();
    void replacesRetiredDefaultWithoutChangingCustomBindings();
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
    QCOMPARE(navigation.bindings().value(QStringLiteral("gg")).toString(),
        QStringLiteral("scroll-top"));
    const auto configuration = navigation.configurationForUrl(
        QUrl(QStringLiteral("https://example.com")));
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

    const auto youtube = navigation.configurationForUrl(
        QUrl(QStringLiteral("https://www.youtube.com/watch?v=1")));
    QCOMPARE(youtube.value(QStringLiteral("passthroughKeys")).toStringList(),
        QStringList{QStringLiteral("k")});
    QVERIFY(!youtube.value(QStringLiteral("passthroughAll")).toBool());

    const auto editor = navigation.configurationForUrl(
        QUrl(QStringLiteral("https://editor.example/document")));
    QVERIFY(editor.value(QStringLiteral("passthroughAll")).toBool());

    const auto unrelated = navigation.configurationForUrl(
        QUrl(QStringLiteral("https://notyoutube.com")));
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
    QCOMPARE(settings.value(QStringLiteral("bindings")).toObject()
                 .value(QStringLiteral("k")).toString(),
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
    QVERIFY(writeConfiguration(root.path(),
        QJsonDocument(settings).toJson(QJsonDocument::Indented)).size() > 0);

    QVERIFY(!KeyboardNavigation::adoptDefaults(path, defaults));
    settings = readConfiguration(path);
    QVERIFY(!settings.value(QStringLiteral("bindings")).toObject()
                 .contains(QStringLiteral("k")));
    QVERIFY(!settings.value(QStringLiteral("browser")).toObject()
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
    QCOMPARE(browser.value(QStringLiteral("Primary+E")).toString(),
        QStringLiteral("focus-sidebar"));
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
            "K": "previous-tab"
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
            "q": "close-tab"
        },
        "passthrough": {}
    })JSON");

    QVERIFY(KeyboardNavigation::adoptDefaults(path, defaults));
    const auto settings = readConfiguration(path);
    const auto pageBindings = settings.value(QStringLiteral("bindings")).toObject();
    const auto browserBindings = settings.value(QStringLiteral("browser")).toObject();
    QCOMPARE(pageBindings.value(QStringLiteral("u")).toString(),
        QStringLiteral("scroll-half-page-up"));
    QVERIFY(!browserBindings.contains(QStringLiteral("u")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gt")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gT")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gs")));
    QVERIFY(!browserBindings.contains(QStringLiteral("gn")));
    QCOMPARE(browserBindings.value(QStringLiteral("X")).toString(),
        QStringLiteral("reopen-tab"));
    QCOMPARE(browserBindings.value(QStringLiteral("q")).toString(),
        QStringLiteral("close-tab"));
}

QTEST_GUILESS_MAIN(KeyboardNavigationTest)

#include "tst_keyboardnavigation.moc"
