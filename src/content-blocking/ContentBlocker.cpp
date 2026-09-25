#include "ContentBlocker.h"

#include "ContentMatcher.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QPointer>
#include <QSaveFile>
#include <QUuid>
#include <QtConcurrentRun>

#include <algorithm>
#include <array>
#include <utility>

namespace omaweb {

// A list Omaweb names on its own. The seeded ones arrive on a first run; the
// rest wait to be asked for.
struct KnownList {
    QString id;
    QString title;
    QString license;
    QString updateAddress;
    bool seeded;
};

namespace {

    constexpr int startupUpdateDelayMilliseconds = 5000;
    // A busy page refuses hundreds of requests. Crediting each one separately
    // would make every reader of the tally re-run its bindings hundreds of
    // times over a single load, so the refusals are batched and delivered
    // together.
    constexpr int refusalFlushIntervalMilliseconds = 250;
    // How many refused addresses a tally lists. It counts every refusal; the
    // list only has to hold more than Site information shows.
    constexpr qsizetype refusedRequestLimit = 100;

    // Whether a request of this type is one an element is drawn by: an image,
    // a frame, a plugin's object, a video or an audio track. A refused script
    // or stylesheet leaves nothing in the layout to take out.
    bool drawnByAnElement(const QString &resourceType)
    {
        return resourceType == QLatin1String("image")
            || resourceType == QLatin1String("subdocument")
            || resourceType == QLatin1String("object") || resourceType == QLatin1String("media");
    }
    constexpr qint64 updateIntervalSeconds = 24 * 60 * 60;

    // Every list here is maintained beside EasyList at easylist.to and uses
    // only rule kinds this contract parses. The two seeded ones are what the
    // filter-list ecosystem is built around; docs/network-requests.md records
    // the startup requests they cost. The cookie list is known and not seeded:
    // it takes a site's consent choice, and that is the reader's to give away
    // (#291). Fanboy's Annoyances is broader and slower to match, and is not
    // offered.
    const QString easylistSource = QStringLiteral("https://easylist.to/");
    const QString easylistLicense = QStringLiteral("GPLv3 or CC BY-SA 3.0");
    const std::array<KnownList, 3> knownListTable = {{
        {QStringLiteral("easylist"), QStringLiteral("EasyList"), easylistLicense,
            QStringLiteral("https://easylist.to/easylist/easylist.txt"), true},
        {QStringLiteral("easyprivacy"), QStringLiteral("EasyPrivacy"), easylistLicense,
            QStringLiteral("https://easylist.to/easylist/easyprivacy.txt"), true},
        {QStringLiteral("easylist-cookie"), QStringLiteral("EasyList Cookie"),
            QStringLiteral("CC BY 3.0"),
            QStringLiteral("https://secure.fanboy.co.nz/fanboy-cookiemonster.txt"), false},
    }};

// Requests are matched on whichever thread the engine hands them to, while a
// finished compile replaces the rule set from this object's thread, so the
// pointer to the active set is published and read atomically rather than
// assigned.
//
// C++20 deprecated the free atomic_load/atomic_store for shared_ptr in favour
// of std::atomic<std::shared_ptr<T>>, which libc++ does not implement — the
// static_assert there rejects any type that is not trivially copyable, and a
// shared_ptr is not. macOS is the development platform, so the replacement is
// unavailable on the build that has to work. The deprecation is answered once,
// here, rather than at each of the call sites, and these two functions are
// what to delete when libc++ carries the replacement.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    template <typename T> std::shared_ptr<T> loadSnapshot(const std::shared_ptr<T> *slot)
    {
        return std::atomic_load(slot);
    }

