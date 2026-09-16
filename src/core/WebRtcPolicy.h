#pragma once

#include <QObject>
#include <QString>

namespace omaweb {

// What a page's call may learn about the reader's network. WebRTC gathers a
// candidate address on every interface the host has and hands the list to
// the page before any call is placed, so a page can read the reader's LAN
// address and, behind a VPN, the address the VPN exists to hide. Public
// interfaces only offers the one address the default route leaves on: a call
// still connects, through that route or a TURN relay, and what it loses is
// the enumeration it did not need.
//
// One setting for the whole browser, on by default. A Space separates
// identities and a Private window is a temporary one; neither changes what
// the reader wants leaked, so neither gets a switch of its own. Off is for a
// reader whose peer is on the same network and has to be reached directly,
// and it lives in the configuration directory with their other decisions
// (ADR 0016).
class WebRtcPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool publicInterfacesOnly READ publicInterfacesOnly WRITE setPublicInterfacesOnly
            NOTIFY publicInterfacesOnlyChanged)

public:
    explicit WebRtcPolicy(QString configRoot, QObject *parent = nullptr);

    bool publicInterfacesOnly() const;
    void setPublicInterfacesOnly(bool publicInterfacesOnly);

signals:
    void publicInterfacesOnlyChanged();

private:
    void load();
    void save() const;

    QString m_configRoot;
    bool m_publicInterfacesOnly = true;
};

} // namespace omaweb
