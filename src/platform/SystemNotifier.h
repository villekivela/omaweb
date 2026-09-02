#pragma once

#include <QObject>
#include <QString>

namespace tanto {

// The desktop's own notifications. A page that asks to interrupt the reader is
// answered by the window system rather than by a surface Tanto draws: the
// reader is looking at something else when it matters, and only the desktop can
// reach them there.
//
// Tanto supplies the text — which always names the origin and the Space — and
// hears back which notification the reader answered. It hands out a key of its
// own rather than the desktop's identifier, because the page waiting for the
// answer belongs to a tab in a Space and nothing about that is the desktop's to
// keep.
//
// Where the running platform has no notification service, `available` is false
// and a page's request goes unanswered rather than being silently swallowed as
// though the reader had seen it.
class SystemNotifier final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit SystemNotifier(QObject *parent = nullptr);
    ~SystemNotifier() override;

    bool available() const;

    // False means the desktop showed nothing, which the caller has to treat as
    // the page never having been answered.
    Q_INVOKABLE bool present(const QString &key, const QString &title, const QString &body);
    // For a notification whose page, tab or Space has gone: nothing left to
    // answer, so nothing left on screen.
    Q_INVOKABLE void withdraw(const QString &key);

signals:
    void activated(const QString &key);
    void dismissed(const QString &key);
};

// Makes `SystemNotifier` available to QML as `import Tanto`. Call once per
// process, before loading QML that uses it.
void registerSystemNotifier();

} // namespace tanto
