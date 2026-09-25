#pragma once

#include <QObject>
#include <QSize>
#include <QString>

namespace omaweb {

// The desktop clipboard, for the commands that put something on it. Clipboard
// access is a window-system service, so it belongs beside the interface rather
// than in the core: the core links no GUI at all, and a browser command that
// only ever copies a string does not need it to.
class SystemClipboard final : public QObject {
    Q_OBJECT

public:
    explicit SystemClipboard(QObject *parent = nullptr);

    // Nothing is copied for empty text. Clearing what the reader had on the
    // clipboard is not what asking to copy nothing means.
    Q_INVOKABLE bool copyText(const QString &text);
    Q_INVOKABLE QString text() const;
    // A file for an image to be written to before it is copied: the engine
    // hands a capture over as a file, as it does a print.
    Q_INVOKABLE QString reserveImage() const;
    // Puts the image in `path` on the clipboard and removes the file, which was
    // only ever the way the image arrived.
    Q_INVOKABLE bool copyImage(const QString &path);
    // The size of the image on the clipboard, empty when it holds none.
    Q_INVOKABLE QSize imageSize() const;
};

// Makes `SystemClipboard` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerSystemClipboard();

} // namespace omaweb
