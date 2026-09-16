#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

namespace omaweb {

// Whether Omaweb tells every site not to sell or share the reader's data.
// Global Privacy Control is the one signal a browser sends that has legal
// weight: a site bound by the CCPA and the state laws written after it must
// treat `Sec-GPC: 1` as an opt-out. The specification requires two halves, the
// request header and `navigator.globalPrivacyControl`, and this is the one
// decision both read.
//
// One setting for the whole browser, on by default. A Space separates
// identities and a Private window is a temporary one; neither changes what the
// reader wants said about them, so neither gets a switch of its own. Off is
// the reader's decision and lives in the configuration directory with their
// other decisions (ADR 0016), rather than in a Space's session store, which a
// Private window does not have.
class GlobalPrivacyControl final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit GlobalPrivacyControl(QString configRoot, QObject *parent = nullptr);

    bool enabled() const;
    void setEnabled(bool enabled);

    // The signal itself, named once so that every engine sends the same one.
    static QByteArray headerName();
    static QByteArray headerValue();
    // Defines `navigator.globalPrivacyControl` as `true`. Meant to run before
    // the page's own scripts, in the main world, in every frame.
    static QString scriptSource();

signals:
    void enabledChanged();

private:
    void load();
    void save() const;

    QString m_configRoot;
    bool m_enabled = true;
};

} // namespace omaweb
