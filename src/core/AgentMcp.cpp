#include "AgentMcp.h"

#include "AgentCommand.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QThread>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>

namespace omaweb {
namespace {

    // A browser that is starting builds its engine and restores its session
    // before it opens the socket.
    constexpr int startTimeoutMs = 30000;
    constexpr int connectTimeoutMs = 1000;

    // What `initialize` answers when a client asks for a version this does
    // not know. The tools are all this serves, and they read the same in each.
    const auto latestProtocol = QStringLiteral("2025-06-18");

    // Every word here is paid for by every conversation the server is
    // registered in, so it says what the tool list cannot: where the page
    // tools work and the loop that uses the fewest calls.
    const auto instructions = QStringLiteral(
        "Omaweb, the reader's browser. The page tools (look, read, do, shot, eval, console) need "
        "Allow agents on and a tab in an Agent Space: space_new, then open with that space. They "
        "work on the current tab, which open sets, or on tab. Loop: open, look, do several steps "
        "in one call, which answers with a fresh look. A label from look lasts as long as its "
        "document.");

    struct Property {
        QString name;
        // A JSON Schema type, or `steps` for the array of step strings and
        // `level` for the console's level.
        QString type;
        // The request field it fills, when it is not the property's name.
        QString field;
    };

    Property property(const QString &name, const QString &type, const QString &field = {})
    {
        return {.name = name, .type = type, .field = field};
    }

    struct Tool {
        QString name;
        QString verb;
        QString description;
        QList<Property> properties;
        QStringList required;
    };

