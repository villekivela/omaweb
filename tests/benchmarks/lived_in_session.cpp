// Seeds the session of the lived-in profile `scripts/benchmark_runtime.py livedin` launches the
// browser on: two Spaces of a reader's day, written through the store the browser reads, so the
// benchmark never writes the schema by hand.
//
// Usage: omaweb-lived-in-session <data root> <address the pages are served from>

#include "PutAwayTab.h"
#include "SpaceListModel.h"
#include "SqliteSessionStore.h"
#include "TabListModel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QUrl>

#include <cstdio>

using omaweb::PutAwayTab;
using omaweb::SpaceState;
using omaweb::SqliteSessionStore;
using omaweb::TabState;

namespace {

// The shape of the profile #618 was reported from, counted rather than copied: what each Space
// holds open, closed and put away, and how many visits its history keeps.
struct SpaceShape {
    const char *id;
    const char *name;
    const char *color;
    int openTabs;
    int closedTabs;
    int putAwayTabs;
    int visits;
};

constexpr SpaceShape personal {"lived-in-personal", "Personal", "green", 2, 0, 0, 26};
constexpr SpaceShape work {"lived-in-work", "Work", "blue", 1, 4, 9, 80};

QUrl page(const QString &base, const QString &path) { return QUrl(base).resolved(QUrl(path)); }

bool seed(SqliteSessionStore &store, const SpaceShape &shape, bool active, const QString &base)
{
    const auto spaceId = QString::fromLatin1(shape.id);
    SpaceState space;
    space.id = spaceId;
    space.name = QString::fromLatin1(shape.name);
    space.color = QString::fromLatin1(shape.color);
    space.active = active;
    if (!store.saveSpace(space)) {
        return false;
    }

    // The first tab is the active one, and the page the benchmark serves at it is what fills
    // the Space's engine storage on the warm-up launch.
    QVector<TabState> tabs;
    for (int index = 0; index < shape.openTabs; ++index) {
        TabState tab;
        tab.id = QStringLiteral("%1-tab-%2").arg(spaceId).arg(index);
        tab.spaceId = spaceId;
        tab.url = page(base, QStringLiteral("%1/%2").arg(spaceId).arg(index));
        tab.title = QStringLiteral("%1 %2").arg(space.name).arg(index);
        tab.active = index == 0;
        tabs.append(tab);
    }
    if (!store.saveTabs(spaceId, tabs, tabs.constFirst().id)) {
        return false;
    }

    QVector<TabState> closed;
    for (int index = 0; index < shape.closedTabs; ++index) {
        TabState tab;
        tab.id = QStringLiteral("%1-closed-%2").arg(spaceId).arg(index);
        tab.spaceId = spaceId;
        tab.url = page(base, QStringLiteral("%1/closed/%2").arg(spaceId).arg(index));
        tab.title = QStringLiteral("Closed %1").arg(index);
        closed.append(tab);
    }
    if (!closed.isEmpty() && !store.recordClosedTabs(spaceId, closed)) {
        return false;
    }

    QVector<PutAwayTab> putAway;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    for (int index = 0; index < shape.putAwayTabs; ++index) {
        PutAwayTab tab;
        tab.id = QStringLiteral("%1-away-%2").arg(spaceId).arg(index);
        tab.url = page(base, QStringLiteral("%1/away/%2").arg(spaceId).arg(index));
        tab.title = QStringLiteral("Put away %1").arg(index);
        tab.putAwayAt = now - index * 3600000LL;
        putAway.append(tab);
    }
    if (!putAway.isEmpty() && !store.recordPutAwayTabs(spaceId, putAway)) {
        return false;
    }

    for (int index = 0; index < shape.visits; ++index) {
        if (!store.recordVisit(spaceId,
                page(base, QStringLiteral("%1/visited/%2").arg(spaceId).arg(index)),
                QStringLiteral("Visit %1").arg(index))) {
            return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    const auto arguments = QCoreApplication::arguments();
    if (arguments.size() != 3) {
        std::fprintf(stderr, "usage: omaweb-lived-in-session <data root> <page address>\n");
        return 2;
    }
    const auto &dataRoot = arguments.at(1);
    const auto &base = arguments.at(2);
    SqliteSessionStore store(dataRoot);
    QString error;
    if (!store.open(&error)) {
        std::fprintf(stderr, "could not open the session store: %s\n", qPrintable(error));
        return 1;
    }
    if (!seed(store, personal, true, base) || !seed(store, work, false, base)
        || !store.setActiveSpace(QString::fromLatin1(personal.id))
        // The reader's password manager is on, so the profile loads its package at start.
        || !store.savePreference(
            QStringLiteral("known-extension-bitwarden-enabled"), QStringLiteral("true"))) {
        std::fprintf(stderr, "could not write the lived-in session\n");
        return 1;
    }
    return 0;
}
