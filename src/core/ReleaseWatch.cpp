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

    // Whether a package owns the running binary is what decides the
    // instruction: a reader who installed the package upgrades with the system,
    // and a build from a checkout is rebuilt by whoever built it.
    //
    // Asked of pacman rather than guessed from where the binary sits, and asked
    // without waiting: this ends up in a QML binding, and a process that takes
    // its time would take the window with it.
    m_askingOrigin = true;
    auto *pacman = new QProcess(this);
    connect(
        pacman, &QProcess::finished, this, [this, pacman](int code, QProcess::ExitStatus status) {
            pacman->deleteLater();
            m_askingOrigin = false;
            // A machine with no pacman, and one where nothing owns the binary,
            // arrive here the same way and mean the same thing.
            m_origin = status == QProcess::NormalExit && code == 0 ? ReleaseCheck::Origin::Package
                                                                   : ReleaseCheck::Origin::Checkout;
            emit changed();
        });
    connect(pacman, &QProcess::errorOccurred, this, [this, pacman] {
        pacman->deleteLater();
        m_askingOrigin = false;
        m_origin = ReleaseCheck::Origin::Checkout;
        emit changed();
    });
    pacman->start(
        QStringLiteral("pacman"), {QStringLiteral("-Qo"), QCoreApplication::applicationFilePath()});
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
