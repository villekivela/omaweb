#pragma once

#include <QList>
#include <QString>

namespace omaweb {

// The Web extensions Omaweb names on its own (ADR 0049). A reader enables one
// in Settings; they cannot point Omaweb at a package of their own, because what
// makes a Known extension knowable is that Omaweb tested it and says where it
// came from.
//
// The list is short and deliberate. Omaweb promises the extensions it names,
// not Chromium extensions in general: the engine patches implement namespaces
// as Chromium defines them, so an untested extension may well run, but nothing
// here offers one.
struct KnownExtension {
    // What the reader enables, and the directory the package is unpacked into.
    // Stable across releases, because it names a stored package.
    QString key;
    QString name;
    // What the reader is told before they enable it: who publishes it, under
    // what licence, and what it reaches. A Known extension a reader cannot
    // judge is one Omaweb should not be naming.
    QString publisher;
    QString licence;
    QString homepage;
    // Where the package comes from, and the identity it has to keep. A store
    // package is signed by its publisher, and the id derived from that key is
    // what the publisher's own desktop application allows; a package that loses
    // it is refused by the vendor rather than by us.
    QString storeId;
    QString summary;
};

// Every Known extension, in the order Settings lists them.
QList<KnownExtension> knownExtensions();

// The one with this key, or a default-constructed entry when Omaweb names no
// such extension. A key that is not in the list is not an error to report to
// the reader: it is a preference left behind by a build that named more than
// this one does.
KnownExtension knownExtension(const QString &key);

} // namespace omaweb
