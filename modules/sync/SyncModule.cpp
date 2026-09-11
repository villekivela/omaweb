#include "SyncModule.h"

#include "SessionStore.h"

#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSaveFile>
#include <QVector>

#include <sodium.h>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <utility>

namespace omaweb {

namespace {

    constexpr auto encryptedRecordHeader = "OMAWEB-SYNC\0";
    constexpr auto recoveryAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    constexpr auto recoveryKeyBytes = 32;
    constexpr auto recoveryChecksumBytes = 4;
    const QStringList syncedPreferences {QStringLiteral("floating-controls"),
        QStringLiteral("ease-sidebar"), QStringLiteral("use-favicons"),
        QStringLiteral("tint-favicons")};

    void setError(QString *destination, const QString &message)
    {
        if (destination) {
            *destination = message;
        }
    }

    QByteArray compactJson(const QJsonObject &object)
    {
        return QJsonDocument(object).toJson(QJsonDocument::Compact);
    }

    QString digest(const QByteArray &contents)
    {
        return QString::fromLatin1(
            QCryptographicHash::hash(contents, QCryptographicHash::Sha256).toHex());
    }

    QString recordDigest(QJsonObject record)
    {
        record.remove(QStringLiteral("modifiedAt"));
        record.remove(QStringLiteral("modifiedBy"));
        return digest(compactJson(record));
    }

    QJsonArray projectedSubscriptions(const QJsonArray &localSubscriptions)
    {
        QJsonArray subscriptions;
        for (const auto &value : localSubscriptions) {
            const auto local = value.toObject();
            QJsonObject subscription;
            for (const auto &name : {QStringLiteral("id"), QStringLiteral("title"),
                     QStringLiteral("source"), QStringLiteral("license"),
                     QStringLiteral("updateAddress"), QStringLiteral("enabled")}) {
                if (local.contains(name)) {
                    subscription.insert(name, local.value(name));
                }
            }
            subscriptions.append(subscription);
        }
        return subscriptions;
    }

    QJsonObject readJsonObject(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    int encryptedVersion(const QByteArray &encrypted)
    {
        constexpr auto headerSize = 12;
        if (encrypted.size() <= headerSize
            || encrypted.first(headerSize) != QByteArray(encryptedRecordHeader, headerSize)) {
            return 0;
        }
        return static_cast<unsigned char>(encrypted.at(headerSize));
    }

    bool writeJsonObject(const QString &path, const QJsonObject &object, QString *errorMessage)
    {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            setError(errorMessage, QStringLiteral("Could not create a Sync record directory"));
            return false;
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            setError(errorMessage, file.errorString());
            return false;
        }
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        if (!file.commit()) {
            setError(errorMessage, file.errorString());
            return false;
        }
        return true;
    }

    QString encodeRecoveryBytes(const QByteArray &bytes)
    {
        QString encoded;
        quint32 accumulator = 0;
        int bits = 0;
        for (const auto byte : bytes) {
            accumulator = (accumulator << 8) | static_cast<unsigned char>(byte);
            bits += 8;
            while (bits >= 5) {
                bits -= 5;
                encoded.append(QLatin1Char(recoveryAlphabet[(accumulator >> bits) & 0x1f]));
            }
        }
        if (bits > 0) {
            encoded.append(QLatin1Char(recoveryAlphabet[(accumulator << (5 - bits)) & 0x1f]));
        }
        for (qsizetype at = 5; at < encoded.size(); at += 6) {
            encoded.insert(at, QLatin1Char('-'));
        }
        return encoded;
    }

    QByteArray decodeRecoveryBytes(QString displayed)
    {
        displayed.remove(QLatin1Char('-'));
        displayed.remove(QLatin1Char(' '));
        displayed = displayed.toUpper();
        QByteArray decoded;
        quint32 accumulator = 0;
        int bits = 0;
        for (auto character : displayed) {
            if (character == QLatin1Char('O')) {
                character = QLatin1Char('0');
            } else if (character == QLatin1Char('I') || character == QLatin1Char('L')) {
                character = QLatin1Char('1');
            }
            const auto value = QByteArrayView(recoveryAlphabet).indexOf(character.toLatin1());
            if (value < 0) {
                return {};
            }
            accumulator = (accumulator << 5) | static_cast<quint32>(value);
            bits += 5;
            if (bits >= 8) {
                bits -= 8;
                decoded.append(static_cast<char>((accumulator >> bits) & 0xff));
            }
        }
        if (bits > 0 && (accumulator & ((quint32(1) << bits) - 1)) != 0) {
            return {};
        }
        return decoded;
    }

    struct RemoteTab {
        TabState state;
        int position = 0;
    };

