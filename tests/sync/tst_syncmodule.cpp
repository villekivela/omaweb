#include "SyncModule.h"
#include "SyncSetup.h"
#include "SecretStore.h"

#include "PrivateSessionStore.h"
#include "SqliteSessionStore.h"

#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

using omaweb::PrivateSessionStore;
using omaweb::SpaceState;
using omaweb::SqliteSessionStore;
using omaweb::SyncModule;
using omaweb::SyncSetup;
using omaweb::TabState;

namespace {

class FakeForge final : public omaweb::ForgeProvider {
public:
    omaweb::DeviceAuthorization beginAuthorization(QString *) override
    {
        return {.deviceCode = QStringLiteral("device-secret"),
            .userCode = QStringLiteral("ABCD-EFGH"),
            .verificationUrl = QUrl(QStringLiteral("https://forge.example/device")),
            .expiresInSeconds = 900,
            .pollIntervalSeconds = 5};
    }

    omaweb::ForgeAuthorization pollAuthorization(const QString &deviceCode, QString *) override
    {
        if (deviceCode != QLatin1String("device-secret")) {
            return {};
        }
        return {.state = omaweb::AuthorizationState::Complete,
            .accessToken = QByteArrayLiteral("short-lived-token"),
            .refreshToken = QByteArrayLiteral("refresh-token"),
            .login = QStringLiteral("octocat"),
            .avatarUrl = QUrl(QStringLiteral("https://avatars.example/octocat"))};
    }

    omaweb::ForgeAuthorization refreshAuthorization(const QByteArray &, QString *) override
    {
        return {};
    }

    omaweb::ForgeRepository provisionPrivateRepository(
        const QByteArray &accessToken, const QString &name, QString *) override
    {
        provisionedWith = accessToken;
        provisionedName = name;
        return {.name = name,
            .cloneUrl = QUrl(QStringLiteral("https://forge.example/octocat/%1.git").arg(name)),
            .isPrivate = true,
            .created = repositoryCreated};
    }

    QByteArray fetchAvatar(const QUrl &avatarUrl, QString *) override
    {
        fetchedAvatarUrl = avatarUrl;
        return QByteArrayLiteral("fake-png");
    }

    QByteArray provisionedWith;
    QString provisionedName;
    QUrl fetchedAvatarUrl;
    bool repositoryCreated = true;
};

class MemorySecretStore final : public omaweb::SecretStore {
public:
    bool store(const QString &name, const QByteArray &secret, QString *) override
    {
        values.insert(name, secret);
        return true;
    }

    QByteArray retrieve(const QString &name, QString *) override { return values.value(name); }

    bool remove(const QString &name, QString *) override
    {
        values.remove(name);
        return true;
    }