    template <typename T> void storeSnapshot(std::shared_ptr<T> *slot, std::shared_ptr<T> snapshot)
    {
        std::atomic_store(slot, std::move(snapshot));
    }

#pragma GCC diagnostic pop

} // namespace

ContentBlocker::ContentBlocker(QString dataRoot, DefaultLists defaults, QObject *parent)
    : QObject(parent)
    , m_dataRoot(std::move(dataRoot))
    , m_defaultLists(defaults)
{
    storeSnapshot(&m_runtime, std::make_shared<const Runtime>());
    m_refusalFlush.setSingleShot(true);
    m_refusalFlush.setInterval(refusalFlushIntervalMilliseconds);
    connect(&m_refusalFlush, &QTimer::timeout, this, &ContentBlocker::flushRefusals);
    load();
    recompile();
    // Refreshing lists competes with the windows and pages coming up, and a
    // list that is hours old blocks just as well as one fetched this second,
    // so the check waits until the browser is on screen and doing nothing.
    QTimer::singleShot(
        startupUpdateDelayMilliseconds, this, &ContentBlocker::updateStaleSubscriptions);
}

int ContentBlocker::refusalTallyGeneration() const { return m_refusalTallyGeneration; }

// A tally belongs to a page address, not to a host: every tab on a host used
// to report what the whole host refused, including tabs on entirely different
// pages of it (ADR 0037). The fragment comes off, because a fragment jump is
// the same document and the tally follows the document. This is not siteKey,
// which keys the per-site switch and stays per-host: conflating the two would
// make one of them wrong.
ContentBlocker::RefusalKey ContentBlocker::refusalKey(
    const QString &spaceId, const QUrl &pageAddress)
{
    return RefusalKey {
        spaceId, pageAddress.adjusted(QUrl::RemoveFragment).toString(QUrl::FullyEncoded)};
}

void ContentBlocker::noteRefusal(
    const RefusalKey &key, const RefusedRequest &request, const QString &elementAddress)
{
    auto &pending = m_pendingRefusals[key];
    pending.count += 1;
    pending.requests.append(request);
    if (!elementAddress.isEmpty()) {
        pending.elementAddresses.append(elementAddress);
    }
    if (!m_refusalFlush.isActive()) {
        m_refusalFlush.start();
    }
}

// A refusal counts towards a page load that is open. One pending for an
// address no view is showing any more is dropped rather than kept: the live
// tallies are the open page loads, and a table of everything ever refused
// would be a record of everywhere the reader has been.
void ContentBlocker::flushRefusals()
{
    m_refusalFlush.stop();
    const auto pending = std::exchange(m_pendingRefusals, {});
    auto moved = false;
    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        const auto tally = m_refusalTallies.find(it.key());
        if (tally == m_refusalTallies.end()) {
            continue;
        }
        tally->refused += it->count;
        for (const auto &request : it->requests) {
            const auto listed
                = std::ranges::any_of(tally->requests, [&request](const RefusedRequest &known) {
                      return known.address == request.address;
                  });
            if (!listed && tally->requests.size() < refusedRequestLimit) {
                tally->requests.append(request);
            }
        }
        moved = true;
        if (it->elementAddresses.isEmpty()) {
            continue;
        }
        for (auto viewed = m_viewedPages.cbegin(); viewed != m_viewedPages.cend(); ++viewed) {
            if (viewed->tally == it.key()) {
                emit elementsRefused(viewed.key(), it->elementAddresses);
            }
        }
    }
    if (moved) {
        ++m_refusalTallyGeneration;
        emit refusalTallyGenerationChanged();
    }
}

void ContentBlocker::showPage(
    QObject *view, const QString &spaceId, const QUrl &pageAddress, int pageGeneration)
{
    if (!view) {
        return;
    }
    const auto key = refusalKey(spaceId, pageAddress);
    const auto viewed = m_viewedPages.constFind(view);
    const auto known = viewed != m_viewedPages.cend();
    const auto sameKey = known && viewed->tally == key;
    // A new load, rather than the same one arriving at a new address. Only a
    // load starts the tally again: a redirect the load resolved to moves the
    // view to another address, and a fragment jump does not even do that.
    const auto newLoad = !known || viewed->pageGeneration != pageGeneration;
    if (sameKey && !newLoad) {
        return;
    }
    // Everything the outgoing document earned is delivered before its tally is
    // let go of or started again, so a refusal it earned lands on it and not
    // on the document that follows.
    flushRefusals();
    if (known) {
        releasePage(view);
    } else {
        connect(view, &QObject::destroyed, this, [this, view] { releasePage(view); });
    }
    auto &tally = m_refusalTallies[key];
    tally.viewers += 1;
    if (newLoad) {
        tally.refused = 0;
        tally.requests.clear();
    }
    m_viewedPages.insert(view, ViewedPage {key, pageGeneration});
    ++m_refusalTallyGeneration;
    emit refusalTallyGenerationChanged();
}

