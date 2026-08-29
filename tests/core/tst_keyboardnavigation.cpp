#include "KeyboardNavigation.h"

#include <QFile>
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

QTEST_GUILESS_MAIN(KeyboardNavigationTest)

#include "tst_keyboardnavigation.moc"
