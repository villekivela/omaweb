#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace omaweb {

// A tab still running in a Space that is not the one on show, and why. Only two
// things earn it: a Pinned tab the reader marked Keep active, and the tab an
// inspector is attached to. Nothing else outlives its Space's suspension.
//
// A type rather than a map of strings because three places answer questions
// about these — which tab a notification may come from, which Space's store to
// write a released one to, and what the reader is shown — and a key spelled
// wrong in any of them would simply read as absent. The one place that speaks
// in strings is the crossing into QML, below.
struct RetainedTab {
    QString tabId {};
    QString spaceId {};
    QString spaceName {};
    QString title {};
    QUrl url {};
    double zoom = 1.0;
    bool muted = false;
    // Retained because an inspector is attached rather than because the reader
    // asked for it. The interface says which, and refuses to stop the one the
    // reader did not choose to keep.
    bool inspected = false;

    bool operator==(const RetainedTab &) const = default;

    QVariantMap toVariantMap() const
    {
        return {
            { QStringLiteral("tabId"), tabId },
            { QStringLiteral("spaceId"), spaceId },
            { QStringLiteral("spaceName"), spaceName },
            { QStringLiteral("title"), title },
            { QStringLiteral("url"), url },
            { QStringLiteral("zoom"), zoom },
            { QStringLiteral("muted"), muted },
            { QStringLiteral("inspected"), inspected },
        };
    }
};

} // namespace omaweb
