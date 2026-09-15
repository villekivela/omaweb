#pragma once

#include "ReleaseCheck.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>
#include <QString>
#include <QUrl>

#include <optional>

namespace omaweb {

class BrowserController;

// Asks once a day what the newest Omaweb release is, and holds the answer for
// the outline footer to show. It never downloads, installs or replaces
// anything: pacman owns /usr, and a browser that rewrites its own binary is a
// security surface this project does not want.
//
// The decisions are all in ReleaseCheck, which needs no network to answer them.
// What is here is the request, the stored answer, and where the reader's
// instruction comes from.
class ReleaseWatch final : public QObject {
    Q_OBJECT
    // Whether Omaweb may ask at all. On by default: an alpha browser that
    // quietly ages is worse for the reader than one outbound request a day.
    Q_PROPERTY(bool checkEnabled READ checkEnabled WRITE setCheckEnabled NOTIFY changed)
    // Whether there is a release to tell the reader about. A Private window is
    // not asked about here, because one ReleaseWatch answers for every window;
    // the window applies that rule itself.
    Q_PROPERTY(bool announcing READ announcing NOTIFY changed)
    Q_PROPERTY(QString release READ release NOTIFY changed)
    Q_PROPERTY(QString instruction READ instruction NOTIFY changed)
    Q_PROPERTY(QUrl notes READ notes NOTIFY changed)

public:
    // The UI lab draws the chrome without an engine and without a network, so
    // it is built with a release to show and never asks GitHub for one.
    enum class Ask { GitHub, Never };

    explicit ReleaseWatch(QString runningVersion, Ask ask = Ask::GitHub, QObject *parent = nullptr);

    // The release to report, given rather than asked for. The lab reviews the
    // mark with it; nothing in the browser calls it.
    void showRelease(const QString &release);

    // Preferences are the browser's to keep, and it is not ready to answer for
    // them when this is built. Given the browser, the watch reads what it
    // remembered and asks if a check is owed.
    void follow(BrowserController *browser);

    bool checkEnabled() const;
    void setCheckEnabled(bool enabled);
    bool announcing() const;
    QString release() const;
    QString instruction() const;
    QUrl notes() const;

    // Stops this release being announced. The next one is announced again:
    // dismissing is for a release, not for the feature.
    Q_INVOKABLE void dismiss();

    // Asks, if a day has passed since the last answer. Silent about every way
    // it can fail — an unreachable endpoint is being offline, which is not a
    // browser fault and is not the reader's to read about.
    void checkIfDue();

signals:
    void changed();

private:
    QString preference(const QString &name, const QString &fallback = {}) const;
    void remember(const QString &name, const QString &value);
    // Asks pacman how this browser got here, once, and only when there is a
    // notice to put an instruction in. Two questions in sequence, then the
    // answer.
    void findOrigin();
    void askWhetherForeign(const QString &packageName);
    void settleOrigin(ReleaseCheck::Origin origin);

    QString m_runningVersion;
    Ask m_ask = Ask::GitHub;
    QString m_newestRelease;
    BrowserController *m_browser = nullptr;
    QNetworkAccessManager m_network;
    // A browser can be left running for a week. Without this the check would be
    // a startup check, and the reader who never quits would never be told.
    QTimer m_daily;
    bool m_asking = false;
    // Asked of pacman when there is a release to announce, rather than at
    // startup: a reader who is on the newest release never pays for it. Until
    // the answer arrives there is no instruction to give, which is a beat
    // rather than a state a reader sees.
    std::optional<ReleaseCheck::Origin> m_origin;
    bool m_askingOrigin = false;
};

} // namespace omaweb