// A view that navigates away or goes away lets go of the tally it held. The
// last holder takes the tally with it, so nothing accumulates for pages that
// are no longer open.
void ContentBlocker::releasePage(QObject *view)
{
    const auto viewed = m_viewedPages.constFind(view);
    if (viewed == m_viewedPages.cend()) {
        return;
    }
    const auto key = viewed->tally;
    m_viewedPages.erase(viewed);
    const auto tally = m_refusalTallies.find(key);
    if (tally == m_refusalTallies.end()) {
        return;
    }
    tally->viewers -= 1;
    if (tally->viewers <= 0) {
        m_refusalTallies.erase(tally);
        m_pendingRefusals.remove(key);
    }
    ++m_refusalTallyGeneration;
    emit refusalTallyGenerationChanged();
}

int ContentBlocker::refusalTally(const QString &spaceId, const QUrl &pageAddress) const
{
    return m_refusalTallies.value(refusalKey(spaceId, pageAddress)).refused;
}

QVariantList ContentBlocker::refusedRequests(const QString &spaceId, const QUrl &pageAddress) const
{
    QVariantList listed;
    for (const auto &request : m_refusalTallies.value(refusalKey(spaceId, pageAddress)).requests) {
        listed.append(QVariantMap {
            {QStringLiteral("address"), request.address},
            {QStringLiteral("canonicalName"), request.canonicalName},
        });
    }
    return listed;
}

// A subscription refreshed within the last day is left alone. Re-downloading
// and recompiling every list on every launch cost a network round trip and a
// full rule compilation for no change in what gets blocked.
void ContentBlocker::updateStaleSubscriptions()
{
    const auto now = QDateTime::currentDateTimeUtc();
    for (const auto &subscription : std::as_const(m_subscriptions)) {
        if (!subscription.enabled) {
            continue;
        }
        const auto updated = QDateTime::fromString(subscription.lastUpdated, Qt::ISODate);
        if (updated.isValid() && updated.secsTo(now) < updateIntervalSeconds
            && QFileInfo::exists(listPath(subscription.id))) {
            continue;
        }
        updateSubscription(subscription.id);
    }
}

QString ContentBlocker::userRules() const { return m_userRules; }

void ContentBlocker::setUserRules(const QString &rules)
{
    if (rules == m_userRules) {
        return;
    }
    m_userRules = rules;
    save();
    emit configurationChanged();
    recompile();
}

QVariantList ContentBlocker::subscriptions() const
{
    QVariantList result;
    for (const auto &subscription : m_subscriptions) {
        result.append(QVariantMap {
            {QStringLiteral("id"), subscription.id},
            {QStringLiteral("title"), subscription.title},
            {QStringLiteral("source"), subscription.source},
            {QStringLiteral("license"), subscription.license},
            {QStringLiteral("updateAddress"), subscription.updateAddress},
            {QStringLiteral("updateStatus"), subscription.updateStatus},
            {QStringLiteral("lastUpdated"), subscription.lastUpdated},
            {QStringLiteral("enabled"), subscription.enabled},
        });
    }
    return result;
}

// The seeded lists are not here even when they are gone: an install with no
// subscriptions is offered them back as a pair by restoreDefaultSubscriptions,
// and listing them here as well would offer the same thing twice.
QVariantList ContentBlocker::knownLists() const
{
    QVariantList result;
    for (const auto &list : knownListTable) {
        if (list.seeded || std::ranges::any_of(m_subscriptions, [&](const auto &subscription) {
                return subscription.id == list.id;
            })) {
            continue;
        }
        result.append(QVariantMap {
            {QStringLiteral("id"), list.id},
            {QStringLiteral("title"), list.title},
            {QStringLiteral("source"), QUrl(easylistSource)},
            {QStringLiteral("license"), list.license},
            {QStringLiteral("updateAddress"), QUrl(list.updateAddress)},
        });
    }
    return result;
}

bool ContentBlocker::compiling() const { return m_activeCompilations > 0; }

QVariantMap ContentBlocker::compilationReport() const { return m_compilationReport; }

