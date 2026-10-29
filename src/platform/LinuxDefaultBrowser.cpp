#include "DefaultBrowser.h"

#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrentRun>

namespace omaweb {
namespace {

    // The desktop entry Omaweb installs, which is what a desktop records
    // against `x-scheme-handler/http` when it opens links with this browser.
    const auto kDesktopEntry = QStringLiteral("omaweb.desktop");

    // `xdg-settings` rather than writing `mimeapps.list` here. The file is only
    // where most desktops keep the answer; the tool knows the ones that keep it
    // somewhere else, and editing a shared file from inside a browser would put
    // Omaweb in the business of parsing whatever else is in it. Looked up on
    // each use, not cached, so a test can put its own on `PATH`.
    QString settingsTool()
    {
        return QStandardPaths::findExecutable(QStringLiteral("xdg-settings"));
    }

    // On Omarchy, `xdg-settings` names Omarchy's router,
    // `omarchy-browser.desktop`, because Hyprland's own `mimeapps.list` is read
    // first and routes links through it. The router opens the browser Omarchy
    // records behind it, and this command names that one.
    QString omarchyTool()
    {
        return QStandardPaths::findExecutable(QStringLiteral("omarchy-cmd-default-browser"));
    }

    // Long enough for a tool that may consult a settings daemon, short enough
    // that a wedged one does not hold the interface.
    constexpr int kTimeoutMs = 4000;

    bool run(const QString &tool, const QStringList &arguments,
        const QProcessEnvironment &environment, QString *output = nullptr)
    {
        if (tool.isEmpty()) {
            return false;
        }
        QProcess process;
        process.setProcessEnvironment(environment);
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

    bool readDefault()
    {
        const auto environment = QProcessEnvironment::systemEnvironment();
        const auto omarchy = omarchyTool();
        QString answer;
        const bool answered = omarchy.isEmpty()
            ? run(settingsTool(), {QStringLiteral("get"), QStringLiteral("default-web-browser")},
                  environment, &answer)
            : run(omarchy, {}, environment, &answer);
        return answered && answer == kDesktopEntry;
    }

} // namespace

DefaultBrowser::DefaultBrowser(QObject *parent)
    : QObject(parent)
{
    refresh();
}

bool DefaultBrowser::available() const { return !settingsTool().isEmpty(); }

bool DefaultBrowser::isDefault() const { return m_isDefault; }

bool DefaultBrowser::known() const { return m_known; }

bool DefaultBrowser::makeDefault()
{
    delete m_reading;
    auto environment = QProcessEnvironment::systemEnvironment();
    if (!omarchyTool().isEmpty()) {
        // What `omarchy-default-browser` does, which takes only the browsers
        // on its own list. With the desktop named, `xdg-settings` checks what
        // it set against Hyprland's list, which still names the router, and
        // fails. `BROWSER` is unset because the session points it at
        // Omarchy's launcher, which the tool would answer with instead.
        environment.remove(QStringLiteral("BROWSER"));
        environment.insert(QStringLiteral("XDG_CURRENT_DESKTOP"), QString());
    }
    if (!run(settingsTool(),
            {QStringLiteral("set"), QStringLiteral("default-web-browser"), kDesktopEntry},
            environment)) {
        return false;
    }
    setDefault(readDefault());
    return m_isDefault;
}

void DefaultBrowser::refresh()
{
    delete m_reading;
    m_reading = new QFutureWatcher<bool>(this);
    connect(m_reading, &QFutureWatcherBase::finished, this, [this, reading = m_reading.data()] {
        setDefault(reading->result());
        reading->deleteLater();
    });
    m_reading->setFuture(QtConcurrent::run(readDefault));
}

void DefaultBrowser::setDefault(bool isDefault)
{
    if (m_known && m_isDefault == isDefault) {
        return;
    }
    m_known = true;
    m_isDefault = isDefault;
    emit changed();
}

} // namespace omaweb
