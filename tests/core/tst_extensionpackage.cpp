#include "ExtensionPackage.h"
#include "KnownExtensions.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

#include <QtCore/private/qzipwriter_p.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

using namespace omaweb;

namespace {

// A publisher, made for the test. Everything the verifier trusts comes from
// this key, so the test owns one and signs its own packages with it: a fixture
// built from a real vendor's package could only ever be tested against that
// vendor's real key, and the cases worth covering are the ones where the key is
// wrong.
class Publisher {
public:
    Publisher()
    {
        EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
        EVP_PKEY_keygen_init(context);
        EVP_PKEY_CTX_set_rsa_keygen_bits(context, 2048);
        EVP_PKEY_keygen(context, &m_key);
        EVP_PKEY_CTX_free(context);
    }

    ~Publisher() { EVP_PKEY_free(m_key); }

    Publisher(const Publisher &) = delete;
    Publisher &operator=(const Publisher &) = delete;

    // The DER SubjectPublicKeyInfo, which is what a CRX header carries and what
    // the manifest's `key` field holds in base64.
    QByteArray publicKey() const
    {
        unsigned char *bytes = nullptr;
        const int length = i2d_PUBKEY(m_key, &bytes);
        const QByteArray out(reinterpret_cast<char *>(bytes), length);
        OPENSSL_free(bytes);
        return out;
    }

    QByteArray sign(const QByteArray &data) const
    {
        EVP_MD_CTX *digest = EVP_MD_CTX_new();
        EVP_DigestSignInit(digest, nullptr, EVP_sha256(), nullptr, m_key);
        EVP_DigestSignUpdate(digest, data.constData(), data.size());
        size_t length = 0;
        EVP_DigestSignFinal(digest, nullptr, &length);
        QByteArray signature(static_cast<qsizetype>(length), Qt::Uninitialized);
        EVP_DigestSignFinal(digest, reinterpret_cast<unsigned char *>(signature.data()), &length);
        signature.resize(static_cast<qsizetype>(length));
        EVP_MD_CTX_free(digest);
        return signature;
    }

private:
    EVP_PKEY *m_key = nullptr;
};

QByteArray varint(quint64 value)
{
    QByteArray out;
    do {
        quint8 byte = value & 0x7f;
        value >>= 7;
        if (value) {
            byte |= 0x80;
        }
        out.append(static_cast<char>(byte));
    } while (value);
    return out;
}

QByteArray field(int number, const QByteArray &payload)
{
    return varint((static_cast<quint64>(number) << 3) | 2) + varint(payload.size()) + payload;
}

QByteArray littleEndian(quint32 value)
{
    QByteArray out(4, Qt::Uninitialized);
    for (int index = 0; index < 4; ++index) {
        out[index] = static_cast<char>((value >> (8 * index)) & 0xff);
    }
    return out;
}

// A ZIP holding a manifest and one file beside it, which is the shape of every
// extension: the manifest is what the installer has to find and rewrite.
QByteArray archiveWith(const QJsonObject &manifest, const QString &extraPath = {})
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);
    {
        QZipWriter writer(&buffer);
        writer.addFile(QStringLiteral("manifest.json"), QJsonDocument(manifest).toJson());
        writer.addFile(extraPath.isEmpty() ? QStringLiteral("background.js") : extraPath,
            QByteArrayLiteral("// nothing\n"));
        writer.close();
    }
    return buffer.data();
}

quint32 crc32Of(const QByteArray &data)
{
    quint32 crc = 0xffffffffu;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (~(crc & 1) + 1));
        }
    }
    return ~crc;
}

QByteArray shortLittleEndian(quint16 value)
{
    QByteArray out(2, Qt::Uninitialized);
    out[0] = static_cast<char>(value & 0xff);
    out[1] = static_cast<char>((value >> 8) & 0xff);
    return out;
}

// A ZIP written by hand, because `QZipWriter` normalises a path and the entry
// worth testing is one that has not been normalised. A hostile package is not
// written by Qt.
QByteArray archiveWithRawPaths(const QList<QPair<QString, QByteArray>> &entries)
{
    QByteArray local;
    QByteArray central;
    for (const auto &[path, content] : entries) {
        const QByteArray name = path.toUtf8();
        const QByteArray shared = shortLittleEndian(20) + shortLittleEndian(0)
            + shortLittleEndian(0) + shortLittleEndian(0) + shortLittleEndian(0)
            + littleEndian(crc32Of(content)) + littleEndian(content.size())
            + littleEndian(content.size()) + shortLittleEndian(name.size());
        central += QByteArrayLiteral("PK\x01\x02") + shortLittleEndian(20) + shared
            + shortLittleEndian(0) + shortLittleEndian(0) + shortLittleEndian(0)
            + shortLittleEndian(0) + littleEndian(0) + littleEndian(local.size()) + name;
        local += QByteArrayLiteral("PK\x03\x04") + shared + shortLittleEndian(0) + name + content;
    }
    return local + central + QByteArrayLiteral("PK\x05\x06") + shortLittleEndian(0)
        + shortLittleEndian(0) + shortLittleEndian(entries.size())
        + shortLittleEndian(entries.size()) + littleEndian(central.size())
        + littleEndian(local.size()) + shortLittleEndian(0);
}