    const QList<Tool> &tools()
    {
        static const auto list = [] {
            const auto tab = property(QStringLiteral("tab"), QStringLiteral("string"));
            const auto space = property(QStringLiteral("space"), QStringLiteral("string"));
            const auto string = QStringLiteral("string");
            const auto boolean = QStringLiteral("boolean");
            const auto integer = QStringLiteral("integer");
            return QList<Tool> {
                {.name = QStringLiteral("spaces"),
                    .verb = QStringLiteral("spaces"),
                    .description = QStringLiteral("List Spaces: id, name, flags."),
                    .properties = {},
                    .required = {}},
                {.name = QStringLiteral("tabs"),
                    .verb = QStringLiteral("tabs"),
                    .description
                    = QStringLiteral("List a Space's tabs: id, address, title, flags."),
                    .properties = {space},
                    .required = {}},
                {.name = QStringLiteral("open"),
                    .verb = QStringLiteral("open"),
                    .description
                    = QStringLiteral("Open an address and make its tab current. new opens "
                                     "another tab."),
                    .properties = {property(QStringLiteral("url"), string), space, tab,
                        property(QStringLiteral("new"), boolean)},
                    .required = {QStringLiteral("url")}},
                {.name = QStringLiteral("close"),
                    .verb = QStringLiteral("close"),
                    .description = QStringLiteral("Close a tab this connection opened."),
                    .properties = {tab},
                    .required = {}},
                {.name = QStringLiteral("space"),
                    .verb = QStringLiteral("space"),
                    .description = QStringLiteral("Switch the window to a Space."),
                    .properties = {space},
                    .required = {QStringLiteral("space")}},
                {.name = QStringLiteral("focus"),
                    .verb = QStringLiteral("focus"),
                    .description = QStringLiteral("Select a tab by id or part of its address, in "
                                                  "whichever Space holds it."),
                    .properties = {property(tab.name, string, QStringLiteral("target"))},
                    .required = {tab.name}},
                {.name = QStringLiteral("commands"),
                    .verb = QStringLiteral("commands"),
                    .description = QStringLiteral("List the browser commands run can run now."),
                    .properties = {},
                    .required = {}},
                {.name = QStringLiteral("run"),
                    .verb = QStringLiteral("run"),
                    .description = QStringLiteral("Run a browser command in the window. position "
                                                  "is for select-tab and select-space, from 1."),
                    .properties = {property(QStringLiteral("command"), string),
                        property(QStringLiteral("position"), integer, QStringLiteral("argument"))},
                    .required = {QStringLiteral("command")}},
                {.name = QStringLiteral("space_new"),
                    .verb = QStringLiteral("space new"),
                    .description = QStringLiteral("Make an Agent Space. A temporary one is deleted "
                                                  "when this server stops."),
                    .properties
                    = {property(QStringLiteral("name"), string, QStringLiteral("space")),
                        property(QStringLiteral("temporary"), boolean)},
                    .required = {}},
                {.name = QStringLiteral("space_delete"),
                    .verb = QStringLiteral("space delete"),
                    .description = QStringLiteral("Delete an Agent Space this connection made."),
                    .properties = {space},
                    .required = {QStringLiteral("space")}},
                {.name = QStringLiteral("look"),
                    .verb = QStringLiteral("look"),
                    .description
                    = QStringLiteral("Title, address, outline and the labelled targets in "
                                     "view. all: off-screen ones too."),
                    .properties = {tab, property(QStringLiteral("all"), boolean)},
                    .required = {}},
                {.name = QStringLiteral("read"),
                    .verb = QStringLiteral("read"),
                    .description = QStringLiteral("The page, or what a CSS selector matches, as "
                                                  "Markdown."),
                    .properties = {tab, property(QStringLiteral("selector"), string)},
                    .required = {}},
                {.name = QStringLiteral("do"),
                    .verb = QStringLiteral("do"),
                    .description = QStringLiteral(
                        "Run steps in order, each waiting for the page to settle; stops at the "
                        "first "
                        "that fails; answers with a look. Steps: click <label>, fill <label> "
                        "<text>, "
                        "press <key>, select <label> <option>, scroll <label|up|down|top|bottom>, "
                        "back, wait text <text>, wait url <address>, dialog accept [text], "
                        "dialog dismiss, upload <label> <path>... (Agent Spaces only)."),
                    .properties = {property(QStringLiteral("steps"), QStringLiteral("steps")), tab,
                        property(QStringLiteral("settle"), integer),
                        property(QStringLiteral("timeout"), integer)},
                    .required = {QStringLiteral("steps")}},
                {.name = QStringLiteral("shot"),
                    .verb = QStringLiteral("shot"),
                    .description = QStringLiteral("Screenshot to a PNG file; answers its path."),
                    .properties = {tab, property(QStringLiteral("full"), boolean),
                        property(QStringLiteral("output"), string)},
                    .required = {}},
                {.name = QStringLiteral("eval"),
                    .verb = QStringLiteral("eval"),
                    .description
                    = QStringLiteral("Evaluate JavaScript in an isolated world with the "
                                     "page's DOM; answers the value."),
                    .properties = {property(QStringLiteral("expression"), string), tab},
                    .required = {QStringLiteral("expression")}},
                {.name = QStringLiteral("console"),
                    .verb = QStringLiteral("console"),
                    .description = QStringLiteral("Console lines since the document loaded, then a "
                                                  "cursor to pass as since."),
                    .properties = {tab, property(QStringLiteral("level"), QStringLiteral("level")),
                        property(QStringLiteral("since"), integer)},
                    .required = {}},
            };
        }();
        return list;
    }

    QJsonObject schemaFor(const Property &property)
    {
        if (property.type == u"steps") {
            return {{QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"),
                    QJsonObject {{QStringLiteral("type"), QStringLiteral("string")}}}};
        }
        if (property.type == u"level") {
            return {{QStringLiteral("enum"),
                QJsonArray {
                    QStringLiteral("error"), QStringLiteral("warning"), QStringLiteral("all")}}};
        }
        return {{QStringLiteral("type"), property.type}};
    }