QString ContentBlocker::addSubscription(
    const QString &title, const QUrl &source, const QString &license, const QUrl &updateAddress)
{
    if (title.trimmed().isEmpty() || !source.isValid() || license.trimmed().isEmpty()
        || !updateAddress.isValid()) {
        return {};
    }
    Subscription subscription;
    subscription.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    subscription.title = title.trimmed();
    subscription.source = source;
    subscription.license = license.trimmed();
    subscription.updateAddress = updateAddress;
    subscription.updateStatus = QStringLiteral("not updated");
    const auto id = subscription.id;
    m_subscriptions.append(std::move(subscription));
    save();
    emit subscriptionsChanged();
    updateSubscription(id);
    return id;
}

void ContentBlocker::setSubscriptionEnabled(const QString &id, bool enabled)
{
    auto *subscription = findSubscription(id);
    if (!subscription || subscription->enabled == enabled) {
        return;
    }
    subscription->enabled = enabled;
    save();
    emit subscriptionsChanged();
    recompile();
}

void ContentBlocker::updateSubscription(const QString &id)
{
    auto *subscription = findSubscription(id);
    if (!subscription || !subscription->enabled) {
        return;
    }
    subscription->updateStatus = QStringLiteral("updating");
    emit subscriptionsChanged();
    auto *reply = m_network.get(QNetworkRequest(subscription->updateAddress));
    connect(reply, &QNetworkReply::finished, this, [this, reply, id] {
        auto *current = findSubscription(id);
        if (!current) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            current->updateStatus = QStringLiteral("failed: %1").arg(reply->errorString());
        } else {
            const auto candidate = reply->readAll();
            current->updateStatus = QStringLiteral("validating");
            ++m_activeCompilations;
            emit compilingChanged();
            auto *watcher = new QFutureWatcher<MatcherCompilation>(this);
            connect(watcher, &QFutureWatcher<MatcherCompilation>::finished, this,
                [this, watcher, id, candidate] {
                    const auto validation = watcher->result();
                    watcher->deleteLater();
                    --m_activeCompilations;
                    emit compilingChanged();
                    auto *subscription = findSubscription(id);
                    if (!subscription) {
                        return;
                    }
                    const auto accepted
                        = validation.report.value(QStringLiteral("acceptedRuleCount")).toInt();
                    const auto invalid
                        = validation.report.value(QStringLiteral("invalidRuleCount")).toInt();
                    // Every published list carries a handful of rules this
                    // contract cannot parse: EasyList alone has one. Refusing
                    // a list over those rejected EasyList and EasyPrivacy in
                    // full and blocked nothing at all, so a list is kept for
                    // the rules that did compile, and refused only when none
                    // did or when the unparsable rules outnumber them.
                    if (!validation.matcher || accepted == 0 || invalid > accepted) {
                        subscription->updateStatus
                            = QStringLiteral("failed: list has no usable rules");
                        save();
                        emit subscriptionsChanged();
                        return;
                    }
                    QDir().mkpath(QFileInfo(listPath(id)).absolutePath());
                    QSaveFile file(listPath(id));
                    if (!file.open(QIODevice::WriteOnly) || file.write(candidate) < 0
                        || !file.commit()) {
                        subscription->updateStatus = QStringLiteral("failed: could not store list");
                        save();
                        emit subscriptionsChanged();
                        return;
                    }
                    subscription->updateStatus = QStringLiteral("compiling");
                    subscription->lastUpdated
                        = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
                    m_pendingCurrent.append(id);
                    save();
                    emit subscriptionsChanged();
                    recompile();
                });
            watcher->setFuture(QtConcurrent::run(
                [candidate] { return ContentMatcher::compile(QString::fromUtf8(candidate)); }));
        }
        save();
        emit subscriptionsChanged();
        reply->deleteLater();
    });
}

void ContentBlocker::updateAllSubscriptions()
{
    for (const auto &subscription : std::as_const(m_subscriptions)) {
        if (subscription.enabled) {
            updateSubscription(subscription.id);
        }
    }
}

bool ContentBlocker::siteEnabled(const QUrl &url) const
{
    return !loadSnapshot(&m_runtime)->disabledSites.contains(siteKey(url));
}

