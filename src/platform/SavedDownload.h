#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace omaweb {

// What Omaweb does to a file once the download that fetched it has finished.
//
// The file came from a page, and the operating system is the only thing that
// can carry that fact to whatever opens it later — an archive manager that
// warns, a desktop that refuses to run it, a scanner that reads the origin. So
// the origin is written into the file's own metadata, in the form the platform
// already reads: freedesktop's `user.xdg.origin.url` on Linux, and Apple's
// `com.apple.quarantine` on macOS. A filesystem that carries no metadata is
// reported rather than assumed to have taken it.
//
// The execute bits go with it. A browser writes documents, and nothing Omaweb
// downloads is a thing it is willing to have the desktop run on a double-click;
// a reader who means to run one says so with their own `chmod`. That is also
// why nothing here opens a file: revealing a download shows the reader the
// directory it landed in, and what happens to it next is theirs to start.
class SavedDownload final : public QObject {
    Q_OBJECT
    // Whether this platform can carry the origin at all. Read by Settings,
    // which says so rather than promising metadata that is not there.
    Q_PROPERTY(bool quarantineAvailable READ quarantineAvailable CONSTANT)

public:
    explicit SavedDownload(QObject *parent = nullptr);

    bool quarantineAvailable() const;

    // Marks one finished download with where it came from and takes its
    // execute bits away. `sourceUrl` is the address the bytes came from and
    // `pageUrl` the page that led there; either may be empty.
    Q_INVOKABLE bool quarantine(const QString &path, const QUrl &sourceUrl,
        const QUrl &pageUrl) const;

    // Shows the reader where a download landed, by handing its directory to
    // the desktop. Never the file: opening a download is theirs to do.
    Q_INVOKABLE bool reveal(const QString &path) const;
};

// Makes `SavedDownload` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerSavedDownload();

} // namespace omaweb
