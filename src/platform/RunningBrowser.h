#pragma once

#include <QObject>
#include <QUrl>

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
    explicit RunningBrowser(QObject *parent = nullptr);
    ~RunningBrowser() override;

    // True when this process holds the desktop's claim, which is also the case
    // on a platform that has no way to tell: one browser that answers is better
    // than none.
    bool isPrimary() const;

    // Hands an address to the Omaweb already running, or asks it to come
    // forward when there is no address. False means nothing took it, and the
    // caller is better off opening the page itself than dropping it.
    bool handOver(const QUrl &url);

signals:
    // Another launch handed this process an address to open.
    void openRequested(const QUrl &url);
    // Another launch asked for the browser without naming an address.
    void activationRequested();

private:
    bool m_primary = true;
};

} // namespace omaweb
