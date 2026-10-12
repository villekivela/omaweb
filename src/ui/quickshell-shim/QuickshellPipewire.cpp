#include "QuickshellPipewire.h"

namespace omaweb::quickshell {

Pipewire::Pipewire(QObject *parent)
    : QObject(parent)
{
}

QObject *Pipewire::defaultAudioSource() const { return nullptr; }

} // namespace omaweb::quickshell