    // Reads one argument into the request, or answers why it cannot.
    QString readArgument(const Property &property, const QJsonValue &value, QJsonObject &request)
    {
        const auto field = property.field.isEmpty() ? property.name : property.field;
        if (property.type == u"string") {
            if (!value.isString()) {
                return QStringLiteral("%1 is a string.").arg(property.name);
            }
            request.insert(field, value);
        } else if (property.type == u"boolean") {
            if (!value.isBool()) {
                return QStringLiteral("%1 is true or false.").arg(property.name);
            }
            // The socket reads a flag's absence as false.
            if (value.toBool()) {
                request.insert(field, true);
            }
        } else if (property.type == u"integer") {
            const auto number = value.toDouble(-1);
            if (!value.isDouble() || number < 0
                || number != static_cast<double>(value.toInteger())) {
                return QStringLiteral("%1 is a whole number.").arg(property.name);
            }
            request.insert(field, value);
        } else if (property.type == u"level") {
            const auto level = value.toString();
            if (level != u"error" && level != u"warning" && level != u"all") {
                return QStringLiteral("level is error, warning or all.");
            }
            request.insert(field, level);
        } else {
            QStringList steps;
            for (const auto &step : value.toArray()) {
                if (!step.isString()) {
                    return QStringLiteral("Each step is a string.");
                }
                steps.append(step.toString());
            }
            QString error;
            const auto read = readAgentSteps(steps, error);
            if (!error.isEmpty()) {
                return error;
            }
            if (!value.isArray() || read.isEmpty()) {
                return QStringLiteral("do takes at least one step.");
            }
            request.insert(field, read);
        }
        return {};
    }

    QJsonObject toolResult(const QString &text, bool error)
    {
        return {{QStringLiteral("content"),
                    QJsonArray {QJsonObject {{QStringLiteral("type"), QStringLiteral("text")},
                        {QStringLiteral("text"), text}}}},
            {QStringLiteral("isError"), error}};
    }

    QString trimmedEnd(QString text)
    {
        while (text.endsWith(u'\n')) {
            text.chop(1);
        }
        return text;
    }

    // What the Agent reads: the CLI's own text for an answer, and the
    // browser's sentence for a refusal. A batch that stopped also says what it
    // did and what the page is like now, which is what the next step is
    // decided from.
    QJsonObject resultFor(const QString &verb, const QJsonObject &answer)
    {
        const auto error = answer.value(QStringLiteral("error")).toString();
        if (answer.value(QStringLiteral("ok")).toBool()) {
            const auto text = trimmedEnd(formatAgentAnswer(verb, answer));
            if (text.isEmpty()) {
                return toolResult(
                    verb == u"run" ? QStringLiteral("ran") : QStringLiteral("none"), false);
            }
            return toolResult(text, false);
        }
        if (verb == u"do" && answer.contains(QStringLiteral("look"))) {
            return toolResult(trimmedEnd(formatAgentAnswer(verb, answer)) + u"\n\n" + error, true);
        }
        return toolResult(error, true);
    }

