#include "DefaultBrowser.h"

#include <QProcess>
#include <QStandardPaths>

namespace omaweb {
namespace {

    // The desktop entry Omaweb installs, which is what a desktop records
    // against `x-scheme-handler/http` when it opens links with this browser.
    const auto kDesktopEntry = QStringLiteral("omaweb.desktop");

    // `xdg-settings` rather than writing `mimeapps.list` here. The file is only
    // where most desktops keep the answer; the tool knows the ones that keep it
    // somewhere else, and editing a shared file from inside a browser would put
    // Omaweb in the business of parsing whatever else is in it.
    QString settingsTool()
    {
        static const QString path = QStandardPaths::findExecutable(QStringLiteral("xdg-settings"));
        return path;
    }

    // Long enough for a tool that may consult a settings daemon, short enough
    // that a wedged one does not hold the interface.
    constexpr int kTimeoutMs = 4000;

    bool run(const QStringList &arguments, QString *output = nullptr)
    {
        const auto tool = settingsTool();
        if (tool.isEmpty()) {
            return false;
        }
        QProcess process;
        process.start(tool, arguments);
        if (!process.waitForFinished(kTimeoutMs)) {
            process.kill();
            process.waitForFinished(500);
            return false;
        }
        if (output) {
            *output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
        }
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

} // namespace

DefaultBrowser::DefaultBrowser(QObject *parent)
    : QObject(parent)
{
}

bool DefaultBrowser::available() const { return !settingsTool().isEmpty(); }

bool DefaultBrowser::isDefault() const
{
    QString answer;
    if (!run({QStringLiteral("get"), QStringLiteral("default-web-browser")}, &answer)) {
        return false;
    }
    return answer == kDesktopEntry;
}

bool DefaultBrowser::makeDefault()
{
    if (!run({QStringLiteral("set"), QStringLiteral("default-web-browser"), kDesktopEntry})) {
        return false;
    }
    emit changed();
    return isDefault();
}

void DefaultBrowser::refresh() { emit changed(); }

} // namespace omaweb
