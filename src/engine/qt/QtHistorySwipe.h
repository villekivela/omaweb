#pragma once

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>
#include <QWheelEvent>

#include <memory>
#include <vector>

namespace omaweb {

// A phased touchpad scroll may become history navigation only after the page says the content
// under the pointer cannot scroll horizontally. An unanswered query leaves scrolling unchanged.
class QtHistorySwipe : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickItem *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(bool enabled MEMBER m_enabled NOTIFY enabledChanged)
    Q_PROPERTY(bool canGoBack MEMBER m_canGoBack)
    Q_PROPERTY(bool canGoForward MEMBER m_canGoForward)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

public:
    static constexpr qreal axisLock = 10;
    static constexpr qreal distance = 120;
    static constexpr int queryTimeout = 150;
    static constexpr int lostRelease = 1000;

    explicit QtHistorySwipe(QObject *parent = nullptr);
    QQuickItem *view() const;
    void setView(QQuickItem *view);
    qreal progress() const;
    Q_INVOKABLE void resolve(int request, bool allowed);
    Q_INVOKABLE void cancel();

signals:
    void viewChanged();
    void enabledChanged();
    void progressChanged();
    void queryNeeded(int request, QPointF position, int direction);
    // Clear the page's pending glide before ending its scroll and taking over.
    void claimed();
    // -1 is Back, 1 is Forward, as the browser's history runs.
    void navigate(int direction);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class State { Idle, Tracking, Passing, Querying, Navigating, Settled };
    void replay();
    void endPage();
    void consume(const QWheelEvent &event);
    void setProgress(qreal value);

    QMetaObject::Connection m_pageVisibility;
    QMetaObject::Connection m_pageDestroyed;
    QMetaObject::Connection m_viewVisibility;
    QMetaObject::Connection m_viewDestroyed;
    QPointer<QQuickItem> m_view;
    QPointer<QQuickItem> m_page;
    bool m_enabled = true;
    bool m_canGoBack = false;
    bool m_canGoForward = false;
    bool m_replaying = false;
    State m_state = State::Idle;
    QPointF m_travel;
    qreal m_progress = 0;
    int m_direction = 0;
    int m_request = 0;
    quint64 m_beganAt = 0;
    std::vector<std::unique_ptr<QWheelEvent>> m_pending;
    std::unique_ptr<QWheelEvent> m_last;
    QTimer m_timeout;
};

void registerQtHistorySwipe();

} // namespace omaweb
