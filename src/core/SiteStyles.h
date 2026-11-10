#pragma once

#include "SettingsFile.h"

#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

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
    Q_PROPERTY(QStringList unsaved READ unsaved NOTIFY unsavedChanged)
    Q_PROPERTY(QVariantList styles READ styles NOTIFY changed)

public:
    // An empty configuration root has no files, which is what a test without
    // a directory gets.
    explicit SiteStyles(QString configRoot, QObject *parent = nullptr);
    ~SiteStyles() override;

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

    // A host's own CSS as it stands, typed or saved.
    Q_INVOKABLE QString source(const QString &host) const;
    // Replaces a host's CSS as the reader types: its pages restyle at once,
    // and the file is written once the typing pauses, or when this closes.
    Q_INVOKABLE void edit(const QString &host, const QString &css);
    // The hosts whose typed CSS is not in their file yet.
    QStringList unsaved() const;

    // Every style by host, in order: its `host`, the `path` of its file, how
    // many `lines` it has, how many elements it has `hidden` and whether it is
    // `enabled`.
    QVariantList styles() const;
    // Where a host's file is, or will be once something is written to it.
    Q_INVOKABLE QString path(const QString &host) const;
    // Opens a host's file in the reader's editor, writing it first: Omarchy's
    // choice of editor, or the desktop's for CSS where there is no Omarchy.
    Q_INVOKABLE bool openInEditor(const QString &host);
    // Opens the folder of styles in the desktop's file manager, making it
    // first if there is none yet.
    Q_INVOKABLE bool openFolder();
    // Deletes a host's file, what was typed into it and its switch.
    Q_INVOKABLE bool remove(const QString &host);

    // Whether a host's file applies. Off is kept in settings.json, so it syncs
    // and the file stays as it is.
    Q_INVOKABLE bool enabled(const QString &host) const;
    Q_INVOKABLE bool setEnabled(const QString &host, bool enabled);

    // Hides what a selector matches on a host's pages, as a rule on a line of
    // its own at the end of the host's file, marked as Omaweb's. The file is
    // written at once, and made if the host has none.
    Q_INVOKABLE bool hide(const QString &host, const QString &selector);
    // Takes a hide's line out of a host's file, and leaves every other line
    // as it was. False when the file has no such line.
    Q_INVOKABLE bool unhide(const QString &host, const QString &selector);
    // The selectors a host's file hides, in the order they were added.
    Q_INVOKABLE QStringList hidden(const QString &host) const;

    // The hosts a page's style can be written for: its own and each above it
    // that is still its site, the narrowest first. Each has its `host` and
    // whether a file `exists` for it.
    Q_INVOKABLE QVariantList hostsFor(const QUrl &url) const;
    // The host a page's style is written for unless the reader picks another:
    // its registrable site, or the host itself where it has none.
    Q_INVOKABLE QString siteFor(const QUrl &url) const;

    // Reads the directory again, as the watch does on its own.
    void reload();

signals:
    // Some file was added, edited or removed.
    void changed();
    void unsavedChanged();

private:
    // Writes a host's file whole and restyles its pages with it.
    bool write(const QString &host, const QString &css);
    void writeUnsaved();
    void watch();

    QString m_configRoot;
    // The CSS of each file, by the host it names, written as the DNS does.
    QMap<QString, QString> m_sheets;
    // The file each host's CSS is read from, by name.
    QMap<QString, QString> m_files;
    // The hosts whose CSS in `m_sheets` was typed and is not in their file yet.
    QSet<QString> m_unsaved;
    SettingsFile m_settings;
    QTimer m_pause;
    QFileSystemWatcher m_watcher;
};

} // namespace omaweb
