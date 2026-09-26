#pragma once

#include <QList>
#include <QSslCertificate>
#include <QVariantList>

namespace omaweb {

// A certificate chain as the certificate view states it, one map per
// certificate from the server's own first to the trust anchor last. Every
// adapter hands the shell this shape, so the view is drawn from one
// description whichever engine verified the chain.
//
// Each map carries:
// - "name": what the certificate is called in the chain, its common name
//   where it has one;
// - "subject" and "issuer": the distinguished names as "CN=..., O=..." in
//   the order Qt lists the attributes, which is not always the order the
//   certificate lists them in;
// - "notBefore" and "notAfter": the validity period in UTC, to the second;
// - "sha256": the SHA-256 fingerprint of the whole certificate, as colon
//   separated upper-case hex;
// - "subjectAlternativeNames": each name the certificate is valid for, with
//   the kind of name in front ("DNS:", "IP:", "email:");
// - "selfSigned": whether the certificate names itself as its issuer and its
//   own key signed it, which a Local-development site's certificate usually
//   does and a public site's own never should.
QVariantList describeCertificateChain(const QList<QSslCertificate> &chain);

} // namespace omaweb
