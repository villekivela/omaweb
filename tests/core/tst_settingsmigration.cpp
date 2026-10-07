#include "SettingsFile.h"
#include "SettingsMigration.h"
#include "SqliteSessionStore.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

using omaweb::SettingsFile;

class SettingsMigrationTest final : public QObject {
    Q_OBJECT

private slots:
    void buildsTheFileFromWhereSettingsWereKept();
    void startsAnEmptyFileWhenThereIsNothingToCarry();
    void leavesAnOldFileBesideAFileThatIsAlreadyThere();
    void keepsAnOldFileItCannotReadAndSaysWhatItDropped();
};

namespace {

void writeJson(const QString &path, const QJsonObject &object)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(object).toJson());
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                          : QJsonObject {};
}

} // namespace

// The first start of this version gathers the three files and the store's
// rows into settings.json, moves the agent keys to agents.json, and leaves
// nothing of the old places behind. What is the machine's, not the reader's,
// stays in the store.
void SettingsMigrationTest::buildsTheFileFromWhereSettingsWereKept()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QDir config(root.filePath(QStringLiteral("config")));
    const auto downloads = root.filePath(QStringLiteral("downloads"));
    QVERIFY(QDir().mkpath(downloads));
    writeJson(config.filePath(QStringLiteral("interface.json")),
        {{QStringLiteral("font-size"), 14},
            {QStringLiteral("page-fonts"),
                QJsonObject {{QStringLiteral("standard-family"), QStringLiteral("Inter")}}}});
    writeJson(config.filePath(QStringLiteral("downloads.json")),
        {{QStringLiteral("version"), 1}, {QStringLiteral("directory"), downloads}});
    writeJson(config.filePath(QStringLiteral("privacy.json")),
        {{QStringLiteral("https-only"), false},
            {QStringLiteral("secure-dns"), QStringLiteral("mullvad")},
            {QStringLiteral("global-privacy-control"), true},
            {QStringLiteral("allow-agents"), true},
            {QStringLiteral("agent-command"), QStringLiteral("codex")}});
    omaweb::SqliteSessionStore store(root.filePath(QStringLiteral("data")));
    QVERIFY(store.open());
    const QList<std::pair<QString, QString>> rows {
        {QStringLiteral("sidebar-side"), QStringLiteral("right")},
        {QStringLiteral("glance"), QStringLiteral("false")},
        {QStringLiteral("use-favicons"), QStringLiteral("true")},
        {QStringLiteral("put-away-unused-tabs-after"), QStringLiteral("3600")},
        {QStringLiteral("start-page-road"), QStringLiteral("false")},
        {QStringLiteral("known-extension-bitwarden-enabled"), QStringLiteral("true")},
        {QStringLiteral("known-extension-bitwarden-checked"), QStringLiteral("2026-10-01")},
        {QStringLiteral("sidebar-width"), QStringLiteral("300")},
    };
    for (const auto &[name, value] : rows) {
        QVERIFY(store.savePreference(name, value));
    }

    QVERIFY(omaweb::migrateSettings(config.path(), store));

    QCOMPARE(readJson(config.filePath(QStringLiteral("settings.json"))),
        (QJsonObject {
            {QStringLiteral("version"), 1},
            {QStringLiteral("sidebar-side"), QStringLiteral("right")},
            {QStringLiteral("glance"), false},
            {QStringLiteral("start-page-scene"), QStringLiteral("none")},
            {QStringLiteral("put-away-unused-tabs-after"), 3600},
            {QStringLiteral("known-extensions"), QJsonObject {{QStringLiteral("bitwarden"), true}}},
            {QStringLiteral("font-size"), 14},
            {QStringLiteral("page-fonts"),
                QJsonObject {{QStringLiteral("standard-family"), QStringLiteral("Inter")}}},
            {QStringLiteral("download-directory"), downloads},
            {QStringLiteral("https-only"), false},
            {QStringLiteral("secure-dns"), QStringLiteral("mullvad")},
        }));
    QCOMPARE(readJson(config.filePath(QStringLiteral("agents.json"))),
        (QJsonObject {{QStringLiteral("allow-agents"), true},
            {QStringLiteral("agent-command"), QStringLiteral("codex")}}));
    for (const auto &name : {QStringLiteral("interface.json"), QStringLiteral("downloads.json"),
             QStringLiteral("privacy.json")}) {
        QVERIFY2(!config.exists(name), qPrintable(name));
    }
    const auto missing = QStringLiteral("missing");
    for (const auto &name :
        {QStringLiteral("sidebar-side"), QStringLiteral("glance"), QStringLiteral("use-favicons"),
            QStringLiteral("put-away-unused-tabs-after"), QStringLiteral("start-page-road"),
            QStringLiteral("known-extension-bitwarden-enabled")}) {
        QCOMPARE(store.preference(name, missing), missing);
    }
    QCOMPARE(store.preference(QStringLiteral("sidebar-width")), QStringLiteral("300"));
    QCOMPARE(store.preference(QStringLiteral("known-extension-bitwarden-checked")),
        QStringLiteral("2026-10-01"));
}

