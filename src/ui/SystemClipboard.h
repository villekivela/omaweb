#pragma once

#include <QObject>
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
};

// Makes `SystemClipboard` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerSystemClipboard();

} // namespace omaweb