    QHash<QString, QByteArray> values;
};

bool runGit(const QString &directory, const QStringList &arguments, QString *error = nullptr)
{
    QProcess git;
    git.setWorkingDirectory(directory);
    git.start(QStringLiteral("git"), arguments);
    if (!git.waitForFinished() || git.exitStatus() != QProcess::NormalExit || git.exitCode() != 0) {
        if (error) {
            *error = QString::fromUtf8(git.readAllStandardError());
        }
        return false;
    }
    return true;
}

QByteArray gitOutput(const QString &directory, const QStringList &arguments)
{
    QProcess git;
    git.setWorkingDirectory(directory);
    git.start(QStringLiteral("git"), arguments);
    if (!git.waitForFinished() || git.exitCode() != 0) {
        return {};
    }
    return git.readAllStandardOutput().trimmed();
}

QByteArray filesBelow(const QString &root)
{
    QByteArray contents;
    QDirIterator files(root, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        QFile file(files.next());
        if (file.open(QIODevice::ReadOnly)) {
            contents += file.readAll();
        }
    }
    return contents;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QJsonObject readObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

class SyncModuleTest : public QObject {
    Q_OBJECT

private slots:
    void writesEncryptedBrowserStateToAGitRemote();
    void restoresBrowserStateOnASecondMachine();
    void syncsOnlyTheApprovedConfiguration();
    void leavesTheRemoteUntouchedWhenNothingChanged();
    void refusesAStoreThatCannotRecordBrowserState();
    void recoveryKeysDetectEntryErrors();
    void twoMachinesConvergeWhenTheyChangeDifferentRecords();
    void preservesUnappliedRemoteChangesDuringALocalEdit();
    void laterRecordWinsWhenTwoMachinesChangeTheSameRecord();
    void aClosedTabDoesNotReturnFromAnotherMachine();
    void setupUsesAForgeIdentityAndCreatesAPrivateRepository();
    void compactsAnOvergrownRepositoryIntoASnapshot();
    void anExistingRepositoryRequiresItsRecoveryKey();
};

void SyncModuleTest::writesEncryptedBrowserStateToAGitRemote()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir dataRoot;
    QTemporaryDir configRoot;
    QTemporaryDir inspectionRoot;
    QVERIFY(remoteRoot.isValid());
    QVERIFY(dataRoot.isValid());
    QVERIFY(configRoot.isValid());
    QVERIFY(inspectionRoot.isValid());

    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));

    SqliteSessionStore store(dataRoot.path());
    QVERIFY(store.open(&error));
    QVERIFY(store.saveSpace(SpaceState {QStringLiteral("space-personal"),
        QStringLiteral("Personal"), QStringLiteral("#7c6cff"), true}));
    QVERIFY(store.saveTabs(QStringLiteral("space-personal"),
        {TabState {.id = QStringLiteral("tab-secret"),
            .spaceId = QStringLiteral("space-personal"),
            .url = QUrl(QStringLiteral("https://private.example/plan")),
            .title = QStringLiteral("Private plan"),
            .pinned = true}},
        QStringLiteral("tab-secret")));

    SyncModule sync(store,
        {.dataRoot = dataRoot.path(),
            .configRoot = configRoot.path(),
            .remoteUrl = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git"))),
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = QByteArray::fromHex(
                "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f")});
    QVERIFY2(sync.open(&error), qPrintable(error));
    QVERIFY2(sync.reconcile(&error), qPrintable(error));

    QVERIFY2(
        runGit(inspectionRoot.path(),
            {QStringLiteral("clone"),
                QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git"))).toString(),
                QStringLiteral("checkout")},
            &error),
        qPrintable(error));
    const auto checkout = inspectionRoot.filePath(QStringLiteral("checkout"));
    QVERIFY(QFile::exists(checkout + QStringLiteral("/meta.json")));
    QVERIFY(QFile::exists(checkout + QStringLiteral("/spaces/space-personal.sync")));
    QVERIFY(QFile::exists(checkout + QStringLiteral("/tabs/tab-secret.sync")));

    const auto contents = filesBelow(checkout);
    QVERIFY(!contents.contains("Personal"));
    QVERIFY(!contents.contains("private.example"));
    QVERIFY(!contents.contains("Private plan"));
}

void SyncModuleTest::restoresBrowserStateOnASecondMachine()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir firstDataRoot;
    QTemporaryDir firstConfigRoot;
    QTemporaryDir secondDataRoot;
    QTemporaryDir secondConfigRoot;
    QVERIFY(remoteRoot.isValid());

    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    const auto key
        = QByteArray::fromHex("202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f");

    SqliteSessionStore firstStore(firstDataRoot.path());
    QVERIFY(firstStore.open(&error));
    QVERIFY(firstStore.saveSpace(SpaceState {
        QStringLiteral("space-work"), QStringLiteral("Work"), QStringLiteral("#42a5f5"), true}));
    QVERIFY(firstStore.saveTabs(QStringLiteral("space-work"),
        {TabState {.id = QStringLiteral("tab-pinned"),
             .spaceId = QStringLiteral("space-work"),
             .url = QUrl(QStringLiteral("https://intranet.example")),
             .title = QStringLiteral("Intranet"),
             .pinned = true,
             .keepActive = true},
            TabState {.id = QStringLiteral("tab-ordinary"),
                .spaceId = QStringLiteral("space-work"),
                .url = QUrl(QStringLiteral("https://mail.example")),
                .title = QStringLiteral("Mail"),
                .muted = true,
                .zoom = 1.25}},
        QStringLiteral("tab-ordinary")));
    SyncModule first(firstStore,
        {.dataRoot = firstDataRoot.path(),
            .configRoot = firstConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = key});
    QVERIFY2(first.open(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    SqliteSessionStore secondStore(secondDataRoot.path());
    QVERIFY(secondStore.open(&error));
    QVERIFY(secondStore.saveSpace(SpaceState {QStringLiteral("generated-default"),
        QStringLiteral("Personal"), QStringLiteral("#7c6cff"), true}));
    QVERIFY(secondStore.saveTabs(QStringLiteral("generated-default"),
        {TabState {.id = QStringLiteral("generated-tab"),
            .spaceId = QStringLiteral("generated-default"),
            .url = QUrl(QStringLiteral("about:blank")),
            .title = QStringLiteral("New tab")}},
        QStringLiteral("generated-tab")));
    SyncModule second(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key,
            .initialRemoteRestore = true,
            .discardPristineLocalState = true,
            .deferRemoteApply = true});
    QVERIFY2(second.open(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));
    QCOMPARE(secondStore.loadSpaces().constFirst().id, QStringLiteral("generated-default"));
    QVERIFY2(second.applyRemoteState(&error), qPrintable(error));

    const auto spaces = secondStore.loadSpaces();
    QCOMPARE(spaces.size(), 1);
    QCOMPARE(spaces.constFirst().id, QStringLiteral("space-work"));
    QCOMPARE(spaces.constFirst().name, QStringLiteral("Work"));
    QCOMPARE(spaces.constFirst().color, QStringLiteral("#42a5f5"));
    const auto tabs = secondStore.loadTabs(QStringLiteral("space-work"));
    QCOMPARE(tabs.size(), 2);
    QCOMPARE(tabs.at(0).id, QStringLiteral("tab-pinned"));
    QCOMPARE(tabs.at(0).keepActive, true);
    QCOMPARE(tabs.at(1).id, QStringLiteral("tab-ordinary"));
    QCOMPARE(tabs.at(1).muted, true);
    QCOMPARE(tabs.at(1).zoom, 1.25);
}