    struct RemoteSpace {
        SpaceState state;
        int position = 0;
    };

} // namespace

SyncModule::SyncModule(SessionStore &store, SyncOptions options, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_options(std::move(options))
{
    if (!m_options.now) {
        m_options.now = [] { return QDateTime::currentMSecsSinceEpoch(); };
    }
}

SyncModule::~SyncModule()
{
    sodium_memzero(m_options.recoveryKey.data(), static_cast<size_t>(m_options.recoveryKey.size()));
    sodium_memzero(m_options.accessToken.data(), static_cast<size_t>(m_options.accessToken.size()));
}

QString SyncModule::createRecoveryKey()
{
    if (sodium_init() < 0) {
        return {};
    }
    QByteArray key(recoveryKeyBytes, Qt::Uninitialized);
    randombytes_buf(key.data(), static_cast<size_t>(key.size()));
    QByteArray checksum(recoveryChecksumBytes, Qt::Uninitialized);
    crypto_generichash(reinterpret_cast<unsigned char *>(checksum.data()), checksum.size(),
        reinterpret_cast<const unsigned char *>(key.constData()), key.size(), nullptr, 0);
    const auto displayed = encodeRecoveryBytes(key + checksum);
    sodium_memzero(key.data(), static_cast<size_t>(key.size()));
    sodium_memzero(checksum.data(), static_cast<size_t>(checksum.size()));
    return displayed;
}

QByteArray SyncModule::decodeRecoveryKey(const QString &displayed, QString *errorMessage)
{
    if (sodium_init() < 0) {
        setError(errorMessage, QStringLiteral("Could not initialize recovery-key validation"));
        return {};
    }
    auto bytes = decodeRecoveryBytes(displayed);
    if (bytes.size() != recoveryKeyBytes + recoveryChecksumBytes) {
        setError(errorMessage, QStringLiteral("The recovery key has a typing error"));
        return {};
    }
    auto key = bytes.first(recoveryKeyBytes);
    const auto suppliedChecksum = bytes.sliced(recoveryKeyBytes);
    QByteArray expectedChecksum(recoveryChecksumBytes, Qt::Uninitialized);
    crypto_generichash(reinterpret_cast<unsigned char *>(expectedChecksum.data()),
        expectedChecksum.size(), reinterpret_cast<const unsigned char *>(key.constData()),
        key.size(), nullptr, 0);
    const auto valid = sodium_memcmp(suppliedChecksum.constData(), expectedChecksum.constData(),
                           recoveryChecksumBytes)
        == 0;
    sodium_memzero(bytes.data(), static_cast<size_t>(bytes.size()));
    sodium_memzero(expectedChecksum.data(), static_cast<size_t>(expectedChecksum.size()));
    if (!valid) {
        sodium_memzero(key.data(), static_cast<size_t>(key.size()));
        setError(errorMessage, QStringLiteral("The recovery key has a typing error"));
        return {};
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return key;
}

QString SyncModule::checkoutRoot() const
{
    return QDir(m_options.dataRoot).filePath(QStringLiteral("sync/repository"));
}

bool SyncModule::runGit(const QStringList &arguments, QString *errorMessage) const
{
    if (cancelled(errorMessage)) {
        return false;
    }
    QProcess git;
    git.setWorkingDirectory(checkoutRoot());
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    git.setProcessEnvironment(environment);
    int credentialDescriptor = -1;
    if (!prepareGitCredential(git, &credentialDescriptor, errorMessage)) {
        return false;
    }
    git.start(QStringLiteral("git"), arguments);
    if (credentialDescriptor >= 0) {
        close(credentialDescriptor);
    }
    if (!git.waitForFinished(30'000) || git.exitStatus() != QProcess::NormalExit
        || git.exitCode() != 0) {
        auto message = QString::fromUtf8(git.readAllStandardError()).trimmed();
        if (message.isEmpty()) {
            message = QStringLiteral("git did not finish successfully");
        }
        setError(errorMessage, message);
        return false;
    }
    return true;
}

bool SyncModule::cancelled(QString *errorMessage) const
{
    if (!m_options.cancellationRequested || !m_options.cancellationRequested->load()) {
        return false;
    }
    setError(errorMessage, QStringLiteral("Sync was cancelled"));
    return true;
}

QByteArray SyncModule::gitOutput(const QStringList &arguments, QString *errorMessage) const
{
    QProcess git;
    git.setWorkingDirectory(checkoutRoot());
    git.start(QStringLiteral("git"), arguments);
    if (!git.waitForFinished(30'000) || git.exitStatus() != QProcess::NormalExit
        || git.exitCode() != 0) {
        setError(errorMessage, QString::fromUtf8(git.readAllStandardError()).trimmed());
        return {};
    }
    return git.readAllStandardOutput().trimmed();
}

bool SyncModule::compactHistoryIfNeeded(QString *errorMessage)
{
    bool countOk = false;
    const auto count
        = gitOutput({QStringLiteral("rev-list"), QStringLiteral("--count"), QStringLiteral("HEAD")},
            errorMessage)
              .toInt(&countOk);
    bool timeOk = false;
    const auto timestamps = gitOutput({QStringLiteral("log"), QStringLiteral("--reverse"),
                                          QStringLiteral("--format=%ct"), QStringLiteral("HEAD")},
        errorMessage);
    const auto oldestSeconds = timestamps.split('\n').constFirst().toLongLong(&timeOk);
    const auto tooOld = timeOk && m_options.historyMaxAgeMilliseconds > 0
        && m_options.now() - oldestSeconds * 1'000 > m_options.historyMaxAgeMilliseconds;
    if (!countOk || (count <= m_options.historyCommitLimit && !tooOld)) {
        return countOk;
    }
    for (const auto &kind : {QStringLiteral("spaces"), QStringLiteral("tabs")}) {
        QDir directory(QDir(checkoutRoot()).filePath(kind));
        for (const auto &name : directory.entryList({QStringLiteral("*.sync")}, QDir::Files)) {
            const auto id = name.chopped(5);
            QFile encryptedFile(directory.filePath(name));
            if (!encryptedFile.open(QIODevice::ReadOnly)) {
                setError(errorMessage, encryptedFile.errorString());
                return false;
            }
            const auto encrypted = encryptedFile.readAll();
            if (encryptedVersion(encrypted) > contractVersion) {
                continue;
            }
            const auto record
                = QJsonDocument::fromJson(decryptEncryptedRecord(kind, id, encrypted, errorMessage))
                      .object();
            if (record.isEmpty()) {
                return false;
            }
            if (record.value(QStringLiteral("deleted")).toBool()
                && m_options.now() - record.value(QStringLiteral("modifiedAt")).toInteger()
                    > m_options.tombstoneRetentionMilliseconds) {
                QFile::remove(directory.filePath(name));
            }
        }
    }
    const auto metaPath = QDir(checkoutRoot()).filePath(QStringLiteral("meta.json"));
    auto meta = readJsonObject(metaPath);
    meta.insert(QStringLiteral("epoch"), meta.value(QStringLiteral("epoch")).toInt() + 1);
    if (!writeJsonObject(metaPath, meta, errorMessage)
        || !runGit({QStringLiteral("add"), QStringLiteral("--all")}, errorMessage)) {
        return false;
    }
    const auto expected
        = gitOutput({QStringLiteral("rev-parse"), QStringLiteral("HEAD")}, errorMessage);
    const auto tree = gitOutput({QStringLiteral("write-tree")}, errorMessage);
    if (expected.isEmpty() || tree.isEmpty()) {
        return false;
    }
    const auto snapshot
        = gitOutput({QStringLiteral("commit-tree"), QString::fromUtf8(tree), QStringLiteral("-m"),
                        QStringLiteral("sync: compact snapshot")},
            errorMessage);
    if (snapshot.isEmpty()
        || !runGit({QStringLiteral("push"),
                       QStringLiteral("--force-with-lease=refs/heads/main:%1")
                           .arg(QString::fromUtf8(expected)),
                       QStringLiteral("origin"),
                       QStringLiteral("%1:main").arg(QString::fromUtf8(snapshot))},
            errorMessage)
        || !runGit({QStringLiteral("reset"), QStringLiteral("--hard"), QString::fromUtf8(snapshot)},
            errorMessage)
        || !runGit({QStringLiteral("reflog"), QStringLiteral("expire"),
                       QStringLiteral("--expire=now"), QStringLiteral("--all")},
            errorMessage)
        || !runGit({QStringLiteral("gc"), QStringLiteral("--prune=now")}, errorMessage)) {
        return false;
    }
    return true;
}

bool SyncModule::prepareGitCredential(
    QProcess &git, int *readDescriptor, QString *errorMessage) const
{
    *readDescriptor = -1;
    if (m_options.accessToken.isEmpty()) {
        return true;
    }
    auto helper = m_options.askPassPath.isEmpty() ? QStringLiteral(OMAWEB_SYNC_ASKPASS_PATH)
                                                  : m_options.askPassPath;
    if (!QFileInfo::exists(helper) && m_options.askPassPath.isEmpty()) {
        helper = QStringLiteral(OMAWEB_SYNC_INSTALLED_ASKPASS_PATH);
    }
    if (!QFileInfo::exists(helper)) {
        setError(errorMessage, QStringLiteral("The secure git credential helper is unavailable"));
        return false;
    }
    int descriptors[2];
    if (pipe2(descriptors, O_CLOEXEC) != 0) {
        setError(errorMessage, QStringLiteral("Could not prepare git authentication"));
        return false;
    }
    const auto bytes = m_options.accessToken + '\n';
    qsizetype written = 0;
    while (written < bytes.size()) {
        const auto count = write(descriptors[1], bytes.constData() + written,
            static_cast<size_t>(bytes.size() - written));
        if (count < 0 && errno == EINTR) {
            continue;
        }
        if (count <= 0) {
            close(descriptors[0]);
            close(descriptors[1]);
            setError(errorMessage, QStringLiteral("Could not prepare git authentication"));
            return false;
        }
        written += count;
    }
    close(descriptors[1]);
    constexpr auto childCredentialDescriptor = 3;
    git.setChildProcessModifier([source = descriptors[0]] {
        if ((source == childCredentialDescriptor && fcntl(source, F_SETFD, 0) < 0)
            || (source != childCredentialDescriptor
                && dup2(source, childCredentialDescriptor) < 0)) {
            _exit(127);
        }
    });
    auto environment = git.processEnvironment();
    environment.insert(QStringLiteral("GIT_ASKPASS"), helper);
    environment.insert(
        QStringLiteral("OMAWEB_SYNC_CREDENTIAL_FD"), QString::number(childCredentialDescriptor));
    git.setProcessEnvironment(environment);
    *readDescriptor = descriptors[0];
    return true;
}

bool SyncModule::gitRefExists(const QString &reference) const
{
    QProcess git;
    git.setWorkingDirectory(checkoutRoot());
    git.start(QStringLiteral("git"),
        {QStringLiteral("rev-parse"), QStringLiteral("--verify"), QStringLiteral("--quiet"),
            reference});
    return git.waitForFinished() && git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0;
}

bool SyncModule::open(QString *errorMessage)
{
    if (!m_store.recordsState()) {
        setError(errorMessage, QStringLiteral("Sync is unavailable in a Private window"));
        return false;
    }
    if (sodium_init() < 0) {
        setError(errorMessage, QStringLiteral("Could not initialize record encryption"));
        return false;
    }
    if (m_options.recoveryKey.size() != crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
        setError(errorMessage, QStringLiteral("The recovery key is not valid"));
        return false;
    }
    if (!m_options.remoteUrl.isValid() || m_options.remoteUrl.isEmpty()) {
        setError(errorMessage, QStringLiteral("The Sync repository address is not valid"));
        return false;
    }

    const auto checkout = checkoutRoot();
    if (QFileInfo::exists(QDir(checkout).filePath(QStringLiteral(".git")))) {
        return true;
    }
    if (!QDir().mkpath(QFileInfo(checkout).absolutePath())) {
        setError(errorMessage, QStringLiteral("Could not create the local Sync area"));
        return false;
    }

    QProcess git;
    git.setWorkingDirectory(QFileInfo(checkout).absolutePath());
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    git.setProcessEnvironment(environment);
    auto remoteUrl = m_options.remoteUrl;
    if (!m_options.accessToken.isEmpty() && remoteUrl.scheme() == QLatin1String("https")) {
        remoteUrl.setUserName(QStringLiteral("x-access-token"));
    }
    int credentialDescriptor = -1;
    if (!prepareGitCredential(git, &credentialDescriptor, errorMessage)) {
        return false;
    }
    git.start(QStringLiteral("git"),
        {QStringLiteral("clone"), remoteUrl.toString(), QFileInfo(checkout).fileName()});
    if (credentialDescriptor >= 0) {
        close(credentialDescriptor);
    }
    if (!git.waitForFinished(30'000) || git.exitStatus() != QProcess::NormalExit
        || git.exitCode() != 0) {
        setError(errorMessage, QString::fromUtf8(git.readAllStandardError()).trimmed());
        return false;
    }
    return true;
}

bool SyncModule::writeEncryptedRecord(
    const QString &kind, const QString &id, const QByteArray &plainText, QString *errorMessage)
{
    const auto directory = QDir(checkoutRoot()).filePath(kind);
    const auto path = QDir(directory).filePath(id + QStringLiteral(".sync"));
    if (QFileInfo::exists(path)) {
        QFile encryptedFile(path);
        if (encryptedFile.open(QIODevice::ReadOnly)
            && encryptedVersion(encryptedFile.readAll()) > contractVersion) {
            return true;
        }
        const auto existing = readEncryptedRecord(kind, id, path, errorMessage);
        if (existing.isEmpty()) {
            return false;
        }
        auto existingObject = QJsonDocument::fromJson(existing).object();
        auto nextObject = QJsonDocument::fromJson(plainText).object();
        if (existingObject.value(QStringLiteral("deleted")).toBool()
            && !nextObject.value(QStringLiteral("deleted")).toBool()) {
            return true;
        }
        existingObject.remove(QStringLiteral("modifiedAt"));
        existingObject.remove(QStringLiteral("modifiedBy"));
        if (existingObject == nextObject) {
            return true;
        }
        const auto inventory = readJsonObject(
            QDir(m_options.dataRoot).filePath(QStringLiteral("sync/applied-records.json")));
        const auto appliedDigest = inventory.value(kind).toObject().value(id).toString();
        if (!appliedDigest.isEmpty() && recordDigest(nextObject) == appliedDigest
            && recordDigest(existingObject) != appliedDigest) {
            return true;
        }
    }

    auto nextObject = QJsonDocument::fromJson(plainText).object();
    nextObject.insert(QStringLiteral("modifiedAt"), m_options.now());
    nextObject.insert(QStringLiteral("modifiedBy"), m_options.machineId);
    const auto versionedPlainText = compactJson(nextObject);

    QByteArray nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, Qt::Uninitialized);
    randombytes_buf(nonce.data(), static_cast<size_t>(nonce.size()));
    const auto associatedData
        = kind.toUtf8() + '\0' + id.toUtf8() + '\0' + QByteArray::number(contractVersion);
    QByteArray cipherText(
        versionedPlainText.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, Qt::Uninitialized);
    unsigned long long cipherTextSize = 0;
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            reinterpret_cast<unsigned char *>(cipherText.data()), &cipherTextSize,
            reinterpret_cast<const unsigned char *>(versionedPlainText.constData()),
            static_cast<unsigned long long>(versionedPlainText.size()),
            reinterpret_cast<const unsigned char *>(associatedData.constData()),
            static_cast<unsigned long long>(associatedData.size()), nullptr,
            reinterpret_cast<const unsigned char *>(nonce.constData()),
            reinterpret_cast<const unsigned char *>(m_options.recoveryKey.constData()))
        != 0) {
        setError(errorMessage, QStringLiteral("Could not encrypt a Sync record"));
        return false;
    }
    cipherText.resize(static_cast<qsizetype>(cipherTextSize));

    if (!QDir().mkpath(directory)) {
        setError(errorMessage, QStringLiteral("Could not create a Sync record directory"));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(errorMessage, file.errorString());
        return false;
    }
    file.write(QByteArray(encryptedRecordHeader, 12));
    file.putChar(static_cast<char>(contractVersion));
    file.write(nonce);
    file.write(cipherText);
    if (!file.commit()) {
        setError(errorMessage, file.errorString());
        return false;
    }
    return true;
}

bool SyncModule::tombstoneMissingRecords(
    const QString &kind, const QSet<QString> &currentIds, QString *errorMessage)
{
    const auto inventory = readJsonObject(
        QDir(m_options.dataRoot).filePath(QStringLiteral("sync/applied-records.json")));
    QSet<QString> appliedIds;
    const auto applied = inventory.value(kind);
    if (applied.isObject()) {
        const auto object = applied.toObject();
        for (auto it = object.begin(); it != object.end(); ++it) {
            appliedIds.insert(it.key());
        }
    } else {
        for (const auto &value : applied.toArray()) {
            appliedIds.insert(value.toString());
        }
    }
    QDir directory(QDir(checkoutRoot()).filePath(kind));
    for (const auto &name : directory.entryList({QStringLiteral("*.sync")}, QDir::Files)) {
        const auto id = name.chopped(5);
        if (currentIds.contains(id) || !appliedIds.contains(id)) {
            continue;
        }
        QFile encryptedFile(directory.filePath(name));
        if (!encryptedFile.open(QIODevice::ReadOnly)) {
            setError(errorMessage, encryptedFile.errorString());
            return false;
        }
        const auto encrypted = encryptedFile.readAll();
        if (encryptedVersion(encrypted) > contractVersion) {
            continue;
        }
        const auto existing
            = QJsonDocument::fromJson(decryptEncryptedRecord(kind, id, encrypted, errorMessage))
                  .object();
        if (existing.isEmpty()) {
            return false;
        }
        if (existing.value(QStringLiteral("deleted")).toBool()) {
            continue;
        }
        if (!writeEncryptedRecord(kind, id,
                compactJson({{QStringLiteral("version"), contractVersion},
                    {QStringLiteral("id"), id}, {QStringLiteral("deleted"), true}}),
                errorMessage)) {
            return false;
        }
    }
    return true;
}

QByteArray SyncModule::readEncryptedRecord(
    const QString &kind, const QString &id, const QString &path, QString *errorMessage) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, file.errorString());
        return {};
    }
    return decryptEncryptedRecord(kind, id, file.readAll(), errorMessage);
}

