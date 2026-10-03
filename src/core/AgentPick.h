#pragma once

#include <QHash>
#include <QJsonArray>
#include <QString>
#include <QStringList>

namespace omaweb {

// The rows `omaweb tabs --pick` offers Omarchy's menu, and the way back from
// what the menu answers to the tab.
//
// A row is `<glyph><TAB><title><TAB><host · Space>`. Omarchy's picker returns
// the title and the subtext of the row chosen, and never the glyph, so those
// two are the only key there is. Tabs that would share one are told apart by
// their address and then by their id, so what comes back is always one tab.
struct TabPicks {
    QStringList rows;

    // The id of the tab whose title and subtext the menu answered, or nothing
    // for an answer no row offered.
    QString idFor(const QString &selection) const;

    QHash<QString, QString> idsByAnswer;
};

// `tabs` is the `tabs` array `tabs --all` answers, each with its `spaceName`.
TabPicks tabPicks(const QJsonArray &tabs);

} // namespace omaweb
