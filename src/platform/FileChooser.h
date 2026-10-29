#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QUrl>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace omaweb {

// The desktop's file chooser, asked through `org.freedesktop.portal.FileChooser`
// (ADR 0070). Qt's own dialogs follow the platform theme, and the GTK theme
// Omarchy sets for every Qt app opens GTK's dialog in process, never the one
// the reader set for the desktop. One chooser holds at most one dialog.
//
// Where no portal answers, `ask` returns false and nothing was shown, which is
// the caller's cue to open Qt's dialog instead.
class FileChooser : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)

public:
    explicit FileChooser(QObject *parent = nullptr);
    ~FileChooser() override;

    bool isOpen() const;

    // `question` holds:
    // - `mode`: "open", "open-multiple", "folder" or "save".
    // - `title`: the dialog's title.
    // - `nameFilters`: Qt's filter strings, such as "Text files (*.txt)".
    // - `acceptedTypes`: a page's `accept` list, MIME types and `.ext`
    //   suffixes, offered first and chosen by default, under the name
    //   `acceptedTypesName`.
    // - `currentFile`: the address a save starts at, its folder and its name.
    //
    // The dialog stands over `window`. A dialog already open is taken back
    // first, unanswered.
    Q_INVOKABLE bool ask(const QVariantMap &question, QWindow *window = nullptr);
    // Takes the open dialog back. Neither `chosen` nor `cancelled` follows.
    Q_INVOKABLE void close();

signals:
    void openChanged();
    // Addresses, `file:` ones for local files, as Qt's own dialog gives them.
    void chosen(const QList<QUrl> &urls);
    void cancelled();

private slots:
    void answered(uint response, const QVariantMap &results);

private:
    void hear(const QString &handle, bool listening);
    void forget();

    QString m_handle;
};

// Makes `FileChooser` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerFileChooser();

} // namespace omaweb
