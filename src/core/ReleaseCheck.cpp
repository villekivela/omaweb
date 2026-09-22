#include "ReleaseCheck.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QRegularExpression>
#include <QStringList>

namespace omaweb::ReleaseCheck {
namespace {

    constexpr qint64 secondsInADay = 24 * 60 * 60;

    // The numbers a version begins with, and nothing else. `v0.5.0`, `0.5.0` and
    // `0.5.0-14-gabc1234` all read as {0, 5, 0}: the description a build carries
    // past its tag says which commit it is, not which release, and comparing
    // releases is what this is for.
    //
    // Empty when there are no numbers to read, which is how the fallback version a
    // tagless tree builds with, and an answer from a rate-limited endpoint, both
    // end up saying nothing rather than saying something wrong.
    QList<int> versionNumbers(const QString &version)
    {
        auto text = version.trimmed();
        if (text.startsWith(QLatin1Char('v'))) {
            text.remove(0, 1);
        }
        text = text.section(QLatin1Char('-'), 0, 0);
        if (text.isEmpty()) {
            return {};
        }

        QList<int> numbers;
        const auto parts = text.split(QLatin1Char('.'));
        for (const auto &part : parts) {
            bool read = false;
            const int number = part.toInt(&read);
            if (!read || number < 0) {
                return {};
            }
            numbers.append(number);
        }
        return numbers;
    }

} // namespace

bool behind(const QString &runningVersion, const QString &releaseTag)
{
    const auto running = versionNumbers(runningVersion);
    const auto release = versionNumbers(releaseTag);
    if (running.isEmpty() || release.isEmpty()) {
        return false;
    }

    // Compared number by number rather than as text, because 10 sorts before 9
    // in a string and a reader would be left on an old browser for a year. A
    // version written with fewer parts than the other is read as zero there, so
    // `0.5` and `0.5.0` are the same release.
    const auto parts = std::max(running.size(), release.size());
    for (auto index = 0; index < parts; ++index) {
        const int mine = index < running.size() ? running.at(index) : 0;
        const int theirs = index < release.size() ? release.at(index) : 0;
        if (mine != theirs) {
            return mine < theirs;
        }
    }

    // The same release. A build past the tag has the release's work and more,
    // so there is nothing to offer it either way.
    return false;
}

QString newestRelease(const QByteArray &releasesAnswer)
{
    const auto document = QJsonDocument::fromJson(releasesAnswer);
    if (!document.isArray()) {
        return {};
    }

    // The endpoint answers newest first, so the first release a reader could
    // install is the answer. Prereleases are kept: every `v0.*` tag is one
    // (ADR 0028), and asking GitHub for `latest` instead would skip them all
    // and leave this project with no release at all to report.
    //
    // Not every release here is a release of the browser. The same repository
    // publishes the patched engine as `engine-6.11.2` and a pacman database
    // per architecture as `repo-aarch64`, both of them newer than any `v0.*`
    // tag. Stopping at the first tag would make the newest release a thing the
    // reader cannot install, and the notice would then say nothing for good
    // rather than say something wrong, because a tag with no version in it is
    // behind nothing. A release of the browser is one this can read a version
    // number out of.
    const auto releases = document.array();
    for (const auto &entry : releases) {
        const auto release = entry.toObject();
        if (release.value(QStringLiteral("draft")).toBool()) {
            continue;
        }
        const auto tag = release.value(QStringLiteral("tag_name")).toString();
        if (!versionNumbers(tag).isEmpty()) {
            return tag;
        }
    }
    return {};
}

QString releaseNumber(const QString &version)
{
    const auto numbers = versionNumbers(version);
    if (numbers.isEmpty()) {
        return {};
    }
    QStringList parts;
    parts.reserve(numbers.size());
    for (const auto number : numbers) {
        parts.append(QString::number(number));
    }
    return parts.join(QLatin1Char('.'));
}

QUrl notesPage(const QString &releaseTag)
{
    // The site builds a page per release at `releases/<tag>/`, for every tag it
    // can make a directory name of. The same rule is applied here rather than
    // assumed, so a tag that would have no page sends the reader to the list
    // rather than to a 404.
    static const QRegularExpression pageable(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    const auto releases = QStringLiteral("https://omaweb.app/releases/");
    if (!pageable.match(releaseTag).hasMatch()) {
        return QUrl(releases);
    }
    return QUrl(releases + releaseTag + QLatin1Char('/'));
}

bool due(const QDateTime &lastCheck, const QDateTime &now)
{
    if (!lastCheck.isValid()) {
        return true;
    }

    const auto elapsed = lastCheck.secsTo(now);
    // A last check in the future is a clock that has moved, on this machine or
    // on the one the configuration came from. Waiting for it to come round
    // again could be a very long silence, and the cost of asking is one
    // request.
    if (elapsed < 0) {
        return true;
    }
    return elapsed >= secondsInADay;
}

bool announce(const Question &question)
{
    // The Setting is the reader's answer about whether Omaweb may ask at all,
    // and a Private window says nothing about this installation — which its age
    // is.
    if (!question.checkEnabled || question.privateWindow) {
        return false;
    }
    if (!behind(question.runningVersion, question.newestRelease)) {
        return false;
    }

    // Dismissing is for one release rather than for the feature, so the next
    // one is announced. The two are compared as releases rather than as text:
    // a notice that came back because a tag was stored without its `v` is one
    // the reader could not get rid of.
    const auto dismissed = versionNumbers(question.dismissedRelease);
    if (!dismissed.isEmpty() && !behind(question.dismissedRelease, question.newestRelease)
        && !behind(question.newestRelease, question.dismissedRelease)) {
        return false;
    }
    return true;
}

Origin originOf(bool owned, bool foreign, const QString &packageName)
{
    if (!owned) {
        return Origin::Checkout;
    }
    if (!foreign) {
        return Origin::Repository;
    }
    // Foreign, so no repository offers it. A `-git` package is one makepkg
    // built from a checkout, which is pulled and built again rather than
    // downloaded.
    return packageName.endsWith(QStringLiteral("-git")) ? Origin::Checkout
                                                        : Origin::DownloadedPackage;
}

QString upgradeInstruction(Origin origin, const QString &releaseTag)
{
    switch (origin) {
    case Origin::Repository:
        return QStringLiteral("Upgrade with the rest of the system: pacman -Syu");
    case Origin::DownloadedPackage:
        return QStringLiteral(
            "Install %1 with pacman -U, or add the Omaweb repository to upgrade with the system.")
            .arg(releaseTag);
    case Origin::Checkout:
        break;
    }
    return QStringLiteral("This build came from a checkout. Pull %1 and build it again.")
        .arg(releaseTag);
}

} // namespace omaweb::ReleaseCheck
