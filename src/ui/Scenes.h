#pragma once

#include <QObject>
#include <QString>
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
};

// Makes `Scenes` available to QML as `import Omaweb`. Call once per process,
// before loading QML that uses it.
void registerScenes();

} // namespace omaweb
