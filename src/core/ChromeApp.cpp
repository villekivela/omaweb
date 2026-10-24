#include "ChromeApp.h"

namespace omaweb {

QString ChromeApp::scriptSource()
{
    return QStringLiteral(R"JS((() => {
    const chrome = window.chrome || (window.chrome = {});
    if (chrome.app)
        return;
    chrome.app = {
        isInstalled: false,
        InstallState: {
            DISABLED: 'disabled',
            INSTALLED: 'installed',
            NOT_INSTALLED: 'not_installed'
        },
        RunningState: {
            CANNOT_RUN: 'cannot_run',
            READY_TO_RUN: 'ready_to_run',
            RUNNING: 'running'
        },
        getDetails() { return null; },
        getIsInstalled() { return false; },
        installState(callback) { if (callback) callback('not_installed'); },
        runningState() { return 'cannot_run'; }
    };
})();
)JS");
}

} // namespace omaweb
