#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

namespace omaweb {

// Whether Omaweb resolves names over DNS-over-HTTPS, and through whose
// resolver. Off by default: a resolver the reader did not choose is not a
// safer default, it is a different party that learns every name. On, it is a
// resolver Omaweb names or an address the reader types, and it is secure mode
// only, so a resolver that cannot be reached fails the lookup rather than
// falling back to the system in the clear.
//
// One setting for the whole browser, like the other privacy decisions: the
// engine resolves names once for every profile, and a Private window follows
// it too. It also decides what CNAME uncloaking sees, because the engine hands
// over a host's whole CNAME chain only through its own DNS client, which it
// runs only here (ADR 0050).
class SecureDns final : public QObject {
    Q_OBJECT
    // Empty when off, else a named resolver's id or "custom".
    Q_PROPERTY(QString resolver READ resolver NOTIFY changed)
    Q_PROPERTY(QString customTemplate READ customTemplate NOTIFY changed)
    // The address lookups go to, or empty when the system resolves names.
    Q_PROPERTY(QString serverTemplate READ serverTemplate NOTIFY changed)
    Q_PROPERTY(QVariantList resolvers READ resolvers CONSTANT)

public:
    explicit SecureDns(QString configRoot, QObject *parent = nullptr);

    QString resolver() const;
    QString customTemplate() const;
    QString serverTemplate() const;
    // Each resolver Omaweb names, as `id`, `title` and `template`.
    QVariantList resolvers() const;

    Q_INVOKABLE bool useResolver(const QString &id);
    Q_INVOKABLE bool useCustom(const QString &address);
    Q_INVOKABLE void turnOff();

signals:
    void changed();

private:
    void load();
    void save() const;

    QString m_configRoot;
    QString m_resolver;
    QString m_customTemplate;
};

} // namespace omaweb