    QJsonObject rpcResult(const QJsonValue &id, const QJsonObject &result)
    {
        return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), id},
            {QStringLiteral("result"), result}};
    }

    QJsonObject rpcError(const QJsonValue &id, int code, const QString &message)
    {
        return {{QStringLiteral("jsonrpc"), QStringLiteral("2.0")}, {QStringLiteral("id"), id},
            {QStringLiteral("error"),
                QJsonObject {
                    {QStringLiteral("code"), code}, {QStringLiteral("message"), message}}}};
    }

    // The one connection this server holds to the browser.
    class BrowserLink {
    public:
        explicit BrowserLink(QString path)
            : m_path(std::move(path))
        {
        }

        std::optional<QJsonObject> send(const QJsonObject &request, QString &error)
        {
            if (!connect(error)) {
                return std::nullopt;
            }
            m_socket.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
            m_socket.waitForBytesWritten(connectTimeoutMs);
            QDeadlineTimer deadline(agentAnswerTimeoutMs(request));
            while (true) {
                while (m_socket.canReadLine()) {
                    const auto line = m_socket.readLine();
                    if (m_owed > 0) {
                        --m_owed;
                        continue;
                    }
                    return QJsonDocument::fromJson(line).object();
                }
                if (m_socket.state() != QLocalSocket::ConnectedState) {
                    error = QStringLiteral("The browser closed the connection.");
                    return std::nullopt;
                }
                if (!m_socket.waitForReadyRead(static_cast<int>(deadline.remainingTime()))
                    && deadline.hasExpired()) {
                    // Its answer may still come, ahead of the next one.
                    ++m_owed;
                    error = QStringLiteral("The browser did not answer in time.");
                    return std::nullopt;
                }
            }
        }

    private:
        bool connect(QString &error)
        {
            if (m_socket.state() == QLocalSocket::ConnectedState) {
                // A browser that quit since the last call closed the
                // connection, and only reading finds that out.
                m_socket.waitForReadyRead(0);
                if (m_socket.state() == QLocalSocket::ConnectedState) {
                    return true;
                }
            }
            m_socket.abort();
            m_owed = 0;
            if (tryConnect()) {
                return true;
            }
            if (!startBrowser()) {
                error = QStringLiteral("No Omaweb is running, and one could not be started.");
                return false;
            }
            QDeadlineTimer deadline(startTimeoutMs);
            while (!deadline.hasExpired()) {
                if (tryConnect()) {
                    return true;
                }
                QThread::msleep(100);
            }
            error = QStringLiteral("Omaweb was started but did not answer within 30 seconds.");
            return false;
        }

        bool tryConnect()
        {
            m_socket.connectToServer(m_path);
            if (m_socket.waitForConnected(connectTimeoutMs)) {
                return true;
            }
            m_socket.abort();
            return false;
        }

        // The browser outlives this server, and standard output is the MCP
        // channel, so it gets none of this process's streams.
        static bool startBrowser()
        {
            QProcess browser;
            browser.setProgram(QCoreApplication::applicationFilePath());
            browser.setStandardInputFile(QProcess::nullDevice());
            browser.setStandardOutputFile(QProcess::nullDevice());
            browser.setStandardErrorFile(QProcess::nullDevice());
            return browser.startDetached();
        }

        QString m_path;
        QLocalSocket m_socket;
        // Answers to requests that timed out, which arrive before the next.
        int m_owed = 0;
    };

} // namespace

bool isAgentMcpCommand(const QStringList &arguments) { return arguments.value(1) == u"mcp"; }

QJsonArray agentMcpTools()
{
    QJsonArray list;
    for (const auto &tool : tools()) {
        QJsonObject properties;
        for (const auto &property : tool.properties) {
            properties.insert(property.name, schemaFor(property));
        }
        QJsonObject schema {{QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), properties}};
        if (!tool.required.isEmpty()) {
            schema.insert(QStringLiteral("required"), QJsonArray::fromStringList(tool.required));
        }
        list.append(QJsonObject {{QStringLiteral("name"), tool.name},
            {QStringLiteral("description"), tool.description},
            {QStringLiteral("inputSchema"), schema}});
    }
    return list;
}

AgentMcpCall readAgentMcpCall(
    const QString &toolName, const QJsonObject &arguments, const QString &name)
{
    AgentMcpCall call;
    const auto &list = tools();
    const auto tool = std::find_if(
        list.cbegin(), list.cend(), [&](const Tool &each) { return each.name == toolName; });
    if (tool == list.cend()) {
        call.unknown = true;
        return call;
    }
    QJsonObject request {{QStringLiteral("verb"), tool->verb}, {QStringLiteral("name"), name}};
    for (auto argument = arguments.constBegin(); argument != arguments.constEnd(); ++argument) {
        const auto property = std::find_if(tool->properties.cbegin(), tool->properties.cend(),
            [&](const Property &each) { return each.name == argument.key(); });
        if (property == tool->properties.cend()) {
            call.error = QStringLiteral("%1 takes no argument %2.").arg(tool->name, argument.key());
            return call;
        }
        call.error = readArgument(*property, argument.value(), request);
        if (!call.error.isEmpty()) {
            return call;
        }
    }
    for (const auto &required : tool->required) {
        if (!arguments.contains(required)) {
            call.error = QStringLiteral("%1 needs %2.").arg(tool->name, required);
            return call;
        }
    }
    call.request = request;
    return call;
}

