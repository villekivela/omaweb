#include "ReleaseWatch.h"

#include "BrowserController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>

#include <chrono>

#include <utility>

namespace omaweb {
namespace {

    // The releases of this repository, newest first. Read in full rather than
    // through the `latest` endpoint, which leaves prereleases out and would answer
    // nothing for a project whose every release so far is one (ADR 0028).
    //
    // Ten is more than a reader can be behind by without the first of them being
    // the answer, and asking for fewer costs GitHub and this browser the same.
    constexpr auto releasesEndpoint
        = "https://api.github.com/repos/villekivela/omaweb/releases?per_page=10";

    // What Omaweb remembers between runs. The names are stored settings and are not
    // renamed without a migration.
    constexpr auto enabledKey = "release-check";
    constexpr auto lastCheckKey = "release-check-last";
    constexpr auto newestKey = "release-check-newest";
    constexpr auto dismissedKey = "release-check-dismissed";

} // namespace

ReleaseWatch::ReleaseWatch(QString runningVersion, Ask ask, QObject *parent)
    : QObject(parent)
    , m_runningVersion(std::move(runningVersion))
    , m_ask(ask)
{
    // Hourly, and `due` turns all but one of those into nothing. The alternative
    // is a timer set to the exact remainder of a day, which a machine that
    // sleeps through it gets wrong.
    m_daily.setInterval(std::chrono::hours(1));
    connect(&m_daily, &QTimer::timeout, this, &ReleaseWatch::checkIfDue);
}

void ReleaseWatch::showRelease(const QString &release)
{
    m_newestRelease = release;
    if (announcing()) {
        findOrigin();
    }
    emit changed();
}

void ReleaseWatch::follow(BrowserController *browser)
{
    m_browser = browser;
    // Settings are not the only thing that writes one: Sync restores them, and
    // clearing browsing data rewrites them. Without this the mark and the
    // Settings switch would go on showing what they read at startup.
    if (m_browser) {
        connect(
            m_browser, &BrowserController::preferenceChanged, this, [this](const QString &name) {
                if (name.startsWith(QStringLiteral("release-check"))) {
                    emit changed();
                }
            });
    }
    // The newest release as of the last answer, so a restart shows what the
    // reader was already being told without asking again for it.
    m_newestRelease = preference(QString::fromLatin1(newestKey));
    if (announcing()) {
        findOrigin();
    }
    emit changed();
    checkIfDue();
    if (m_ask == Ask::GitHub) {
        m_daily.start();
    }
}

QString ReleaseWatch::preference(const QString &name, const QString &fallback) const
{
    return m_browser ? m_browser->preference(name, fallback) : fallback;
}

void ReleaseWatch::remember(const QString &name, const QString &value)
{
    if (m_browser) {
        m_browser->setPreference(name, value);
    }
}

bool ReleaseWatch::checkEnabled() const
{
    return preference(QString::fromLatin1(enabledKey), QStringLiteral("true"))
        == QStringLiteral("true");
}

void ReleaseWatch::setCheckEnabled(bool enabled)
{
    remember(QString::fromLatin1(enabledKey),
        enabled ? QStringLiteral("true") : QStringLiteral("false"));
    emit changed();
    if (enabled) {
        checkIfDue();
    }
}

bool ReleaseWatch::announcing() const
{
    return ReleaseCheck::announce({.checkEnabled = checkEnabled(),
        .privateWindow = false,
        .runningVersion = m_runningVersion,
        .newestRelease = m_newestRelease,
        .dismissedRelease = preference(QString::fromLatin1(dismissedKey))});
}

QString ReleaseWatch::release() const { return m_newestRelease; }

QString ReleaseWatch::instruction() const
{
    // Read from a binding, so it waits for nothing. Until pacman has answered
    // there is no instruction, and the answer arrives as a change like any
    // other.
    if (!m_origin.has_value()) {
        return {};
    }
    return ReleaseCheck::upgradeInstruction(*m_origin, m_newestRelease);
}

void ReleaseWatch::findOrigin()
{
    if (m_origin.has_value() || m_askingOrigin) {
        return;
    }

    // Two questions, because one is not enough. `pacman -Qo` says whether a
    // package owns the running binary; `pacman -Qm` says whether that package
    // is one no repository carries. A reader who installed a downloaded release
    // by hand answers yes to both, and telling them to run `pacman -Syu` would
    // be telling them to watch a system upgrade pass their browser over.
    //
    // Asked without waiting: this ends up in a QML binding, and a process that
    // takes its time would take the window with it.
    m_askingOrigin = true;
    auto *owner = new QProcess(this);
    connect(owner, &QProcess::finished, this, [this, owner](int code, QProcess::ExitStatus status) {
        owner->deleteLater();
        const auto owned = status == QProcess::NormalExit && code == 0;
        if (!owned) {
            // No pacman, or nothing owns the binary. Both mean a build run from
            // where it was built.
            settleOrigin(ReleaseCheck::originOf(false, false, {}));
            return;
        }
        // "/usr/bin/omaweb is owned by omaweb 0.5.0-1", whose second-to-last
        // word is the name the next question needs.
        const auto answer = QString::fromUtf8(owner->readAllStandardOutput()).trimmed();
        const auto words = answer.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (words.size() < 2) {
            settleOrigin(ReleaseCheck::originOf(false, false, {}));
            return;
        }
        askWhetherForeign(words.at(words.size() - 2));
    });
    connect(owner, &QProcess::errorOccurred, this, [this, owner] {
        owner->deleteLater();
        settleOrigin(ReleaseCheck::Origin::Checkout);
    });
    owner->start(
        QStringLiteral("pacman"), {QStringLiteral("-Qo"), QCoreApplication::applicationFilePath()});
}

void ReleaseWatch::askWhetherForeign(const QString &packageName)
{
    auto *foreign = new QProcess(this);
    connect(foreign, &QProcess::finished, this,
        [this, foreign, packageName](int code, QProcess::ExitStatus status) {
            foreign->deleteLater();
            // `pacman -Qm <name>` succeeds for a package no repository carries
            // and fails for one a repository does.
            const auto unlistedAnywhere = status == QProcess::NormalExit && code == 0;
            settleOrigin(ReleaseCheck::originOf(true, unlistedAnywhere, packageName));
        });
    connect(foreign, &QProcess::errorOccurred, this, [this, foreign, packageName] {
        foreign->deleteLater();
        // pacman answered the first question and not the second. A package that
        // is owned is at least installed, so the reader is told what to install
        // rather than told to rebuild something they never built.
        settleOrigin(ReleaseCheck::originOf(true, true, packageName));
    });
    foreign->start(QStringLiteral("pacman"), {QStringLiteral("-Qm"), packageName});
}

void ReleaseWatch::settleOrigin(ReleaseCheck::Origin origin)
{
    m_askingOrigin = false;
    m_origin = origin;
    emit changed();
}

QUrl ReleaseWatch::notes() const
{
    if (m_newestRelease.isEmpty()) {
        return {};
    }
    return ReleaseCheck::notesPage(m_newestRelease);
}

void ReleaseWatch::dismiss()
{
    if (m_newestRelease.isEmpty()) {
        return;
    }
    remember(QString::fromLatin1(dismissedKey), m_newestRelease);
    emit changed();
}

void ReleaseWatch::checkIfDue()
{
    if (!checkEnabled() || m_asking || m_ask == Ask::Never) {
        return;
    }

    const auto last
        = QDateTime::fromString(preference(QString::fromLatin1(lastCheckKey)), Qt::ISODate);
    if (!ReleaseCheck::due(last, QDateTime::currentDateTimeUtc())) {
        return;
    }

    QNetworkRequest request {QUrl(QString::fromLatin1(releasesEndpoint))};
    request.setRawHeader("Accept", "application/vnd.github+json");
    // GitHub refuses a request that names no client. It is the browser's own
    // name and version, which the endpoint would see in any case, and nothing
    // about this machine or what is browsed with it.
    request.setRawHeader(
        "User-Agent", QStringLiteral("Omaweb/%1").arg(m_runningVersion).toLatin1());

    m_asking = true;
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_asking = false;
        // Every failure is the same silence. A machine with no network is not
        // a browser fault, and a reader who is offline has nothing to do about
        // an answer that did not arrive.
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }

        const auto newest = ReleaseCheck::newestRelease(reply->readAll());
        if (newest.isEmpty()) {
            return;
        }

        // The day is counted from an answer rather than from an attempt, so a
        // spell offline does not spend the reader's one check a day on
        // nothing.
        remember(QString::fromLatin1(lastCheckKey),
            QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        if (newest != m_newestRelease) {
            m_newestRelease = newest;
            remember(QString::fromLatin1(newestKey), newest);
        }
        if (announcing()) {
            findOrigin();
        }
        emit changed();
    });
}

} // namespace omaweb
