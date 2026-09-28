#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace omaweb {

// The `omaweb` CLI's Agent verbs (ADR 0051): `spaces`, `tabs`, `open`,
// `close`, `space new` and `space delete`, each sent to the running browser
// over the Agent socket as one request.
//
// A process run with one of them is a client and never a browser. It builds no
// engine, claims no desktop name and exits once it has its answer.

// Whether a command line asks for an Agent verb rather than for the browser.
bool isAgentCommand(const QStringList &arguments);

struct AgentCommand {
    // What goes over the socket, or nothing when `error` says why not.
    QJsonObject request;
    // Print the answer as it came, for a program to read.
    bool json = false;
    QString error;
};

// `defaultName` names the connection when `--name` does not. The CLI passes
// its parent process's name, so an Agent's commands share one connection.
AgentCommand readAgentCommand(const QStringList &arguments, const QString &defaultName);

// What the CLI prints for an answer that succeeded: one line per row, fields
// separated by tabs, so a shell script can cut it.
QString formatAgentAnswer(const QString &verb, const QJsonObject &answer);

// The name of the process that ran this one, or nothing.
QString parentProcessName();

// Sends the command to the browser on `socketPath`, prints its answer and
// answers the exit status: 0 done, 1 refused, 2 a malformed command, 3 no
// browser answering.
int runAgentCommand(const QStringList &arguments, const QString &socketPath);

} // namespace omaweb