QByteArray SyncModule::decryptEncryptedRecord(const QString &kind, const QString &id,
    const QByteArray &encrypted, QString *errorMessage) const
{
    constexpr auto headerSize = 12;
    constexpr auto versionSize = 1;
    const auto minimumSize = headerSize + versionSize + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
        + crypto_aead_xchacha20poly1305_ietf_ABYTES;
    if (encrypted.size() < minimumSize
        || encrypted.first(headerSize) != QByteArray(encryptedRecordHeader, headerSize)) {
        setError(errorMessage, QStringLiteral("A Sync record has an invalid format"));
        return {};
    }
    const auto version = encryptedVersion(encrypted);
    if (version != contractVersion) {
        setError(errorMessage,
            version > contractVersion
                ? QStringLiteral("A Sync record was written by a newer browser")
                : QStringLiteral("A Sync record has an unsupported version"));
        return {};
    }
    const auto nonce
        = encrypted.sliced(headerSize + versionSize, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    const auto cipherText
        = encrypted.sliced(headerSize + versionSize + crypto_aead_xchacha20poly1305_ietf_NPUBBYTES);
    const auto associatedData
        = kind.toUtf8() + '\0' + id.toUtf8() + '\0' + QByteArray::number(contractVersion);
    QByteArray plainText(
        cipherText.size() - crypto_aead_xchacha20poly1305_ietf_ABYTES, Qt::Uninitialized);
    unsigned long long plainTextSize = 0;
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            reinterpret_cast<unsigned char *>(plainText.data()), &plainTextSize, nullptr,
            reinterpret_cast<const unsigned char *>(cipherText.constData()),
            static_cast<unsigned long long>(cipherText.size()),
            reinterpret_cast<const unsigned char *>(associatedData.constData()),
            static_cast<unsigned long long>(associatedData.size()),
            reinterpret_cast<const unsigned char *>(nonce.constData()),
            reinterpret_cast<const unsigned char *>(m_options.recoveryKey.constData()))
        != 0) {
        setError(
            errorMessage, QStringLiteral("This recovery key cannot unlock the Sync repository"));
        return {};
    }
    plainText.resize(static_cast<qsizetype>(plainTextSize));
    return plainText;
}

