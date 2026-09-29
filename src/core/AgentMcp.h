#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <functional>
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
    const QString &tool, const QJsonObject &arguments, const QString &name);

// The answer to one JSON-RPC message, or nothing for a notification. `send`
// carries a request to the browser and brings its answer back, or nothing
// with `error` saying why the browser could not be reached.
using AgentMcpSend
    = std::function<std::optional<QJsonObject>(const QJsonObject &request, QString &error)>;
std::optional<QJsonObject> answerAgentMcp(
    const QJsonObject &message, const QString &name, const AgentMcpSend &send);

// Serves MCP on standard input and output until input ends. Answers 0, or 2
// for a malformed command line.
int runAgentMcp(const QStringList &arguments, const QString &socketPath);

} // namespace omaweb
