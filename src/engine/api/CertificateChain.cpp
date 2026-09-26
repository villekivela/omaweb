#include "CertificateChain.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QMultiMap>
#include <QStringList>
#include <QVariantMap>

namespace omaweb {
namespace {

    QString distinguishedName(const QSslCertificate &certificate, bool issuer)
    {
        const auto attributes
            = issuer ? certificate.issuerInfoAttributes() : certificate.subjectInfoAttributes();
        QStringList parts;
        for (const auto &attribute : attributes) {
            const auto values
                = issuer ? certificate.issuerInfo(attribute) : certificate.subjectInfo(attribute);
            for (const auto &value : values)
                parts.append(QString::fromLatin1(attribute) + QLatin1Char('=') + value);
        }
        return parts.join(QStringLiteral(", "));
    }

    QString utc(const QDateTime &moment)
    {
        return moment.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"));
    }

    // QMultiMap hands back the values for one key newest first, so each kind is
    // read in reverse to keep the order the certificate lists its names in.
    QStringList alternativeNames(const QSslCertificate &certificate)
    {
        const auto names = certificate.subjectAlternativeNames();
        const std::pair<QSsl::AlternativeNameEntryType, QString> kinds[] = {
            {QSsl::DnsEntry, QStringLiteral("DNS:")},
            {QSsl::IpAddressEntry, QStringLiteral("IP:")},
            {QSsl::EmailEntry, QStringLiteral("email:")},
        };
        QStringList described;
        for (const auto &[kind, prefix] : kinds) {
            const auto values = names.values(kind);
            for (auto value = values.crbegin(); value != values.crend(); ++value)
                described.append(prefix + *value);
        }
        return described;
    }

    QString nameOf(const QSslCertificate &certificate)
    {
        const auto common = certificate.subjectInfo(QSslCertificate::CommonName);
        if (!common.isEmpty())
            return common.first();
        const auto display = certificate.subjectDisplayName();
        return display.isEmpty() ? distinguishedName(certificate, false) : display;
    }

} // namespace

QVariantList describeCertificateChain(const QList<QSslCertificate> &chain)
{
    QVariantList described;
    for (const auto &certificate : chain) {
        if (certificate.isNull())
            continue;
        described.append(QVariantMap {
            {QStringLiteral("name"), nameOf(certificate)},
            {QStringLiteral("subject"), distinguishedName(certificate, false)},
            {QStringLiteral("issuer"), distinguishedName(certificate, true)},
            {QStringLiteral("notBefore"), utc(certificate.effectiveDate())},
            {QStringLiteral("notAfter"), utc(certificate.expiryDate())},
            {QStringLiteral("sha256"),
                QString::fromLatin1(
                    certificate.digest(QCryptographicHash::Sha256).toHex(':').toUpper())},
            {QStringLiteral("subjectAlternativeNames"), alternativeNames(certificate)},
            {QStringLiteral("selfSigned"), certificate.isSelfSigned()},
        });
    }
    return described;
}

} // namespace omaweb
