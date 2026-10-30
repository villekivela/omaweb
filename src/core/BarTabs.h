#pragma once

#include "BarFeed.h"
#include "BarSocket.h"

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace omaweb {

// The tabs in Omarchy's bar, as the main window sees them (#906): whether the
// reader may choose them, whether the bar is showing them now, the state the
// window hands it, and the actions that come back.
//
// The socket listens only while the reader has chosen the bar. A Private
// window keeps its own sidebar and never publishes.
class BarTabs final : public QObject {
    Q_OBJECT
    // Whether Omarchy's shell answered, so Settings can offer the choice.
    Q_PROPERTY(bool offered READ offered NOTIFY offeredChanged)
    // Whether the window should leave its sidebar away: the reader chose the
    // bar, and a widget is there or has not been gone for long.
    Q_PROPERTY(bool showsTabs READ showsTabs NOTIFY showsTabsChanged)

public:
    BarTabs(QString socketPath, QString iconDirectory, QObject *parent = nullptr);

    bool offered() const;
    void setOffered(bool offered);
    bool showsTabs() const;

    void setOn(bool on);
    bool on() const;

    // The state the widgets read, laid out as BarSocket says, with each tab's
    // icon as the address the window draws it from.
    Q_INVOKABLE void publish(const QVariantMap &state);
    // Asks again whether the shell answers, as Settings opening does.
    Q_INVOKABLE void checkOffered();
    // Stands in for a widget connecting, for the interface's tests.
    Q_INVOKABLE void setShowsTabsForTests(bool shows);

    BarSocket &socket();

signals:
    void offeredChanged();
    void showsTabsChanged();
    void offerCheckRequested();
    void actionRequested(const QString &action, const QVariantMap &arguments);
    // A state the window published, whether or not the bar is listening.
    void published(const QVariantMap &state);

private:
    QString m_socketPath;
    BarSocket m_socket;
    BarFeed m_feed;
    bool m_on = false;
    bool m_offered = false;
    bool m_showsForTests = false;
};

} // namespace omaweb
