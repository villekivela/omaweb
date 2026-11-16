#pragma once

#include "SettingsFile.h"

#include <QObject>
#include <QString>

namespace omaweb {

// What each touchpad swipe does, and how far the fingers travel before any of
// them acts: `gestures` in settings.json (ADR 0061), an object that names only
// what the reader changed. A gesture is "page", two fingers sideways over a
// page, "sidebar", two fingers sideways over the sidebar, or "three-fingers",
// three sideways anywhere in the window. Each does one of "back-and-forward",
// "switch-space", "switch-tab" or "nothing".
//
// The recognizers read these and the chrome runs the action; this only holds
// the reader's choice and follows the file.
class GestureSettings final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString pageAction READ pageAction NOTIFY changed)
    Q_PROPERTY(QString sidebarAction READ sidebarAction NOTIFY changed)
    Q_PROPERTY(QString threeFingerAction READ threeFingerAction NOTIFY changed)
    // In logical pixels.
    Q_PROPERTY(int distance READ distance NOTIFY changed)
    Q_PROPERTY(int minimumDistance READ minimumDistance CONSTANT)
    Q_PROPERTY(int maximumDistance READ maximumDistance CONSTANT)
    Q_PROPERTY(int distanceStep READ distanceStep CONSTANT)
    Q_PROPERTY(int defaultDistance READ defaultDistance CONSTANT)

public:
    explicit GestureSettings(QString configRoot, QObject *parent = nullptr);

    QString pageAction() const;
    QString sidebarAction() const;
    QString threeFingerAction() const;
    int distance() const;

    static constexpr int minimumDistance() { return 60; }
    static constexpr int maximumDistance() { return 240; }
    static constexpr int distanceStep() { return 10; }
    static constexpr int defaultDistance() { return 120; }

    // A gesture or an action this build does not name changes nothing. A
    // refused write says so, which draws a control the reader moved back where
    // the value stands.
    Q_INVOKABLE void setAction(const QString &gesture, const QString &action);
    // Takes the nearest step inside the range.
    Q_INVOKABLE void setDistance(int distance);

signals:
    void changed();

private:
    void read();

    SettingsFile m_settings;
    QString m_page;
    QString m_sidebar;
    QString m_threeFingers;
    int m_distance = defaultDistance();
};

} // namespace omaweb
