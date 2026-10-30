#pragma once

#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace omaweb {

class BarSocket;

// What the window publishes, made into what the bar's widgets read (#906).
//
// The window names each tab's icon by the address it draws it from, which only
// this process's QML engine can open. The feed reads that icon and hands the
// widgets a PNG file in `directory` instead. A file is named by its contents,
// because the widget's Image caches by path: a page that draws a count into its
// favicon reports a new icon, and the new picture has to arrive under a new
// name. A file no tab shows any more is deleted.
//
// A page can change its icon many times a second, as a blinking one does, so
// the feed sends at most one state per `interval`: the first at once, and the
// latest of a burst once the interval has passed.
class BarFeed final : public QObject {
    Q_OBJECT

public:
    BarFeed(BarSocket *socket, QString directory, QObject *parent = nullptr);
    ~BarFeed() override;

    // Every tab is an object with an `icon`, in `spaces[].pins` and
    // `spaces[].tabs`. The icon is an image address, or empty for none.
    void publish(const QJsonObject &state);
    void setInterval(int milliseconds);
    // Deletes every icon file, for when the bar no longer shows the tabs.
    void clear();

private:
    void send();
    void read(const QString &address);
    QJsonObject withFiles(const QJsonObject &state, QSet<QString> &addresses) const;
    void forgetAllBut(const QSet<QString> &addresses);

    BarSocket *m_socket;
    QDir m_directory;
    QJsonObject m_state;
    // The file each icon address was written to, and the addresses being read.
    QHash<QString, QString> m_files;
    QSet<QString> m_reading;
    QTimer m_pause;
    QElapsedTimer m_sinceSent;
    int m_interval = 250;
};

} // namespace omaweb
