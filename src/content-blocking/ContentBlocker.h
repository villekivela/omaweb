#pragma once

#include "ContentMatcher.h"

#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include <memory>

namespace omaweb {

class ContentMatcher;

class ContentBlocker final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString userRules READ userRules WRITE setUserRules NOTIFY configurationChanged)
    Q_PROPERTY(QVariantList subscriptions READ subscriptions NOTIFY subscriptionsChanged)
    Q_PROPERTY(bool compiling READ compiling NOTIFY compilingChanged)
    Q_PROPERTY(QVariantMap compilationReport READ compilationReport NOTIFY rulesChanged)

public:
    // The default lists are what makes blocking work on a first run, so they
    // are the default. Tests and the engine-free UI lab pass None to keep a
    // fresh data directory from reaching the network for them.
    enum class DefaultLists { Seed, None };

    explicit ContentBlocker(
        QString dataRoot, DefaultLists defaults = DefaultLists::Seed, QObject *parent = nullptr);

    QString userRules() const;
    void setUserRules(const QString &rules);
    QVariantList subscriptions() const;
    bool compiling() const;
    QVariantMap compilationReport() const;

    Q_INVOKABLE QString addSubscription(const QString &title, const QUrl &source,
        const QString &license, const QUrl &updateAddress);
    Q_INVOKABLE void setSubscriptionEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void updateSubscription(const QString &id);
    Q_INVOKABLE void updateAllSubscriptions();
    Q_INVOKABLE void updateStaleSubscriptions();
    Q_INVOKABLE bool siteEnabled(const QUrl &url) const;
    Q_INVOKABLE void setSiteEnabled(const QUrl &url, bool enabled);
    Q_INVOKABLE int blockedRequestCount(const QUrl &url) const;
    Q_INVOKABLE QString cosmeticStyleSheet(const QUrl &url) const;
    Q_INVOKABLE QString scriptletSource(const QUrl &url) const;
    Q_INVOKABLE bool cosmeticSurveyWanted(const QUrl &url) const;
    Q_INVOKABLE QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids) const;

    // The window a page asked for is refused from QML, where the request
    // arrives, so unlike checkRequest this one is invokable.
    Q_INVOKABLE bool shouldBlockPopup(const QUrl &requestUrl, const QUrl &openerUrl) const;

    RequestDecision checkRequest(
        const QUrl &requestUrl, const QUrl &sourceUrl, const QString &resourceType) const;

signals:
    void configurationChanged();
    void subscriptionsChanged();
    void compilingChanged();
    void rulesChanged();
    void blockedRequestCountChanged(const QUrl &siteUrl);

private:
    struct Subscription {
        QString id;
        QString title;
        QUrl source;
        QString license;
        QUrl updateAddress;
        QString updateStatus;
        QString lastUpdated;
        bool enabled = true;
    };
    struct Runtime {
        std::shared_ptr<const ContentMatcher> matcher;
        QSet<QString> disabledSites;
    };

    static QString siteKey(const QUrl &url);
    std::shared_ptr<const ContentMatcher> matcherFor(const QUrl &siteUrl) const;
    QString settingsPath() const;
    QString listPath(const QString &id) const;
    void load();
    void seedDefaultSubscriptions();
    void countBlockedRequest(const QUrl &sourceUrl) const;
    void noteBlockedRequest(const QUrl &sourceUrl);
    void flushBlockedRequestCounts();
    void save() const;
    void recompile();
    void replaceDisabledSites();
    Subscription *findSubscription(const QString &id);

    QString m_dataRoot;
    DefaultLists m_defaultLists;
    QString m_userRules;
    QList<Subscription> m_subscriptions;
    QSet<QString> m_disabledSites;
    std::shared_ptr<const Runtime> m_runtime;
    QHash<QString, int> m_blockedCounts;
    // A busy page blocks hundreds of requests. Announcing each one separately
    // would make every open tab re-read its counter and re-run its bindings
    // hundreds of times over a single load, so the announcements are batched.
    QHash<QString, QUrl> m_pendingBlockedSites;
    QTimer m_blockedCountFlush;
    QNetworkAccessManager m_network;
    QVariantMap m_compilationReport;
    QStringList m_pendingCurrent;
    quint64 m_compileGeneration = 0;
    int m_activeCompilations = 0;
};

} // namespace omaweb
