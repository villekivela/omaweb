#pragma once

#include <QString>
#include <QStringList>

#include <atomic>

namespace omaweb {

// What a reader's coding agents are given so they can use Omaweb: the Agent
// skill the package installs, linked into each agent's skills directory, and
// `omaweb mcp` registered as an MCP server for an agent that asks for one.
//
// Nothing here is the reader's to lose. A skill entry named `omaweb` that is
// not a link to the packaged skill, such as a directory or a link to a
// checkout, is left as it is, and only a link to the packaged skill is ever
// removed. An MCP server goes in through the agent's own command, never by
// editing its configuration, and one already named `omaweb` is left alone.
class AgentSetup {
public:
    // What one call did, by agent name: Claude Code, Codex, pi, Hermes, or an
    // empty name for `~/.agents/skills`, which any agent may read.
    struct Outcome {
        // Linked, unlinked or registered by this call.
        QStringList changed;
        // Already in place, or the reader's own and left alone.
        QStringList kept;
        // Could not be done: a directory that could not be made, or an
        // agent command that failed or took too long.
        QStringList failed;
    };

    // `home` is the reader's home and `skillPath` the directory the package
    // installs the skill to. Agents are found by their home directory and
    // their commands on `PATH`.
    AgentSetup(QString home, QString skillPath);

    QString skillPath() const;

    // Links the skill into `~/.agents/skills` and into the skills directory
    // of every agent whose home exists. No home is made for an agent that is
    // not there.
    Outcome linkSkill() const;
    // Removes every link named `omaweb` that points at the packaged skill,
    // and nothing else.
    Outcome unlinkSkill() const;

    // Whether `claude` or `codex` is on `PATH`.
    static bool mcpAgentPresent();
    // Runs `claude mcp add -s user omaweb -- omaweb mcp` and
    // `codex mcp add omaweb -- omaweb mcp` for whichever is on `PATH`, after
    // asking each whether it has a server named `omaweb` already. Blocks for
    // as long as the agents take, up to `timeoutMs` for each command, so it is
    // called off the interface thread. The commands run at home, where no
    // project's own MCP configuration answers for the reader's.
    // A command still running when `cancelled` turns true is stopped and
    // counted as failed, so a browser that quits does not wait on it.
    Outcome addMcpServer(
        int timeoutMs = defaultMcpTimeoutMs, const std::atomic_bool *cancelled = nullptr) const;

    static constexpr int defaultMcpTimeoutMs = 30000;

private:
    QString m_home;
    QString m_skillPath;
};

} // namespace omaweb
