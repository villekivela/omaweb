#pragma once

#include <QObject>
#include <QPointer>

#include <vector>

namespace omaweb {

class WebRtcPolicy;

// The reader's WebRTC address policy, set on every Engine profile QtWebEngine
// runs: a Space's and the Private windows' shared one alike. The policy is a
// profile's settings attribute, `WebRTCPublicInterfacesOnly`, which every
// view of the profile inherits, so a change reaches every open page at once
// and a call placed after it gathers candidates under the new policy.
//
// The attribute is reached the way the reader's fonts are (ADR 0047), and
// under the same terms: a Qt other than the one this was compiled against is
// not reached into, `available()` reads false, the engine keeps its own
// default of every interface, and the Settings group says so.
class QtWebRtcPolicy final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit QtWebRtcPolicy(WebRtcPolicy *policy, QObject *parent = nullptr);

    bool available() const;

public slots:
    // Called for every profile Content blocking attaches to, which is every
    // profile there is.
    void attachToProfile(QObject *profile);

private:
    void apply();

    WebRtcPolicy *m_policy;
    std::vector<QPointer<QObject>> m_profiles;
};

} // namespace omaweb