void ContentBlocker::setSiteEnabled(const QUrl &url, bool enabled)
{
    const auto key = siteKey(url);
    if (key.isEmpty()) {
        return;
    }
    if (enabled) {
        m_disabledSites.remove(key);
    } else {
        m_disabledSites.insert(key);
    }
    replaceDisabledSites();
    save();
    emit configurationChanged();
}

// The rules in force for one site: the compiled set, unless there is none yet
// or the user turned blocking off here, in which case there are no rules and
// every caller below is finished before it starts.
std::shared_ptr<const ContentMatcher> ContentBlocker::matcherFor(const QUrl &siteUrl) const
{
    const auto runtime = loadSnapshot(&m_runtime);
    if (!runtime->matcher || runtime->disabledSites.contains(siteKey(siteUrl))) {
        return {};
    }
    return runtime->matcher;
}

QString ContentBlocker::cosmeticStyleSheet(const QUrl &url) const
{
    const auto matcher = matcherFor(url);
    return matcher ? matcher->cosmeticStyleSheet(url) : QString();
}

// A scriptlet is list-named code running in the page, so the per-site switch
// has to reach it: matcherFor answers with nothing for a site the user turned
// blocking off, and this page then runs none.
QString ContentBlocker::scriptletSource(const QUrl &url) const
{
    const auto matcher = matcherFor(url);
    return matcher ? matcher->scriptletSource(url) : QString();
}

bool ContentBlocker::cosmeticSurveyWanted(const QUrl &url) const
{
    const auto matcher = matcherFor(url);
    return matcher && matcher->cosmeticSurveyWanted(url);
}

QString ContentBlocker::genericCosmeticStyleSheet(
    const QUrl &url, const QStringList &classes, const QStringList &ids) const
{
    const auto matcher = matcherFor(url);
    return matcher ? matcher->genericCosmeticStyleSheet(url, classes, ids) : QString();
}

// A refused request and a replaced one are the same refusal to the page that
// asked, so both land in that page's tally. A request the lists only stripped
// parameters off was never refused and does not.
RequestDecision ContentBlocker::checkRequest(const QUrl &requestUrl, const QUrl &sourceUrl,
    const QString &resourceType, const QString &spaceId, const QStringList &dnsAliases) const
{
    const auto matcher = matcherFor(sourceUrl);
    if (!matcher) {
        return {};
    }
    auto decision = matcher->check(requestUrl, sourceUrl, resourceType);
    // Every name in the chain, because the engine does not say which one is
    // canonical: Chromium keeps a host's aliases in a sorted set.
    for (const auto &alias : dnsAliases) {
        if (decision.blocked) {
            break;
        }
        const auto uncloaked = matcher->checkUncloaked(requestUrl, sourceUrl, resourceType, alias);
        if (uncloaked.blocked) {
            decision = uncloaked;
            decision.canonicalName = alias;
        }
    }
    if (decision.blocked) {
        countRefusal(sourceUrl, spaceId,
            {requestUrl.toString(QUrl::FullyEncoded), decision.canonicalName},
            drawnByAnElement(resourceType) ? requestUrl : QUrl());
    }
    return decision;
}

// A window the page never got to open is a request the page never got to
// make, so it lands in the same tally as the rest and the number keeps
// meaning one thing.
bool ContentBlocker::uncloaks(const QUrl &sourceUrl) const
{
    return matcherFor(sourceUrl) != nullptr;
}

bool ContentBlocker::shouldBlockPopup(
    const QUrl &requestUrl, const QUrl &openerUrl, const QString &spaceId) const
{
    const auto matcher = matcherFor(openerUrl);
    if (!matcher || !matcher->shouldBlockPopup(requestUrl, openerUrl)) {
        return false;
    }
    countRefusal(openerUrl, spaceId, {requestUrl.toString(QUrl::FullyEncoded), {}});
    return true;
}

// Requests are matched on whichever thread the engine hands them to, and the
// tallies belong to this object's thread. The element's address, where the
// request had one, travels in the form the page names it in, so the view has
// nothing to translate.
void ContentBlocker::countRefusal(const QUrl &sourceUrl, const QString &spaceId,
    const RefusedRequest &request, const QUrl &elementAddress) const
{
    QPointer<ContentBlocker> guard(const_cast<ContentBlocker *>(this));
    const auto key = refusalKey(spaceId, sourceUrl);
    const auto address = elementAddress.toString(QUrl::FullyEncoded);
    QMetaObject::invokeMethod(
        const_cast<ContentBlocker *>(this),
        [guard, key, request, address] {
            if (guard) {
                guard->noteRefusal(key, request, address);
            }
        },
        Qt::QueuedConnection);
}

