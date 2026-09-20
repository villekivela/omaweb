#include "ExtensionPackage.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <QtCore/private/qzipreader_p.h>

#include <openssl/evp.h>
#include <openssl/x509.h>

#include <optional>

namespace omaweb {

namespace {

    // CRX3, as Chromium writes it: "Cr24", a version, a header length, that
    // many bytes of `CrxFileHeader`, and the ZIP archive.
    // `components/crx_file/crx3.proto` and `crx_verifier.cc`.
    constexpr char kMagic[] = "Cr24";
    constexpr quint32 kVersion = 3;
    // What the signature is taken over, ahead of the header and the archive.
    // `crx_verifier.cc`'s `kSignatureContext`, NUL included.
    constexpr char kSignatureContext[] = "CRX3 SignedData";
    // Field numbers in `CrxFileHeader` and `AsymmetricKeyProof`.
    constexpr int kSha256WithRsa = 2;
    constexpr int kSignedHeaderData = 10000;
    constexpr int kPublicKey = 1;
    constexpr int kSignature = 2;
    constexpr int kCrxId = 1;
    constexpr int kCrxIdLength = 16;
    // A store package is tens of megabytes. Anything far past that is not one,
    // and reading it into memory is the reason to say so before we do.
    constexpr qint64 kLargestPackage = 128 * 1024 * 1024;

    // Protocol-buffer reading, only as much as this header needs: the fields
    // read here are all length-delimited, and anything else is stepped over.
    // A real protobuf dependency for three fields would be a dependency for
    // three fields.
    class WireReader final {
    public:
        explicit WireReader(const QByteArray &data)
            : m_data(data)
        {
        }

        bool atEnd() const { return m_failed || m_at >= m_data.size(); }
        bool failed() const { return m_failed; }

        // The next field's number, with its payload in `payload` when it is a
        // length-delimited one. Other wire types leave `payload` empty.
        std::optional<int> next(QByteArray &payload)
        {
            payload.clear();
            const std::optional<quint64> tag = varint();
            if (!tag) {
                return std::nullopt;
            }
            const int field = static_cast<int>(*tag >> 3);
            switch (*tag & 7) {
            case 0:
                if (!varint()) {
                    return std::nullopt;
                }
                return field;
            case 1:
                return skip(8) ? std::optional<int> {field} : std::nullopt;
            case 2: {
                const std::optional<quint64> length = varint();
                if (!length || *length > static_cast<quint64>(m_data.size() - m_at)) {
                    m_failed = true;
                    return std::nullopt;
                }
                payload = m_data.mid(m_at, static_cast<qsizetype>(*length));
                m_at += static_cast<qsizetype>(*length);
                return field;
            }
            case 5:
                return skip(4) ? std::optional<int> {field} : std::nullopt;
            default:
                m_failed = true;
                return std::nullopt;
            }
        }

    private:
        std::optional<quint64> varint()
        {
            quint64 value = 0;
            for (int shift = 0; shift < 64; shift += 7) {
                if (m_at >= m_data.size()) {
                    break;
                }
                const quint8 byte = static_cast<quint8>(m_data.at(m_at++));
                value |= static_cast<quint64>(byte & 0x7f) << shift;
                if (!(byte & 0x80)) {
                    return value;
                }
            }
            m_failed = true;
            return std::nullopt;
        }

        bool skip(qsizetype count)
        {
            if (m_data.size() - m_at < count) {
                m_failed = true;
                return false;
            }
            m_at += count;
            return true;
        }

        QByteArray m_data;
        qsizetype m_at = 0;
        bool m_failed = false;
    };

    quint32 readLittleEndian(const QByteArray &data, qsizetype at)
    {
        return static_cast<quint8>(data.at(at)) | static_cast<quint8>(data.at(at + 1)) << 8
            | static_cast<quint8>(data.at(at + 2)) << 16
            | static_cast<quint8>(data.at(at + 3)) << 24;
    }

    QByteArray littleEndian(quint32 value)
    {
        QByteArray out(4, Qt::Uninitialized);
        for (int index = 0; index < 4; ++index) {
            out[index] = static_cast<char>((value >> (8 * index)) & 0xff);
        }
        return out;
    }

    // RSA PKCS#1 v1.5 over SHA-256, which is the only proof kind Omaweb reads.
    // The key arrives as a DER SubjectPublicKeyInfo, which is also what the
    // manifest's `key` field carries in base64.
    bool signatureHolds(const QByteArray &publicKey, const QByteArray &signature,
        const QByteArray &context, const QByteArray &header, const QByteArray &archive)
    {
        const unsigned char *keyBytes
            = reinterpret_cast<const unsigned char *>(publicKey.constData());
        EVP_PKEY *key = d2i_PUBKEY(nullptr, &keyBytes, publicKey.size());
        if (!key) {
            return false;
        }
        EVP_MD_CTX *digest = EVP_MD_CTX_new();
        bool held = digest != nullptr
            && EVP_DigestVerifyInit(digest, nullptr, EVP_sha256(), nullptr, key) == 1
            && EVP_DigestVerifyUpdate(digest, context.constData(), context.size()) == 1
            && EVP_DigestVerifyUpdate(digest, header.constData(), header.size()) == 1
            && EVP_DigestVerifyUpdate(digest, archive.constData(), archive.size()) == 1
            && EVP_DigestVerifyFinal(digest,
                   reinterpret_cast<const unsigned char *>(signature.constData()), signature.size())
                == 1;
        EVP_MD_CTX_free(digest);
        EVP_PKEY_free(key);
        return held;
    }

