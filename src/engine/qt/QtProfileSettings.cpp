#include "QtProfileSettings.h"

#include <QScopedPointer>
#include <QWebEngineSettings>
#include <QtGlobal>
#include <QtWebEngineQuick/QQuickWebEngineProfile>
// A Quick profile keeps its settings behind a private method, and the class
// that method returns is one Qt ships without a public header and nothing
// public on it reaches the `QWebEngineSettings` it wraps. Both are reached the
// standard's own way to a private member: access checking does not apply to
// the names in an explicit instantiation (C++23 [temp.explicit]), so one names
// each member once and a friend hands the pointer out. Everything after that
// is the public core API.
#include <QtWebEngineQuick/private/qquickwebenginesettings_p.h>

#include <cstring>

namespace omaweb {
namespace {

    template <typename Tag, typename Tag::Type Member> struct Reveal {
        friend typename Tag::Type reveal(Tag) { return Member; }
    };

    struct ProfileSettings {
        using Type = QQuickWebEngineSettings *(QQuickWebEngineProfile::*)() const;
        friend Type reveal(ProfileSettings);
    };
    template struct Reveal<ProfileSettings, &QQuickWebEngineProfile::settings>;

    struct CoreSettings {
        using Type = QScopedPointer<QWebEngineSettings> QQuickWebEngineSettings::*;
        friend Type reveal(CoreSettings);
    };
    template struct Reveal<CoreSettings, &QQuickWebEngineSettings::d_ptr>;

} // namespace

// Exactly the same build, not a compatible one: the offset of a private
// member is promised by nothing, so only the Qt this was compiled against is
// reached into, the way `LinuxPortalWindow.cpp` guards its own call.
bool QtProfileSettings::reachable() { return std::strcmp(qVersion(), QT_VERSION_STR) == 0; }

QWebEngineSettings *QtProfileSettings::of(QObject *profileObject)
{
    if (!reachable()) {
        return nullptr;
    }
    auto *profile = qobject_cast<QQuickWebEngineProfile *>(profileObject);
    if (!profile) {
        return nullptr;
    }
    auto *settings = (profile->*reveal(ProfileSettings {}))();
    return settings ? (settings->*reveal(CoreSettings {})).data() : nullptr;
}

} // namespace omaweb
