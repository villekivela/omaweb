#pragma once

#include "SettingsFile.h"

#include <QObject>
#include <QString>

#include <optional>

namespace omaweb {

// The reader's say over how much of the desktop shows through the sidebar.
//
// An override and not a new default, as the interface font size is (see
// `FontSettings`): the theme names the sidebar's opacity, and this holds what
// the reader chose over it, in `sidebar-opacity` in settings.json (ADR 0061).
// It stands until reset, and a theme switch changes the sidebar only while none
// stands. A value the file holds outside the range reads as none.
//
// The drawing is the `ThemeController`'s: this reports the value, and whoever
// owns both hands it over.
class SidebarOpacity final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool overridden READ overridden NOTIFY changed)
    Q_PROPERTY(double value READ value NOTIFY changed)
    Q_PROPERTY(double minimum READ minimum CONSTANT)
    Q_PROPERTY(double maximum READ maximum CONSTANT)
    Q_PROPERTY(double step READ step CONSTANT)

public:
    explicit SidebarOpacity(QString configRoot, QObject *parent = nullptr);

    // The reader's value, or none while the theme's stands.
    std::optional<double> opacity() const;
    bool overridden() const;
    // The reader's value, or zero while there is none.
    double value() const;

    static constexpr double minimum() { return 0.5; }
    static constexpr double maximum() { return 1.0; }
    // The whole percent a slider moves by, as a fraction.
    static constexpr double step() { return 0.05; }

    // Takes the nearest step inside the range. A refused write says so, which
    // draws a control the reader moved back where the value stands.
    Q_INVOKABLE void set(double opacity);
    Q_INVOKABLE void reset();

signals:
    void changed();

private:
    SettingsFile m_settings;
    std::optional<double> m_opacity;
};

} // namespace omaweb
