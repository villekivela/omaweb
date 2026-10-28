#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace omaweb {

// A Link rule: a pattern, and where a link from another app that matches it
// opens. The rules are `"link-rules"` in settings.json, in the reader's order,
// and the first that matches wins.
struct LinkRule {
    QString pattern;
    // One of the reader's Spaces, by name, or `privateWindow()`.
    QString space;

    bool operator==(const LinkRule &) const = default;
};

namespace linkrules {

    // The target that opens a link in a Private window rather than a Space.
    QString privateWindow();

    // A host, optionally after `*.`, then optionally a path prefix that ends
    // in `*`: `*.acme.com`, `github.com/acme/*`. Nothing else is a pattern.
    bool parses(const QString &pattern);
    // Whether a web address falls under a pattern. `*.acme.com` takes
    // `acme.com` as well, and a pattern that does not parse takes nothing.
    bool matches(const QString &pattern, const QUrl &url);

    // The whole-site pattern for a web address: its registrable domain, the
    // public suffix and one label before it, after `*.`, so `app.acme.com`
    // gives `*.acme.com`. None for an address that is not `http` or `https`,
    // an IP address, or a host with no label before its public suffix.
    std::optional<QString> sitePattern(const QUrl &url);

    // The rules a settings value holds, in order, or none for one that is not
    // a list of them.
    QList<LinkRule> read(const QJsonValue &value);
    QJsonArray write(const QList<LinkRule> &rules);

    // The first rule whose pattern parses and takes the address.
    std::optional<LinkRule> route(const QList<LinkRule> &rules, const QUrl &url);

} // namespace linkrules

} // namespace omaweb
