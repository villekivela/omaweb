#pragma once

#include <QObject>
#include <QVariant>

namespace omaweb {

// The factor the desktop asks every application to enlarge its type by, the
// Desktop text scaling (ADR 0067). It is GNOME's `text-scaling-factor` in
// `org.gnome.desktop.interface`, read through the desktop's settings portal
// as reduced motion is, and heard again whenever it changes. Omarchy's
// `omarchy-display-text-size` writes it, 1.3636 for 16 px.
//
// Where no portal answers, as on macOS or a desktop without one, the factor
// is 1 and nothing is enlarged.
class DesktopTextScaling final : public QObject {
    Q_OBJECT
    Q_PROPERTY(double factor READ factor NOTIFY factorChanged)

public:
    explicit DesktopTextScaling(QObject *parent = nullptr);
    ~DesktopTextScaling() override;

    double factor() const;

signals:
    void factorChanged();

private:
    void setFactor(double factor);

    double m_factor = 1.0;
};

// The portal's answer for `text-scaling-factor`: GSettings holds a double
// above zero, and anything else reads as 1.
double portalTextScalingFactor(const QVariant &value);

} // namespace omaweb
