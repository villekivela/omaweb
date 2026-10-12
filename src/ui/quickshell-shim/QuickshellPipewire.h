#pragma once

// The `Quickshell.Services.Pipewire` half of the shim: see Quickshell.h for why this exists.

#include <QObject>

namespace omaweb::quickshell {

// `Pipewire.defaultAudioSource`, which `qs.Commons`' `UntypedInput` reads to find a microphone it
// can drive through `wpctl`. Omaweb offers no audio graph, so there is never a source.
class Pipewire : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *defaultAudioSource READ defaultAudioSource CONSTANT)

public:
    explicit Pipewire(QObject *parent = nullptr);

    QObject *defaultAudioSource() const;
};

} // namespace omaweb::quickshell
