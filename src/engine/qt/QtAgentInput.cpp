#include "QtAgentInput.h"

#include <QCoreApplication>
#include <QFile>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointingDevice>
#include <QQmlEngine>
#include <QQuickItem>
#include <QPointer>
#include <QQuickWindow>

#include <cstring>
#include <optional>

namespace omaweb {
namespace {

    // The item Chromium's input and frames go through. Qt makes it a child of
    // the view and does not export its type, so it is found by class name.
    bool isDelegate(const QQuickItem *item)
    {
        return std::strstr(item->metaObject()->className(), "RenderWidgetHostViewQtDelegateItem")
            != nullptr;
    }

    QQuickItem *delegateOf(QQuickItem *item)
    {
        if (item == nullptr) {
            return nullptr;
        }
        if (isDelegate(item)) {
            return item;
        }
        const auto children = item->childItems();
        for (auto *child : children) {
            if (auto *found = delegateOf(child)) {
                return found;
            }
        }
        return nullptr;
    }

    void collectDelegates(QQuickItem *item, QList<QQuickItem *> &found)
    {
        const auto children = item->childItems();
        for (auto *child : children) {
            if (isDelegate(child)) {
                found.append(child);
            }
            collectDelegates(child, found);
        }
    }

    // Swallows the delegates' focus events while a press takes Qt's focus and
    // the previous item is given it back. Qt Quick sends focus changes to an
    // item as events, so neither page's Chromium hears of either.
    class FocusSilence final : public QObject {
    public:
        explicit FocusSilence(QQuickWindow *window)
        {
            collectDelegates(window->contentItem(), m_delegates);
            for (auto *delegate : std::as_const(m_delegates)) {
                delegate->installEventFilter(this);
            }
        }
        ~FocusSilence() override
        {
            for (auto *delegate : std::as_const(m_delegates)) {
                delegate->removeEventFilter(this);
            }
        }
        FocusSilence(const FocusSilence &) = delete;
        FocusSilence &operator=(const FocusSilence &) = delete;

    protected:
        bool eventFilter(QObject *, QEvent *event) override
        {
            return event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut;
        }

    private:
        QList<QQuickItem *> m_delegates;
    };

    struct NamedKey {
        Qt::Key key;
        QString text;
    };

    std::optional<NamedKey> namedKey(const QString &name)
    {
        static const QHash<QString, NamedKey> keys {
            {QStringLiteral("enter"), {Qt::Key_Return, QStringLiteral("\r")}},
            {QStringLiteral("return"), {Qt::Key_Return, QStringLiteral("\r")}},
            {QStringLiteral("tab"), {Qt::Key_Tab, QStringLiteral("\t")}},
            {QStringLiteral("escape"), {Qt::Key_Escape, {}}},
            {QStringLiteral("esc"), {Qt::Key_Escape, {}}},
            {QStringLiteral("backspace"), {Qt::Key_Backspace, {}}},
            {QStringLiteral("delete"), {Qt::Key_Delete, {}}},
            {QStringLiteral("space"), {Qt::Key_Space, QStringLiteral(" ")}},
            {QStringLiteral("arrowup"), {Qt::Key_Up, {}}},
            {QStringLiteral("arrowdown"), {Qt::Key_Down, {}}},
            {QStringLiteral("arrowleft"), {Qt::Key_Left, {}}},
            {QStringLiteral("arrowright"), {Qt::Key_Right, {}}},
            {QStringLiteral("up"), {Qt::Key_Up, {}}},
            {QStringLiteral("down"), {Qt::Key_Down, {}}},
            {QStringLiteral("left"), {Qt::Key_Left, {}}},
            {QStringLiteral("right"), {Qt::Key_Right, {}}},
            {QStringLiteral("home"), {Qt::Key_Home, {}}},
            {QStringLiteral("end"), {Qt::Key_End, {}}},
            {QStringLiteral("pageup"), {Qt::Key_PageUp, {}}},
            {QStringLiteral("pagedown"), {Qt::Key_PageDown, {}}},
        };
        const auto lowered = name.toLower();
        if (const auto found = keys.constFind(lowered); found != keys.cend()) {
            return *found;
        }
        if (lowered.size() >= 2 && lowered.size() <= 3 && lowered.startsWith(u'f')) {
            bool number = false;
            const auto index = lowered.mid(1).toInt(&number);
            if (number && index >= 1 && index <= 12) {
                return NamedKey {static_cast<Qt::Key>(Qt::Key_F1 + index - 1), {}};
            }
        }
        if (name.size() == 1) {
            return NamedKey {static_cast<Qt::Key>(name.at(0).toUpper().unicode()), name};
        }
        return std::nullopt;
    }

