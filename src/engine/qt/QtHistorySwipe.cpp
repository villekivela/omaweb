#include "QtHistorySwipe.h"

#include "QtPageInput.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScopedValueRollback>

#include <algorithm>
#include <cmath>

namespace omaweb {

namespace {
    QPointer<QtHistorySwipeDispatcher> dispatcher;
}

QtHistorySwipeDispatcher::QtHistorySwipeDispatcher(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(!dispatcher);
    dispatcher = this;
    if (auto *application = QCoreApplication::instance())
        application->installEventFilter(this);
}

QtHistorySwipeDispatcher::~QtHistorySwipeDispatcher() { dispatcher.clear(); }

void QtHistorySwipeDispatcher::add(QQuickItem *view, QtHistorySwipe *swipe)
{
    if (view)
        m_views.insert(view, swipe);
}

void QtHistorySwipeDispatcher::remove(QQuickItem *view, QtHistorySwipe *swipe)
{
    if (m_views.value(view) == swipe)
        m_views.remove(view);
    m_active.remove(swipe);
}

bool QtHistorySwipeDispatcher::eventFilter(QObject *watched, QEvent *event)
{
    if (pageGlideEvent)
        return false;
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::KeyPress
        || event->type() == QEvent::MouseButtonPress) {
        QList<QPointer<QtHistorySwipe>> active;
        for (auto *swipe : std::as_const(m_active))
            active.append(swipe);
        for (const auto &swipe : active) {
            if (swipe)
                swipe->cancel();
        }
        return false;
    }
    if (event->type() != QEvent::Wheel)
        return false;
    auto *page = qobject_cast<QQuickItem *>(watched);
    if (!isPageInput(page))
        return false;
    for (auto *item = page->parentItem(); item; item = item->parentItem()) {
        if (auto *swipe = m_views.value(item, nullptr))
            return swipe->handleWheel(page, static_cast<QWheelEvent *>(event));
    }
    return false;
}

QtHistorySwipe::QtHistorySwipe(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_state == State::Querying) {
            resolve(m_request, false);
        } else {
            cancel();
        }
    });
    connect(this, &QtHistorySwipe::enabledChanged, this, [this] {
        if (!m_enabled) {
            cancel();
        }
    });
}

QtHistorySwipe::~QtHistorySwipe()
{
    if (dispatcher)
        dispatcher->remove(m_view, this);
}

QQuickItem *QtHistorySwipe::view() const { return m_view; }

void QtHistorySwipe::setView(QQuickItem *view)
{
    if (m_view == view) {
        return;
    }
    cancel();
    if (dispatcher)
        dispatcher->remove(m_view, this);
    disconnect(m_viewVisibility);
    disconnect(m_viewDestroyed);
    m_view = view;
    if (view) {
        if (dispatcher)
            dispatcher->add(view, this);
        m_viewVisibility = connect(view, &QQuickItem::visibleChanged, this, [this] {
            if (m_view && !m_view->isVisible())
                cancel();
        });
        m_viewDestroyed = connect(view, &QObject::destroyed, this, [this, view] {
            if (dispatcher)
                dispatcher->remove(view, this);
            cancel();
            emit viewChanged();
        });
    }
    emit viewChanged();
}

qreal QtHistorySwipe::progress() const { return m_progress; }

void QtHistorySwipe::setProgress(qreal value)
{
    if (m_progress != value) {
        m_progress = value;
        emit progressChanged();
    }
}

void QtHistorySwipe::cancel()
{
    if (dispatcher)
        dispatcher->m_active.remove(this);
    if (m_state == State::Querying && m_page && m_page->isVisible()) {
        replay();
    } else if (m_state == State::Navigating) {
        endPage();
    }
    disconnect(m_pageVisibility);
    disconnect(m_pageDestroyed);
    ++m_request;
    m_state = State::Idle;
    m_timeout.stop();
    m_pending.clear();
    m_last.reset();
    m_page.clear();
    m_travel = {};
    m_samples.clear();
    setProgress(0);
}

void QtHistorySwipe::endPage()
{
    if (!m_page || !m_last) {
        return;
    }
    emit claimed();
    QWheelEvent end(m_last->position(), m_last->globalPosition(), {}, {}, Qt::NoButton,
        Qt::NoModifier, Qt::ScrollEnd, m_last->inverted(), Qt::MouseEventSynthesizedBySystem,
        m_last->pointingDevice());
    end.setTimestamp(m_last->timestamp());
    const QScopedValueRollback<bool> guard(m_replaying, true);
    QCoreApplication::sendEvent(m_page, &end);
}

void QtHistorySwipe::replay()
{
    auto pending = std::move(m_pending);
    const QScopedValueRollback<bool> guard(m_replaying, true);
    for (const auto &event : pending) {
        if (m_page) {
            QCoreApplication::sendEvent(m_page, event.get());
        }
    }
}

void QtHistorySwipe::resolve(int request, bool allowed)
{
    if (request != m_request || m_state != State::Querying) {
        return;
    }
    m_timeout.stop();
    if (!allowed || !m_enabled || !m_view || !m_view->isVisible() || !m_page
        || !m_page->isVisible()) {
        m_state = State::Passing;
        replay();
        return;
    }
    endPage();
    m_state = State::Navigating;
    auto pending = std::move(m_pending);
    for (const auto &event : pending) {
        if (m_state == State::Navigating) {
            consume(*event);
        }
    }
}