QString ContentBlocker::siteKey(const QUrl &url) { return url.host().toLower(); }

QString ContentBlocker::settingsPath() const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("content-blocking/settings.json"));
}

QString ContentBlocker::listPath(const QString &id) const
{
    return QDir(m_dataRoot).filePath(QStringLiteral("content-blocking/lists/%1.txt").arg(id));
}

// The stored record for a known list, which is the same record a list the
// reader typed in gets, under the id the table gives it rather than a fresh
// one: an install that kept the list, or one migrating from before the seeded
// marker, must not end up subscribed to it twice.
bool ContentBlocker::appendKnownList(const KnownList &list)
{
    if (findSubscription(list.id)) {
        return false;
    }
    Subscription subscription;
    subscription.id = list.id;
    subscription.title = list.title;
    subscription.source = QUrl(easylistSource);
    subscription.license = list.license;
    subscription.updateAddress = QUrl(list.updateAddress);
    subscription.updateStatus = QStringLiteral("not updated");
    m_subscriptions.append(std::move(subscription));
    return true;
}

// A browser whose blocking stays off until the user types four fields of list
// provenance blocks nothing for almost everyone, so the seeded lists arrive on
// a first run and Settings can disable either one.
void ContentBlocker::seedDefaultSubscriptions()
{
    for (const auto &list : knownListTable) {
        if (list.seeded) {
            appendKnownList(list);
        }
    }
    m_seeded = true;
    save();
}

// The one way a known list that is not seeded arrives. Like a list the reader
// typed in, it is fetched as soon as it is subscribed.
void ContentBlocker::subscribeKnownList(const QString &id)
{
    const auto list = std::ranges::find(knownListTable, id, &KnownList::id);
    if (list == knownListTable.end() || !appendKnownList(*list)) {
        return;
    }
    save();
    emit subscriptionsChanged();
    updateSubscription(id);
}

// Seeding on its own only reaches the stored subscriptions, because load()
// compiles and fetches after it. Asked for from Settings it has to do both.
void ContentBlocker::restoreDefaultSubscriptions()
{
    const auto before = m_subscriptions.size();
    seedDefaultSubscriptions();
    if (m_subscriptions.size() == before) {
        return;
    }
    emit subscriptionsChanged();
    recompile();
    updateStaleSubscriptions();
}

