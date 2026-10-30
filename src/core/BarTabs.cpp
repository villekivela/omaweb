#include "BarTabs.h"

namespace omaweb {

BarTabs::BarTabs(QString socketPath, QString iconDirectory, QObject *parent)
    : QObject(parent)
    , m_socketPath(std::move(socketPath))
    , m_feed(&m_socket, std::move(iconDirectory))
{
    connect(&m_socket, &BarSocket::widgetsPresentChanged, this, &BarTabs::showsTabsChanged);
    connect(&m_socket, &BarSocket::actionRequested, this, &BarTabs::actionRequested);
}

bool BarTabs::offered() const { return m_offered; }

void BarTabs::setOffered(bool offered)
{
    if (m_offered == offered) {
        return;
    }
    m_offered = offered;
    emit offeredChanged();
}

bool BarTabs::showsTabs() const { return m_showsForTests || (m_on && m_socket.widgetsPresent()); }

void BarTabs::setOn(bool on)
{
    if (m_on == on) {
        return;
    }
    m_on = on;
    if (on) {
        m_socket.listen(m_socketPath);
    } else {
        m_socket.close();
        m_feed.clear();
    }
    emit showsTabsChanged();
}

bool BarTabs::on() const { return m_on; }

void BarTabs::publish(const QVariantMap &state)
{
    emit published(state);
    if (m_on) {
        m_feed.publish(QJsonObject::fromVariantMap(state));
    }
}

void BarTabs::checkOffered() { emit offerCheckRequested(); }

QString BarTabs::spaceNames() const { return m_spaceNames; }

void BarTabs::showSpaceNames(const QString &spaceNames)
{
    if (m_spaceNames == spaceNames) {
        return;
    }
    m_spaceNames = spaceNames;
    emit spaceNamesChanged();
}

void BarTabs::setSpaceNames(const QString &spaceNames)
{
    showSpaceNames(spaceNames);
    emit spaceNamesRequested(spaceNames);
}

void BarTabs::setShowsTabsForTests(bool shows)
{
    if (m_showsForTests == shows) {
        return;
    }
    m_showsForTests = shows;
    emit showsTabsChanged();
}

} // namespace omaweb
