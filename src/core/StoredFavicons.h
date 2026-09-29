#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <functional>

class QObject;

namespace omaweb::storedfavicons {

// How the interface reaches the favicons a window's store keeps: an
// `image://omaweb-favicon/<source>/<space>/<page>` address, answered by an
// image provider on the QML engine. The source names the window whose store
// answers, so a Private window's address never reaches a Space's store, and a
// Space's address never reaches another Space's.
//
// Nothing here makes a network request. A page the store has no icon for
// answers empty, and the interface draws the site's host code instead.
inline constexpr auto providerId = "omaweb-favicon";

using Answer = std::function<void(const QByteArray &image)>;
// Asks a window's store, from whichever thread the interface loads images on,
// and answers later on a thread of the window's choosing.
using Lookup = std::function<void(const QString &spaceId, const QUrl &pageUrl, Answer answer)>;
// Reads the icon a page reported, from wherever the QML engine already
// reaches it, as encoded image bytes. Answers on `context`'s thread, and not
// at all once `context` is gone.
using Reader = std::function<void(const QUrl &iconUrl, QObject *context, Answer answer)>;

// Registers a window's store and names it. The window removes it before it
// lets go of the store.
QString addSource(Lookup lookup);
void removeSource(const QString &source);

// The address the interface draws a page's stored favicon from, or an empty
// one for an address that is not a web page.
QUrl address(const QString &source, const QString &spaceId, const QUrl &pageUrl);

// Answers the favicon for an image provider's identifier: the address above
// without its scheme and provider. Answers exactly once: empty at once for an
// identifier that names no registered source, and empty whenever the window
// goes before its store has answered.
void find(const QString &identifier, Answer answer);

// The reader is the interface's: the engine that can reach a page's icon is
// the QML engine. Until one is installed, nothing is read and nothing kept.
void setReader(Reader reader);
void read(const QUrl &iconUrl, QObject *context, Answer answer);

} // namespace omaweb::storedfavicons
