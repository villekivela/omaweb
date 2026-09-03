#pragma once

#include <QAtomicInt>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QUrl>

class QWebEngineCookieStore;

namespace omaweb {

class BrowserController;

// Third-party cookies are blocked, and this is the mechanics of it.
//
// Whether an origin may act as a third party inside a Space is the core's
// decision: it holds the temporary allowances a sign-in or a payment is given
// and takes them back. Chromium asks its question on the network thread, where
// nothing may touch a controller that lives on the interface's, so the answer
// is copied here whenever the reader changes it and read from the copy.
// Chromium's cookie filter also governs DOM storage, IndexedDB, the filesystem
// API and service workers, so refusing a third party here refuses it the whole
// of a site's state and not only its cookies.
class QtCookiePolicy final : public QObject {
    Q_OBJECT

public:
    explicit QtCookiePolicy(QObject *parent = nullptr);
    ~QtCookiePolicy() override;

    // One Space's profile, the controller whose allowances govern it, and the
    // Space those allowances belong to. A Private window has no Space of its
    // own and passes the empty name its shared session already keys on.
    Q_INVOKABLE bool attachToProfile(QObject *profile, QObject *controller,
        const QString &spaceId);
    // How many third-party accesses have been refused since the browser
    // started, so the blocking can be seen to be doing something rather than
    // taken on trust.
    Q_INVOKABLE int refusedCount() const;
    // The third parties refused inside one page, so Site information can name
    // them rather than leaving the reader to guess which embedded flow is
    // failing. Bounded per page: this is a list to act on, not a log.
    Q_INVOKABLE QStringList refusedOrigins(const QUrl &firstParty) const;
    // The origin a cookie access belongs to, in the shape the core's
    // allowances are keyed by.
    static QString cookieOrigin(const QUrl &url);

private:
    struct Attachment {
        QPointer<BrowserController> controller;
        QString spaceId;
    };

    bool allows(const QString &spaceId, const QUrl &origin);
    void rememberRefusal(const QUrl &firstParty, const QUrl &origin);
    void refreshAllowances();

    QHash<QWebEngineCookieStore *, Attachment> m_attachments;
    mutable QMutex m_guard;
    QSet<QString> m_allowed;
    QHash<QString, QStringList> m_refusedOrigins;
    QAtomicInt m_refused;
};

} // namespace omaweb
