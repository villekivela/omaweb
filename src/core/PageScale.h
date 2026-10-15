#pragma once

#include "SettingsFile.h"

#include <QObject>
#include <QString>

#include <optional>

namespace omaweb {

// The size every page is drawn at before its tab's zoom (ADR 0067). It is the
// Desktop text scaling until the reader sets one in Settings, kept as
// `page-scale` in settings.json (ADR 0061) and absent while it follows the
// desktop. A value the file holds outside the range reads as none.
//
// One value for the whole browser: every Space and Private window draws its
// pages at it, and a change reaches every open page at once. The desktop's
// factor is handed in by whoever reads it, so the core needs no portal.
class PageScale final : public QObject {
    Q_OBJECT
    // The Page scale in effect, as a factor: 1.0 is 100 percent.
    Q_PROPERTY(double value READ value NOTIFY changed)
    Q_PROPERTY(bool overridden READ overridden NOTIFY changed)
    Q_PROPERTY(
        double desktopFactor READ desktopFactor WRITE setDesktopFactor NOTIFY desktopFactorChanged)
    Q_PROPERTY(double minimum READ minimum CONSTANT)
    Q_PROPERTY(double maximum READ maximum CONSTANT)
    Q_PROPERTY(double step READ step CONSTANT)

public:
    explicit PageScale(QString configRoot, QObject *parent = nullptr);

    double value() const;
    bool overridden() const;
    double desktopFactor() const;
    void setDesktopFactor(double factor);

    static constexpr double minimum() { return 0.5; }
    static constexpr double maximum() { return 2.0; }
    static constexpr double step() { return 0.1; }
    // What QtWebEngine draws a page between.
    static constexpr double minimumDrawn() { return 0.25; }
    static constexpr double maximumDrawn() { return 5.0; }

    // The factor an engine draws a page at: the Page scale times its tab's
    // zoom, held within what the engine draws. A zoom that does not fit is
    // drawn at the limit and stays the tab's.
    static double drawn(double scale, double zoom);
    // The same for QML, which passes `value` in so that a binding calling it
    // follows the Page scale.
    Q_INVOKABLE double drawnAt(double scale, double zoom) const;

    // Takes the nearest step inside the range. A refused write says so, which
    // draws a control the reader moved back where the value stands.
    Q_INVOKABLE void set(double scale);
    // One step from the value in effect. The desktop's may fall between
    // steps, and the first step from it lands on one.
    Q_INVOKABLE void increase();
    Q_INVOKABLE void decrease();
    // Follows the desktop again.
    Q_INVOKABLE void reset();

signals:
    void changed();
    void desktopFactorChanged();

private:
    SettingsFile m_settings;
    std::optional<double> m_scale;
    double m_desktopFactor = 1.0;
};

} // namespace omaweb
