#pragma once

#include <QObject>
#include <QPointF>
#include <QString>

class QQuickItem;
class QQuickWindow;

namespace omaweb {

// Input an Agent sends a page, as Qt events to the item QtWebEngine draws
// the page in, which a page sees as trusted (ADR 0051, #376). It never takes
// the reader's focus: a press moves Qt's keyboard focus to the view it lands
// on, so a click gives it straight back, and neither page hears that it moved.
//
// The moment's move is still seen by the window's own items, which hear
// their focus go and come back. So `focusPlace` says where the reader's focus
// is, and a click waits while it is in the interface rather than in a page.
class QtAgentInput final : public QObject {
    Q_OBJECT
    // The page's half of the verbs, `agent-page.js`, as the adapter runs it.
    Q_PROPERTY(QString pageScript READ pageScript CONSTANT)

public:
    explicit QtAgentInput(QObject *parent = nullptr);

    QString pageScript() const;

    // A press and release at `point`, in the view's coordinates. False when
    // the view has no page to take it.
    Q_INVOKABLE bool click(QQuickItem *view, QPointF point);
    // Each character as a key press and release, into whatever the page has
    // focused. A newline is Return and a tab is Tab.
    Q_INVOKABLE bool typeText(QQuickItem *view, const QString &text);
    // One key by name, with modifiers before it: `Enter`, `Tab`, `Escape`,
    // `ArrowDown`, `Control+a`. False for a name Omaweb does not know.
    Q_INVOKABLE bool pressKey(QQuickItem *view, const QString &name);
    // Tells Chromium the view's size again. Qt drops a size change made while
    // a view is hidden, so a page that arrived then is shown at none until
    // something resizes it.
    Q_INVOKABLE void refreshGeometry(QQuickItem *view);
    // "page" while the reader's keyboard focus is in a page, "interface"
    // while it is in the window's own items, and "none" when nothing has it
    // or it rests on an item that says `pageFocusRest: true`.
    Q_INVOKABLE QString focusPlace(QQuickWindow *window) const;

private:
    QString m_pageScript;
};

void registerQtAgentInput();

} // namespace omaweb