void SyncModuleTest::syncsOnlyTheApprovedConfiguration()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir firstDataRoot;
    QTemporaryDir firstConfigRoot;
    QTemporaryDir secondDataRoot;
    QTemporaryDir secondConfigRoot;
    QTemporaryDir inspectionRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    const auto key
        = QByteArray::fromHex("404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f");

    SqliteSessionStore firstStore(firstDataRoot.path());
    QVERIFY(firstStore.open(&error));
    QVERIFY(
        firstStore.savePreference(QStringLiteral("floating-controls"), QStringLiteral("false")));
    QVERIFY(firstStore.savePreference(QStringLiteral("ease-sidebar"), QStringLiteral("false")));
    QVERIFY(firstStore.savePreference(QStringLiteral("use-favicons"), QStringLiteral("false")));
    QVERIFY(firstStore.savePreference(QStringLiteral("tint-favicons"), QStringLiteral("true")));
    QVERIFY(firstStore.savePreference(QStringLiteral("sidebar-width"), QStringLiteral("500")));
    QVERIFY(firstStore.savePreference(
        QStringLiteral("clear-data-range"), QStringLiteral("machine-a-only")));
    const auto keybindings = QByteArrayLiteral(
        R"({"version":1,"browser":{"Primary+K":"settings"},"bindings":{"j":"scroll-down"}})");
    QVERIFY(writeFile(firstConfigRoot.filePath(QStringLiteral("keybindings.json")), keybindings));
    const auto blocker = QJsonObject {
        {QStringLiteral("version"), 1},
        {QStringLiteral("seeded"), true},
        {QStringLiteral("userRules"), QStringLiteral("private.example##.secret")},
        {QStringLiteral("disabledSites"), QJsonArray {QStringLiteral("visited.example")}},
        {QStringLiteral("subscriptions"),
            QJsonArray {QJsonObject {{QStringLiteral("id"), QStringLiteral("reader-list")},
                {QStringLiteral("title"), QStringLiteral("Reader list")},
                {QStringLiteral("source"), QStringLiteral("https://lists.example")},
                {QStringLiteral("license"), QStringLiteral("MIT")},
                {QStringLiteral("updateAddress"), QStringLiteral("https://lists.example/list.txt")},
                {QStringLiteral("enabled"), true},
                {QStringLiteral("updateStatus"), QStringLiteral("downloaded")},
                {QStringLiteral("lastUpdated"), QStringLiteral("2030-01-01T00:00:00Z")}}}},
    };
    QVERIFY(writeFile(firstDataRoot.filePath(QStringLiteral("content-blocking/settings.json")),
        QJsonDocument(blocker).toJson()));

    SyncModule first(firstStore,
        {.dataRoot = firstDataRoot.path(),
            .configRoot = firstConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = key});
    QVERIFY2(first.open(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    QVERIFY2(runGit(inspectionRoot.path(),
                 {QStringLiteral("clone"), remote.toString(), QStringLiteral("checkout")}, &error),
        qPrintable(error));
    const auto checkout = inspectionRoot.filePath(QStringLiteral("checkout"));
    QVERIFY(QFile::exists(checkout + QStringLiteral("/settings/floating-controls.json")));
    QVERIFY(!QFile::exists(checkout + QStringLiteral("/settings/sidebar-width.json")));
    QVERIFY(!filesBelow(checkout).contains("visited.example"));
    QVERIFY(!filesBelow(checkout).contains("private.example"));
    QVERIFY(!filesBelow(checkout).contains("downloaded"));
    QVERIFY(!filesBelow(checkout).contains("2030-01-01"));

    SqliteSessionStore secondStore(secondDataRoot.path());
    QVERIFY(secondStore.open(&error));
    QVERIFY(secondStore.savePreference(QStringLiteral("sidebar-width"), QStringLiteral("311")));
    QVERIFY(secondStore.savePreference(
        QStringLiteral("clear-data-range"), QStringLiteral("machine-b-only")));
    const auto localBlocker = QJsonObject {
        {QStringLiteral("version"), 1},
        {QStringLiteral("seeded"), true},
        {QStringLiteral("userRules"), QStringLiteral("local.example##.private")},
        {QStringLiteral("disabledSites"), QJsonArray {QStringLiteral("local-visited.example")}},
        {QStringLiteral("subscriptions"), QJsonArray {}},
    };
    QVERIFY(writeFile(secondDataRoot.filePath(QStringLiteral("content-blocking/settings.json")),
        QJsonDocument(localBlocker).toJson()));
    SyncModule second(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key});
    QVERIFY2(second.open(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));

    QCOMPARE(secondStore.preference(QStringLiteral("floating-controls")), QStringLiteral("false"));
    QCOMPARE(secondStore.preference(QStringLiteral("tint-favicons")), QStringLiteral("true"));
    QCOMPARE(secondStore.preference(QStringLiteral("sidebar-width")), QStringLiteral("311"));
    QCOMPARE(secondStore.preference(QStringLiteral("clear-data-range")),
        QStringLiteral("machine-b-only"));
    QFile restoredKeybindings(secondConfigRoot.filePath(QStringLiteral("keybindings.json")));
    QVERIFY(restoredKeybindings.open(QIODevice::ReadOnly));
    QCOMPARE(restoredKeybindings.readAll(), keybindings);
    const auto restoredBlocker
        = readObject(secondDataRoot.filePath(QStringLiteral("content-blocking/settings.json")));
    QCOMPARE(restoredBlocker.value(QStringLiteral("userRules")).toString(),
        QStringLiteral("local.example##.private"));
    QCOMPARE(restoredBlocker.value(QStringLiteral("disabledSites")).toArray().first().toString(),
        QStringLiteral("local-visited.example"));
    QCOMPARE(restoredBlocker.value(QStringLiteral("subscriptions"))
                 .toArray()
                 .first()
                 .toObject()
                 .value(QStringLiteral("id"))
                 .toString(),
        QStringLiteral("reader-list"));
}

