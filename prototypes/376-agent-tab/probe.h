#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <vector>

class QQuickItem;
class QQuickWindow;

// What the scenario in main.qml cannot do from QML: send Qt events to the view's delegate, read the
// CPU the process tree spent, time the window's frames and read pixels.
class Probe : public QObject {
    Q_OBJECT

public:
    explicit Probe(QObject *parent = nullptr);

    Q_INVOKABLE QString delegateClass(QQuickItem *view) const;
    Q_INVOKABLE bool mouseClick(QQuickItem *view, qreal x, qreal y);
    Q_INVOKABLE bool typeText(QQuickItem *view, const QString &text);
    // A click that leaves Qt's focus where it was and tells neither page's Chromium about the
    // moment it moved: the delegates' focus events are swallowed while the press takes focus and
    // the previous focus item is given it back.
    Q_INVOKABLE bool quietClick(QQuickItem *view, qreal x, qreal y);
    Q_INVOKABLE bool keyTap(QQuickItem *view, int key);
    Q_INVOKABLE bool sendFocusIn(QQuickItem *view);
    Q_INVOKABLE QString activeFocus(QQuickWindow *window) const;

    // CPU seconds spent so far by this process and every descendant, split by Chromium process
    // type, with the two renderers named by pid.
    Q_INVOKABLE QVariantMap cpuSeconds(qint64 agentRenderer, qint64 readerRenderer) const;

    Q_INVOKABLE void watchFrames(QQuickWindow *window);
    Q_INVOKABLE QVariantMap frameReport();

    Q_INVOKABLE QString pixel(const QImage &image, qreal fx, qreal fy) const;
    Q_INVOKABLE QString windowPixel(QQuickWindow *window, qreal fx, qreal fy) const;
    Q_INVOKABLE bool saveImage(const QImage &image, const QString &path) const;
    Q_INVOKABLE double milliseconds() const;
    Q_INVOKABLE void emit_(const QString &line) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QElapsedTimer m_clock;
    QPointer<QQuickWindow> m_watched;
    std::vector<qint64> m_frameNanoseconds;
    std::vector<double> m_gpuMilliseconds;
    qint64 m_frameStart = 0;
};
