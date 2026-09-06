#include "LaunchRequest.h"

namespace omaweb {
namespace {

    // What a browser is for. Everything else a URL can name is either a way to
    // run code in a page that is already open, or a way to reach something that
    // is not the web, and neither is a thing the desktop gets to ask for from
    // outside. `javascript:` and `data:` are the ones that would run in whatever
    // page received them, and Omaweb's own scheme serves the browser's internal
    // substitutes, so an outside caller naming one is refused rather than
    // resolved.
    bool isOpenableScheme(const QString &scheme)
    {
        return scheme == QLatin1String("http") || scheme == QLatin1String("https")
            || scheme == QLatin1String("file");
    }

} // namespace

QUrl readLaunchUrl(const QStringList &arguments)
{
    // The first argument is the program. Everything else is either one of
    // Omaweb's own switches or the address the desktop is asking for, and the
    // first address wins: a desktop handing over several is asking for several
    // windows, and it gets them by running Omaweb again.
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const auto argument = arguments.at(index);
        if (argument.startsWith(QLatin1Char('-'))) {
            continue;
        }
        // Parsed strictly, so a string that only looks like an address is not
        // quietly repaired into one. A relative or schemeless argument is not
        // an address the desktop meant, and guessing turns a stray word on the
        // command line into a search nobody asked for.
        const QUrl url(argument, QUrl::StrictMode);
        if (!url.isValid() || url.isRelative() || !isOpenableScheme(url.scheme())) {
            continue;
        }
        return url;
    }
    return {};
}

} // namespace omaweb
