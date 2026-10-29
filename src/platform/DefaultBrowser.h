#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QPointer>

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
    // Not constant: the reader can change this from outside the browser. It is
    // read when the object is made, and again by `refresh` and `makeDefault`.
    Q_PROPERTY(bool isDefault READ isDefault NOTIFY changed)
    // False until the desktop first answers, so nothing is said before then.
    Q_PROPERTY(bool known READ known NOTIFY changed)

public:
    explicit DefaultBrowser(QObject *parent = nullptr);

    bool available() const;
    bool isDefault() const;
    bool known() const;

    // Asks the desktop to open links with Omaweb. Only ever called from an
    // explicit action by the reader. False means the desktop refused and the
    // setting stands where it was.
    Q_INVOKABLE bool makeDefault();
    // Re-reads the desktop's setting, for a reader who changed it elsewhere
    // while the browser was open. The tools can take longer than a frame, so
    // this read, and the first, run off the interface's thread, and `changed`
    // follows when the answer differs. Settings calls it each time it opens.
    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    void setDefault(bool isDefault);

    bool m_isDefault = false;
    bool m_known = false;
    // The read in flight. A newer one, or setting the default, deletes it, so
    // an older answer never lands after a newer one.
    QPointer<QFutureWatcher<bool>> m_reading;
};

// Makes `DefaultBrowser` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerDefaultBrowser();

} // namespace omaweb