void SyncModuleTest::leavesTheRemoteUntouchedWhenNothingChanged()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir dataRoot;
    QTemporaryDir configRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto repository = remoteRoot.filePath(QStringLiteral("sync.git"));

    SqliteSessionStore store(dataRoot.path());
    QVERIFY(store.open(&error));
    QVERIFY(store.saveSpace(SpaceState {
        QStringLiteral("space-1"), QStringLiteral("Personal"), QStringLiteral("#7c6cff"), true}));
    QVERIFY(store.saveTabs(QStringLiteral("space-1"),
        {TabState {.id = QStringLiteral("tab-1"),
            .spaceId = QStringLiteral("space-1"),
            .url = QUrl(QStringLiteral("https://example.com")),
            .title = QStringLiteral("Example")}},
        QStringLiteral("tab-1")));
    SyncModule sync(store,
        {.dataRoot = dataRoot.path(),
            .configRoot = configRoot.path(),
            .remoteUrl = QUrl::fromLocalFile(repository),
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = QByteArray::fromHex(
                "606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f")});
    QVERIFY2(sync.open(&error), qPrintable(error));
    QVERIFY2(sync.reconcile(&error), qPrintable(error));
    const auto first = gitOutput(repository, {QStringLiteral("rev-parse"), QStringLiteral("main")});
    QVERIFY(!first.isEmpty());

    QVERIFY2(sync.reconcile(&error), qPrintable(error));
    QCOMPARE(gitOutput(repository, {QStringLiteral("rev-parse"), QStringLiteral("main")}), first);
}