// A CRX3, signed the way the store signs one.
QByteArray packageFor(
    const Publisher &publisher, const QByteArray &archive, const QByteArray &signedId = {})
{
    const QByteArray key = publisher.publicKey();
    const QByteArray id = signedId.isEmpty()
        ? QCryptographicHash::hash(key, QCryptographicHash::Sha256).left(16)
        : signedId;
    const QByteArray signedHeaderData = field(1, id);
    const QByteArray context
        = QByteArray("CRX3 SignedData\0", 16) + littleEndian(signedHeaderData.size());
    const QByteArray signature = publisher.sign(context + signedHeaderData + archive);
    const QByteArray proof = field(1, key) + field(2, signature);
    const QByteArray header = field(2, proof) + field(10000, signedHeaderData);
    return QByteArrayLiteral("Cr24") + littleEndian(3) + littleEndian(header.size()) + header
        + archive;
}

KnownExtension entryFor(const Publisher &publisher)
{
    KnownExtension extension;
    extension.key = QStringLiteral("fixture");
    extension.name = QStringLiteral("Fixture");
    extension.publisher = QStringLiteral("The test");
    extension.publisherKey = QString::fromLatin1(publisher.publicKey().toBase64());
    extension.storeId = ExtensionPackage::identityFor(publisher.publicKey());
    return extension;
}

QJsonObject plainManifest()
{
    return QJsonObject {{QStringLiteral("name"), QStringLiteral("Fixture")},
        {QStringLiteral("version"), QStringLiteral("1.0")},
        {QStringLiteral("manifest_version"), 3}};
}

} // namespace

// What a store package has to prove before any of it is written to disk. The
// pinned publisher key is the only thing trusted here, so these are the cases
// where something other than that key is offering the package.
class ExtensionPackageTest : public QObject {
    Q_OBJECT

private slots:
    void aPackageSignedByThePinnedKeyIsUnpackedUnderItsStoreIdentity()
    {
        const Publisher publisher;
        const KnownExtension extension = entryFor(publisher);
        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(QStringLiteral("fixture"));

        const ExtensionPackage::Result result = ExtensionPackage::install(
            packageFor(publisher, archiveWith(plainManifest())), extension, destination);
        QVERIFY2(result.ok, qPrintable(result.error));

        // The key is written into the manifest, which is the whole point: the
        // engine derives the store id from it instead of from the path, and the
        // publisher's own desktop application answers that id.
        QFile manifest(QDir(destination).filePath(QStringLiteral("manifest.json")));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const QJsonObject written = QJsonDocument::fromJson(manifest.readAll()).object();
        QCOMPARE(written.value(QStringLiteral("key")).toString(), extension.publisherKey);
        QCOMPARE(ExtensionPackage::identityFor(QByteArray::fromBase64(
                     written.value(QStringLiteral("key")).toString().toLatin1())),
            extension.storeId);
        QVERIFY(QFile::exists(QDir(destination).filePath(QStringLiteral("background.js"))));
    }

    void aPackageSignedByAnotherKeyIsRefused()
    {
        const Publisher publisher;
        const Publisher impostor;
        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(QStringLiteral("fixture"));

        const ExtensionPackage::Result result = ExtensionPackage::install(
            packageFor(impostor, archiveWith(plainManifest())), entryFor(publisher), destination);
        QVERIFY(!result.ok);
        QVERIFY(!QDir(destination).exists());
    }

    void aPackageChangedAfterItWasSignedIsRefused()
    {
        const Publisher publisher;
        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(QStringLiteral("fixture"));

        QByteArray crx = packageFor(publisher, archiveWith(plainManifest()));
        // One byte of the archive, which is signed and therefore cannot move.
        crx[crx.size() - 32] = static_cast<char>(crx.at(crx.size() - 32) ^ 0xff);

        const ExtensionPackage::Result result
            = ExtensionPackage::install(crx, entryFor(publisher), destination);
        QVERIFY(!result.ok);
        QVERIFY(!QDir(destination).exists());
    }

    void aPackageSignedForAnotherExtensionIsRefused()
    {
        const Publisher publisher;
        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(QStringLiteral("fixture"));

        const ExtensionPackage::Result result = ExtensionPackage::install(
            packageFor(publisher, archiveWith(plainManifest()), QByteArray(16, 'z')),
            entryFor(publisher), destination);
        QVERIFY(!result.ok);
        QVERIFY(!QDir(destination).exists());
    }