    // An entry that would be written outside the destination is a package
    // attacking the filesystem rather than one we failed to read.
    bool pathIsContained(const QString &entry)
    {
        if (entry.isEmpty() || entry.startsWith(QLatin1Char('/'))
            || entry.contains(QLatin1String(":"))) {
            return false;
        }
        const QStringList parts = entry.split(QLatin1Char('/'));
        return !parts.contains(QStringLiteral("..")) && !entry.contains(QLatin1Char('\\'));
    }

    ExtensionPackage::Result refuse(const QString &error) { return {false, error}; }

} // namespace

QUrl ExtensionPackage::storeAddress(const QString &storeId)
{
    // Chromium's own update endpoint, which answers with the CRX3 for an id.
    // The version is Chromium's because what is being asked for is a Chromium
    // package; the endpoint refuses to answer without one.
    QUrl address(QStringLiteral("https://clients2.google.com/service/update2/crx"));
    // The inner query is one value, so its own separators are encoded rather
    // than left to read as this query's.
    const QString request
        = QString::fromLatin1(QUrl::toPercentEncoding(QStringLiteral("id=%1&uc").arg(storeId)));
    address.setQuery(QStringLiteral("response=redirect&acceptformat=crx3"
                                    "&prodversion=140.0.0.0&x=")
        + request);
    return address;
}

QString ExtensionPackage::identityFor(const QByteArray &publisherKey)
{
    const QByteArray digest = QCryptographicHash::hash(publisherKey, QCryptographicHash::Sha256);
    QString id;
    id.reserve(kCrxIdLength * 2);
    for (int index = 0; index < kCrxIdLength; ++index) {
        const quint8 byte = static_cast<quint8>(digest.at(index));
        id.append(QLatin1Char(static_cast<char>('a' + (byte >> 4))));
        id.append(QLatin1Char(static_cast<char>('a' + (byte & 0x0f))));
    }
    return id;
}

ExtensionPackage::Result ExtensionPackage::install(
    const QByteArray &crx, const KnownExtension &extension, const QString &destination)
{
    const QByteArray pinned = QByteArray::fromBase64(extension.publisherKey.toLatin1());
    if (pinned.isEmpty()) {
        return refuse(QStringLiteral("Omaweb has no signing key for this extension."));
    }
    if (crx.size() > kLargestPackage) {
        return refuse(QStringLiteral("The package is larger than an extension should be."));
    }
    if (crx.size() < 12 || !crx.startsWith(kMagic)) {
        return refuse(QStringLiteral("The download is not an extension package."));
    }
    if (readLittleEndian(crx, 4) != kVersion) {
        return refuse(QStringLiteral("The package is not in the format Omaweb reads."));
    }
    const quint32 headerLength = readLittleEndian(crx, 8);
    if (headerLength > static_cast<quint32>(crx.size() - 12)) {
        return refuse(QStringLiteral("The package is truncated."));
    }
    const QByteArray header = crx.mid(12, static_cast<qsizetype>(headerLength));
    const QByteArray archive = crx.mid(12 + static_cast<qsizetype>(headerLength));

    QByteArray signedHeaderData;
    QByteArray signature;
    bool signedByPinnedKey = false;
    WireReader reader(header);
    while (!reader.atEnd()) {
        QByteArray payload;
        const std::optional<int> field = reader.next(payload);
        if (!field) {
            break;
        }
        if (*field == kSignedHeaderData) {
            signedHeaderData = payload;
        } else if (*field == kSha256WithRsa) {
            // Every proof is read, and the one made with the pinned key is the
            // one that counts. A package carries the store's proof as well,
            // and which proof comes first is not ours to rely on.
            QByteArray proofKey;
            QByteArray proofSignature;
            WireReader proof(payload);
            while (!proof.atEnd()) {
                QByteArray part;
                const std::optional<int> proofField = proof.next(part);
                if (!proofField) {
                    break;
                }
                if (*proofField == kPublicKey) {
                    proofKey = part;
                } else if (*proofField == kSignature) {
                    proofSignature = part;
                }
            }
            if (proofKey == pinned) {
                signature = proofSignature;
                signedByPinnedKey = true;
            }
        }
    }
    if (reader.failed()) {
        return refuse(QStringLiteral("The package's header could not be read."));
    }
    if (!signedByPinnedKey) {
        return refuse(QStringLiteral("The package is not signed by %1.").arg(extension.publisher));
    }

    // The id the publisher signed into the header, which has to be the id
    // Omaweb names. A package signed by the right key for a different extension
    // is still the wrong package.
    QByteArray signedId;
    WireReader signedData(signedHeaderData);
    while (!signedData.atEnd()) {
        QByteArray payload;
        const std::optional<int> field = signedData.next(payload);
        if (!field) {
            break;
        }
        if (*field == kCrxId) {
            signedId = payload;
        }
    }
    if (signedId.size() != kCrxIdLength
        || QCryptographicHash::hash(pinned, QCryptographicHash::Sha256).left(kCrxIdLength)
            != signedId) {
        return refuse(QStringLiteral("The package names an extension it is not signed for."));
    }
    if (identityFor(pinned) != extension.storeId) {
        return refuse(QStringLiteral("Omaweb's own record of this extension disagrees with "
                                     "itself and the package was not installed."));
    }

    // What Chromium signs: the context string with its NUL, the length of the
    // signed header rather than of the whole header, then that header and the
    // archive. `crx_verifier.cc`, `VerifyCrx3`.
    const QByteArray context = QByteArray(kSignatureContext, sizeof(kSignatureContext))
        + littleEndian(static_cast<quint32>(signedHeaderData.size()));
    if (!signatureHolds(pinned, signature, context, signedHeaderData, archive)) {
        return refuse(QStringLiteral("The package's signature does not hold."));
    }

    // Unpacked beside the destination and moved into place, so a failure
    // anywhere in the archive leaves nothing an engine would try to load.
    const QString staging = destination + QStringLiteral(".incoming");
    QDir(staging).removeRecursively();
    if (!QDir().mkpath(staging)) {
        return refuse(QStringLiteral("Omaweb could not write where extensions are kept."));
    }
    const auto abandon = [&staging](const QString &error) {
        QDir(staging).removeRecursively();
        return refuse(error);
    };

    QBuffer archiveBuffer;
    archiveBuffer.setData(archive);
    if (!archiveBuffer.open(QIODevice::ReadOnly)) {
        return abandon(QStringLiteral("The package's archive could not be opened."));
    }
    QZipReader zip(&archiveBuffer);
    if (!zip.isReadable()) {
        return abandon(QStringLiteral("The package's archive could not be read."));
    }
    const QList<QZipReader::FileInfo> entries = zip.fileInfoList();
    // Which names are folders. A ZIP marks a folder with a trailing slash and
    // an attribute, and not every packer sets the attribute: Bitwarden's own
    // archive carries `images` as an empty entry that `QZipReader` reports as
    // a file, and writing that file leaves nowhere to put `images/icon16.png`.
    // A name another entry is inside is a folder, whatever the entry says.
    QSet<QString> folders;
    for (const QZipReader::FileInfo &entry : entries) {
        QString path = entry.filePath;
        while (true) {
            const qsizetype slash = path.lastIndexOf(QLatin1Char('/'));
            if (slash <= 0) {
                break;
            }
            path.truncate(slash);
            folders.insert(path);
        }
    }

    for (const QZipReader::FileInfo &entry : entries) {
        if (!pathIsContained(entry.filePath)) {
            return abandon(QStringLiteral("The package tried to write outside its own folder."));
        }
        const QString path = QDir(staging).filePath(entry.filePath);
        if (entry.isDir || folders.contains(entry.filePath)) {
            if (!QDir().mkpath(path)) {
                return abandon(QStringLiteral("The package could not be unpacked."));
            }
            continue;
        }
        if (!entry.isFile) {
            continue;
        }
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            return abandon(QStringLiteral("The package could not be unpacked."));
        }
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(zip.fileData(entry.filePath)) < 0
            || !file.commit()) {
            return abandon(QStringLiteral("The package could not be unpacked."));
        }
    }

    // The key goes into the manifest, which is what makes the engine derive the
    // store id from it rather than from the directory's path. Without this the
    // package works and the vendor's desktop application refuses it, which is
    // the failure this whole path exists to avoid.
    const QString manifestPath = QDir(staging).filePath(QStringLiteral("manifest.json"));
    QFile manifestFile(manifestPath);
    if (!manifestFile.open(QIODevice::ReadOnly)) {
        return abandon(QStringLiteral("The package has no manifest."));
    }
    QJsonParseError parse {};
    QJsonDocument manifest = QJsonDocument::fromJson(manifestFile.readAll(), &parse);
    manifestFile.close();
    if (parse.error != QJsonParseError::NoError || !manifest.isObject()) {
        return abandon(QStringLiteral("The package's manifest could not be read."));
    }
    QJsonObject root = manifest.object();
    root.insert(QStringLiteral("key"), QString::fromLatin1(pinned.toBase64()));
    manifest.setObject(root);
    QSaveFile rewritten(manifestPath);
    if (!rewritten.open(QIODevice::WriteOnly) || rewritten.write(manifest.toJson()) < 0
        || !rewritten.commit()) {
        return abandon(QStringLiteral("The package's manifest could not be written."));
    }

    QDir(destination).removeRecursively();
    if (!QDir().rename(staging, destination)) {
        return abandon(QStringLiteral("The package could not be put where it is kept."));
    }
    return {true, {}};
}

} // namespace omaweb
