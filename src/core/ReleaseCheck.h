#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

// What Omaweb decides about its own age. The decisions are here, apart from the
// request that answers them, because every one of them is a question with a
// right answer that can be asked without a network: whether a version is behind
// a release, whether today's check has already happened, and what a reader is
// told to run.
namespace omaweb::ReleaseCheck {

// How the running browser got onto this machine, which is what decides the
// instruction. A packaged build upgrades with the system; a build from a
// checkout is rebuilt by whoever built it.
enum class Origin { Package, Checkout };

// Whether the running version is older than the release the tag names. A build
// carrying a commit count is past the tag it is named for, so it is never
// behind that tag. A version either side cannot read is not an upgrade the
// reader is missing, and answers false.
bool behind(const QString &runningVersion, const QString &releaseTag);

// The tag of the newest release in an answer from the releases endpoint, or an
// empty string when the answer carries none. Drafts are not releases a reader
// can install and are passed over.
QString newestRelease(const QByteArray &releasesAnswer);

// The release number a version begins with, as `x.y.z`, with the description a
// build carries past its tag left off. Empty when there is no version to read.
QString releaseNumber(const QString &version);

// Whether a check is owed. At most one a day, counted from the last one that
// finished.
bool due(const QDateTime &lastCheck, const QDateTime &now);

// What the notice tells the reader to do about the release.
QString upgradeInstruction(Origin origin, const QString &releaseTag);

// Everything that decides whether the reader is told about a release, gathered
// so that the decision can be asked as one question rather than assembled from
// five conditions at the place it is needed.
struct Question {
    bool checkEnabled = true;
    bool privateWindow = false;
    QString runningVersion;
    QString newestRelease;
    QString dismissedRelease;
};

// Whether there is a release to tell the reader about.
bool announce(const Question &question);

} // namespace omaweb::ReleaseCheck