bool SyncModule::resolveMergeConflicts(QString *errorMessage)
{
    const auto gitOutput = [this](const QStringList &arguments, bool *ok = nullptr) {
        QProcess git;
        git.setWorkingDirectory(checkoutRoot());
        git.start(QStringLiteral("git"), arguments);
        const auto finished = git.waitForFinished(30'000);
        const auto succeeded
            = finished && git.exitStatus() == QProcess::NormalExit && git.exitCode() == 0;
        if (ok) {
            *ok = succeeded;
        }
        return git.readAllStandardOutput();
    };

    bool listed = false;
    const auto output = gitOutput({QStringLiteral("diff"), QStringLiteral("--name-only"),
                                      QStringLiteral("--diff-filter=U"), QStringLiteral("-z")},
        &listed);
    const auto paths = output.split('\0');
    if (!listed || paths.isEmpty()) {
        setError(errorMessage, QStringLiteral("Could not list conflicting Sync records"));
        return false;
    }
    for (const auto &encodedPath : paths) {
        if (encodedPath.isEmpty()) {
            continue;
        }
        const auto path = QString::fromUtf8(encodedPath);
        const auto parts = path.split(QLatin1Char('/'));
        if (parts.size() != 2 || !parts.constLast().endsWith(QLatin1String(".sync"))) {
            bool localRead = false;
            bool remoteRead = false;
            const auto local
                = gitOutput({QStringLiteral("show"), QStringLiteral(":2:") + path}, &localRead);
            const auto remote
                = gitOutput({QStringLiteral("show"), QStringLiteral(":3:") + path}, &remoteRead);
            if (!localRead || !remoteRead) {
                setError(errorMessage, QStringLiteral("Could not read conflicting Sync settings"));
                return false;
            }
            const auto localHash = QCryptographicHash::hash(local, QCryptographicHash::Sha256);
            const auto remoteHash = QCryptographicHash::hash(remote, QCryptographicHash::Sha256);
            const auto &winner = remoteHash > localHash ? remote : local;
            QSaveFile file(QDir(checkoutRoot()).filePath(path));
            if (!file.open(QIODevice::WriteOnly) || file.write(winner) != winner.size()
                || !file.commit()
                || !runGit({QStringLiteral("add"), QStringLiteral("--"), path}, errorMessage)) {
                setError(
                    errorMessage, QStringLiteral("Could not resolve conflicting Sync settings"));
                return false;
            }
            continue;
        }
        const auto kind = parts.constFirst();
        const auto id = parts.constLast().chopped(5);
        bool localRead = false;
        bool remoteRead = false;
        const auto localEncrypted
            = gitOutput({QStringLiteral("show"), QStringLiteral(":2:") + path}, &localRead);
        const auto remoteEncrypted
            = gitOutput({QStringLiteral("show"), QStringLiteral(":3:") + path}, &remoteRead);
        if (!localRead || !remoteRead) {
            setError(errorMessage, QStringLiteral("Could not read conflicting Sync records"));
            return false;
        }
        const auto localVersion = encryptedVersion(localEncrypted);
        const auto remoteVersion = encryptedVersion(remoteEncrypted);
        if (localVersion > contractVersion || remoteVersion > contractVersion) {
            const auto winner = remoteVersion > localVersion ? remoteEncrypted
                : localVersion > remoteVersion               ? localEncrypted
                : QCryptographicHash::hash(remoteEncrypted, QCryptographicHash::Sha256)
                    > QCryptographicHash::hash(localEncrypted, QCryptographicHash::Sha256)
                ? remoteEncrypted
                : localEncrypted;
            QSaveFile file(QDir(checkoutRoot()).filePath(path));
            if (!file.open(QIODevice::WriteOnly) || file.write(winner) != winner.size()
                || !file.commit()
                || !runGit({QStringLiteral("add"), QStringLiteral("--"), path}, errorMessage)) {
                setError(errorMessage, QStringLiteral("Could not preserve a newer Sync record"));
                return false;
            }
            continue;
        }
        const auto local = QJsonDocument::fromJson(
            decryptEncryptedRecord(kind, id, localEncrypted, errorMessage))
                               .object();
        const auto remote = QJsonDocument::fromJson(
            decryptEncryptedRecord(kind, id, remoteEncrypted, errorMessage))
                                .object();
        if (local.isEmpty() || remote.isEmpty()) {
            return false;
        }
        const auto revision = [](const QJsonObject &record) {
            return std::pair {record.value(QStringLiteral("modifiedAt")).toInteger(),
                record.value(QStringLiteral("modifiedBy")).toString()};
        };
        const auto winner = revision(remote) > revision(local) ? remoteEncrypted : localEncrypted;
        QSaveFile file(QDir(checkoutRoot()).filePath(path));
        if (!file.open(QIODevice::WriteOnly) || file.write(winner) != winner.size()
            || !file.commit()
            || !runGit({QStringLiteral("add"), QStringLiteral("--"), path}, errorMessage)) {
            setError(errorMessage, QStringLiteral("Could not resolve a conflicting Sync record"));
            return false;
        }
    }
    return runGit({QStringLiteral("commit"), QStringLiteral("--no-edit")}, errorMessage);
}

bool SyncModule::mergeRemote(QString *errorMessage)
{
    const auto localEpoch
        = readJsonObject(QDir(checkoutRoot()).filePath(QStringLiteral("meta.json")))
              .value(QStringLiteral("epoch"))
              .toInt();
    const auto remoteMeta = QJsonDocument::fromJson(
        gitOutput({QStringLiteral("show"), QStringLiteral("refs/remotes/origin/main:meta.json")},
            errorMessage));
    if (!remoteMeta.isObject()) {
        setError(errorMessage, QStringLiteral("The remote Sync metadata is not valid"));
        return false;
    }
    const auto remoteEpoch = remoteMeta.object().value(QStringLiteral("epoch")).toInt();
    if (remoteEpoch < localEpoch) {
        setError(errorMessage,
            QStringLiteral("The Sync repository is older than this machine remembers"));
        return false;
    }
    if (remoteEpoch > localEpoch) {
        m_remoteEpochAdvanced = true;
        return runGit({QStringLiteral("reset"), QStringLiteral("--hard"),
                          QStringLiteral("refs/remotes/origin/main")},
            errorMessage);
    }
    QProcess git;
    git.setWorkingDirectory(checkoutRoot());
    git.start(QStringLiteral("git"),
        {QStringLiteral("merge"), QStringLiteral("--no-edit"),
            QStringLiteral("refs/remotes/origin/main")});
    if (!git.waitForFinished(30'000) || git.exitStatus() != QProcess::NormalExit) {
        setError(errorMessage, QStringLiteral("git did not finish reconciling the remote"));
        return false;
    }
    if (git.exitCode() == 0) {
        return true;
    }
    return resolveMergeConflicts(errorMessage);
}

bool SyncModule::restoreRemoteState(QString *errorMessage)
{
    const auto localSpaces = m_store.loadSpaces();
    QString localActiveSpaceId;
    QHash<QString, QString> localActiveTabIds;
    for (const auto &space : localSpaces) {
        if (space.active) {
            localActiveSpaceId = space.id;
        }
        for (const auto &tab : m_store.loadTabs(space.id)) {
            if (tab.active) {
                localActiveTabIds.insert(space.id, tab.id);
                break;
            }
        }
    }
    QVector<RemoteSpace> remoteSpaces;
    QHash<QString, QVector<RemoteTab>> tabsBySpace;
    QSet<QString> deletedSpaceIds;
    const auto readDirectory =
        [this, errorMessage](const QString &kind,
            const std::function<bool(const QString &, const QJsonObject &)> &read) {
            QDir directory(QDir(checkoutRoot()).filePath(kind));
            for (const auto &name : directory.entryList({QStringLiteral("*.sync")}, QDir::Files)) {
                const auto id = name.chopped(5);
                QFile encryptedFile(directory.filePath(name));
                if (encryptedFile.open(QIODevice::ReadOnly)
                    && encryptedVersion(encryptedFile.readAll()) > contractVersion) {
                    continue;
                }
                const auto plainText
                    = readEncryptedRecord(kind, id, directory.filePath(name), errorMessage);
                if (plainText.isEmpty()) {
                    return false;
                }
                QJsonParseError parseError;
                const auto document = QJsonDocument::fromJson(plainText, &parseError);
                if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                    setError(errorMessage, QStringLiteral("A Sync record contains invalid data"));
                    return false;
                }
                const auto object = document.object();
                if (object.value(QStringLiteral("id")).toString() != id
                    || object.value(QStringLiteral("version")).toInt() != contractVersion
                    || !read(id, object)) {
                    setError(
                        errorMessage, QStringLiteral("A Sync record does not match its identity"));
                    return false;
                }
            }
            return true;
        };

    if (!readDirectory(QStringLiteral("spaces"),
            [&remoteSpaces, &deletedSpaceIds](const QString &recordId, const QJsonObject &record) {
                if (record.value(QStringLiteral("deleted")).toBool()) {
                    deletedSpaceIds.insert(recordId);
                    return true;
                }
                const auto id = record.value(QStringLiteral("id")).toString();
                const auto name = record.value(QStringLiteral("name")).toString();
                const auto color = record.value(QStringLiteral("color")).toString();
                if (id.isEmpty() || name.isEmpty() || color.isEmpty()) {
                    return false;
                }
                remoteSpaces.append(RemoteSpace {
                    .state = SpaceState {id, name, color, false},
                    .position = record.value(QStringLiteral("position")).toInt(),
                });
                return true;
            })
        || !readDirectory(
            QStringLiteral("tabs"), [&tabsBySpace](const QString &, const QJsonObject &record) {
                if (record.value(QStringLiteral("deleted")).toBool()) {
                    return true;
                }
                TabState tab {.id = record.value(QStringLiteral("id")).toString(),
                    .spaceId = record.value(QStringLiteral("spaceId")).toString(),
                    .url = QUrl(record.value(QStringLiteral("url")).toString()),
                    .title = record.value(QStringLiteral("title")).toString(),
                    .pinned = record.value(QStringLiteral("pinned")).toBool(),
                    .muted = record.value(QStringLiteral("muted")).toBool(),
                    .zoom = record.value(QStringLiteral("zoom")).toDouble(1.0),
                    .keepActive = record.value(QStringLiteral("keepActive")).toBool()};
                if (tab.id.isEmpty() || tab.spaceId.isEmpty() || !tab.url.isValid()) {
                    return false;
                }
                tabsBySpace[tab.spaceId].append(
                    RemoteTab {std::move(tab), record.value(QStringLiteral("position")).toInt()});
                return true;
            })) {
        return false;
    }

    std::ranges::sort(remoteSpaces, {}, &RemoteSpace::position);
    QVector<SpaceState> spaces;
    spaces.reserve(remoteSpaces.size());
    for (auto &space : remoteSpaces) {
        spaces.append(std::move(space.state));
    }
    const auto replacementSpaceId = spaces.isEmpty() ? QString {} : spaces.constFirst().id;
    for (const auto &localSpace : m_store.loadSpaces()) {
        const auto absentFromReplacement = (m_options.replaceLocalState || m_remoteEpochAdvanced)
            && std::ranges::none_of(spaces,
                [&localSpace](const SpaceState &space) { return space.id == localSpace.id; });
        if ((deletedSpaceIds.contains(localSpace.id) || absentFromReplacement)
            && !m_store.deleteSpace(
                localSpace.id, localSpace.active ? replacementSpaceId : QString {})) {
            setError(errorMessage, QStringLiteral("Could not apply a deleted remote Space"));
            return false;
        }
    }

    if (std::ranges::none_of(spaces, [&localActiveSpaceId](const SpaceState &space) {
            return space.id == localActiveSpaceId;
        })) {
        localActiveSpaceId = replacementSpaceId;
    }
    for (auto &space : spaces) {
        space.active = space.id == localActiveSpaceId;
    }
    if (!spaces.isEmpty() && !m_store.saveSpaces(spaces)) {
        setError(errorMessage, QStringLiteral("Could not apply remote Space order"));
        return false;
    }
    for (qsizetype index = 0; index < spaces.size(); ++index) {
        auto tabs = tabsBySpace.take(spaces[index].id);
        std::ranges::sort(tabs, {}, &RemoteTab::position);
        QVector<TabState> states;
        states.reserve(tabs.size());
        for (auto &tab : tabs) {
            states.append(std::move(tab.state));
        }
        if (!m_options.protectedTabId.isEmpty()
            && std::ranges::none_of(states,
                [this](const TabState &tab) { return tab.id == m_options.protectedTabId; })) {
            const auto localTabs = m_store.loadTabs(spaces[index].id);
            const auto protectedTab
                = std::ranges::find(localTabs, m_options.protectedTabId, &TabState::id);
            if (protectedTab != localTabs.end()) {
                states.append(*protectedTab);
            }
        }
        auto activeTabId = localActiveTabIds.value(spaces[index].id);
        if (std::ranges::none_of(
                states, [&activeTabId](const TabState &tab) { return tab.id == activeTabId; })) {
            activeTabId = states.isEmpty() ? QString {} : states.constFirst().id;
        }
        if (!m_store.saveTabs(spaces[index].id, states, activeTabId)) {
            setError(errorMessage, QStringLiteral("Could not apply remote browser state"));
            return false;
        }
    }
    return restoreConfiguration(errorMessage) && writeAppliedRecordInventory(errorMessage);
}

bool SyncModule::writeAppliedRecordInventory(QString *errorMessage) const
{
    QJsonObject inventory {{QStringLiteral("version"), 1}};
    for (const auto &kind : {QStringLiteral("spaces"), QStringLiteral("tabs")}) {
        QJsonObject records;
        QDir directory(QDir(checkoutRoot()).filePath(kind));
        for (const auto &name : directory.entryList({QStringLiteral("*.sync")}, QDir::Files)) {
            const auto id = name.chopped(5);
            QFile file(directory.filePath(name));
            if (!file.open(QIODevice::ReadOnly)) {
                setError(errorMessage, file.errorString());
                return false;
            }
            const auto encrypted = file.readAll();
            if (encryptedVersion(encrypted) > contractVersion) {
                continue;
            }
            const auto record
                = QJsonDocument::fromJson(decryptEncryptedRecord(kind, id, encrypted, errorMessage))
                      .object();
            if (record.isEmpty()) {
                return false;
            }
            if (!record.value(QStringLiteral("deleted")).toBool()) {
                records.insert(id, recordDigest(record));
            }
        }
        inventory.insert(kind, records);
    }
    QJsonObject configuration;
    for (const auto &relativePath : {QStringLiteral("config/keybindings.json"),
             QStringLiteral("settings/content-blocking.json"),
             QStringLiteral("settings/floating-controls.json"),
             QStringLiteral("settings/ease-sidebar.json"),
             QStringLiteral("settings/use-favicons.json"),
             QStringLiteral("settings/tint-favicons.json")}) {
        QFile file(QDir(checkoutRoot()).filePath(relativePath));
        if (file.open(QIODevice::ReadOnly)) {
            configuration.insert(relativePath, digest(file.readAll()));
        }
    }
    inventory.insert(QStringLiteral("configuration"), configuration);
    return writeJsonObject(
        QDir(m_options.dataRoot).filePath(QStringLiteral("sync/applied-records.json")), inventory,
        errorMessage);
}

bool SyncModule::writeLocalBaselineInventory(QString *errorMessage) const
{
    const auto path
        = QDir(m_options.dataRoot).filePath(QStringLiteral("sync/applied-records.json"));
    if (QFileInfo::exists(path)) {
        return true;
    }
    QJsonObject inventory {{QStringLiteral("version"), 1}};
    QJsonObject spaces;
    QJsonObject tabs;
    const auto localSpaces = m_store.loadSpaces();
    for (qsizetype spacePosition = 0; spacePosition < localSpaces.size(); ++spacePosition) {
        const auto &space = localSpaces.at(spacePosition);
        spaces.insert(space.id,
            recordDigest(
                {{QStringLiteral("version"), contractVersion}, {QStringLiteral("id"), space.id},
                    {QStringLiteral("name"), space.name}, {QStringLiteral("color"), space.color},
                    {QStringLiteral("position"), spacePosition}}));
        const auto localTabs = m_store.loadTabs(space.id);
        for (qsizetype position = 0; position < localTabs.size(); ++position) {
            const auto &tab = localTabs.at(position);
            tabs.insert(tab.id,
                recordDigest({{QStringLiteral("version"), contractVersion},
                    {QStringLiteral("id"), tab.id}, {QStringLiteral("spaceId"), tab.spaceId},
                    {QStringLiteral("url"), tab.url.toString(QUrl::FullyEncoded)},
                    {QStringLiteral("title"), tab.title}, {QStringLiteral("pinned"), tab.pinned},
                    {QStringLiteral("position"), position}, {QStringLiteral("muted"), tab.muted},
                    {QStringLiteral("zoom"), tab.zoom},
                    {QStringLiteral("keepActive"), tab.keepActive}}));
        }
    }
    inventory.insert(QStringLiteral("spaces"), spaces);
    inventory.insert(QStringLiteral("tabs"), tabs);
    QJsonObject configuration;
    const auto missing = QString(QChar(0));
    for (const auto &name : syncedPreferences) {
        const auto value = m_store.preference(name, missing);
        if (value != missing) {
            const auto contents = QJsonDocument(
                QJsonObject {{QStringLiteral("version"), contractVersion},
                    {QStringLiteral("key"), name},
                    {QStringLiteral("value"),
                        value}}).toJson(QJsonDocument::Indented);
            configuration.insert(QStringLiteral("settings/%1.json").arg(name), digest(contents));
        }
    }
    QFile keybindings(QDir(m_options.configRoot).filePath(QStringLiteral("keybindings.json")));
    if (keybindings.open(QIODevice::ReadOnly)) {
        configuration.insert(
            QStringLiteral("config/keybindings.json"), digest(keybindings.readAll()));
    }
    const auto localBlocking = readJsonObject(
        QDir(m_options.dataRoot).filePath(QStringLiteral("content-blocking/settings.json")));
    if (localBlocking.contains(QStringLiteral("subscriptions"))) {
        const auto contents = QJsonDocument(
            QJsonObject {{QStringLiteral("version"), contractVersion},
                {QStringLiteral("subscriptions"),
                    projectedSubscriptions(
                        localBlocking.value(QStringLiteral("subscriptions")).toArray())}})
                                  .toJson(QJsonDocument::Indented);
        configuration.insert(QStringLiteral("settings/content-blocking.json"), digest(contents));
    }
    inventory.insert(QStringLiteral("configuration"), configuration);
    return writeJsonObject(path, inventory, errorMessage);
}

bool SyncModule::stageConfigurationFile(
    const QString &relativePath, const QByteArray &contents, QString *errorMessage) const
{
    const auto destination = QDir(checkoutRoot()).filePath(relativePath);
    const auto inventory = readJsonObject(
        QDir(m_options.dataRoot).filePath(QStringLiteral("sync/applied-records.json")));
    const auto appliedDigest = inventory.value(QStringLiteral("configuration"))
                                   .toObject()
                                   .value(relativePath)
                                   .toString();
    QFile existing(destination);
    const auto existingContents
        = existing.open(QIODevice::ReadOnly) ? existing.readAll() : QByteArray {};
    if (!appliedDigest.isEmpty() && digest(contents) == appliedDigest
        && digest(existingContents) != appliedDigest) {
        return true;
    }
    if (!QDir().mkpath(QFileInfo(destination).absolutePath())) {
        setError(errorMessage, QStringLiteral("Could not create a Sync configuration directory"));
        return false;
    }
    QSaveFile file(destination);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()
        || !file.commit()) {
        setError(errorMessage, QStringLiteral("Could not stage Sync configuration"));
        return false;
    }
    return true;
}