    // What a window would deliver for one key: ShortcutOverride first, which
    // is where the delegate works out an editing command such as select-all,
    // then the press and the release.
    void sendKey(
        QQuickItem *delegate, int key, Qt::KeyboardModifiers modifiers, const QString &text)
    {
        QKeyEvent shortcut(QEvent::ShortcutOverride, key, modifiers, text);
        QCoreApplication::sendEvent(delegate, &shortcut);
        QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
        QCoreApplication::sendEvent(delegate, &press);
        QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
        QCoreApplication::sendEvent(delegate, &release);
    }

} // namespace

QtAgentInput::QtAgentInput(QObject *parent)
    : QObject(parent)
{
    QFile script(QStringLiteral(":/omaweb/engine-qt/agent-page.js"));
    if (script.open(QIODevice::ReadOnly)) {
        m_pageScript = QString::fromUtf8(script.readAll());
    }
}

QString QtAgentInput::pageScript() const { return m_pageScript; }

bool QtAgentInput::click(QQuickItem *view, QPointF point)
{
    auto *delegate = delegateOf(view);
    auto *window = delegate ? delegate->window() : nullptr;
    if (delegate == nullptr || window == nullptr) {
        return false;
    }
    const QPointer<QQuickItem> previous = window->activeFocusItem();
    const auto local = delegate->mapFromItem(view, point);
    const auto scene = delegate->mapToScene(local);
    const auto global = delegate->mapToGlobal(local);
    const auto *mouse = QPointingDevice::primaryPointingDevice();
    {
        // All of it in one turn of the event loop, so no input of the reader's
        // arrives while the view holds their focus.
        const FocusSilence silence(window);
        QMouseEvent move(QEvent::MouseMove, local, scene, global, Qt::NoButton, Qt::NoButton,
            Qt::NoModifier, mouse);
        QCoreApplication::sendEvent(delegate, &move);
        QMouseEvent press(QEvent::MouseButtonPress, local, scene, global, Qt::LeftButton,
            Qt::LeftButton, Qt::NoModifier, mouse);
        QCoreApplication::sendEvent(delegate, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, local, scene, global, Qt::LeftButton,
            Qt::NoButton, Qt::NoModifier, mouse);
        QCoreApplication::sendEvent(delegate, &release);
        if (previous && previous != window->contentItem()) {
            previous->forceActiveFocus(Qt::OtherFocusReason);
        } else {
            // Nothing had the keyboard, and the window's own item hands focus
            // down the chain the press just made, so the chain is undone.
            for (auto *item = delegate; item && item != window->contentItem();
                item = item->parentItem()) {
                item->setFocus(false);
            }
        }
    }
    return true;
}

bool QtAgentInput::typeText(QQuickItem *view, const QString &text)
{
    auto *delegate = delegateOf(view);
    if (delegate == nullptr) {
        return false;
    }
    for (const auto character : text) {
        if (character == u'\n') {
            sendKey(delegate, Qt::Key_Return, Qt::NoModifier, QStringLiteral("\r"));
        } else if (character == u'\t') {
            sendKey(delegate, Qt::Key_Tab, Qt::NoModifier, QStringLiteral("\t"));
        } else {
            sendKey(delegate, character.toUpper().unicode(), Qt::NoModifier, QString(character));
        }
    }
    return true;
}

bool QtAgentInput::pressKey(QQuickItem *view, const QString &name)
{
    auto *delegate = delegateOf(view);
    if (delegate == nullptr) {
        return false;
    }
    auto parts = name.split(u'+');
    // `Control++` is Control and the plus key.
    if (name.endsWith(QStringLiteral("++"))) {
        parts.removeLast();
        parts.last() = QStringLiteral("+");
    }
    Qt::KeyboardModifiers modifiers;
    for (qsizetype index = 0; index + 1 < parts.size(); ++index) {
        const auto modifier = parts.at(index).toLower();
        if (modifier == u"control" || modifier == u"ctrl") {
            modifiers |= Qt::ControlModifier;
        } else if (modifier == u"shift") {
            modifiers |= Qt::ShiftModifier;
        } else if (modifier == u"alt") {
            modifiers |= Qt::AltModifier;
        } else if (modifier == u"meta") {
            modifiers |= Qt::MetaModifier;
        } else {
            return false;
        }
    }
    const auto key = namedKey(parts.constLast());
    if (!key) {
        return false;
    }
    // A modified key types nothing, which is what keeps Control+a a command.
    const auto text = modifiers & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)
        ? QString()
        : key->text;
    sendKey(delegate, key->key, modifiers, text);
    return true;
}

void QtAgentInput::refreshGeometry(QQuickItem *view)
{
    if (auto *delegate = delegateOf(view)) {
        delegate->polish();
    }
}

QString QtAgentInput::focusPlace(QQuickWindow *window) const
{
    auto *item = window ? window->activeFocusItem() : nullptr;
    if (item == nullptr || item == window->contentItem()) {
        return QStringLiteral("none");
    }
    for (auto *ancestor = item; ancestor != nullptr; ancestor = ancestor->parentItem()) {
        if (isDelegate(ancestor)
            || std::strstr(ancestor->metaObject()->className(), "WebEngineView") != nullptr) {
            return QStringLiteral("page");
        }
        // Where the shell rests the keyboard when no page has it, such as the
        // Start page standing in for a blank tab. Nothing there hears focus
        // come and go, so the reader is in no control a click would disturb.
        if (ancestor->property("pageFocusRest").toBool()) {
            return QStringLiteral("none");
        }
    }
    return QStringLiteral("interface");
}

void registerQtAgentInput()
{
    qmlRegisterSingletonType<QtAgentInput>("Omaweb.Engine", 1, 0, "QtAgentInput",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new QtAgentInput; });
}

} // namespace omaweb
