#include "OmarchyBar.h"

#include "OmarchyTheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QProcess>
#include <QSaveFile>
#include <QTimer>

namespace omaweb {
namespace {

    // The widget first in the center. Omarchy 4.0.6 has no section before the
    // center's anchor, so first in the center is as near the middle as it can
    // ask to be. Any earlier placement goes first, so placing it twice places
    // it once, and the pair Omaweb once placed, its tabs on the left and the
    // Location in the center, leaves one widget with the tabs' Space names.
    const auto placeFilter = QStringLiteral(R"(
(first((.bar.layout // {}) | .[]? | arrays | .[] | select(.id == $id) | .spaceNames | strings)
  // null) as $names
| .bar.layout.left |= ((. // []) | map(select(.id != $id)))
| .bar.layout.center = [{id: $id} + (if $names == null then {} else {spaceNames: $names} end)]
    + ((.bar.layout.center // []) | map(select(.id != $id)))
| .bar.layout.right |= ((. // []) | map(select(.id != $id)))
)");

    const auto removeFilter = QStringLiteral(R"(
if (.bar.layout | type) == "object"
then .bar.layout |= map_values(if type == "array" then map(select(.id != $id)) else . end)
else . end
)");

    // The widget's own setting, wherever the reader has moved it.
    const auto spaceNamesFilter = QStringLiteral(R"(
if (.bar.layout | type) == "object"
then .bar.layout |= map_values(if type == "array"
  then map(if .id == $id then .spaceNames = "%1" else . end) else . end)
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
                if (error != QProcess::FailedToStart) {
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
        {}, [guard = QPointer(this)](int exitCode, const QByteArray &output) {
            if (!guard) {
                return;
            }
            const auto answered = exitCode == 0 && !output.trimmed().isEmpty();
            auto *self = guard.data();
            if (self->m_available != answered) {
                self->m_available = answered;
                emit self->availableChanged();
            }
        });
}

void OmarchyBarPlacement::place()
{
    enqueue([this](const std::function<void()> &next) {
        if (!link()) {
            next();
            return;
        }
        // The shell finds a plugin by scanning, which it does on its own
        // time; the layout naming the widget waits for it rather than failing.
        shell(QStringLiteral("rescanPlugins"));
        editLayout(placeFilter, [this, next] {
            shell(QStringLiteral("reloadConfig"));
            next();
        });
    });
}

void OmarchyBarPlacement::remove()
{
    enqueue([this](const std::function<void()> &next) {
        editLayout(removeFilter, [this, next] {
            shell(QStringLiteral("reloadConfig"));
            const QFileInfo link(m_paths.link());
            if (link.isSymLink()) {
                QFile::remove(link.filePath());
                shell(QStringLiteral("rescanPlugins"));
            } else if (link.exists()) {
                qWarning("%s is not the link Omaweb made, so it is left where it is",
                    qPrintable(link.filePath()));
            }
            next();
        });
    });
}

QString OmarchyBarPlacement::spaceNames() const { return m_spaceNames; }

void OmarchyBarPlacement::setSpaceNames(const QString &spaceNames)
{
    if (spaceNames != u"full" && spaceNames != u"letter") {
        return;
    }
    if (m_spaceNames != spaceNames) {
        m_spaceNames = spaceNames;
        emit spaceNamesChanged();
    }
    enqueue([this, spaceNames](const std::function<void()> &next) {
        editLayout(spaceNamesFilter.arg(spaceNames), [this, next] {
            shell(QStringLiteral("reloadConfig"));
            next();
        });
    });
}

void OmarchyBarPlacement::readSpaceNames()
{
    const auto layout = QJsonDocument::fromJson(readFile(m_paths.shellConfig))
                            .object()
                            .value(QStringLiteral("bar"))
                            .toObject()
                            .value(QStringLiteral("layout"))
                            .toObject();
    auto read = QStringLiteral("full");
    for (const auto &section : layout) {
        for (const auto &entry : section.toArray()) {
            const auto fields = entry.toObject();
            if (fields.value(QStringLiteral("id")).toString()
                    == QLatin1StringView(omarchyBarWidgetId)
                && fields.value(QStringLiteral("spaceNames")).toString() == u"letter") {
                read = QStringLiteral("letter");
            }
        }
    }
    if (m_spaceNames != read) {
        m_spaceNames = read;
        emit spaceNamesChanged();
    }
}

void OmarchyBarPlacement::relink()
{
    enqueue([this](const std::function<void()> &next) {
        link();
        if (!holdsTheEarlierPair()) {
            next();
            return;
        }
        editLayout(placeFilter, [this, next] {
            shell(QStringLiteral("reloadConfig"));
            next();
        });
    });
}

// Omaweb once placed the widget twice, each entry with a role, and the shell
// now draws the whole widget for each.
bool OmarchyBarPlacement::holdsTheEarlierPair() const
{
    const auto layout = QJsonDocument::fromJson(readFile(m_paths.shellConfig))
                            .object()
                            .value(QStringLiteral("bar"))
                            .toObject()
                            .value(QStringLiteral("layout"))
                            .toObject();
    for (const auto &section : layout) {
        for (const auto &entry : section.toArray()) {
            const auto fields = entry.toObject();
            if (fields.value(QStringLiteral("id")).toString()
                    == QLatin1StringView(omarchyBarWidgetId)
                && fields.contains(QStringLiteral("role"))) {
                return true;
            }
        }
    }
    return false;
}

// One change at a time, each reading the layout the one before it wrote: two
// edits under way at once would each start from the same file, and whichever
// jq finished last would undo the other.
void OmarchyBarPlacement::enqueue(Step step)
{
    m_steps.push_back(std::move(step));
    if (!m_stepping) {
        runNextStep();
    }
}

void OmarchyBarPlacement::runNextStep()
{
    if (m_steps.empty()) {
        m_stepping = false;
        return;
    }
    m_stepping = true;
    auto step = std::move(m_steps.front());
    m_steps.pop_front();
    step([guard = QPointer(this)] {
        if (guard) {
            guard->runNextStep();
        }
    });
}

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

// `then` runs once the edit is over, written or not.
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
        layout,
        [this, guard = QPointer(this), then = std::move(then)](
            int exitCode, const QByteArray &edited) {
            if (!guard) {
                return;
            }
            QSaveFile file(m_paths.shellConfig);
            if (exitCode != 0 || edited.trimmed().isEmpty()
                || !QDir().mkpath(QFileInfo(m_paths.shellConfig).absolutePath())
                || !file.open(QIODevice::WriteOnly) || file.write(edited) != edited.size()
                || !file.commit()) {
                qWarning("The bar's layout in %s could not be changed (jq exited %d)",
                    qPrintable(m_paths.shellConfig), exitCode);
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
