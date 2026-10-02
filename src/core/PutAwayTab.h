#pragma once

#include <QString>
#include <QUrl>
#include <QVariantMap>

namespace omaweb {

// A tab Omaweb closed because the reader had not shown it for longer than
// the setting allows, kept in its Space's put-away list so it can be opened
// again where it was. What is kept is what a closed tab keeps: the address,
// the title, the zoom and the muting. The page itself is gone.
//
// The list is not the stack of recent closes. The reader closed those, and
// `Primary+Shift+T` walks them back; nobody closed these, so they wait where
// the History sheet and the Omnibar list them.
struct PutAwayTab {
    QString id {};
    QUrl url {};
    QString title {};
    double zoom = 1.0;
    bool muted = false;
    // When it was put away, in milliseconds since the epoch.
    qint64 putAwayAt = 0;

    bool operator==(const PutAwayTab &) const = default;

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("id"), id},
            {QStringLiteral("url"), url},
            {QStringLiteral("host"), url.host()},
            {QStringLiteral("title"), title},
            {QStringLiteral("putAwayAt"), putAwayAt},
        };
    }
};

} // namespace omaweb
