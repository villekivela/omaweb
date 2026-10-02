#include "LauncherMenu.h"

#include "BrowserController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <utility>

namespace omaweb {
namespace {

    constexpr auto enabledKey = "launcher-tabs";

    // The file's name in both places, and the directory a package puts it in. A link whose target
    // ends this way is one Omaweb made, whichever prefix it was installed under.
    const auto menuFileName = QStringLiteral("omawebtabs.lua");
    const auto packagedSuffix = QStringLiteral("/omaweb/elephant/menus/omawebtabs.lua");

    bool isOwnLink(const QFileInfo &entry)
    {
        return entry.isSymLink() && entry.symLinkTarget().endsWith(packagedSuffix);
    }

} // namespace

LauncherMenuPaths LauncherMenuPaths::forThisMachine()
{
    return {
        .source = QDir(QCoreApplication::applicationDirPath())
            .absoluteFilePath(QStringLiteral("../share/omaweb/elephant/menus/") + menuFileName),
        .directory = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
            .filePath(QStringLiteral("elephant/menus")),
        .elephantInstalled = !QStandardPaths::findExecutable(QStringLiteral("elephant")).isEmpty(),
    };
}

void updateLauncherLink(const LauncherMenuPaths &paths, bool enabled)
{
    if (!paths.elephantInstalled) {
        return;
    }
    const auto linkPath = QDir(paths.directory).filePath(menuFileName);
    const QFileInfo there(linkPath);
    if (!enabled) {
        if (isOwnLink(there)) {
            QFile::remove(linkPath);
        }
        return;
    }
    // A build tree has no package beside it, and a link to nothing helps nobody.
    const auto source = QFileInfo(paths.source).absoluteFilePath();
    if (!QFileInfo::exists(source)) {
        return;
    }
    if (there.isSymLink() || there.exists()) {
        if (!isOwnLink(there) || there.symLinkTarget() == source) {
            return;
        }
        if (!QFile::remove(linkPath)) {
            return;
        }
    }
    if (!QDir().mkpath(paths.directory)) {
        return;
    }
    QFile::link(source, linkPath);
}

LauncherMenu::LauncherMenu(BrowserController *browser, LauncherMenuPaths paths, QObject *parent)
    : QObject(parent)
    , m_browser(browser)
    , m_paths(std::move(paths))
{
    updateLauncherLink(m_paths, enabled());
}

bool LauncherMenu::available() const { return m_paths.elephantInstalled; }

bool LauncherMenu::enabled() const
{
    return m_browser->preference(QString::fromLatin1(enabledKey), QStringLiteral("true"))
        != QStringLiteral("false");
}

void LauncherMenu::setEnabled(bool enabled)
{
    if (enabled == this->enabled()) {
        return;
    }
    m_browser->setPreference(QString::fromLatin1(enabledKey),
        enabled ? QStringLiteral("true") : QStringLiteral("false"));
    updateLauncherLink(m_paths, enabled);
    emit enabledChanged();
}

} // namespace omaweb