void SyncModuleTest::refusesAStoreThatCannotRecordBrowserState()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir dataRoot;
    QTemporaryDir configRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto repository = remoteRoot.filePath(QStringLiteral("sync.git"));
    QVERIFY(writeFile(configRoot.filePath(QStringLiteral("keybindings.json")),
        QByteArrayLiteral(R"({"private":"must-not-leave"})")));

    PrivateSessionStore store(QSharedPointer<QHash<QString, int>>::create());
    QVERIFY(store.open(&error));
    SyncModule sync(store,
        {.dataRoot = dataRoot.path(),
            .configRoot = configRoot.path(),
            .remoteUrl = QUrl::fromLocalFile(repository),
            .machineId = QStringLiteral("private-window"),
            .recoveryKey = QByteArray::fromHex(
                "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f")});
    QVERIFY(!sync.open(&error));
    QCOMPARE(error, QStringLiteral("Sync is unavailable in a Private window"));
    QVERIFY(gitOutput(repository, {QStringLiteral("show-ref")}).isEmpty());
}

void SyncModuleTest::recoveryKeysDetectEntryErrors()
{
    const auto displayed = SyncModule::createRecoveryKey();
    QVERIFY(!displayed.isEmpty());
    QCOMPARE(displayed, displayed.toUpper());
    QVERIFY(displayed.contains(QLatin1Char('-')));

    QString error;
    const auto decoded = SyncModule::decodeRecoveryKey(displayed, &error);
    QCOMPARE(decoded.size(), 32);
    QVERIFY(error.isEmpty());

    auto mistyped = displayed;
    const auto at = mistyped.indexOf(QRegularExpression(QStringLiteral("[A-Z0-9]")));
    QVERIFY(at >= 0);
    mistyped[at] = mistyped[at] == QLatin1Char('A') ? QLatin1Char('B') : QLatin1Char('A');
    QVERIFY(SyncModule::decodeRecoveryKey(mistyped, &error).isEmpty());
    QCOMPARE(error, QStringLiteral("The recovery key has a typing error"));
}

