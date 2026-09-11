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
    // How many times a Refusal tally has moved. A tally is keyed by page
    // address and Space, so it is asked for rather than bound to; reading this
    // beside the question is what makes the answer arrive again when it
    // changes.
    Q_PROPERTY(
        int refusalTallyGeneration READ refusalTallyGeneration NOTIFY refusalTallyGenerationChanged)

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
    // Settings offers the default lists back when there are none. Seeding is
    // otherwise a one-off, so this is the only way they return.
    Q_INVOKABLE void restoreDefaultSubscriptions();
    void reloadSyncedConfiguration();
    Q_INVOKABLE bool siteEnabled(const QUrl &url) const;
    Q_INVOKABLE void setSiteEnabled(const QUrl &url, bool enabled);
    int refusalTallyGeneration() const;
    // A view says which document it is showing, and each load of one carries a
    // generation of its own. A tally belongs to one page address in one Space:
    // the view takes the tally for the address it arrives at, and a load newer
    // than the last one announced starts that tally again at zero. An address
    // that differs only in its fragment is the same document, so its tally
    // carries on.
    //
    // Whatever the document being replaced earned is delivered to its own
    // tally before the view lets go of it, so a refusal is never credited to
    // the document that follows. A view that goes away lets go of its tally
    // too, which leaves the live tallies the page loads that are open.
    Q_INVOKABLE void showPage(
        QObject *view, const QString &spaceId, const QUrl &pageAddress, int pageGeneration);
    // What Content blocking has refused for one page address in one Space.
    // Zero for an address no view is showing, which is also the answer for one
    // that has refused nothing.
    Q_INVOKABLE int refusalTally(const QString &spaceId, const QUrl &pageAddress) const;
    Q_INVOKABLE QString cosmeticStyleSheet(const QUrl &url) const;
    Q_INVOKABLE QString scriptletSource(const QUrl &url) const;
    Q_INVOKABLE bool cosmeticSurveyWanted(const QUrl &url) const;
    Q_INVOKABLE QString genericCosmeticStyleSheet(
        const QUrl &url, const QStringList &classes, const QStringList &ids) const;

    // The window a page asked for is refused from QML, where the request
    // arrives, so unlike checkRequest this one is invokable.
    Q_INVOKABLE bool shouldBlockPopup(
        const QUrl &requestUrl, const QUrl &openerUrl, const QString &spaceId) const;

    RequestDecision checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
        const QString &resourceType, const QString &spaceId) const;

signals:
    void configurationChanged();
    void subscriptionsChanged();
    void compilingChanged();
    void rulesChanged();
    void refusalTallyGenerationChanged();

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
    // One page address in one Space, in the shape the tallies are kept under.
    // The fragment is off the address: a fragment jump is the same document,
    // and the tally follows the document.
    struct RefusalKey {
        QString spaceId;
        QString address;

        bool operator==(const RefusalKey &other) const = default;

        friend size_t qHash(const RefusalKey &key, size_t seed = 0)
        {
            return qHashMulti(seed, key.spaceId, key.address);
        }
    };
    // What one open page load has been refused, and how many views are showing
    // it. Two tabs on the same address in the same Space read one tally, and
    // it stays live until the last of them lets go (ADR 0037).
    struct RefusalTally {
        int refused = 0;
        int viewers = 0;
    };
    // The tally one view is holding open, and the page load it last said so
    // at. The page load is what tells a reload from a redirect: both arrive
    // without a load of their own to distinguish them.
    struct ViewedPage {
        RefusalKey tally;
        int pageGeneration = 0;
    };
    struct Runtime {
        std::shared_ptr<const ContentMatcher> matcher;
        QSet<QString> disabledSites;
    };

    static QString siteKey(const QUrl &url);
    static RefusalKey refusalKey(const QString &spaceId, const QUrl &pageAddress);
    void releasePage(QObject *view);
    std::shared_ptr<const ContentMatcher> matcherFor(const QUrl &siteUrl) const;
    QString settingsPath() const;
    QString listPath(const QString &id) const;
    void load();
    void seedDefaultSubscriptions();
    void countRefusal(const QUrl &sourceUrl, const QString &spaceId) const;
    void noteRefusal(const RefusalKey &key);
    void flushRefusals();
    void save() const;
    void recompile();
    void replaceDisabledSites();
    Subscription *findSubscription(const QString &id);

    QString m_dataRoot;
    DefaultLists m_defaultLists;
    // Whether this install has ever been offered the default lists. Recorded
    // rather than inferred: a settings file that lists nothing used to be
    // indistinguishable from one that had never been seeded, so an install
    // that reached that state blocked nothing for ever (#43).
    bool m_seeded = false;
    QString m_userRules;
    QList<Subscription> m_subscriptions;
    QSet<QString> m_disabledSites;
    std::shared_ptr<const Runtime> m_runtime;
    // Refusals waiting to be credited, and the batch that delivers them.
    QHash<RefusalKey, int> m_pendingRefusals;
    QTimer m_refusalFlush;
    QHash<RefusalKey, RefusalTally> m_refusalTallies;
    // Kept here rather than in the view: what a load does to a tally is
    // Content blocking's invariant, and a line in one adapter is not reachable
    // from a test of it (#142).
    QHash<QObject *, ViewedPage> m_viewedPages;
    int m_refusalTallyGeneration = 0;
    QNetworkAccessManager m_network;
    QVariantMap m_compilationReport;
    QStringList m_pendingCurrent;
    quint64 m_compileGeneration = 0;
    int m_activeCompilations = 0;
};

} // namespace omaweb
