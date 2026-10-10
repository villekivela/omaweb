#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QWindow>

namespace omaweb {

// Where the window holding an extension's page is, which an extension reads
// through chrome.windows and places a window of its own from (#684). Bitwarden
// opens its passkey prompt at the right edge of the window it is told about,
// and with no numbers there it opens nothing. The engine asks the application,
// with the page's QtWebEngine view or with no page for a call from a worker.
class QtExtensionWindows final : public QObject {
    Q_OBJECT

public:
    enum class Positions {
        // The platform says where a window is.
        FromPlatform,
        // It does not, as on Wayland: a window is reported at the top left.
        Unknown,
    };

    explicit QtExtensionWindows(QObject *parent = nullptr);
    explicit QtExtensionWindows(Positions positions, QObject *parent = nullptr);

    // The window a call with no page behind it is answered with.
    void setMainWindow(QWindow *window);

    // The frame of the window the page is drawn in, or of the main window when
    // there is no page or it is drawn nowhere.
    QRect geometryOf(const QObject *page) const;

public slots:
    // Called for every profile Content blocking attaches to, which is every
    // profile there is. Answers its extensions' chrome.windows calls where
    // the engine lets the application, and does nothing where it does not.
    void attachToProfile(QObject *profile);

private:
    Positions m_positions;
    QPointer<QWindow> m_mainWindow;
};

} // namespace omaweb
