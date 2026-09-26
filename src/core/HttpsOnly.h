#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <functional>

namespace omaweb {

// HTTPS-only mode: a page's own address never goes over plain HTTP unless the
// reader said it may. Every top-level `http:` navigation, whether typed, a
// link, a redirect or an address from another application, is sent as `https:`
// before it leaves the browser. Where that fails the shell shows a page of its
// own naming the host, and the reader decides.
//
// One setting for the whole browser, on by default: refusing plaintext is the
// one transport protection Omaweb offers on its own. A Private window follows
// it too. The reader's choice lives in the configuration directory with the
// other privacy switches (ADR 0016).
//
// Only the page's own address is upgraded. Its subresources stay with the
// engine's mixed-content policy, which already blocks the active ones on a
// secure page; upgrading them would break the pages that policy lets load.
// Local-development hosts are left alone, the same set the Omnibar sends over
// plain HTTP.
//
// A site the reader lets through is let through once, for the load they asked
// for, or for good within one Space, which is kept with the site's permissions
// in that Space. A Private window never remembers.
class HttpsOnly final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    // Whether the reader chose plain HTTP for an origin in a Space for good.
    using Remembered = std::function<bool(const QString &spaceId, const QString &origin)>;

    explicit HttpsOnly(QString configRoot, QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);
    void setRemembered(Remembered remembered);

    // What a top-level navigation to `url` in `spaceId` becomes: the `https:`
    // address it is sent to instead, or an invalid URL for one that goes as
    // asked.
    QUrl upgrade(const QUrl &url, const QString &spaceId);
    // A navigation this mode would upgrade but the engine cannot redirect: a
    // form sent to a plain address, whose body a redirect would drop. It is
    // refused rather than let out in the clear, and the shell asks the reader.
    void refuse(const QUrl &url, const QString &spaceId);
    // Whether a top-level request to `url` is a site sending a load it was
    // upgraded for straight back to plain HTTP. Upgrading it again would go
    // round for good, so it is refused, and the shell asks the reader.
    bool refusesDowngrade(const QUrl &url, const QString &spaceId);

    // A load of `url` in `spaceId` failed. When the failure is this mode's,
    // the map names the plain address the reader asked for (`plainUrl`), its
    // host, and why (`reason`): `unreachable` for an upgraded address that
    // could not be loaded, `downgrade` for a site that sent the load back to
    // plain HTTP, and `form` for a form sent to a plain address. Empty for any
    // other failure.
    Q_INVOKABLE QVariantMap failure(const QString &spaceId, const QUrl &url) const;
    // Whether the page at `url` arrived in `spaceId` because this mode sent
    // its load over HTTPS. Site information says so.
    Q_INVOKABLE bool upgradedTo(const QString &spaceId, const QUrl &url) const;
    // The load arrived, so a later plain request to its host is a new one
    // rather than the site sending it back.
    Q_INVOKABLE void arrived(const QString &spaceId, const QUrl &url);
    // The reader let `url`'s origin through for the next load in `spaceId`.
    Q_INVOKABLE void allowOnce(const QString &spaceId, const QUrl &url);

signals:
    void enabledChanged();

private:
    struct Upgrade {
        QUrl plainUrl;
        QDateTime at;
    };
    struct Space {
        // Upgraded address -> the plain one it was upgraded from.
        QHash<QString, Upgrade> upgrades;
        // Hosts upgraded for a load that has not arrived yet.
        QHash<QString, QDateTime> pending;
        // Plain addresses refused, and why.
        QHash<QString, QString> refused;
        // Origins let through for the load the reader asked for, and when.
        // The allowance lasts the load, redirects on the same site included,
        // and ends when the page arrives.
        QHash<QString, QDateTime> allowedOnce;
    };

    void load();
    void save() const;
    bool exempt(const QUrl &url, const QString &spaceId);

    QString m_configRoot;
    bool m_enabled = true;
    Remembered m_remembered;
    QHash<QString, Space> m_spaces;
};

} // namespace omaweb
