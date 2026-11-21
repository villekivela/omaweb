#pragma once

#include <QObject>
#include <QUrl>

#include <chrono>

namespace omaweb {

// The one Omaweb the desktop talks to, and the way a later launch reaches it.
//
// A desktop opens a link by running the browser with an address on the command
// line, and it does that for every link. Omaweb is one window with its tabs
// down the side, so the answer to a second launch is a tab in the browser
// already running rather than a second browser beside it: two would each hold
// their own live tab state while writing the same session on disk, and the
// reader would have asked for neither.
//
// The first process to claim the desktop's name keeps it for as long as it
// runs. A later one finds the name taken, hands over what it was asked to open,
// and exits without building a browser at all.
class RunningBrowser final : public QObject {
    Q_OBJECT

public:
    // What became of a hand-over.
    enum class HandOver {
        // The running browser took it.
        Taken,
        // No browser holds the name, or the one that did let it go during the
        // wait, and the caller is better off opening the page itself than
        // dropping it.
        NoBrowser,
        // A browser still holds the name and has not answered. It has the
        // request and may yet act on it, so a second browser beside it would
        // open the page twice on the same profiles.
        Unanswered,
    };
    Q_ENUM(HandOver)

    explicit RunningBrowser(QObject *parent = nullptr);
    ~RunningBrowser() override;

    // True when this process holds the desktop's claim, which is also the case
    // on a platform that has no way to tell: one browser that answers is better
    // than none.
    bool isPrimary() const;

    // Hands an address to the Omaweb already running, or asks it to come
    // forward when there is no address, in a Private window when asked for
    // one. The request is sent once, and the wait ends when the browser
    // answers, when it lets go of the name, or after `patience`.
    HandOver handOver(const QUrl &url, bool privateWindow = false,
        std::chrono::milliseconds patience = kHandOverPatience);

    // Long enough to outlast a browser frozen while the machine is short of
    // memory, which answers once it catches up.
    static constexpr std::chrono::milliseconds kHandOverPatience = std::chrono::minutes(1);

signals:
    // Another launch handed this process an address to open.
    void openRequested(const QUrl &url);
    // Another launch asked for the browser to come forward, after any address
    // it handed over. The token is the one the compositor gave that launch to
    // come forward with, and is empty when it had none.
    void activationRequested(const QString &activationToken);
    // Another launch asked for a Private window, with an address to open in
    // it or none. The window that comes forward is that one, so the token
    // comes with the request rather than with activationRequested.
    void privateWindowRequested(const QUrl &url, const QString &activationToken);

private:
    bool m_primary = true;
};

} // namespace omaweb
