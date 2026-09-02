#include "SystemClipboard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QQmlEngine>

namespace tanto {

SystemClipboard::SystemClipboard(QObject *parent)
    : QObject(parent)
{
}

bool SystemClipboard::copyText(const QString &text)
{
    auto *clipboard = QGuiApplication::clipboard();
    if (!clipboard || text.isEmpty()) {
        return false;
    }
    clipboard->setText(text);
    return true;
}

QString SystemClipboard::text() const
{
    auto *clipboard = QGuiApplication::clipboard();
    return clipboard ? clipboard->text() : QString{};
}

void registerSystemClipboard()
{
    qmlRegisterSingletonType<SystemClipboard>("Tanto", 1, 0, "SystemClipboard",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemClipboard; });
}

} // namespace tanto
