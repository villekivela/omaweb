#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace omaweb {

// The `omaweb` CLI's Agent verbs (ADR 0051): the browser commands `spaces`,
// `tabs`, `open`, `close`, `space`, `focus`, `commands`, `run` and `dev` (ADR
// 0059), and the verbs behind Allow agents, each sent to the running browser
// over the Agent socket as one request. `dev` carries the folder the CLI ran
// in.
//
// A process run with one of them is a client and never a browser. It builds no
// engine, claims no desktop name and exits once it has its answer.

// Where the verb stands: the first argument after the Agent options that lead
// the command line, which may come before the verb as after it
// (`omaweb --name x open URL`). The options are those some verb's grammar
// takes, with their values. `arguments.size()` when nothing follows them.
qsizetype agentVerbIndex(const QStringList &arguments);

// Whether a command line asks for an Agent verb rather than for the browser.
bool isAgentCommand(const QStringList &arguments);

// The Agent option that leads a command line which has no verb to give it to,
// such as `--json` in `omaweb --json https://example.com`, or nothing. The
// browser must never be launched for it.
QString misplacedAgentOption(const QStringList &arguments);

// What the CLI says of a misplaced option, which it exits 2 on.
QString misplacedAgentOptionMessage(const QString &option);

struct AgentCommand {
    // What goes over the socket, or nothing when `error` says why not.
    QJsonObject request;
    // Print the answer as it came, for a program to read.
    bool json = false;
    // `tabs --pick`: the request lists every Space's tabs, and the CLI offers
    // them to Omarchy's menu and focuses the one chosen.
    bool pick = false;
    QString error;
};

// `defaultName` names the connection when `--name` does not. The CLI passes
// its parent process's name, so an Agent's commands share one connection.
AgentCommand readAgentCommand(const QStringList &arguments, const QString &defaultName);

// The steps of a `do`, each argument one step or several separated by `;`:
// `click 3`, `fill 5 "text"`, `press Enter`, `select 7 Finland`,
// `scroll down`, `back`, `wait text Thanks`, `wait url /done`, `dialog accept`,
// `dialog dismiss` and `upload 4 report.pdf`, whose paths are made whole
// against the working directory. Empty, with `error` saying why, when one is
// malformed.
QJsonArray readAgentSteps(const QStringList &arguments, QString &error);

// The name a request carries: `name` without what cannot be printed, cut to
// fit the log, or a fallback when nothing is left.
QString agentConnectionName(const QString &name);

// The levels `console` keeps: errors, warnings and errors, or everything.
const QStringList &agentConsoleLevels();

// How long a client waits for the answer to `request`, a little longer than
// the browser waits for the page, so the browser's answer is the one heard.
int agentAnswerTimeoutMs(const QJsonObject &request);

// What the CLI prints for an answer that succeeded: one line per row, fields
// separated by tabs, so a shell script can cut it.
QString formatAgentAnswer(const QString &verb, const QJsonObject &answer);

// The name of the process that ran this one, or nothing.
QString parentProcessName();

// Sends the command to the browser on `socketPath`, prints its answer and
// answers the exit status: 0 done, 1 refused, 2 a malformed command, 3 no
// browser answering.
//
// `tabs --pick` offers every Space's tabs to Omarchy's `omarchy-menu-select`
// and then runs `focus --raise` on the one chosen. Dismissing the menu is
// done, 0. Without `omarchy-menu-select` on the PATH it says so and answers 1.
//
// `space new --temporary` prints the Space and then keeps open the connection
// the Space lives on, until the process is interrupted, terminated or hung up
// on, or the browser goes.
int runAgentCommand(const QStringList &arguments, const QString &socketPath);

} // namespace omaweb
