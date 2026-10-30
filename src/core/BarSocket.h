#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

class QLocalServer;
class QLocalSocket;

namespace omaweb {

// The bar socket: a Unix socket only this user can open, over which Omaweb's
// Omarchy bar widget shows the reader's Spaces and tabs (#906). It is apart
// from the Agent socket and not gated by Allow agents, because what it carries
// is the reader's own view of their tabs and a fixed set of actions on them,
// and no page is reached through it.
//
// The browser sends state, one JSON object per line, to every widget: one per
// monitor's bar. A widget sends one action per line. An action whose name or
// arguments are not one of the fixed set is dropped.
class BarSocket final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool widgetsPresent READ widgetsPresent NOTIFY widgetsPresentChanged)

public:
    explicit BarSocket(QObject *parent = nullptr);
    ~BarSocket() override;

    // Listens at `path` with mode 0600, replacing a socket left behind by a
    // browser that has gone. A socket another browser answers on is left alone
    // and this answers false.
    bool listen(const QString &path);
    // Stops listening, lets every widget go and removes the socket.
    void close();
    bool listening() const;

    // Sends `state` to every widget now, and to each one that connects later.
    void publish(const QJsonObject &state);

    // Whether the bar is showing the tabs, so the window can leave its sidebar
    // away. It holds from the moment the socket listens, and while a widget is
    // connected, and goes once none has been for `absenceGrace`: long enough
    // for a shell reload to bring the widgets back without the sidebar
    // flashing in between.
    bool widgetsPresent() const;
    void setAbsenceGrace(int milliseconds);

signals:
    // An action a widget asked for: `activate`, `close`, `pin`, `unpin`,
    // `duplicate` and `popout` with a `tab`, `activate` with its `space` too,
    // `space` with a `space`, `cycle` with a `step` of 1 or -1, `move` with a
    // `tab` and an `index`, and `back`, `forward`, `reload` and `omnibar` alone.
    void actionRequested(const QString &action, const QVariantMap &arguments);
    void widgetsPresentChanged();

private:
    void accept();
    void read(QLocalSocket *socket);
    void send(QLocalSocket *socket);
    void setWidgetsPresent(bool present);

    QLocalServer *m_server;
    QList<QLocalSocket *> m_widgets;
    QByteArray m_line;
    QTimer m_absence;
    bool m_widgetsPresent = false;
};

// `$XDG_RUNTIME_DIR/omaweb/bar.sock`, or `OMAWEB_BAR_SOCKET` where it is set.
QString barSocketPath();

} // namespace omaweb
