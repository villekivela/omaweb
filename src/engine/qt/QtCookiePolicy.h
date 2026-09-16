#pragma once

#include <QAtomicInt>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QPair>
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
    Q_INVOKABLE bool attachToProfile(QObject *profile, QObject *controller, const QString &spaceId);
    // How many third-party accesses have been refused since the browser
    // started, so the blocking can be seen to be doing something rather than
    // taken on trust.
    Q_INVOKABLE int refusedCount() const;
    // The third parties refused inside one page, so Site information can name
    // them rather than leaving the reader to guess which embedded flow is
    // failing. Bounded per page: this is a list to act on, not a log.
    Q_INVOKABLE QStringList refusedOrigins(const QUrl &firstParty) const;
    // Every cookie in one profile, gone. Chromium reaches its cookie store
    // through a plain C++ accessor rather than a property, so QML cannot ask
    // for this and the request comes through here — the one object that
    // already holds the store.
    Q_INVOKABLE bool deleteAllCookies(QObject *profile);
    // The origin a cookie access belongs to, in the shape the core's
    // allowances are keyed by.
    static QString cookieOrigin(const QUrl &url);
    // A view says which document it is showing: the address the load set out
    // from and the one it arrived at. The engine reports every access a
    // document makes against the address its load set out from, so a document
    // that arrived somewhere else through a cross-site redirect has its own
    // cookies reported as a third party's (ADR 0046). The policy judges such an
    // access against the arrival instead. One document per view, replaced by
    // the next and let go of with the view.
    Q_INVOKABLE void showDocument(QObject *view, const QUrl &setOutFrom, const QUrl &arrivedAt);
    // Whether a cookie for `origin` belongs to the site a document arrived at,
    // given the address its load set out from. Public for the test that pins
    // the rule down without a page.
    bool arrivedAtOwnSite(const QUrl &setOutFrom, const QUrl &origin) const;

private:
    struct Attachment {
        QPointer<BrowserController> controller;
        QString spaceId;
    };

    // The address a load set out from and the host it arrived at.
    using Document = QPair<QString, QString>;

    bool allows(const QString &spaceId, const QUrl &origin);
    void forgetDocument(QObject *view);
    void forgetArrival(const Document &document);
    void rememberRefusal(const QUrl &firstParty, const QUrl &origin);
    void refreshAllowances();

    QHash<QWebEngineCookieStore *, Attachment> m_attachments;
    mutable QMutex m_guard;
    // Keyed by Space, so nothing here has to know how the core spells a key.
    QHash<QString, QSet<QString>> m_allowed;
    QHash<QString, QStringList> m_refusedOrigins;
    // The document each view is showing, and the arrivals among them keyed by
    // the address the load set out from, which is how the engine names the
    // first party. Two views can arrive at one host from one address, so the
    // arrivals are a multi-hash and a view takes out only its own.
    QHash<QObject *, Document> m_documents;
    QMultiHash<QString, QString> m_arrivals;
    QAtomicInt m_refused;
};

} // namespace omaweb
