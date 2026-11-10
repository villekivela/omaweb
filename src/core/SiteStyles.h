#pragma once

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace omaweb {

// The reader's own CSS for a site: one file per host in `styles/` in the
// configuration directory. `github.com.css` styles github.com and every host
// under it, as a Link rule's `*.github.com` does. A file names no path,
// because a file name cannot hold one.
//
// A style is the reader's, not Content blocking's, so nothing here asks
// whether blocking is on for a site. The files are watched, and an edit made
// in any editor is `changed` once its bytes are read.
class SiteStyles final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString directory READ directory CONSTANT)

public:
    // An empty configuration root has no files, which is what a test without
    // a directory gets.
    explicit SiteStyles(QString configRoot, QObject *parent = nullptr);

    static QString directoryName();
    // The style files in a directory, by name: each `<host>.css` that names a
    // host other than an IPv6 address. An editor's backup, a hidden file and a
    // directory are not styles, and stay where they are, unread and unsynced.
    static QStringList fileNames(const QString &directory);

    QString directory() const;
    // The CSS for the page at an address: every file whose host is the
    // address's or one above it, the broadest first, so a file for a narrower
    // host wins a tie. An IP address takes its own file alone. Empty for an
    // address with no host or no file.
    Q_INVOKABLE QString styleSheet(const QUrl &url) const;

    // Reads the directory again, as the watch does on its own.
    void reload();

signals:
    // Some file was added, edited or removed.
    void changed();

private:
    void watch();

    QString m_configRoot;
    // The CSS of each file, by the host it names, written as the DNS does.
    QMap<QString, QString> m_sheets;
    QFileSystemWatcher m_watcher;
};

} // namespace omaweb
