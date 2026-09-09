#pragma once

#include <QStringList>
#include <QUrl>

namespace omaweb {

// The address the desktop handed Omaweb to open, if it handed one over at all.
//
// This is how a browser is asked to be the default one: the desktop runs it
// with an address on the command line and expects that address on screen. What
// arrives is whatever the desktop was given, which is whatever a page, a mail
// client or another program put in front of the reader, so it is treated as
// untrusted rather than as an instruction from the person running the browser.
QUrl readLaunchUrl(const QStringList &arguments);

// True when the command line asks what this build is rather than for a page.
bool readVersionRequest(const QStringList &arguments);

// What that question is answered with. The engine's versions are here beside
// Omaweb's own because on Linux they are the distribution's rather than
// Omaweb's, so a bug report that names one without the other names half of
// what it was running.
QString versionReport(const QString &omaweb, const QString &engine, const QString &chromium);

} // namespace omaweb
