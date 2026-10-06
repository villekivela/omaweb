#pragma once

// The `Quickshell.Hyprland` half of the shim: see Quickshell.h for why this exists.

#include <QObject>

namespace omaweb::quickshell {

// `Hyprland.rawEvent(event)`, which `qs.Commons`' `Style` connects to so that it re-reads
// `hyprctl` when Hyprland reloads its config. A `Connections` whose target lacks the signal warns
// on every load, so the signal is declared. Omaweb does not listen on Hyprland's event socket for
// the kit, so it is never emitted.
class Hyprland : public QObject {
    Q_OBJECT

public:
    explicit Hyprland(QObject *parent = nullptr);

signals:
    void rawEvent(QObject *event);
};

} // namespace omaweb::quickshell
