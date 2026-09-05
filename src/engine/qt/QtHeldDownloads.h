#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace omaweb {

// The downloads Omaweb has taken off the engine while it asks the reader about
// them.
//
// QtWebEngine decides a download's fate inside the signal handler: a request
// that is neither accepted nor cancelled before the handler returns is thrown
// away, and a request that has been accepted can no longer be renamed. So there
// is no way to hold one open while a question is on screen. What there is, is
// the page that asked: cancelling the request costs nothing yet — not a byte
// has been written — and the same page can be asked for the same file again
// once the reader has answered.
//
// That is what this keeps: for each held download, the view whose page can be
// asked again and the address and name to ask for. It is deliberately little,
// because a held download is a question the reader may simply never answer, and
// everything else about it would then be kept for nothing.
class QtHeldDownloads final : public QObject {
    Q_OBJECT

public:
    explicit QtHeldDownloads(QObject *parent = nullptr);

    // Takes the engine's download away before anything is written, and returns
    // a token for asking again. Empty where there is nothing to ask again with
    // — a download with no page behind it — in which case the request is still
    // cancelled: the reader is told it was refused rather than left waiting for
    // a question nothing can answer.
    Q_INVOKABLE QString hold(QObject *download);

    // What was held: the `sourceUrl` and `fileName` the question names, and
    // the `view` whose page can be asked for it again. Empty for a token
    // nothing is held under, and for one whose view has since gone.
    Q_INVOKABLE QVariantMap held(const QString &token) const;

    // The reader answered, either way, or the question went away with its tab.
    Q_INVOKABLE bool discard(const QString &token);

    Q_INVOKABLE int heldCount() const;

private:
    struct HeldDownload {
        // The view may be gone by the time the reader answers — its tab closed,
        // its Space discarded — and then there is nothing to ask.
        QPointer<QObject> view;
        QUrl sourceUrl;
        QString fileName;
    };

    QHash<QString, HeldDownload> m_held;
};

} // namespace omaweb
