#pragma once

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

namespace omaweb {

// The platform's own print dialog, and the temporary file a page is rendered
// into on the way to it. Printing is a window-system service like the
// clipboard: the engine adapter renders a page to PDF and knows nothing about
// where it goes, and the desktop's print panel — with its PDF destination — is
// what the reader actually answers.
//
// Where the running platform has no print panel, `available` is false and the
// print command is unavailable rather than silently doing nothing.
class PagePrinter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)

public:
    explicit PagePrinter(QObject *parent = nullptr);

    bool available() const;

    // A path in a Omaweb-owned temporary directory for the adapter to render
    // into. Empty when no such directory can be made, which is the shell's cue
    // to report the failure rather than ask for a print that goes nowhere.
    Q_INVOKABLE QString reserveDestination(const QString &name) const;
    // Presents the rendered document in the platform's print dialog and takes
    // the temporary file away afterwards. False means the reader never saw a
    // dialog; a reader who cancels one has printed nothing and is not a failure.
    //
    // The window is the one the reader asked from, so the dialog stands over it.
    // Passing none still prints, from wherever the desktop puts an unparented
    // dialog.
    Q_INVOKABLE bool present(
        const QString &path, const QString &jobName, QWindow *window = nullptr);
    // For a render that never arrived: nothing to present, nothing to keep.
    Q_INVOKABLE void discard(const QString &path);
};

// Makes `PagePrinter` available to QML as `import Omaweb`. Call once per
// process, before loading QML that uses it.
void registerPagePrinter();

} // namespace omaweb
