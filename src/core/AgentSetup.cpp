#include "AgentSetup.h"

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

    const auto skillName = QStringLiteral("omaweb");

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
            {QStringLiteral("pi"), QStringLiteral(".pi"), QStringLiteral(".pi/agent/skills")},
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
                    QStringLiteral("user"), skillName, QStringLiteral("--"), skillName,
                    QStringLiteral("mcp")}},
            {QStringLiteral("Codex"), QStringLiteral("codex"),
                {QStringLiteral("mcp"), QStringLiteral("add"), skillName, QStringLiteral("--"),
                    skillName, QStringLiteral("mcp")}},
        };
        return agents;
    }

    // An entry is there when it is a link, even one whose target is gone.
    bool entryPresent(const QFileInfo &entry) { return entry.isSymLink() || entry.exists(); }

    // Exit status of a command that finished, or nothing for one that did
    // not start or ran past `timeoutMs`.
    std::optional<int> run(const QString &program, const QStringList &arguments,
        const QString &directory, int timeoutMs)
    {
        QProcess process;
        process.setWorkingDirectory(directory);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.setStandardInputFile(QProcess::nullDevice());
        process.start(program, arguments);
        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            process.waitForFinished();
            return std::nullopt;
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

QString AgentSetup::skillPath() const { return m_skillPath; }

AgentSetup::Outcome AgentSetup::linkSkill() const
{
    Outcome outcome;
    const QDir home(m_home);
    for (const auto &agent : skillHomes()) {
        if (!agent.home.isEmpty() && !QFileInfo(home.filePath(agent.home)).isDir()) {
            continue;
        }
        const QDir skills(home.filePath(agent.skills));
        const QFileInfo entry(skills.filePath(skillName));
        if (entryPresent(entry)) {
            outcome.kept.append(agent.name);
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
        const QFileInfo entry(QDir(home.filePath(agent.skills)).filePath(skillName));
        if (!entry.isSymLink() || QDir::cleanPath(entry.readSymLink()) != m_skillPath) {
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

AgentSetup::Outcome AgentSetup::addMcpServer(int timeoutMs) const
{
    Outcome outcome;
    for (const auto &agent : mcpAgents()) {
        const auto program = QStandardPaths::findExecutable(agent.program);
        if (program.isEmpty()) {
            continue;
        }
        // `mcp get` answers 0 for a server the agent has under that name, in
        // any scope.
        const auto existing = run(
            program, {QStringLiteral("mcp"), QStringLiteral("get"), skillName}, m_home, timeoutMs);
        if (!existing) {
            outcome.failed.append(agent.name);
            continue;
        }
        if (*existing == 0) {
            outcome.kept.append(agent.name);
            continue;
        }
        const auto added = run(program, agent.add, m_home, timeoutMs);
        if (added == 0) {
            outcome.changed.append(agent.name);
        } else {
            outcome.failed.append(agent.name);
        }
    }
    return outcome;
}

} // namespace omaweb
