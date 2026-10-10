#include "AgentMcp.h"

#include "AgentCommand.h"
#include "AgentProtocol.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QThread>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <string>
#include <utility>

namespace omaweb {
namespace {

    // A browser that is starting builds its engine and restores its session
    // before it opens the socket.
    constexpr int startTimeoutMs = 30000;
    constexpr int connectTimeoutMs = 1000;

    // The MCP versions this answers in, oldest first. The tools are all it
    // serves, and they read the same in each. A client that asks for a
    // version not here is answered with the newest, as the handshake asks.
    const QStringList &protocols()
    {
        static const QStringList known {QStringLiteral("2024-11-05"), QStringLiteral("2025-03-26"),
            QStringLiteral("2025-06-18"), QStringLiteral("2025-11-25")};
        return known;
    }

    const auto namePrefix = QStringLiteral("--name=");

    // Every word here is paid for by every conversation the server is
    // registered in, so it says what the tool list cannot: where the page
    // tools work and the loop that uses the fewest calls.
    const auto instructions = QStringLiteral(
        "Omaweb, the reader's browser. The page tools (look, read, do, shot, eval, console) need "
        "Allow agents on and a tab in an Agent Space: space_new, then open with that space. In "
        "the reader's own Spaces they first ask the reader, once per Space. They "
        "work on the current tab, which open sets, or on tab. Loop: open, look, do several steps "
        "in one call, which answers with a fresh look. A label from look lasts as long as its "
        "document.");

    enum class Kind {
        String,
        Boolean,
        Integer,
        // The array of step strings `do` takes.
        Steps,
        // The console's level, one of agentConsoleLevels().
        Level,
    };

    struct Property {
        QString name;
        Kind kind;
        // The request field it fills, when it is not the property's name.
        QString field;
    };

