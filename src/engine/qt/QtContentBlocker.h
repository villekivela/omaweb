#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>
#include <QWebEngineScript>
#include <QWebEngineUrlRequestInfo>

#include <atomic>
#include <map>
#include <memory>
#include <vector>

class QWebEngineUrlRequestInterceptor;
class QWebEngineUrlSchemeHandler;

namespace omaweb {

class ContentBlocker;
class GlobalPrivacyControl;
class HttpsOnly;
struct RequestDecision;

class QtContentBlocker final : public QObject {
    Q_OBJECT
    // The HTTPS-only policy the interceptor applies, for a view to ask what
    // became of a load it upgraded.
    Q_PROPERTY(QObject *httpsOnly READ httpsOnlyObject CONSTANT)

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
        QWebEngineUrlRequestInfo::ResourceType resourceType, const QString &spaceId,
        const QStringList &dnsAliases = {}) const;
    // Whether a request from this page is worth resolving for the names behind
    // its host. Content blocking answers.
    bool uncloaks(const QUrl &sourceUrl) const;
    QString cosmeticStyleSheet(const QUrl &url) const;
    bool cosmeticSurveyWanted(const QUrl &url) const;
    QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids) const;
    // Whether every request gets the `Sec-GPC: 1` header. Read on whichever
    // thread Chromium runs the interceptor on, hence the atomic.
    bool sendsGlobalPrivacyControl() const;
    // HTTPS-only mode rides the same interceptor: it is the one place a
    // page's own address can be changed before the request leaves. Given no
    // policy, the adapter upgrades nothing.
    void setHttpsOnly(HttpsOnly *httpsOnly);
    HttpsOnly *httpsOnly() const;
    QObject *httpsOnlyObject() const;

signals:
    // A profile attached for the first time. The interceptor is the one
    // thing every profile is handed to, so this is where the rest of what
    // rides a profile, a page's fonts for one, learns of it without a second
    // attachment path through the QML.
    void profileAttached(QObject *profile);

private:
    void applyGlobalPrivacyControl();
    void installGlobalPrivacyControlScript(QObject *profile, bool wanted) const;

    ContentBlocker *m_contentBlocker;
    const GlobalPrivacyControl *m_globalPrivacyControl;
    HttpsOnly *m_httpsOnly = nullptr;
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
};

} // namespace omaweb
