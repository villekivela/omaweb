#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QString>
#include <QStringList>

#include <functional>
#include <iosfwd>
#include <optional>

namespace omaweb {

// `omaweb mcp` (ADR 0051): a stdio MCP server that serves each Agent verb as
// one tool and bridges every call to the Agent socket.
//
// It holds one socket connection for as long as it runs, so the Agent keeps
// its current tab between calls and a temporary Agent Space lasts as long as
// the server. Like the CLI, it is a client and never a browser, but it starts
// one when none is answering.

// Whether a command line asks for the MCP server.
bool isAgentMcpCommand(const QStringList &arguments);

// The tools as `tools/list` answers them.
QJsonArray agentMcpTools();

// A tool call and the socket request it stands for, or `error` saying why it
// is not one. A call to no tool at all leaves both empty and sets `unknown`.
struct AgentMcpCall {
    QJsonObject request;
    QString error;
    bool unknown = false;
};

// `name` is the connection's name, already cleaned.
AgentMcpCall readAgentMcpCall(
    const QString &toolName, const QJsonObject &arguments, const QString &name);

// The answer to one JSON-RPC message, or nothing for a notification. `send`
// carries a request to the browser and brings its answer back, or nothing
// with `error` saying why the browser could not be reached.
using AgentMcpSend
    = std::function<std::optional<QJsonObject>(const QJsonObject &request, QString &error)>;
std::optional<QJsonObject> answerAgentMcp(
    const QJsonObject &message, const QString &name, const AgentMcpSend &send);

// Answers each line of `input` on `output`, one JSON-RPC message a line, until
// `input` ends or `finished`, asked after each answer, says to stop.
void serveAgentMcp(std::istream &input, std::ostream &output, const QString &name,
    const AgentMcpSend &send, const std::function<bool()> &finished = {});

// The connection's name from `omaweb mcp [--name <name>]`, uncleaned, or
// nothing when the command line is malformed.
std::optional<QString> readAgentMcpName(const QStringList &arguments, const QString &defaultName);

// The one connection the server holds to the browser at `path`. When nothing
// answers there, the first call runs `start` and waits up to `startTimeoutMs`
// for the socket. A browser that was started and never answered is not
// started again: a browser that is running with its socket closed would be
// brought forward by every call instead.
//
// An empty `start` is a machine with no browser installed, such as a sandbox
// that reaches one over a forwarded socket. A call there that finds nothing
// answering strands the link.
class AgentMcpLink {
public:
    AgentMcpLink(QString path, std::function<bool()> start, int startTimeoutMs);

    std::optional<QJsonObject> send(const QJsonObject &request, QString &error);

    // Why the link has no browser to reach and none to start, or nothing.
    QString stranded() const { return m_stranded; }

private:
    bool connect(QString &error);
    bool tryConnect();
    // Says which protocol this speaks on a new connection, and warns once a
    // session when the browser speaks another.
    void greet();

    QString m_path;
    std::function<bool()> m_start;
    int m_startTimeoutMs;
    bool m_startFailed = false;
    bool m_warned = false;
    QString m_stranded;
    QLocalSocket m_socket;
    // Answers to requests that timed out, which the browser still sends, in
    // order, ahead of the next.
    int m_owed = 0;
};

// Serves MCP on standard input and output until input ends, starting the
// browser with `start` when none answers on `socketPath`. Answers 0, 2 for a
// malformed command line, or 1 when `start` is empty and no browser answered:
// the server says so and leaves.
int runAgentMcp(
    const QStringList &arguments, const QString &socketPath, const std::function<bool()> &start);

} // namespace omaweb