    Property property(const QString &name, Kind kind, const QString &field = {})
    {
        return {.name = name, .kind = kind, .field = field};
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
            const auto string = Kind::String;
            const auto boolean = Kind::Boolean;
            const auto integer = Kind::Integer;
            const auto tab = property(QStringLiteral("tab"), string);
            const auto space = property(QStringLiteral("space"), string);
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
                    .description
                    = QStringLiteral("Run a browser command in the window. position "
                                     "is for select-tab and select-space, from 1; tab "
                                     "is a tab's id for pop-out-tab and put-back-tab."),
                    .properties = {property(QStringLiteral("command"), string),
                        property(QStringLiteral("position"), integer, QStringLiteral("argument")),
                        property(QStringLiteral("tab"), string, QStringLiteral("argument"))},
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
                        "first that fails; answers with a look. Steps: click <label>, fill "
                        "<label> <text>, press <key>, select <label> <option>, scroll "
                        "<label|up|down|top|bottom>, back, wait text <text>, wait url <address>, "
                        "dialog accept [text], dialog dismiss, upload <label> <path>... (Agent "
                        "Spaces only)."),
                    .properties = {property(QStringLiteral("steps"), Kind::Steps), tab,
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
                    .properties = {tab, property(QStringLiteral("level"), Kind::Level),
                        property(QStringLiteral("since"), integer)},
                    .required = {}},
            };
        }();
        return list;
    }

    QJsonObject schemaFor(const Property &property)
    {
        const auto type
            = [](const QString &name) { return QJsonObject {{QStringLiteral("type"), name}}; };
        switch (property.kind) {
        case Kind::String:
            return type(QStringLiteral("string"));
        case Kind::Boolean:
            return type(QStringLiteral("boolean"));
        case Kind::Integer:
            return type(QStringLiteral("integer"));
        case Kind::Steps:
            return {{QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), type(QStringLiteral("string"))}};
        case Kind::Level:
            return {{QStringLiteral("enum"), QJsonArray::fromStringList(agentConsoleLevels())}};
        }
        return {};
    }

    // Reads one argument into the request, or answers why it cannot.
    QString readArgument(const Property &property, const QJsonValue &value, QJsonObject &request)
    {
        const auto field = property.field.isEmpty() ? property.name : property.field;
        switch (property.kind) {
        case Kind::String:
            if (!value.isString()) {
                return QStringLiteral("%1 is a string.").arg(property.name);
            }
            request.insert(field, value);
            break;
        case Kind::Boolean:
            if (!value.isBool()) {
                return QStringLiteral("%1 is true or false.").arg(property.name);
            }
            // The socket reads a flag's absence as false.
            if (value.toBool()) {
                request.insert(field, true);
            }
            break;
        case Kind::Integer: {
            const auto number = value.toDouble(-1);
            if (!value.isDouble() || number < 0
                || number != static_cast<double>(value.toInteger())) {
                return QStringLiteral("%1 is a whole number.").arg(property.name);
            }
            request.insert(field, value);
            break;
        }
        case Kind::Level:
            if (!agentConsoleLevels().contains(value.toString())) {
                return QStringLiteral("level is error, warning or all.");
            }
            request.insert(field, value);
            break;
        case Kind::Steps: {
            if (!value.isArray()) {
                return QStringLiteral("do takes at least one step.");
            }
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
            if (read.isEmpty()) {
                return QStringLiteral("do takes at least one step.");
            }
            request.insert(field, read);
            break;
        }
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

} // namespace

bool isAgentMcpCommand(const QStringList &arguments)
{
    return arguments.value(agentVerbIndex(arguments)) == u"mcp";
}

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
    if (!message.contains(QStringLiteral("id")) || message.contains(QStringLiteral("result"))
        || message.contains(QStringLiteral("error"))) {
        return std::nullopt;
    }
    const auto id = message.value(QStringLiteral("id"));
    if (method.isEmpty()) {
        return rpcError(id, -32600, QStringLiteral("A request names its method."));
    }
    const auto params = message.value(QStringLiteral("params")).toObject();
    if (method == u"initialize") {
        const auto asked = params.value(QStringLiteral("protocolVersion")).toString();
        return rpcResult(id,
            {{QStringLiteral("protocolVersion"),
                 protocols().contains(asked) ? asked : protocols().constLast()},
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

void serveAgentMcp(std::istream &input, std::ostream &output, const QString &name,
    const AgentMcpSend &send, const std::function<bool()> &finished)
{
    std::string line;
    while (std::getline(input, line)) {
        const auto bytes = QByteArray::fromStdString(line).trimmed();
        if (bytes.isEmpty()) {
            continue;
        }
        QJsonParseError parse {};
        const auto document = QJsonDocument::fromJson(bytes, &parse);
        std::optional<QJsonObject> reply;
        if (parse.error != QJsonParseError::NoError) {
            reply = rpcError(
                QJsonValue::Null, -32700, QStringLiteral("A message is one JSON object per line."));
        } else if (!document.isObject()) {
            // A batch among them: MCP has not sent one since 2025-06-18, and
            // this server has never answered one.
            reply = rpcError(
                QJsonValue::Null, -32600, QStringLiteral("A message is one JSON object per line."));
        } else {
            reply = answerAgentMcp(document.object(), name, send);
        }
        if (reply) {
            output << QJsonDocument(*reply).toJson(QJsonDocument::Compact).toStdString() << '\n';
            output.flush();
        }
        if (finished && finished()) {
            return;
        }
    }
}

std::optional<QString> readAgentMcpName(const QStringList &arguments, const QString &defaultName)
{
    auto name = defaultName;
    const auto verb = agentVerbIndex(arguments);
    const auto options = arguments.mid(1, verb - 1) + arguments.mid(verb + 1);
    for (qsizetype index = 0; index < options.size(); ++index) {
        const auto &argument = options.at(index);
        if (argument == u"--name" && index + 1 < options.size()) {
            name = options.at(++index);
        } else if (argument.startsWith(namePrefix)) {
            name = argument.mid(namePrefix.size());
        } else {
            return std::nullopt;
        }
    }
    return name;
}

AgentMcpLink::AgentMcpLink(QString path, std::function<bool()> start, int startTimeoutMs)
    : m_path(std::move(path))
    , m_start(std::move(start))
    , m_startTimeoutMs(startTimeoutMs)
{
}

std::optional<QJsonObject> AgentMcpLink::send(const QJsonObject &request, QString &error)
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

bool AgentMcpLink::connect(QString &error)
{
    if (m_socket.state() == QLocalSocket::ConnectedState) {
        // A browser that quit since the last call closed the connection, and
        // only reading finds that out.
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
    if (m_startFailed) {
        error = QStringLiteral("Omaweb is not answering on its Agent socket.");
        return false;
    }
    if (!m_start) {
        m_stranded = QStringLiteral("There is no browser installed here to start. ")
            + agentSocketUnreachable(m_path);
        error = m_stranded;
        return false;
    }
    if (!m_start()) {
        error = QStringLiteral("No Omaweb is running, and one could not be started.");
        return false;
    }
    QDeadlineTimer deadline(m_startTimeoutMs);
    while (!deadline.hasExpired()) {
        if (tryConnect()) {
            return true;
        }
        QThread::msleep(100);
    }
    m_startFailed = true;
    error = QStringLiteral("Omaweb was started but did not answer within %1 seconds.")
                .arg(m_startTimeoutMs / 1000);
    return false;
}

bool AgentMcpLink::tryConnect()
{
    m_socket.connectToServer(m_path);
    if (m_socket.waitForConnected(connectTimeoutMs)) {
        greet();
        return true;
    }
    m_socket.abort();
    return false;
}

void AgentMcpLink::greet()
{
    const auto mismatch = greetAgentBrowser(m_socket, connectTimeoutMs);
    if (!mismatch) {
        // A browser busy past this still answers, ahead of the next call.
        if (m_socket.state() == QLocalSocket::ConnectedState) {
            ++m_owed;
        }
        return;
    }
    if (!mismatch->isEmpty() && !m_warned) {
        m_warned = true;
        std::fprintf(stderr, "%s\n", qPrintable(*mismatch));
    }
}

int runAgentMcp(
    const QStringList &arguments, const QString &socketPath, const std::function<bool()> &start)
{
    const auto name = readAgentMcpName(arguments, parentProcessName());
    if (!name) {
        std::fputs(
            "omaweb: use `omaweb mcp [--name <name>]`, with --name before or after mcp.\n", stderr);
        return 2;
    }
    AgentMcpLink browser(socketPath, start, startTimeoutMs);
    serveAgentMcp(
        std::cin, std::cout, agentConnectionName(*name),
        [&browser](
            const QJsonObject &request, QString &error) { return browser.send(request, error); },
        [&browser] { return !browser.stranded().isEmpty(); });
    if (const auto stranded = browser.stranded(); !stranded.isEmpty()) {
        std::fprintf(stderr, "omaweb: %s\n", qPrintable(stranded));
        return 1;
    }
    return 0;
}

} // namespace omaweb
