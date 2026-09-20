#include "KnownExtensions.h"

namespace omaweb {

QList<KnownExtension> knownExtensions()
{
    return {
        KnownExtension {
            QStringLiteral("bitwarden"),
            QStringLiteral("Bitwarden Password Manager"),
            QStringLiteral("Bitwarden, Inc."),
            QStringLiteral("GPL-3.0-or-later"),
            QStringLiteral("https://github.com/bitwarden/clients"),
            QStringLiteral("nngceckbapebfimnlniiiahkandclblb"),
            QStringLiteral("Fills and saves passwords and passkeys from a Bitwarden vault. "
                           "Reads and writes the pages you use it on."),
        },
        KnownExtension {
            QStringLiteral("1password"),
            QStringLiteral("1Password"),
            QStringLiteral("AgileBits Inc."),
            // Named rather than softened. Omaweb says where an extension came
            // from and under what terms, and for this one the terms are the
            // publisher's own and the source is not published.
            QStringLiteral("Proprietary"),
            QStringLiteral("https://1password.com/downloads/browser-extension"),
            QStringLiteral("aeblfdkhhhdcdjpifhhbdiojplfjncoa"),
            QStringLiteral("Fills and saves passwords and passkeys from a 1Password account. "
                           "Reads and writes the pages you use it on, and talks to the "
                           "1Password desktop application."),
        },
    };
}

KnownExtension knownExtension(const QString &key)
{
    for (const KnownExtension &extension : knownExtensions()) {
        if (extension.key == key) {
            return extension;
        }
    }
    return {};
}

} // namespace omaweb
