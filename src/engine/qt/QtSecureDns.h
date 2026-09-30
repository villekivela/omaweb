#pragma once

#include <QObject>
#include <QWebEngineGlobalSettings>

#include <functional>

namespace omaweb {

class SecureDns;

// The reader's Secure DNS choice, handed to QtWebEngine's resolver. The engine
// resolves names once for the whole process, so this is one setting for every
// Engine profile, and a change reaches the resolver at once: a lookup made
// after it goes to the new resolver, and names already cached stay until they
// expire.
class QtSecureDns final : public QObject {
    Q_OBJECT
    // Whether the engine took the resolver it was given. Qt checks a template
    // with Chromium's own parser, which is stricter than a form can be, so a
    // refusal is reported rather than assumed away.
    Q_PROPERTY(bool applied READ applied NOTIFY appliedChanged)

public:
    // How a mode reaches the engine, and whether it took it. The engine offers
    // no way to read its mode back, so a test hands in one that watches.
    using SetDnsMode = std::function<bool(const QWebEngineGlobalSettings::DnsMode &)>;

    explicit QtSecureDns(SecureDns *secureDns, QObject *parent = nullptr);
    QtSecureDns(SecureDns *secureDns, SetDnsMode setDnsMode, QObject *parent = nullptr);

    bool applied() const;

signals:
    void appliedChanged();

private:
    void apply();

    SecureDns *m_secureDns;
    SetDnsMode m_setDnsMode;
    bool m_applied = false;
};

} // namespace omaweb
