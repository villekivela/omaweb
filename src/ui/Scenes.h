#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace omaweb {

// The parameter files of the Scenes the Start page can stand on, as the
// website reads them too: share/scenes/<id>.json, built into the binary. A
// Scene draws by its file rather than restating it, so the road the website
// draws and the one the browser draws are changed in one place.
class Scenes final : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // The parameters of the Scene `id`, or an empty map for one Omaweb does
    // not ship.
    Q_INVOKABLE QVariantMap parameters(const QString &id) const;

    // PROTOTYPE (#814), throwaway: what the Telemetry Scene reads. A file
    // under /proc or /sys, empty for any other path or one that cannot be
    // read; the entries of such a directory; and the layout the
    // OMAWEB_TELEMETRY_LAYOUT environment variable asks for.
    Q_INVOKABLE QString systemFile(const QString &path) const;
    Q_INVOKABLE QStringList systemEntries(const QString &path) const;
    Q_INVOKABLE QString telemetryLayout() const;
};

// Makes `Scenes`, and the `LifeBoard` the Game of Life Scene draws on,
// available to QML as `import Omaweb`. Call once per process, before loading
// QML that uses them.
void registerScenes();

} // namespace omaweb
