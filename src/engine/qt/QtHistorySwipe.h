#pragma once

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QQuickItem>
#include <QSet>
#include <QTimer>
#include <QWheelEvent>

#include <deque>
#include <memory>
#include <vector>

namespace omaweb {

class QtHistorySwipe;

// One application filter routes a page's input to its own history recognizer. Construct it
// before the recognizers, and keep it alive until they have all been destroyed.
class QtHistorySwipeDispatcher : public QObject {
public:
    explicit QtHistorySwipeDispatcher(QObject *parent = nullptr);
    ~QtHistorySwipeDispatcher() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class QtHistorySwipe;
    void add(QQuickItem *view, QtHistorySwipe *swipe);
    void remove(QQuickItem *view, QtHistorySwipe *swipe);
    QHash<QQuickItem *, QtHistorySwipe *> m_views;
    QSet<QtHistorySwipe *> m_active;
};

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
    static constexpr qreal axisLock = 6;
    static constexpr qreal distance = 120;
    static constexpr qreal flickSpeed = 1000;
    static constexpr quint64 speedWindow = 100;
    static constexpr int queryTimeout = 150;
    static constexpr int lostRelease = 1000;

    explicit QtHistorySwipe(QObject *parent = nullptr);
    ~QtHistorySwipe() override;
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

private:
    friend class QtHistorySwipeDispatcher;
    bool handleWheel(QQuickItem *page, QWheelEvent *wheel);
    enum class State { Idle, Tracking, Passing, Querying, Navigating, Settled };
    struct Sample {
        quint64 at;
        qreal offset;
    };
    void replay();
    void endPage();
    void consume(const QWheelEvent &event);
    void setProgress(qreal value);
    qreal speedAt(quint64 at) const;

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
    std::deque<Sample> m_samples;
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
