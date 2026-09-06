#include "RunningBrowser.h"

namespace omaweb {

// macOS hands a second launch to the running application itself, through the
// bundle and an Apple Event rather than through anything Omaweb registers, so
// there is no name to claim here and no launch to forward. Every process is the
// browser that answers, which is what the development build has always done.
RunningBrowser::RunningBrowser(QObject *parent)
    : QObject(parent)
{
}

RunningBrowser::~RunningBrowser() = default;

bool RunningBrowser::isPrimary() const { return m_primary; }

bool RunningBrowser::handOver(const QUrl &) { return false; }

} // namespace omaweb
