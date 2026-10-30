#include "OmarchyBar.h"

#include "OmarchyTheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>

namespace omaweb {
namespace {

    // The tabs after the workspaces, or at the end of the left where the reader
    // has none, and the location first in the center. Omarchy 4.0.6 has no
    // section before the center's anchor, so first in the center is as near
    // the middle as the location can ask to be. Any earlier placement of the
    // widget goes first, so placing it twice places it once.
    const auto placeFilter = QStringLiteral(R"(
.bar.layout.left |= ((. // []) | map(select(.id != $id))
  | (map(.id) | index("omarchy.workspaces")) as $at
  | if $at == null then . + [{id: $id, role: "tabs"}]
    else .[: $at + 1] + [{id: $id, role: "tabs"}] + .[$at + 1 :] end)
| .bar.layout.center = [{id: $id, role: "location"}]
    + ((.bar.layout.center // []) | map(select(.id != $id)))
| .bar.layout.right |= ((. // []) | map(select(.id != $id)))
)");

    const auto removeFilter = QStringLiteral(R"(
if (.bar.layout | type) == "object"
then .bar.layout |= map_values(if type == "array" then map(select(.id != $id)) else . end)
else . end
)");

    // An IPC call the shell does not answer in this long is one it will not.
    constexpr int commandTimeoutMs = 5000;

} // namespace

OmarchyBarPaths OmarchyBarPaths::fromEnvironment()
{
    const QDir configuration(OmarchyThemePaths::fromEnvironment().configuration);
    auto omarchy = qEnvironmentVariable("OMARCHY_PATH");
    if (omarchy.isEmpty()) {
        omarchy = QStringLiteral("/usr/share/omarchy");
    }
    return {
        .plugins = configuration.filePath(QStringLiteral("plugins")),
        .shellConfig = configuration.filePath(QStringLiteral("shell.json")),
        .defaultShellConfig = QDir(omarchy).filePath(QStringLiteral("config/omarchy/shell.json")),
    };
}

QString OmarchyBarPaths::link() const
{
    return QDir(plugins).filePath(QLatin1StringView(omarchyBarWidgetId));
}

OmarchyCommand runOmarchyCommand()
{
    return [](const QString &program, const QStringList &arguments, const QByteArray &input,
               std::function<void(int, const QByteArray &)> done) {
        auto *process = new QProcess;
        auto answer
            = std::make_shared<std::function<void(int, const QByteArray &)>>(std::move(done));
        QObject::connect(process, &QProcess::finished, process, [process, answer](int exitCode) {
            if (*answer) {
                (*answer)(exitCode, process->readAllStandardOutput());
                *answer = nullptr;
            }
            process->deleteLater();
        });
        QObject::connect(process, &QProcess::errorOccurred, process,
            [process, answer](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart && error != QProcess::Timedout) {
                    return;
                }
                if (*answer) {
                    (*answer)(-1, {});
                    *answer = nullptr;
                }
                process->deleteLater();
            });
        QTimer::singleShot(commandTimeoutMs, process, [process] { process->kill(); });
        process->start(program, arguments);
        if (!input.isEmpty()) {
            process->write(input);
        }
        process->closeWriteChannel();
    };
}

OmarchyBarPlacement::OmarchyBarPlacement(
    OmarchyBarPaths paths, QString shippedWidget, OmarchyCommand command, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_shippedWidget(std::move(shippedWidget))
    , m_command(std::move(command))
{
}

bool OmarchyBarPlacement::available() const { return m_available; }

void OmarchyBarPlacement::checkAvailable()
{
    m_command(QStringLiteral("omarchy-shell"), {QStringLiteral("shell"), QStringLiteral("ping")},
        {}, [this](int exitCode, const QByteArray &output) {
            const auto answered = exitCode == 0 && !output.trimmed().isEmpty();
            if (m_available != answered) {
                m_available = answered;
                emit availableChanged();
            }
        });
}

void OmarchyBarPlacement::place()
{
    if (!link()) {
        return;
    }
    // The shell finds a plugin by scanning, which it does on its own time;
    // the layout naming the widget waits for it rather than failing.
    shell(QStringLiteral("rescanPlugins"));
    editLayout(placeFilter, [this] { shell(QStringLiteral("reloadConfig")); });
}

void OmarchyBarPlacement::remove()
{
    editLayout(removeFilter, [this] {
        shell(QStringLiteral("reloadConfig"));
        const QFileInfo link(m_paths.link());
        if (link.isSymLink()) {
            QFile::remove(link.filePath());
            shell(QStringLiteral("rescanPlugins"));
        } else if (link.exists()) {
            qWarning("%s is not the link Omaweb made, so it is left where it is",
                qPrintable(link.filePath()));
        }
    });
}

void OmarchyBarPlacement::relink() { link(); }

// A plugin directory of that name that is not a link is the reader's, such as
// a copy they are working on, and Omaweb leaves it be.
bool OmarchyBarPlacement::link()
{
    const auto path = m_paths.link();
    const QFileInfo existing(path);
    if (existing.isSymLink()) {
        if (existing.symLinkTarget() == QFileInfo(m_shippedWidget).absoluteFilePath()) {
            return true;
        }
        QFile::remove(path);
    } else if (existing.exists()) {
        qWarning(
            "%s is not a link Omaweb made, so the bar keeps the widget there", qPrintable(path));
        return true;
    }
    if (!QDir().mkpath(m_paths.plugins) || !QFile::link(m_shippedWidget, path)) {
        qWarning("The bar widget could not be linked into %s", qPrintable(m_paths.plugins));
        return false;
    }
    return true;
}

void OmarchyBarPlacement::editLayout(const QString &filter, std::function<void()> then)
{
    auto layout = readFile(m_paths.shellConfig);
    if (layout.isEmpty()) {
        layout = readFile(m_paths.defaultShellConfig);
    }
    if (layout.isEmpty()) {
        layout = "{}";
    }
    m_command(QStringLiteral("jq"),
        {QStringLiteral("--arg"), QStringLiteral("id"), QLatin1StringView(omarchyBarWidgetId),
            QStringLiteral("--indent"), QStringLiteral("2"), filter},
        layout, [this, then = std::move(then)](int exitCode, const QByteArray &edited) {
            QSaveFile file(m_paths.shellConfig);
            if (exitCode != 0 || edited.trimmed().isEmpty()
                || !QDir().mkpath(QFileInfo(m_paths.shellConfig).absolutePath())
                || !file.open(QIODevice::WriteOnly) || file.write(edited) != edited.size()
                || !file.commit()) {
                qWarning("The bar's layout in %s could not be changed (jq exited %d)",
                    qPrintable(m_paths.shellConfig), exitCode);
                return;
            }
            then();
        });
}

void OmarchyBarPlacement::shell(const QString &method)
{
    m_command(QStringLiteral("omarchy-shell"),
        {QStringLiteral("-q"), QStringLiteral("shell"), method}, {},
        [](int, const QByteArray &) { });
}

} // namespace omaweb