bool SyncModule::restoreConfiguration(QString *errorMessage)
{
    const auto settingsDirectory = QDir(checkoutRoot()).filePath(QStringLiteral("settings"));
    for (const auto &name : syncedPreferences) {
        const auto record = readJsonObject(QDir(settingsDirectory).filePath(name + ".json"));
        if (record.isEmpty()) {
            continue;
        }
        if (record.value(QStringLiteral("version")).toInt() != contractVersion
            || record.value(QStringLiteral("key")).toString() != name
            || !record.value(QStringLiteral("value")).isString()
            || !m_store.savePreference(name, record.value(QStringLiteral("value")).toString())) {
            setError(errorMessage, QStringLiteral("A synced Setting is not valid"));
            return false;
        }
    }

    const auto remoteKeybindings
        = QDir(checkoutRoot()).filePath(QStringLiteral("config/keybindings.json"));
    if (QFileInfo::exists(remoteKeybindings)) {
        QFile source(remoteKeybindings);
        const auto destination
            = QDir(m_options.configRoot).filePath(QStringLiteral("keybindings.json"));
        if (!source.open(QIODevice::ReadOnly)
            || !QDir().mkpath(QFileInfo(destination).absolutePath())) {
            setError(errorMessage, QStringLiteral("Could not read the synced keybindings"));
            return false;
        }
        const auto contents = source.readAll();
        QJsonParseError parseError;
        if (QJsonDocument::fromJson(contents, &parseError).isNull()
            || parseError.error != QJsonParseError::NoError) {
            setError(errorMessage, QStringLiteral("The synced keybindings are not valid JSON"));
            return false;
        }
        QSaveFile file(destination);
        if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size()
            || !file.commit()) {
            setError(errorMessage, QStringLiteral("Could not apply the synced keybindings"));
            return false;
        }
    }

    const auto remoteBlocking
        = readJsonObject(QDir(settingsDirectory).filePath(QStringLiteral("content-blocking.json")));
    if (!remoteBlocking.isEmpty()) {
        const auto subscriptions = remoteBlocking.value(QStringLiteral("subscriptions"));
        if (remoteBlocking.value(QStringLiteral("version")).toInt() != contractVersion
            || !subscriptions.isArray()) {
            setError(errorMessage, QStringLiteral("The synced filter subscriptions are not valid"));
            return false;
        }
        const auto localPath
            = QDir(m_options.dataRoot).filePath(QStringLiteral("content-blocking/settings.json"));
        auto local = readJsonObject(localPath);
        local.insert(QStringLiteral("version"), 1);
        local.insert(QStringLiteral("subscriptions"), subscriptions.toArray());
        if (!writeJsonObject(localPath, local, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool SyncModule::captureConfiguration(QString *errorMessage)
{
    const auto missing = QString(QChar(0));
    for (const auto &name : syncedPreferences) {
        const auto value = m_store.preference(name, missing);
        if (value == missing) {
            continue;
        }
        const auto contents = QJsonDocument(
            QJsonObject {{QStringLiteral("version"), contractVersion},
                {QStringLiteral("key"), name},
                {QStringLiteral("value"),
                    value}}).toJson(QJsonDocument::Indented);
        if (!stageConfigurationFile(
                QStringLiteral("settings/%1.json").arg(name), contents, errorMessage)) {
            return false;
        }
    }

    const auto localKeybindings
        = QDir(m_options.configRoot).filePath(QStringLiteral("keybindings.json"));
    if (QFileInfo::exists(localKeybindings)) {
        QFile source(localKeybindings);
        if (!source.open(QIODevice::ReadOnly)) {
            setError(errorMessage, QStringLiteral("Could not read the local keybindings"));
            return false;
        }
        const auto contents = source.readAll();
        if (!stageConfigurationFile(
                QStringLiteral("config/keybindings.json"), contents, errorMessage)) {
            return false;
        }
    }

    const auto localBlocking = readJsonObject(
        QDir(m_options.dataRoot).filePath(QStringLiteral("content-blocking/settings.json")));
    if (localBlocking.contains(QStringLiteral("subscriptions"))) {
        const auto contents = QJsonDocument(
            QJsonObject {{QStringLiteral("version"), contractVersion},
                {QStringLiteral("subscriptions"),
                    projectedSubscriptions(
                        localBlocking.value(QStringLiteral("subscriptions")).toArray())}})
                                  .toJson(QJsonDocument::Indented);
        if (!stageConfigurationFile(
                QStringLiteral("settings/content-blocking.json"), contents, errorMessage)) {
            return false;
        }
    }
    return true;
}

bool SyncModule::reconcile(QString *errorMessage)
{
    if (m_options.initialRemoteRestore && !m_options.deferRemoteApply) {
        if (m_options.discardPristineLocalState) {
            for (const auto &space : m_store.loadSpaces()) {
                if (!m_store.deleteSpace(space.id)) {
                    setError(
                        errorMessage, QStringLiteral("Could not replace the initial local Space"));
                    return false;
                }
            }
        }
        if (!restoreRemoteState(errorMessage)) {
            return false;
        }
    } else if (!m_options.initialRemoteRestore && m_store.loadSpaces().isEmpty()
        && !restoreRemoteState(errorMessage)) {
        return false;
    }
    if (m_options.initialRemoteRestore && m_options.deferRemoteApply) {
        if (!writeLocalBaselineInventory(errorMessage)) {
            return false;
        }
        if (!runGit({QStringLiteral("fetch"), QStringLiteral("origin")}, errorMessage)) {
            return false;
        }
        if (gitRefExists(QStringLiteral("refs/remotes/origin/main"))
            && !mergeRemote(errorMessage)) {
            return false;
        }
        return !cancelled(errorMessage);
    }
    QSet<QString> currentSpaceIds;
    QSet<QString> currentTabIds;
    const auto localSpaces = m_store.loadSpaces();
    for (qsizetype spacePosition = 0; spacePosition < localSpaces.size(); ++spacePosition) {
        const auto &space = localSpaces.at(spacePosition);
        currentSpaceIds.insert(space.id);
        if (!writeEncryptedRecord(QStringLiteral("spaces"), space.id,
                compactJson({{QStringLiteral("version"), contractVersion},
                    {QStringLiteral("id"), space.id}, {QStringLiteral("name"), space.name},
                    {QStringLiteral("color"), space.color},
                    {QStringLiteral("position"), spacePosition}}),
                errorMessage)) {
            return false;
        }
        const auto tabs = m_store.loadTabs(space.id);
        for (qsizetype position = 0; position < tabs.size(); ++position) {
            const auto &tab = tabs.at(position);
            currentTabIds.insert(tab.id);
            if (!writeEncryptedRecord(QStringLiteral("tabs"), tab.id,
                    compactJson({{QStringLiteral("version"), contractVersion},
                        {QStringLiteral("id"), tab.id}, {QStringLiteral("spaceId"), tab.spaceId},
                        {QStringLiteral("url"), tab.url.toString(QUrl::FullyEncoded)},
                        {QStringLiteral("title"), tab.title},
                        {QStringLiteral("pinned"), tab.pinned},
                        {QStringLiteral("position"), position},
                        {QStringLiteral("muted"), tab.muted}, {QStringLiteral("zoom"), tab.zoom},
                        {QStringLiteral("keepActive"), tab.keepActive}}),
                    errorMessage)) {
                return false;
            }
        }
    }
    if (!tombstoneMissingRecords(QStringLiteral("spaces"), currentSpaceIds, errorMessage)
        || !tombstoneMissingRecords(QStringLiteral("tabs"), currentTabIds, errorMessage)) {
        return false;
    }
    if (!captureConfiguration(errorMessage)) {
        return false;
    }

    const auto metaPath = QDir(checkoutRoot()).filePath(QStringLiteral("meta.json"));
    const auto existingMeta = readJsonObject(metaPath);
    QSaveFile meta(metaPath);
    if (!meta.open(QIODevice::WriteOnly)) {
        setError(errorMessage, meta.errorString());
        return false;
    }
    meta.write(QJsonDocument(
        QJsonObject {{QStringLiteral("format"), QStringLiteral("omaweb-sync")},
            {QStringLiteral("version"), contractVersion},
            {QStringLiteral("epoch"), existingMeta.value(QStringLiteral("epoch")).toInt()}})
            .toJson(QJsonDocument::Indented));
    if (!meta.commit()) {
        setError(errorMessage, meta.errorString());
        return false;
    }

    if (!runGit({QStringLiteral("add"), QStringLiteral("--all")}, errorMessage)
        || !runGit({QStringLiteral("config"), QStringLiteral("user.name"),
                       m_options.authorName.isEmpty() ? QStringLiteral("Omaweb Sync")
                                                      : m_options.authorName},
            errorMessage)
        || !runGit({QStringLiteral("config"), QStringLiteral("user.email"),
                       m_options.authorName.isEmpty()
                           ? QStringLiteral("sync@omaweb.local")
                           : m_options.authorName + QStringLiteral("@users.noreply.github.com")},
            errorMessage)) {
        return false;
    }
    QProcess changed;
    changed.setWorkingDirectory(checkoutRoot());
    changed.start(QStringLiteral("git"),
        {QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--quiet")});
    if (!changed.waitForFinished() || changed.exitStatus() != QProcess::NormalExit) {
        setError(errorMessage, QStringLiteral("Could not inspect local Sync changes"));
        return false;
    }
    if (changed.exitCode() != 0
        && !runGit({QStringLiteral("commit"), QStringLiteral("-m"),
                       QStringLiteral("sync: reconcile browser state")},
            errorMessage)) {
        return false;
    }
    if (!runGit({QStringLiteral("fetch"), QStringLiteral("origin")}, errorMessage)) {
        return false;
    }
    if (gitRefExists(QStringLiteral("refs/remotes/origin/main"))) {
        if (!mergeRemote(errorMessage)
            || (!m_options.deferRemoteApply && !restoreRemoteState(errorMessage))) {
            return false;
        }
    }
    if (!runGit({QStringLiteral("push"), QStringLiteral("origin"), QStringLiteral("HEAD:main")},
            errorMessage)) {
        return false;
    }
    return compactHistoryIfNeeded(errorMessage)
        && (m_options.deferRemoteApply || writeAppliedRecordInventory(errorMessage));
}

bool SyncModule::applyRemoteState(QString *errorMessage)
{
    if (m_options.initialRemoteRestore && m_options.discardPristineLocalState) {
        for (const auto &space : m_store.loadSpaces()) {
            if (!m_store.deleteSpace(space.id)) {
                setError(errorMessage, QStringLiteral("Could not replace the initial local Space"));
                return false;
            }
        }
    }
    return restoreRemoteState(errorMessage);
}

bool SyncModule::remoteEpochAdvanced() const { return m_remoteEpochAdvanced; }

} // namespace omaweb