void SyncModuleTest::compactsAnOvergrownRepositoryIntoASnapshot()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir dataRoot;
    QTemporaryDir configRoot;
    QTemporaryDir staleDataRoot;
    QTemporaryDir staleConfigRoot;
    QString error;
    QVERIFY(runGit(remoteRoot.path(),
        {QStringLiteral("init"), QStringLiteral("--bare"), QStringLiteral("--initial-branch=main"),
            QStringLiteral("sync.git")},
        &error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    SqliteSessionStore store(dataRoot.path());
    QVERIFY(store.open(&error));
    QVERIFY(store.saveSpace(
        {QStringLiteral("space"), QStringLiteral("Space"), QStringLiteral("#000000"), true}));
    SyncModule sync(store,
        {.dataRoot = dataRoot.path(),
            .configRoot = configRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine"),
            .recoveryKey = QByteArray(32, 'k'),
            .historyCommitLimit = 3,
            .historyMaxAgeMilliseconds = 0});
    QVERIFY2(sync.open(&error), qPrintable(error));
    QVERIFY(store.savePreference(QStringLiteral("floating-controls"), QStringLiteral("0")));
    QVERIFY2(sync.reconcile(&error), qPrintable(error));

    SqliteSessionStore staleStore(staleDataRoot.path());
    QVERIFY(staleStore.open(&error));
    SyncModule stale(staleStore,
        {.dataRoot = staleDataRoot.path(),
            .configRoot = staleConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("stale-machine"),
            .recoveryKey = QByteArray(32, 'k'),
            .historyCommitLimit = 3,
            .historyMaxAgeMilliseconds = 0});
    QVERIFY2(stale.open(&error), qPrintable(error));
    QVERIFY2(stale.reconcile(&error), qPrintable(error));

    for (int revision = 1; revision < 5; ++revision) {
        QVERIFY(
            store.savePreference(QStringLiteral("floating-controls"), QString::number(revision)));
        QVERIFY2(sync.reconcile(&error), qPrintable(error));
    }
    QVERIFY(
        staleStore.savePreference(QStringLiteral("floating-controls"), QStringLiteral("stale")));
    QVERIFY(staleStore.saveSpace({QStringLiteral("stale-space"), QStringLiteral("Stale"),
        QStringLiteral("#ffffff"), false}));
    QVERIFY2(stale.reconcile(&error), qPrintable(error));
    QCOMPARE(staleStore.preference(QStringLiteral("floating-controls")), QStringLiteral("4"));
    QCOMPARE(staleStore.loadSpaces().size(), 1);
    QCOMPARE(staleStore.loadSpaces().constFirst().id, QStringLiteral("space"));
    QCOMPARE(gitOutput(remoteRoot.filePath(QStringLiteral("sync.git")),
                 {QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("main")})
                 .toInt(),
        2);
}

void SyncModuleTest::anExistingRepositoryRequiresItsRecoveryKey()
{
    QTemporaryDir dataRoot;
    FakeForge forge;
    forge.repositoryCreated = false;
    MemorySecretStore secrets;
    SyncSetup missingKey(forge, secrets, dataRoot.path());
    QString error;
    QVERIFY(!missingKey.beginConnect({}, &error).deviceCode.isEmpty());
    QVERIFY(!missingKey.finishConnect(&error).ready);
    QCOMPARE(error, QStringLiteral("Enter the recovery key for the existing Sync repository"));
    QVERIFY(secrets.values.isEmpty());

    const auto recoveryKey = SyncModule::createRecoveryKey();
    SyncSetup paired(forge, secrets, dataRoot.path());
    QVERIFY(!paired.beginConnect(recoveryKey, &error).deviceCode.isEmpty());
    const auto connection = paired.finishConnect(&error);
    QVERIFY2(connection.ready, qPrintable(error));
    QCOMPARE(connection.recoveryKey, recoveryKey);
    QCOMPARE(secrets.values.value(QStringLiteral("sync-key/octocat")),
        SyncModule::decodeRecoveryKey(recoveryKey));
}

void SyncModuleTest::twoMachinesConvergeWhenTheyChangeDifferentRecords()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir firstDataRoot;
    QTemporaryDir firstConfigRoot;
    QTemporaryDir secondDataRoot;
    QTemporaryDir secondConfigRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    const auto key
        = QByteArray::fromHex("a0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebf");

    SqliteSessionStore firstStore(firstDataRoot.path());
    QVERIFY(firstStore.open(&error));
    QVERIFY(firstStore.saveSpace(SpaceState {
        QStringLiteral("space-1"), QStringLiteral("Original"), QStringLiteral("#7c6cff"), true}));
    const auto originalTab = TabState {.id = QStringLiteral("tab-1"),
        .spaceId = QStringLiteral("space-1"),
        .url = QUrl(QStringLiteral("https://example.com")),
        .title = QStringLiteral("Original title")};
    QVERIFY(firstStore.saveTabs(QStringLiteral("space-1"), {originalTab}, QStringLiteral("tab-1")));
    SyncModule first(firstStore,
        {.dataRoot = firstDataRoot.path(),
            .configRoot = firstConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = key});
    QVERIFY2(first.open(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    SqliteSessionStore secondStore(secondDataRoot.path());
    QVERIFY(secondStore.open(&error));
    SyncModule second(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key});
    QVERIFY2(second.open(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));

    QVERIFY(firstStore.saveSpace(SpaceState {
        QStringLiteral("space-1"), QStringLiteral("Renamed"), QStringLiteral("#7c6cff"), true}));
    QVERIFY2(first.reconcile(&error), qPrintable(error));
    auto changedTab = originalTab;
    changedTab.title = QStringLiteral("Changed title");
    QVERIFY(secondStore.saveTabs(QStringLiteral("space-1"), {changedTab}, QStringLiteral("tab-1")));
    QVERIFY2(second.reconcile(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    QCOMPARE(firstStore.loadSpaces().constFirst().name, QStringLiteral("Renamed"));
    QCOMPARE(secondStore.loadSpaces().constFirst().name, QStringLiteral("Renamed"));
    QCOMPARE(firstStore.loadTabs(QStringLiteral("space-1")).constFirst().title,
        QStringLiteral("Changed title"));
    QCOMPARE(secondStore.loadTabs(QStringLiteral("space-1")).constFirst().title,
        QStringLiteral("Changed title"));
}

void SyncModuleTest::preservesUnappliedRemoteChangesDuringALocalEdit()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir firstDataRoot;
    QTemporaryDir firstConfigRoot;
    QTemporaryDir secondDataRoot;
    QTemporaryDir secondConfigRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    const auto key
        = QByteArray::fromHex("b0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecf");

    SqliteSessionStore firstStore(firstDataRoot.path());
    QVERIFY(firstStore.open(&error));
    QVERIFY(firstStore.saveSpace(SpaceState {
        QStringLiteral("space-1"), QStringLiteral("Original"), QStringLiteral("#7c6cff"), true}));
    SyncModule first(firstStore,
        {.dataRoot = firstDataRoot.path(),
            .configRoot = firstConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = key});
    QVERIFY2(first.open(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    SqliteSessionStore secondStore(secondDataRoot.path());
    QVERIFY(secondStore.open(&error));
    SyncModule second(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key,
            .initialRemoteRestore = true,
            .deferRemoteApply = true});
    QVERIFY2(second.open(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));
    QVERIFY2(second.applyRemoteState(&error), qPrintable(error));

    QVERIFY(firstStore.saveSpace(SpaceState {QStringLiteral("space-1"),
        QStringLiteral("Remote rename"), QStringLiteral("#7c6cff"), true}));
    const auto remoteTab = TabState {.id = QStringLiteral("remote-tab"),
        .spaceId = QStringLiteral("space-1"),
        .url = QUrl(QStringLiteral("https://remote.example")),
        .title = QStringLiteral("Remote tab")};
    QVERIFY(
        firstStore.saveTabs(QStringLiteral("space-1"), {remoteTab}, QStringLiteral("remote-tab")));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    SyncModule staged(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key,
            .deferRemoteApply = true});
    QVERIFY2(staged.open(&error), qPrintable(error));
    QVERIFY2(staged.reconcile(&error), qPrintable(error));
    QVERIFY(
        secondStore.savePreference(QStringLiteral("floating-controls"), QStringLiteral("false")));

    SyncModule settled(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key});
    QVERIFY2(settled.open(&error), qPrintable(error));
    QVERIFY2(settled.reconcile(&error), qPrintable(error));
    QCOMPARE(secondStore.loadSpaces().constFirst().name, QStringLiteral("Remote rename"));
    QCOMPARE(secondStore.loadTabs(QStringLiteral("space-1")).constFirst().id,
        QStringLiteral("remote-tab"));
}

void SyncModuleTest::laterRecordWinsWhenTwoMachinesChangeTheSameRecord()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir firstDataRoot;
    QTemporaryDir firstConfigRoot;
    QTemporaryDir secondDataRoot;
    QTemporaryDir secondConfigRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    const auto key
        = QByteArray::fromHex("c0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedf");
    qint64 firstNow = 1'000;
    qint64 secondNow = 1'000;

    SqliteSessionStore firstStore(firstDataRoot.path());
    QVERIFY(firstStore.open(&error));
    QVERIFY(firstStore.saveSpace(SpaceState {
        QStringLiteral("space-1"), QStringLiteral("Original"), QStringLiteral("#7c6cff"), true}));
    SyncModule first(firstStore,
        {.dataRoot = firstDataRoot.path(),
            .configRoot = firstConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = key,
            .now = [&firstNow] { return firstNow; }});
    QVERIFY2(first.open(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    SqliteSessionStore secondStore(secondDataRoot.path());
    QVERIFY(secondStore.open(&error));
    SyncModule second(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key,
            .now = [&secondNow] { return secondNow; }});
    QVERIFY2(second.open(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));

    firstNow = 2'000;
    QVERIFY(firstStore.saveSpace(SpaceState {QStringLiteral("space-1"),
        QStringLiteral("Older rename"), QStringLiteral("#7c6cff"), true}));
    QVERIFY2(first.reconcile(&error), qPrintable(error));
    secondNow = 3'000;
    QVERIFY(secondStore.saveSpace(SpaceState {QStringLiteral("space-1"),
        QStringLiteral("Newer rename"), QStringLiteral("#7c6cff"), true}));
    QVERIFY2(second.reconcile(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    QCOMPARE(firstStore.loadSpaces().constFirst().name, QStringLiteral("Newer rename"));
    QCOMPARE(secondStore.loadSpaces().constFirst().name, QStringLiteral("Newer rename"));
}

void SyncModuleTest::aClosedTabDoesNotReturnFromAnotherMachine()
{
    QTemporaryDir remoteRoot;
    QTemporaryDir firstDataRoot;
    QTemporaryDir firstConfigRoot;
    QTemporaryDir secondDataRoot;
    QTemporaryDir secondConfigRoot;
    QString error;
    QVERIFY2(runGit(remoteRoot.path(),
                 {QStringLiteral("init"), QStringLiteral("--bare"),
                     QStringLiteral("--initial-branch=main"), QStringLiteral("sync.git")},
                 &error),
        qPrintable(error));
    const auto remote = QUrl::fromLocalFile(remoteRoot.filePath(QStringLiteral("sync.git")));
    const auto key
        = QByteArray::fromHex("e0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
    qint64 now = 1'000;

    SqliteSessionStore firstStore(firstDataRoot.path());
    QVERIFY(firstStore.open(&error));
    QVERIFY(firstStore.saveSpace(SpaceState {
        QStringLiteral("space-1"), QStringLiteral("Personal"), QStringLiteral("#7c6cff"), true}));
    const auto kept = TabState {.id = QStringLiteral("tab-kept"),
        .spaceId = QStringLiteral("space-1"),
        .url = QUrl(QStringLiteral("https://kept.example")),
        .title = QStringLiteral("Kept")};
    const auto closed = TabState {.id = QStringLiteral("tab-closed"),
        .spaceId = QStringLiteral("space-1"),
        .url = QUrl(QStringLiteral("https://closed.example")),
        .title = QStringLiteral("Closed")};
    QVERIFY(
        firstStore.saveTabs(QStringLiteral("space-1"), {kept, closed}, QStringLiteral("tab-kept")));
    SyncModule first(firstStore,
        {.dataRoot = firstDataRoot.path(),
            .configRoot = firstConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-a"),
            .recoveryKey = key,
            .now = [&now] { return now; }});
    QVERIFY2(first.open(&error), qPrintable(error));
    QVERIFY2(first.reconcile(&error), qPrintable(error));

    SqliteSessionStore secondStore(secondDataRoot.path());
    QVERIFY(secondStore.open(&error));
    SyncModule second(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key,
            .protectedTabId = QStringLiteral("tab-closed"),
            .now = [&now] { return now; }});
    QVERIFY2(second.open(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));

    now = 2'000;
    QVERIFY(firstStore.saveTabs(QStringLiteral("space-1"), {kept}, QStringLiteral("tab-kept")));
    QVERIFY2(first.reconcile(&error), qPrintable(error));
    QVERIFY2(second.reconcile(&error), qPrintable(error));

    QCOMPARE(secondStore.loadTabs(QStringLiteral("space-1")).size(), 2);
    SyncModule afterSwitch(secondStore,
        {.dataRoot = secondDataRoot.path(),
            .configRoot = secondConfigRoot.path(),
            .remoteUrl = remote,
            .machineId = QStringLiteral("machine-b"),
            .recoveryKey = key,
            .now = [&now] { return now; }});
    QVERIFY2(afterSwitch.open(&error), qPrintable(error));
    QVERIFY2(afterSwitch.reconcile(&error), qPrintable(error));
    const auto tabs = secondStore.loadTabs(QStringLiteral("space-1"));
    QCOMPARE(tabs.size(), 1);
    QCOMPARE(tabs.constFirst().id, QStringLiteral("tab-kept"));
}

void SyncModuleTest::setupUsesAForgeIdentityAndCreatesAPrivateRepository()
{
    QTemporaryDir dataRoot;
    FakeForge forge;
    MemorySecretStore secrets;
    SyncSetup setup(forge, secrets, dataRoot.path());
    QString error;

    const auto prompt = setup.beginConnect({}, &error);
    QCOMPARE(prompt.userCode, QStringLiteral("ABCD-EFGH"));
    QCOMPARE(prompt.verificationUrl, QUrl(QStringLiteral("https://forge.example/device")));
    const auto connected = setup.finishConnect(&error);
    QVERIFY2(connected.ready, qPrintable(error));
    QCOMPARE(connected.login, QStringLiteral("octocat"));
    QCOMPARE(connected.repositoryName, QStringLiteral("omaweb-sync"));
    QCOMPARE(
        connected.remoteUrl, QUrl(QStringLiteral("https://forge.example/octocat/omaweb-sync.git")));
    QCOMPARE(forge.provisionedName, QStringLiteral("omaweb-sync"));
    QCOMPARE(forge.provisionedWith, QByteArrayLiteral("short-lived-token"));
    QCOMPARE(forge.fetchedAvatarUrl, QUrl(QStringLiteral("https://avatars.example/octocat")));
    QCOMPARE(secrets.values.value(QStringLiteral("forge-refresh/octocat")),
        QByteArrayLiteral("refresh-token"));
    QCOMPARE(secrets.values.value(QStringLiteral("sync-key/octocat")).size(), 32);
    QVERIFY(!connected.recoveryKey.isEmpty());
    QCOMPARE(SyncModule::decodeRecoveryKey(connected.recoveryKey, &error),
        secrets.values.value(QStringLiteral("sync-key/octocat")));
    QFile avatar(dataRoot.filePath(QStringLiteral("sync/avatar")));
    QVERIFY(avatar.open(QIODevice::ReadOnly));
    QCOMPARE(avatar.readAll(), QByteArrayLiteral("fake-png"));
}

QTEST_GUILESS_MAIN(SyncModuleTest)

#include "tst_syncmodule.moc"
