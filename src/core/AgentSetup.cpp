#include "AgentSetup.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>
#include <optional>
#include <utility>

namespace omaweb {
namespace {

    // The skill's directory, the MCP server's name and the program it runs.
    const auto omawebName = QStringLiteral("omaweb");

    struct SkillHome {
        QString name;
        // Relative to the reader's home. The skills directory is made only
        // when `home` is there, and `home` is empty for `~/.agents`, which
        // is always made.
        QString home;
        QString skills;
    };

    // The directories Omarchy's own migrations link its skills into.
    const QList<SkillHome> &skillHomes()
    {
        static const QList<SkillHome> homes {
            {QString(), QString(), QStringLiteral(".agents/skills")},
            {QStringLiteral("Claude Code"), QStringLiteral(".claude"),
                QStringLiteral(".claude/skills")},
            {QStringLiteral("Codex"), QStringLiteral(".codex"), QStringLiteral(".codex/skills")},
            {QStringLiteral("pi"), QStringLiteral(".pi/agent"), QStringLiteral(".pi/agent/skills")},
            {QStringLiteral("Hermes"), QStringLiteral(".hermes"), QStringLiteral(".hermes/skills")},
        };
        return homes;
    }

    struct McpAgent {
        QString name;
        QString program;
        QStringList add;
    };

    const QList<McpAgent> &mcpAgents()
    {
        static const QList<McpAgent> agents {
            {QStringLiteral("Claude Code"), QStringLiteral("claude"),
                {QStringLiteral("mcp"), QStringLiteral("add"), QStringLiteral("-s"),
                    QStringLiteral("user"), omawebName, QStringLiteral("--"), omawebName,
                    QStringLiteral("mcp")}},
            {QStringLiteral("Codex"), QStringLiteral("codex"),
                {QStringLiteral("mcp"), QStringLiteral("add"), omawebName, QStringLiteral("--"),
                    omawebName, QStringLiteral("mcp")}},
        };
        return agents;
    }

    // An entry is there when it is a link, even one whose target is gone.
    bool entryPresent(const QFileInfo &entry) { return entry.isSymLink() || entry.exists(); }

    // A link to the packaged skill, whether or not the package is still there.
    bool linksTo(const QFileInfo &entry, const QString &skillPath)
    {
        return entry.isSymLink() && QDir::cleanPath(entry.readSymLink()) == skillPath;
    }

    // A command is looked in on this often to see whether it was cancelled.
    constexpr int cancelCheckMs = 100;

    // Exit status of a command that finished, or nothing for one that did
    // not start, ran past `timeoutMs` or was cancelled.
    std::optional<int> run(const QString &program, const QStringList &arguments,
        const QString &directory, int timeoutMs, const std::atomic_bool *cancelled)
    {
        QProcess process;
        process.setWorkingDirectory(directory);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setStandardInputFile(QProcess::nullDevice());
        process.start(program, arguments);
        if (!process.waitForStarted()) {
            return std::nullopt;
        }
        const QDeadlineTimer deadline(timeoutMs);
        while (!process.waitForFinished(cancelCheckMs)) {
            if (process.state() == QProcess::NotRunning || deadline.hasExpired()
                || (cancelled && *cancelled)) {
                process.kill();
                process.waitForFinished();
                return std::nullopt;
            }
        }
        if (process.exitStatus() != QProcess::NormalExit) {
            return std::nullopt;
        }
        return process.exitCode();
    }

} // namespace

AgentSetup::AgentSetup(QString home, QString skillPath)
    : m_home(std::move(home))
    , m_skillPath(QDir::cleanPath(std::move(skillPath)))
{
}

AgentSetup::Outcome AgentSetup::linkSkill() const
{
    Outcome outcome;
    const QDir home(m_home);
    for (const auto &agent : skillHomes()) {
        if (!agent.home.isEmpty() && !QFileInfo(home.filePath(agent.home)).isDir()) {
            continue;
        }
        const QDir skills(home.filePath(agent.skills));
        const QFileInfo entry(skills.filePath(omawebName));
        if (linksTo(entry, m_skillPath)) {
            outcome.kept.append(agent.name);
            continue;
        }
        if (entryPresent(entry)) {
            outcome.leftAlone.append(agent.name);
            continue;
        }
        if (!skills.mkpath(QStringLiteral(".")) || !QFile::link(m_skillPath, entry.filePath())) {
            outcome.failed.append(agent.name);
            continue;
        }
        outcome.changed.append(agent.name);
    }
    return outcome;
}

AgentSetup::Outcome AgentSetup::unlinkSkill() const
{
    Outcome outcome;
    const QDir home(m_home);
    for (const auto &agent : skillHomes()) {
        const QFileInfo entry(QDir(home.filePath(agent.skills)).filePath(omawebName));
        if (!linksTo(entry, m_skillPath)) {
            continue;
        }
        if (QFile::remove(entry.filePath())) {
            outcome.changed.append(agent.name);
        } else {
            outcome.failed.append(agent.name);
        }
    }
    return outcome;
}

bool AgentSetup::mcpAgentPresent()
{
    return std::ranges::any_of(mcpAgents(),
        [](const auto &agent) { return !QStandardPaths::findExecutable(agent.program).isEmpty(); });
}

AgentSetup::Outcome AgentSetup::addMcpServer(int timeoutMs, const std::atomic_bool *cancelled) const
{
    Outcome outcome;
    for (const auto &agent : mcpAgents()) {
        const auto program = QStandardPaths::findExecutable(agent.program);
        if (program.isEmpty()) {
            continue;
        }
        // `mcp get` answers 0 for a server the agent has under that name, in
        // any scope.
        const auto existing
            = run(program, {QStringLiteral("mcp"), QStringLiteral("get"), omawebName}, m_home,
                timeoutMs, cancelled);
        if (!existing) {
            outcome.failed.append(agent.name);
            continue;
        }
        if (*existing == 0) {
            outcome.kept.append(agent.name);
            continue;
        }
        const auto added = run(program, agent.add, m_home, timeoutMs, cancelled);
        if (added == 0) {
            outcome.changed.append(agent.name);
        } else {
            outcome.failed.append(agent.name);
        }
    }
    return outcome;
}

} // namespace omaweb
