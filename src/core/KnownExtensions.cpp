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
            QStringLiteral(
                "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAmqKbvreshyXRuN2gikeR1idqR6KL0Di89JZcMy"
                "D4bjJRZVmQO7aznSGSALIHzSAUGYocUYBNDOP5QAhImxXyQ1qG8+goXs93v9GzrNJETdVuCEhqBggC4/"
                "DFabryJZDiKvZ2Jl0DM7MsWdoybZPwrj70V3aJ/"
                "nVNOMkf868scNTMliwitCqqjT5baTANsG0DkZWQExD4lSXzSZHH9MEO8q0iZ7RRlNuGRBAkZgNV8FwZRsP"
                "Km/rwQ9dy3VpgLcmLp5GiMt+kAEncqKAkuRYnhVXXBsKqIyYTMjHSLkLnpfFySyOPLBdS617i/PGNiP/"
                "MT6Xy6z//v5NozUgaAZ4gJQIDAQAB"),
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
            QStringLiteral("MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAnHpaUll4uWujpAdbIXOQY2WE6hk"
                           "8PllsYsnoUaj5qHXwv4IB6A9pONqGaTL2KL20u6E6XVhncY6Ae6SQSBQqiIkgjPsiG0NDNs"
                           "Dlju/kzBnfimKFC/bpzOrqFqbhswQHifnet5uHlpG97whTzLO3ka0M5aqB9V9mD/"
                           "0qVXvNgAVVnSTULH254YqpeCcAhmsKiFZSL6OrOZmCp8kZ/"
                           "OeOUK9iYWYylL7VcOXVrZf10EPrlaCNXzVk7K35dPuQ7svhA0Pgju3kngB4RLa5Iojhw3IT"
                           "+B5+m8pisjOSd1oKMrRmhGs7rDhF5IEtAiVxqVp7uOOMPQj3vrbMDAzf7vqLtQIDAQAB"),
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

KnownExtension knownExtensionByStoreId(const QString &storeId)
{
    for (const KnownExtension &extension : knownExtensions()) {
        if (extension.storeId == storeId) {
            return extension;
        }
    }
    return {};
}

} // namespace omaweb
