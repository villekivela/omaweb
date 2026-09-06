#pragma once

#include <QObject>

namespace omaweb {

// Whether the desktop opens links with Omaweb, and the one way to ask that it
// should.
//
// Being the default browser is the desktop's setting rather than the browser's,
// so Omaweb reads it and offers to change it, and never changes it because it
// happened to start. A browser that quietly takes the setting on first run has
// taken something the reader did not give, and the reader is the only one who
// knows what they were using before.
//
// `available` is false where the desktop offers no way to ask, and the offer is
// not made rather than being made and failing.
class DefaultBrowser final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    // Not constant: the reader can change this from outside the browser, and
    // the answer here follows whenever it is asked for again.
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)

public:
    explicit DefaultBrowser(QObject *parent = nullptr);

    bool available() const;
    bool isDefault() const;

    // Asks the desktop to open links with Omaweb. Only ever called from an
    // explicit action by the reader. False means the desktop refused and the
    // setting stands where it was.
    Q_INVOKABLE bool makeDefault();
    // Re-reads the desktop's setting, for a reader who changed it elsewhere
    // while the browser was open.
    Q_INVOKABLE void refresh();

signals:
    void changed();
};

// Makes `DefaultBrowser` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerDefaultBrowser();

} // namespace omaweb
