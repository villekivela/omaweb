#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QWebEngineScript>
#include <QWebEngineUrlRequestInfo>

#include <atomic>
#include <map>
#include <memory>
#include <utility>
#include <vector>

class QWebEngineUrlRequestInterceptor;
class QWebEngineUrlSchemeHandler;

namespace omaweb {

class ContentBlocker;
class GlobalPrivacyControl;
struct RequestDecision;

class QtContentBlocker final : public QObject {
    Q_OBJECT

public:
    // The scheme a substitute is served under. Chromium refuses to redirect a
    // request to a `data:` URL, and it has to learn about a scheme of its own
    // before it starts, so this runs before QtWebEngineQuick::initialize().
    static void registerSubstituteScheme();

    // Global Privacy Control rides the same attachment: the interceptor is
    // the one thing that sees every request a profile makes, and a profile is
    // the one thing every page of a Space or a Private window shares. Given
    // no control, the adapter sends nothing.
    explicit QtContentBlocker(ContentBlocker *contentBlocker,
        const GlobalPrivacyControl *globalPrivacyControl = nullptr, QObject *parent = nullptr);
    ~QtContentBlocker() override;

    // One Space's profile and the Space it belongs to. A Private window has no
    // Space of its own and passes the empty name its shared session already
    // keys on, the same way QtCookiePolicy is told.
    Q_INVOKABLE bool attachToProfile(QObject *profile, const QString &spaceId);
    RequestDecision checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
        QWebEngineUrlRequestInfo::ResourceType resourceType, const QString &spaceId) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids) const;
    // Whether every request gets the `Sec-GPC: 1` header. Read on whichever
    // thread Chromium runs the interceptor on, hence the atomic.
    bool sendsGlobalPrivacyControl() const;
    // A request an element made was refused, or answered with a substitute.
    // The interceptor knows the address and the page knows the element, and
    // this is where the two are joined: the address is queued for the page it
    // was refused on, and requestsRefused delivers the batch. Called on
    // whichever thread the engine hands requests to.
    void noteRefusedElement(
        const QString &spaceId, const QUrl &pageAddress, const QUrl &requestUrl) const;

signals:
    // The addresses refused for one page since the last delivery, for the
    // view showing that page to take the elements that asked for them out of
    // the layout. Keyed the way the Refusal tally is, by Space and page
    // address without its fragment, and batched for the same reason: a page
    // load refuses hundreds of requests, and a script round trip per refusal
    // in every open tab is what the batching spares (ADR 0037).
    void requestsRefused(
        const QString &spaceId, const QUrl &pageAddress, const QStringList &addresses);
    // A profile attached for the first time. The interceptor is the one
    // thing every profile is handed to, so this is where the rest of what
    // rides a profile, a page's fonts for one, learns of it without a second
    // attachment path through the QML.
    void profileAttached(QObject *profile);

private:
    void applyGlobalPrivacyControl();
    void installGlobalPrivacyControlScript(QObject *profile, bool wanted) const;
    void flushRefusedElements();

    ContentBlocker *m_contentBlocker;
    const GlobalPrivacyControl *m_globalPrivacyControl;
    std::atomic<bool> m_sendGlobalPrivacyControl {false};
    // The script that defines `navigator.globalPrivacyControl`, one instance
    // so that the collection it was inserted into can be asked to remove it.
    QWebEngineScript m_globalPrivacyControlScript;
    // Every profile attached so far, because turning the signal off has to
    // reach the script already installed on each of them. A profile that has
    // been destroyed reads as null and is skipped.
    std::vector<QPointer<QObject>> m_profiles;
    // An interceptor for each Space, because the interceptor is the only thing
    // that sees a request and Chromium tells it nothing about where the
    // request came from. Interception attaches to a profile and a profile
    // belongs to a Space (ADR 0037), so the Space is what the interceptor
    // carries and one shared instance cannot.
    std::map<QString, std::unique_ptr<QWebEngineUrlRequestInterceptor>> m_interceptors;
    std::unique_ptr<QWebEngineUrlSchemeHandler> m_substitutes;
    // The refused addresses waiting to be delivered, per Space and page
    // address, and the batch that delivers them.
    QHash<std::pair<QString, QString>, QStringList> m_pendingRefusedElements;
    QTimer m_refusedElementFlush;
};

} // namespace omaweb