// A reader with nothing to carry still gets the file, so Settings has one to
// name and the next start knows the move is done.
void SettingsMigrationTest::startsAnEmptyFileWhenThereIsNothingToCarry()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QDir config(root.filePath(QStringLiteral("config")));
    omaweb::SqliteSessionStore store(root.filePath(QStringLiteral("data")));
    QVERIFY(store.open());

    QVERIFY(omaweb::migrateSettings(config.path(), store));

    QCOMPARE(readJson(config.filePath(QStringLiteral("settings.json"))),
        (QJsonObject {{QStringLiteral("version"), 1}}));
    QVERIFY(!config.exists(QStringLiteral("agents.json")));
}

// A settings.json from the reader's dotfiles is theirs, so an old file beside
// it is not read and not deleted: Settings names it for the reader to remove.
void SettingsMigrationTest::leavesAnOldFileBesideAFileThatIsAlreadyThere()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QDir config(root.filePath(QStringLiteral("config")));
    const QJsonObject theirs {{QStringLiteral("version"), 1}, {QStringLiteral("glance"), false}};
    writeJson(config.filePath(QStringLiteral("settings.json")), theirs);
    writeJson(
        config.filePath(QStringLiteral("interface.json")), {{QStringLiteral("font-size"), 14}});
    omaweb::SqliteSessionStore store(root.filePath(QStringLiteral("data")));
    QVERIFY(store.open());
    QVERIFY(store.savePreference(QStringLiteral("sidebar-side"), QStringLiteral("right")));

    QVERIFY(omaweb::migrateSettings(config.path(), store));

    QCOMPARE(readJson(config.filePath(QStringLiteral("settings.json"))), theirs);
    QVERIFY(config.exists(QStringLiteral("interface.json")));
    SettingsFile settings(config.path());
    QCOMPARE(settings.leftoverFiles(), QStringList {QStringLiteral("interface.json")});
    QVERIFY2(settings.problem().contains(QStringLiteral("interface.json")),
        qPrintable(settings.problem()));
    QVERIFY(!settings.isSet(QStringLiteral("font-size")));
}

// An old file the reader edited by hand and left unparsable still holds
// their settings, so it is not deleted: it stays, Settings names it, and the
// next look at it is the reader's. A value that cannot be carried is said.
void SettingsMigrationTest::keepsAnOldFileItCannotReadAndSaysWhatItDropped()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QDir config(root.filePath(QStringLiteral("config")));
    QVERIFY(QDir().mkpath(config.path()));
    const QByteArray privacy(R"({"allow-agents": true, "agent-command": "codex",})");
    {
        QFile file(config.filePath(QStringLiteral("privacy.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(privacy);
    }
    writeJson(config.filePath(QStringLiteral("interface.json")),
        {{QStringLiteral("font-size"), QStringLiteral("big")},
            {QStringLiteral("page-fonts"),
                QJsonObject {{QStringLiteral("fixed-family"), QStringLiteral("Iosevka")}}}});
    omaweb::SqliteSessionStore store(root.filePath(QStringLiteral("data")));
    QVERIFY(store.open());
    QVERIFY(store.savePreference(QStringLiteral("glance"), QStringLiteral("maybe")));

    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("privacy\\.json.*not .*read")));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("\"font-size\".*interface\\.json")));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("\"glance\".*session store")));
    QTest::ignoreMessage(
        QtWarningMsg, QRegularExpression(QStringLiteral("ignores .*privacy\\.json")));
    QVERIFY(omaweb::migrateSettings(config.path(), store));

    QFile left(config.filePath(QStringLiteral("privacy.json")));
    QVERIFY(left.open(QIODevice::ReadOnly));
    QCOMPARE(left.readAll(), privacy);
    QVERIFY(!config.exists(QStringLiteral("agents.json")));
    QVERIFY(!config.exists(QStringLiteral("interface.json")));
    QCOMPARE(readJson(config.filePath(QStringLiteral("settings.json"))),
        (QJsonObject {{QStringLiteral("version"), 1},
            {QStringLiteral("page-fonts"),
                QJsonObject {{QStringLiteral("fixed-family"), QStringLiteral("Iosevka")}}}}));
    const SettingsFile settings(config.path());
    QCOMPARE(settings.leftoverFiles(), QStringList {QStringLiteral("privacy.json")});
}

QTEST_GUILESS_MAIN(SettingsMigrationTest)
#include "tst_settingsmigration.moc"