std::optional<QJsonObject> answerAgentMcp(
    const QJsonObject &message, const QString &name, const AgentMcpSend &send)
{
    // A notification, or an answer to a request, neither of which this server
    // sends, has no reply.
    const auto method = message.value(QStringLiteral("method")).toString();
    if (!message.contains(QStringLiteral("id")) || method.isEmpty()) {
        return std::nullopt;
    }
    const auto id = message.value(QStringLiteral("id"));
    const auto params = message.value(QStringLiteral("params")).toObject();
    if (method == u"initialize") {
        static const QStringList known {QStringLiteral("2024-11-05"), QStringLiteral("2025-03-26"),
            latestProtocol, QStringLiteral("2025-11-25")};
        const auto asked = params.value(QStringLiteral("protocolVersion")).toString();
        return rpcResult(id,
            {{QStringLiteral("protocolVersion"), known.contains(asked) ? asked : latestProtocol},
                {QStringLiteral("capabilities"),
                    QJsonObject {{QStringLiteral("tools"), QJsonObject {}}}},
                {QStringLiteral("serverInfo"),
                    QJsonObject {{QStringLiteral("name"), QStringLiteral("omaweb")},
                        {QStringLiteral("version"), QCoreApplication::applicationVersion()}}},
                {QStringLiteral("instructions"), instructions}});
    }
    if (method == u"ping") {
        return rpcResult(id, {});
    }
    if (method == u"tools/list") {
        return rpcResult(id, {{QStringLiteral("tools"), agentMcpTools()}});
    }
    if (method != u"tools/call") {
        return rpcError(id, -32601, QStringLiteral("There is no method %1.").arg(method));
    }
    const auto tool = params.value(QStringLiteral("name")).toString();
    const auto call
        = readAgentMcpCall(tool, params.value(QStringLiteral("arguments")).toObject(), name);
    if (call.unknown) {
        return rpcError(id, -32602, QStringLiteral("There is no tool %1.").arg(tool));
    }
    if (!call.error.isEmpty()) {
        return rpcResult(id, toolResult(call.error, true));
    }
    QString error;
    const auto answer = send(call.request, error);
    if (!answer) {
        return rpcResult(id, toolResult(error, true));
    }
    return rpcResult(id, resultFor(call.request.value(QStringLiteral("verb")).toString(), *answer));
}

int runAgentMcp(const QStringList &arguments, const QString &socketPath)
{
    auto name = parentProcessName();
    for (qsizetype index = 2; index < arguments.size(); ++index) {
        const auto &argument = arguments.at(index);
        if (argument == u"--name" && index + 1 < arguments.size()) {
            name = arguments.at(++index);
        } else if (argument.startsWith(u"--name=")) {
            name = argument.mid(7);
        } else {
            std::fputs("omaweb: use `omaweb mcp [--name <name>]`.\n", stderr);
            return 2;
        }
    }
    name = agentConnectionName(name);

    BrowserLink browser(socketPath);
    const auto send = [&browser](const QJsonObject &request, QString &error) {
        return browser.send(request, error);
    };
    std::string line;
    while (std::getline(std::cin, line)) {
        const auto bytes = QByteArray::fromStdString(line).trimmed();
        if (bytes.isEmpty()) {
            continue;
        }
        QJsonParseError parse {};
        const auto document = QJsonDocument::fromJson(bytes, &parse);
        std::optional<QJsonObject> reply;
        if (parse.error != QJsonParseError::NoError || !document.isObject()) {
            reply = rpcError(QJsonValue::Null, -32700,
                QStringLiteral("A message is one JSON "
                               "object per line."));
        } else {
            reply = answerAgentMcp(document.object(), name, send);
        }
        if (reply) {
            const auto out = QJsonDocument(*reply).toJson(QJsonDocument::Compact) + '\n';
            std::fwrite(out.constData(), 1, static_cast<size_t>(out.size()), stdout);
            std::fflush(stdout);
        }
    }
    return 0;
}

} // namespace omaweb