void QtHistorySwipe::consume(const QWheelEvent &event)
{
    if (event.phase() == Qt::ScrollMomentum) {
        return;
    }
    m_timeout.start(lostRelease);
    QPointF delta = event.pixelDelta();
    if (delta.isNull()) {
        delta = QPointF(event.angleDelta()) / 12.0;
    }
    m_travel += delta;
    if (!delta.isNull()) {
        m_samples.push_back({event.timestamp(), m_travel.x()});
        while (m_samples.size() > 1 && event.timestamp() >= m_samples.front().at
            && event.timestamp() - m_samples.front().at > speedWindow) {
            m_samples.pop_front();
        }
    }
    // Reversing towards the starting point cancels the swipe rather than arming the other way.
    const qreal along = std::max(0.0, m_travel.x() * -m_direction);
    setProgress(-m_direction * std::min(1.25, along / m_distance));
    if (event.phase() == Qt::ScrollEnd) {
        m_timeout.stop();
        m_state = State::Settled;
        setProgress(0);
        const qreal speed = speedAt(event.timestamp());
        const bool flicked = along > 0 && speed * -m_direction > flickSpeed;
        if ((along >= m_distance || flicked) && (m_direction < 0 ? m_canGoBack : m_canGoForward)) {
            emit navigate(m_direction);
        }
    }
}

qreal QtHistorySwipe::speedAt(quint64 at) const
{
    const auto from = std::find_if(m_samples.cbegin(), m_samples.cend(),
        [at](const Sample &sample) { return sample.at <= at && at - sample.at <= speedWindow; });
    if (from == m_samples.cend() || m_samples.back().at <= from->at || m_samples.back().at > at) {
        return 0;
    }
    const auto &to = m_samples.back();
    return (to.offset - from->offset) * 1000.0 / static_cast<qreal>(to.at - from->at);
}

bool QtHistorySwipe::handleWheel(QQuickItem *page, QWheelEvent *wheel)
{
    if (m_replaying || !m_enabled || !m_view || !m_view->isVisible())
        return false;
    if (!m_enabled || !m_view->isVisible() || wheel->modifiers() != Qt::NoModifier
        || wheel->buttons() != Qt::NoButton || wheel->phase() == Qt::NoScrollPhase) {
        if (m_state == State::Querying) {
            m_state = State::Passing;
            replay();
        }
        cancel();
        return false;
    }
    if (wheel->phase() == Qt::ScrollBegin) {
        // Qt's Wayland plugin can deliver the same zero-delta begin twice.
        if (m_state != State::Idle && m_beganAt == wheel->timestamp() && m_page == page) {
            return m_state == State::Querying || m_state == State::Navigating;
        }
        cancel();
        m_state = State::Tracking;
        if (dispatcher)
            dispatcher->m_active.insert(this);
        m_page = page;
        m_pageVisibility = connect(page, &QQuickItem::visibleChanged, this, [this] {
            if (m_page && !m_page->isVisible())
                cancel();
        });
        m_pageDestroyed = connect(page, &QObject::destroyed, this, [this] { cancel(); });
        m_beganAt = wheel->timestamp();
    }
    if (m_state == State::Idle || m_state == State::Passing) {
        return false;
    }
    if (m_state == State::Settled) {
        wheel->accept();
        return true;
    }
    m_last.reset(wheel->clone());
    if (m_state == State::Navigating) {
        consume(*wheel);
        wheel->accept();
        return true;
    }
    if (m_state == State::Querying) {
        m_pending.emplace_back(wheel->clone());
        wheel->accept();
        return true;
    }
    if (wheel->phase() == Qt::ScrollEnd || wheel->phase() == Qt::ScrollMomentum) {
        m_state = State::Passing;
        return false;
    }
    QPointF delta = wheel->pixelDelta();
    if (delta.isNull()) {
        delta = QPointF(wheel->angleDelta()) / 12.0;
    }
    m_travel += delta;
    const bool across
        = std::abs(m_travel.x()) >= axisLock && std::abs(m_travel.x()) > std::abs(m_travel.y());
    if (!across && std::abs(m_travel.y()) >= axisLock) {
        m_state = State::Passing;
        return false;
    }
    if (!across) {
        return false;
    }
    m_direction = m_travel.x() > 0 ? -1 : 1;
    if (!(m_direction < 0 ? m_canGoBack : m_canGoForward)) {
        m_state = State::Passing;
        return false;
    }
    m_state = State::Querying;
    // The event that crossed the threshold belongs to the queue, not to the initial travel too.
    m_travel -= delta;
    m_pending.emplace_back(wheel->clone());
    m_timeout.start(queryTimeout);
    emit queryNeeded(++m_request, page->mapToItem(m_view, wheel->position()), m_direction);
    wheel->accept();
    return true;
}

void registerQtHistorySwipe()
{
    qmlRegisterType<QtHistorySwipe>("Omaweb.Engine", 1, 0, "QtHistorySwipe");
}

} // namespace omaweb