    // A package naming a path above its own folder writes nothing above it.
    // `QZipReader` strips the leading steps itself, so such an entry lands
    // inside the folder rather than being refused; the guard in the installer
    // is there for the day that changes. What is asserted is the outcome that
    // matters, which is that nothing was written where it should not be.
    void anEntryNamingAPathAboveTheFolderStaysInsideIt()
    {
        const Publisher publisher;
        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(QStringLiteral("held/fixture"));

        const QByteArray archive = archiveWithRawPaths(
            {{QStringLiteral("manifest.json"), QJsonDocument(plainManifest()).toJson()},
                {QStringLiteral("../../escaped.js"), QByteArrayLiteral("// escaped\n")}});
        const ExtensionPackage::Result result = ExtensionPackage::install(
            packageFor(publisher, archive), entryFor(publisher), destination);
        QVERIFY2(result.ok, qPrintable(result.error));

        QVERIFY(!QFile::exists(QDir(root.path()).filePath(QStringLiteral("escaped.js"))));
        QVERIFY(!QFile::exists(QDir(root.path()).filePath(QStringLiteral("held/escaped.js"))));
        QVERIFY(QFile::exists(QDir(destination).filePath(QStringLiteral("escaped.js"))));
    }

    void aPackageWithNoManifestIsRefused()
    {
        const Publisher publisher;
        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(QStringLiteral("fixture"));

        QBuffer buffer;
        buffer.open(QIODevice::WriteOnly);
        {
            QZipWriter writer(&buffer);
            writer.addFile(QStringLiteral("background.js"), QByteArrayLiteral("//\n"));
            writer.close();
        }
        const ExtensionPackage::Result result = ExtensionPackage::install(
            packageFor(publisher, buffer.data()), entryFor(publisher), destination);
        QVERIFY(!result.ok);
        QVERIFY(!QDir(destination).exists());
    }

    void theStoreAddressAsksForAPackageThisEngineReads()
    {
        const QUrl address
            = ExtensionPackage::storeAddress(QStringLiteral("nngceckbapebfimnlniiiahkandclblb"));
        QCOMPARE(address.host(), QStringLiteral("clients2.google.com"));
        QCOMPARE(address.scheme(), QStringLiteral("https"));
        const QString query = address.query(QUrl::FullyDecoded);
        QVERIFY(query.contains(QStringLiteral("acceptformat=crx3")));
        QVERIFY(query.contains(QStringLiteral("id=nngceckbapebfimnlniiiahkandclblb")));
    }

    // The catalogue's two halves have to agree: the id Omaweb names is the one
    // the pinned key produces. If they ever drift, every download is refused by
    // the check above, and this says which entry drifted.
    void everyPinnedKeyProducesTheStoreIdTheCatalogueNames()
    {
        for (const KnownExtension &extension : knownExtensions()) {
            const QByteArray key = QByteArray::fromBase64(extension.publisherKey.toLatin1());
            QVERIFY2(!key.isEmpty(), qPrintable(extension.key));
            QCOMPARE(ExtensionPackage::identityFor(key), extension.storeId);
        }
    }

    // The real thing, when there is one to hand. A store package is tens of
    // megabytes, so it is not a fixture in the repository; point this at one and
    // it proves the reader against what the store actually serves rather than
    // against a package this test wrote itself.
    //
    //   OMAWEB_TEST_CRX=bitwarden=/path/to.crx ctest -R omaweb-extension-package
    void aRealStorePackageIsRead()
    {
        const QString argument
            = QProcessEnvironment::systemEnvironment().value(QStringLiteral("OMAWEB_TEST_CRX"));
        if (argument.isEmpty()) {
            QSKIP("set OMAWEB_TEST_CRX=<key>=<path> to read a real store package");
        }
        const qsizetype split = argument.indexOf(QLatin1Char('='));
        QVERIFY2(split > 0, "OMAWEB_TEST_CRX is <key>=<path>");
        const KnownExtension extension = knownExtension(argument.left(split));
        QVERIFY2(!extension.key.isEmpty(), "Omaweb names no such extension");
        QFile file(argument.mid(split + 1));
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));

        QTemporaryDir root;
        const QString destination = QDir(root.path()).filePath(extension.key);
        const ExtensionPackage::Result result
            = ExtensionPackage::install(file.readAll(), extension, destination);
        QVERIFY2(result.ok, qPrintable(result.error));

        QFile manifest(QDir(destination).filePath(QStringLiteral("manifest.json")));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        const QJsonObject written = QJsonDocument::fromJson(manifest.readAll()).object();
        QCOMPARE(written.value(QStringLiteral("key")).toString(), extension.publisherKey);
        QCOMPARE(written.value(QStringLiteral("manifest_version")).toInt(), 3);
    }
};

QTEST_MAIN(ExtensionPackageTest)
#include "tst_extensionpackage.moc"
