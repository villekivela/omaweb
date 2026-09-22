#pragma once

#include "KnownExtensions.h"

#include <QByteArray>
#include <QString>
#include <QUrl>

namespace omaweb {

// A Chrome Web Store package, read and put on disk under the identity its
// publisher signed it with.
//
// The store is the only source that carries a publisher's signature, and that
// signature is the whole point: the id the vendor's own desktop application
// allows is the first sixteen bytes of the SHA-256 of the publisher's key, so a
// package that keeps the key keeps the identity, and one assembled any other
// way is refused by the vendor rather than by us (ADR 0049,
// `docs/research/password-manager-extensions.md`).
//
// What is trusted here is the key pinned in the catalogue, and nothing else. A
// package is accepted only if it is signed by that exact key, so neither the
// network, nor the store, nor a mirror can put something else on disk under a
// name Omaweb offers. The store is asked over TLS, but the answer is not taken
// on TLS's word.
class ExtensionPackage final {
public:
    struct Result {
        bool ok = false;
        // What to tell the reader, already a sentence. Empty when `ok`.
        QString error;
    };

    // Where the store serves this extension's package. Chromium's own update
    // endpoint, asked for a package this version of Chromium accepts.
    static QUrl storeAddress(const QString &storeId);

    // Where the store answers what version it is offering, without sending the
    // package itself. A freshness check that downloaded twenty megabytes to
    // discover nothing had changed would be a reason not to check.
    static QUrl updateAddress(const QString &storeId);

    // The version in an update answer, or an empty string when the answer says
    // nothing Omaweb understands. The store answers in Omaha's XML.
    static QString versionOffered(const QByteArray &answer);

    // The version of the package unpacked at `path`, or an empty string when
    // there is none there.
    static QString versionInstalled(const QString &path);

    // Verify `crx` against the extension's pinned key and unpack it into
    // `destination`, which is emptied first. The manifest is written back with
    // the publisher's key in it, which is what makes the engine load the
    // unpacked directory under the store id rather than under one derived from
    // its path.
    //
    // Every failure leaves `destination` absent rather than half-written: an
    // extension that is partly on disk is one the engine would try to load.
    static Result install(
        const QByteArray &crx, const KnownExtension &extension, const QString &destination);

    // The id the store lists for this key, which is the first sixteen bytes of
    // its SHA-256 written in Chromium's own alphabet. Exposed because the
    // catalogue's id and key have to agree, and a test should be able to say so.
    static QString identityFor(const QByteArray &publisherKey);
};

} // namespace omaweb