void ContentBlocker::load()
{
    QFile file(settingsPath());
    if (!file.open(QIODevice::ReadOnly)) {
        if (m_defaultLists == DefaultLists::Seed) {
            seedDefaultSubscriptions();
        }
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    // A file that does not parse says nothing either way, and the seeding
    // below would write over it. Whatever the reader configured is still in
    // there, and a truncated write is repairable by hand only while it is.
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    const auto root = document.object();
    m_seeded = root.value(QStringLiteral("seeded")).toBool(false);
    m_userRules = root.value(QStringLiteral("userRules")).toString();
    for (const auto &value : root.value(QStringLiteral("disabledSites")).toArray()) {
        m_disabledSites.insert(value.toString());
    }
    for (const auto &value : root.value(QStringLiteral("subscriptions")).toArray()) {
        const auto object = value.toObject();
        Subscription subscription;
        subscription.id = object.value(QStringLiteral("id")).toString();
        subscription.title = object.value(QStringLiteral("title")).toString();
        subscription.source = QUrl(object.value(QStringLiteral("source")).toString());
        subscription.license = object.value(QStringLiteral("license")).toString();
        subscription.updateAddress = QUrl(object.value(QStringLiteral("updateAddress")).toString());
        subscription.updateStatus = object.value(QStringLiteral("updateStatus")).toString();
        subscription.lastUpdated = object.value(QStringLiteral("lastUpdated")).toString();
        subscription.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        if (!subscription.id.isEmpty()) {
            m_subscriptions.append(std::move(subscription));
        }
    }
    // A file written before the marker existed was never told it had been
    // seeded, whatever it lists. Seeding once more is the migration; after
    // that the marker answers, and an empty list stays empty.
    if (!m_seeded && m_defaultLists == DefaultLists::Seed) {
        seedDefaultSubscriptions();
    }
}

void ContentBlocker::reloadSyncedConfiguration()
{
    m_subscriptions.clear();
    m_disabledSites.clear();
    load();
    emit configurationChanged();
    emit subscriptionsChanged();
    recompile();
    updateStaleSubscriptions();
}

void ContentBlocker::save() const
{
    QJsonArray subscriptions;
    for (const auto &subscription : m_subscriptions) {
        subscriptions.append(QJsonObject {
            {QStringLiteral("id"), subscription.id},
            {QStringLiteral("title"), subscription.title},
            {QStringLiteral("source"), subscription.source.toString()},
            {QStringLiteral("license"), subscription.license},
            {QStringLiteral("updateAddress"), subscription.updateAddress.toString()},
            {QStringLiteral("updateStatus"), subscription.updateStatus},
            {QStringLiteral("lastUpdated"), subscription.lastUpdated},
            {QStringLiteral("enabled"), subscription.enabled},
        });
    }
    QJsonArray disabledSites;
    for (const auto &site : m_disabledSites) {
        disabledSites.append(site);
    }
    QDir().mkpath(QFileInfo(settingsPath()).absolutePath());
    QSaveFile file(settingsPath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(QJsonDocument(QJsonObject {
                                 {QStringLiteral("version"), 1},
                                 {QStringLiteral("seeded"), m_seeded},
                                 {QStringLiteral("userRules"), m_userRules},
                                 {QStringLiteral("disabledSites"), disabledSites},
                                 {QStringLiteral("subscriptions"), subscriptions},
                             })
            .toJson(QJsonDocument::Indented));
    file.commit();
}

void ContentBlocker::recompile()
{
    QString rules = m_userRules;
    for (const auto &subscription : std::as_const(m_subscriptions)) {
        if (!subscription.enabled) {
            continue;
        }
        QFile file(listPath(subscription.id));
        if (file.open(QIODevice::ReadOnly)) {
            rules += QLatin1Char('\n') + QString::fromUtf8(file.readAll());
        }
    }
    const auto generation = ++m_compileGeneration;
    ++m_activeCompilations;
    emit compilingChanged();
    auto *watcher = new QFutureWatcher<MatcherCompilation>(this);
    connect(
        watcher, &QFutureWatcher<MatcherCompilation>::finished, this, [this, watcher, generation] {
            const auto compilation = watcher->result();
            watcher->deleteLater();
            --m_activeCompilations;
            emit compilingChanged();
            if (generation != m_compileGeneration || !compilation.matcher) {
                return;
            }
            auto runtime = std::make_shared<Runtime>();
            runtime->matcher = compilation.matcher;
            runtime->disabledSites = m_disabledSites;
            storeSnapshot(&m_runtime, std::shared_ptr<const Runtime>(std::move(runtime)));
            m_compilationReport = compilation.report.toVariantMap();
            for (const auto &id : std::as_const(m_pendingCurrent)) {
                if (auto *subscription = findSubscription(id)) {
                    subscription->updateStatus = QStringLiteral("current");
                }
            }
            m_pendingCurrent.clear();
            save();
            emit subscriptionsChanged();
            emit rulesChanged();
        });
    watcher->setFuture(
        QtConcurrent::run([rules = std::move(rules)] { return ContentMatcher::compile(rules); }));
}

void ContentBlocker::replaceDisabledSites()
{
    const auto current = loadSnapshot(&m_runtime);
    auto replacement = std::make_shared<Runtime>();
    replacement->matcher = current->matcher;
    replacement->disabledSites = m_disabledSites;
    storeSnapshot(&m_runtime, std::shared_ptr<const Runtime>(std::move(replacement)));
}

ContentBlocker::Subscription *ContentBlocker::findSubscription(const QString &id)
{
    for (auto &subscription : m_subscriptions) {
        if (subscription.id == id) {
            return &subscription;
        }
    }
    return nullptr;
}

} // namespace omaweb
