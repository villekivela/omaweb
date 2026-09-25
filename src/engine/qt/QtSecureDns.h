#pragma once

#include <QObject>

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
    explicit QtSecureDns(SecureDns *secureDns, QObject *parent = nullptr);

    bool applied() const;

signals:
    void appliedChanged();

private:
    void apply();

    SecureDns *m_secureDns;
    bool m_applied = false;
};

} // namespace omaweb
