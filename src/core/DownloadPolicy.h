#pragma once

#include <QString>

namespace omaweb::DownloadPolicy {

// What a file about to be written to the reader's disk is, where being wrong
// about it costs more than a document.
//
// A browser hands over two sorts of file. One is read by something the reader
// chooses to open it with; the other is run, installed, or mounted, and the
// desktop is willing to do that on a double-click. Omaweb asks before writing
// the second sort down, so the question is which sort this is — named as a word
// the reader can be asked about rather than a boolean they cannot see.
//
// The name decides wherever it can, because the name is what lands on disk and
// what the desktop reads when the reader opens it later; a server's declared
// type only answers where the name has nothing to say. Both are attacker-
// supplied, so neither is trusted to argue the other down: a `.exe` stays an
// executable however politely it was served.
//
// One of "executable", "script", "installer", "disk image" or "archive", or an
// empty string for an everyday document.
QString riskKind(const QString &fileName, const QString &mimeType);

// Whether Omaweb asks before this one is written down at all.
bool highRisk(const QString &fileName, const QString &mimeType);

} // namespace omaweb::DownloadPolicy
