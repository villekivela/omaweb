#pragma once

#include <QQuickItem>

#include <cstring>

namespace omaweb {

// Whether `item` is the one Chromium's input and frames go through. Qt makes it a child of the
// view and does not export its type, so it is known by its class name.
inline bool isPageInput(const QQuickItem *item)
{
    return item != nullptr
        && std::strstr(item->metaObject()->className(), "RenderWidgetHostViewQtDelegateItem")
        != nullptr;
}

} // namespace omaweb
