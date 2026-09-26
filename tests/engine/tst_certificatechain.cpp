#include "CertificateChain.h"

#include <QCryptographicHash>
#include <QSslCertificate>
#include <QTest>

using omaweb::describeCertificateChain;

class CertificateChainTest final : public QObject {
    Q_OBJECT

private slots:
    void describesEachCertificateFromTheServersToTheAnchor();
    void describesASelfSignedLocalDevelopmentCertificate();
    void skipsWhatIsNotACertificate();
};

// A chain as a public site's is: the server's own certificate, an
// intermediate, and the root that anchors it. The values are the ones
// `openssl x509` prints for the fixture.
void CertificateChainTest::describesEachCertificateFromTheServersToTheAnchor()
{
    const auto chain
        = QSslCertificate::fromPath(QStringLiteral(OMAWEB_CHAIN_CERTIFICATES_PATH), QSsl::Pem);
    QCOMPARE(chain.size(), 3);

    const auto described = describeCertificateChain(chain);
    QCOMPARE(described.size(), 3);

    const auto server = described.at(0).toMap();
    QCOMPARE(server.value(QStringLiteral("name")).toString(), QStringLiteral("chain.example"));
    QCOMPARE(server.value(QStringLiteral("subject")).toString(),
        QStringLiteral("CN=chain.example, O=Omaweb test"));
    QCOMPARE(server.value(QStringLiteral("issuer")).toString(),
        QStringLiteral("CN=Omaweb Test Intermediate, O=Omaweb test"));
    QCOMPARE(server.value(QStringLiteral("notBefore")).toString(),
        QStringLiteral("2026-09-26 08:44:49 UTC"));
    QCOMPARE(server.value(QStringLiteral("notAfter")).toString(),
        QStringLiteral("2295-01-19 08:44:49 UTC"));
    QCOMPARE(server.value(QStringLiteral("sha256")).toString(),
        QStringLiteral("CA:8A:4F:9C:AA:B5:AB:D3:11:7B:0E:82:B7:75:41:44:"
                       "52:82:1D:CD:36:FD:D7:96:36:B7:3E:6E:97:53:E3:4F"));
    // In the order the certificate lists them, which is the order a reader
    // comparing against another tool expects.
    QCOMPARE(server.value(QStringLiteral("subjectAlternativeNames")).toStringList(),
        QStringList({QStringLiteral("DNS:chain.example"), QStringLiteral("DNS:www.chain.example"),
            QStringLiteral("IP:127.0.0.1")}));
    QVERIFY(!server.value(QStringLiteral("selfSigned")).toBool());

    const auto intermediate = described.at(1).toMap();
    QCOMPARE(intermediate.value(QStringLiteral("name")).toString(),
        QStringLiteral("Omaweb Test Intermediate"));
    QVERIFY(intermediate.value(QStringLiteral("subjectAlternativeNames")).toStringList().isEmpty());
    QVERIFY(!intermediate.value(QStringLiteral("selfSigned")).toBool());

    const auto anchor = described.at(2).toMap();
    QCOMPARE(anchor.value(QStringLiteral("name")).toString(), QStringLiteral("Omaweb Test Root"));
    QCOMPARE(anchor.value(QStringLiteral("issuer")).toString(),
        anchor.value(QStringLiteral("subject")).toString());
    QCOMPARE(anchor.value(QStringLiteral("sha256")).toString(),
        QStringLiteral("F0:6A:56:7B:18:F7:E2:C6:77:87:9D:AC:77:E6:46:D2:"
                       "58:90:CC:4D:A2:FE:12:EE:72:DA:EB:39:A5:64:28:D5"));
    QVERIFY(anchor.value(QStringLiteral("selfSigned")).toBool());
}

// The certificate a Local-development site usually has: one certificate,
// signed by its own key, which is its own chain.
void CertificateChainTest::describesASelfSignedLocalDevelopmentCertificate()
{
    const auto chain
        = QSslCertificate::fromPath(QStringLiteral(OMAWEB_UNTRUSTED_CERTIFICATE_PATH), QSsl::Pem);
    QCOMPARE(chain.size(), 1);

    const auto described = describeCertificateChain(chain);
    QCOMPARE(described.size(), 1);
    const auto only = described.first().toMap();
    QCOMPARE(only.value(QStringLiteral("name")).toString(), QStringLiteral("localhost"));
    QCOMPARE(only.value(QStringLiteral("subject")).toString(),
        QStringLiteral("CN=localhost, O=Omaweb test"));
    QCOMPARE(only.value(QStringLiteral("issuer")).toString(),
        QStringLiteral("CN=localhost, O=Omaweb test"));
    QCOMPARE(only.value(QStringLiteral("notBefore")).toString(),
        QStringLiteral("2026-09-03 09:30:47 UTC"));
    QCOMPARE(only.value(QStringLiteral("notAfter")).toString(),
        QStringLiteral("2300-06-19 09:30:47 UTC"));
    QCOMPARE(only.value(QStringLiteral("sha256")).toString(),
        QString::fromLatin1(chain.first().digest(QCryptographicHash::Sha256).toHex(':').toUpper()));
    QCOMPARE(only.value(QStringLiteral("subjectAlternativeNames")).toStringList(),
        QStringList({QStringLiteral("DNS:localhost"), QStringLiteral("IP:127.0.0.1")}));
    QVERIFY(only.value(QStringLiteral("selfSigned")).toBool());
}

void CertificateChainTest::skipsWhatIsNotACertificate()
{
    QVERIFY(describeCertificateChain({}).isEmpty());
    QVERIFY(describeCertificateChain({QSslCertificate()}).isEmpty());
}

QTEST_GUILESS_MAIN(CertificateChainTest)

#include "tst_certificatechain.moc"
