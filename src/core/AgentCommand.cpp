#include "AgentCommand.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QSet>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>

#include <poll.h>
#include <unistd.h>

#if defined(Q_OS_MACOS)
#include <libproc.h>
#endif

namespace omaweb {
namespace {

    // A name is for the log and the markers, so it is kept short enough to
    // read in either.
    constexpr qsizetype maximumNameLength = 64;

    // Long enough for a browser busy with a page to answer a browser command.
    constexpr int answerTimeoutMs = 10000;
    // A page verb waits in the browser, and the browser gives up on a page
    // after a minute, or two for a whole-page screenshot. The CLI waits a
    // little longer than it does, so the browser's answer is the one heard.
    constexpr int pageAnswerTimeoutMs = 65000;
    constexpr int fullShotAnswerTimeoutMs = 125000;

    const auto fallbackName = QStringLiteral("agent");

    struct Grammar {
        // The options this verb takes a value for, and those it takes alone.
        QSet<QString> valued;
        QSet<QString> flags;
        qsizetype minimumPositionals = 0;
        // -1 for as many as are given.
        qsizetype maximumPositionals = 0;
        // Where the one positional argument goes in the request.
        QString positionalField;
    };

    Grammar grammarFor(const QString &verb)
    {
        const auto space = QStringLiteral("space");
        const auto tab = QStringLiteral("tab");
        if (verb == u"spaces") {
            return {.valued = {},
                .flags = {},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"tabs") {
            return {.valued = {space},
                .flags = {},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"open") {
            return {.valued = {space, tab},
                .flags = {QStringLiteral("new")},
                .minimumPositionals = 1,
                .maximumPositionals = 1,
                .positionalField = QStringLiteral("url")};
        }
        if (verb == u"close") {
            return {.valued = {tab},
                .flags = {},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"look") {
            return {.valued = {tab},
                .flags = {QStringLiteral("all")},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"read") {
            return {.valued = {tab},
                .flags = {},
                .minimumPositionals = 0,
                .maximumPositionals = 1,
                .positionalField = QStringLiteral("selector")};
        }
        if (verb == u"do") {
            return {.valued = {tab, QStringLiteral("settle"), QStringLiteral("timeout")},
                .flags = {},
                .minimumPositionals = 1,
                .maximumPositionals = -1,
                .positionalField = {}};
        }
        if (verb == u"shot") {
            return {.valued = {tab, QStringLiteral("output")},
                .flags = {QStringLiteral("full")},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"eval") {
            return {.valued = {tab},
                .flags = {},
                .minimumPositionals = 1,
                .maximumPositionals = -1,
                .positionalField = {}};
        }
        if (verb == u"console") {
            return {.valued = {tab, QStringLiteral("level"), QStringLiteral("since")},
                .flags = {},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"commands") {
            return {.valued = {},
                .flags = {},
                .minimumPositionals = 0,
                .maximumPositionals = 0,
                .positionalField = {}};
        }
        if (verb == u"run") {
            return {.valued = {},
                .flags = {},
                .minimumPositionals = 1,
                .maximumPositionals = 2,
                .positionalField = {}};
        }
        if (verb == u"focus") {
            return {.valued = {},
                .flags = {},
                .minimumPositionals = 1,
                .maximumPositionals = 1,
                .positionalField = QStringLiteral("target")};
        }
        if (verb == u"space new") {
            return {.valued = {},
                .flags = {QStringLiteral("temporary")},
                .minimumPositionals = 0,
                .maximumPositionals = 1,
                .positionalField = space};
        }
        return {.valued = {},
            .flags = {},
            .minimumPositionals = 1,
            .maximumPositionals = 1,
            .positionalField = space};
    }

    QString cleanName(const QString &name)
    {
        QString cleaned;
        for (const auto character : name.trimmed()) {
            if (character.isPrint()) {
                cleaned.append(character);
            }
        }
        return cleaned.left(maximumNameLength);
    }

    // Written to by a signal handler, so it is set up before any handler is.
    int stopPipe[2] = {-1, -1};

    void requestStop(int)
    {
        const char byte = 0;
        if (::write(stopPipe[1], &byte, 1) < 0) {
            // Nothing a signal handler could do about it.
        }
    }

    // A temporary Space lasts as long as the connection that made it, so this
    // process keeps that connection open until it is told to stop or the
    // browser closes it. False when it cannot wait, and the connection closes
    // at once, taking the Space.
    bool holdConnection(QLocalSocket &socket)
    {
        std::fflush(stdout);
        if (::pipe(stopPipe) != 0) {
            return false;
        }
        struct sigaction action {};
        action.sa_handler = requestStop;
        sigemptyset(&action.sa_mask);
        for (const auto signal : {SIGINT, SIGTERM, SIGHUP}) {
            ::sigaction(signal, &action, nullptr);
        }
        pollfd watched[] {
            {.fd = stopPipe[0], .events = POLLIN, .revents = 0},
            {.fd = static_cast<int>(socket.socketDescriptor()), .events = POLLIN, .revents = 0},
        };
        while (true) {
            if (::poll(watched, 2, -1) < 0) {
                if (errno == EINTR) {
                    continue;
                }
                break;
            }
            if (watched[0].revents != 0) {
                break;
            }
            if (watched[1].revents != 0) {
                char discarded[256];
                if (::read(watched[1].fd, discarded, sizeof(discarded)) <= 0) {
                    break;
                }
            }
        }
        socket.disconnectFromServer();
        return true;
    }

    void print(FILE *stream, const QString &text)
    {
        std::fputs(text.toLocal8Bit().constData(), stream);
    }

    // A step's words, with a quoted run kept as one word and its quotes
    // dropped. Steps are separated by `;` outside quotes.
    QList<QStringList> splitSteps(const QString &text, QString &error)
    {
        QList<QStringList> steps;
        QStringList words;
        QString word;
        bool inWord = false;
        QChar quote;
        const auto endWord = [&] {
            if (inWord) {
                words.append(word);
            }
            word.clear();
            inWord = false;
        };
        const auto endStep = [&] {
            endWord();
            if (!words.isEmpty()) {
                steps.append(words);
            }
            words.clear();
        };
        for (qsizetype index = 0; index < text.size(); ++index) {
            const auto character = text.at(index);
            if (!quote.isNull()) {
                if (character == quote) {
                    quote = QChar();
                } else if (character == u'\\' && quote == u'"' && index + 1 < text.size()) {
                    word.append(text.at(++index));
                } else {
                    word.append(character);
                }
                continue;
            }
            if (character == u'"' || character == u'\'') {
                quote = character;
                inWord = true;
            } else if (character == u';') {
                endStep();
            } else if (character.isSpace()) {
                endWord();
            } else {
                word.append(character);
                inWord = true;
            }
        }
        if (!quote.isNull()) {
            error = QStringLiteral("A quote in the steps is never closed.");
            return {};
        }
        endStep();
        return steps;
    }

    // `click 3`, `fill 5 "text"`, `press Enter`, `select 7 Finland`,
    // `scroll down`, `back`, `wait text Thanks`, `wait url /done`,
    // `dialog accept`, `dialog dismiss` and `upload 4 report.pdf`.
    QJsonArray readSteps(const QStringList &arguments, QString &error)
    {
        QJsonArray steps;
        for (const auto &argument : arguments) {
            for (const auto &words : splitSteps(argument, error)) {
                const auto action = words.constFirst();
                const auto rest = [&](qsizetype from) { return words.mid(from).join(u' '); };
                QJsonObject step {{QStringLiteral("action"), action}};
                if (action == u"click" || action == u"scroll") {
                    if (words.size() != 2) {
                        error = QStringLiteral("`%1` takes one label.").arg(action);
                        return {};
                    }
                    step.insert(QStringLiteral("target"), words.at(1));
                } else if (action == u"fill" || action == u"select") {
                    if (words.size() < 2 || (action == u"select" && words.size() < 3)) {
                        error = QStringLiteral("`%1` takes a label and %2.")
                                    .arg(action,
                                        action == u"fill" ? QStringLiteral("the text to type")
                                                          : QStringLiteral("the option"));
                        return {};
                    }
                    step.insert(QStringLiteral("target"), words.at(1));
                    step.insert(QStringLiteral("text"), rest(2));
                } else if (action == u"press") {
                    if (words.size() != 2) {
                        error = QStringLiteral("`press` takes one key, such as Enter or "
                                               "Control+a.");
                        return {};
                    }
                    step.insert(QStringLiteral("key"), words.at(1));
                } else if (action == u"back") {
                    if (words.size() != 1) {
                        error = QStringLiteral("`back` takes nothing.");
                        return {};
                    }
                } else if (action == u"dialog") {
                    const auto answer = words.value(1);
                    if ((answer != u"accept" && answer != u"dismiss")
                        || (answer == u"dismiss" && words.size() > 2)) {
                        error = QStringLiteral(
                            "Use `dialog accept`, `dialog accept <text>` or `dialog dismiss`.");
                        return {};
                    }
                    step.insert(QStringLiteral("answer"), answer);
                    if (words.size() > 2) {
                        step.insert(QStringLiteral("text"), rest(2));
                    }
                } else if (action == u"upload") {
                    if (words.size() < 3) {
                        error = QStringLiteral("`upload` takes a label and the files to give it.");
                        return {};
                    }
                    step.insert(QStringLiteral("target"), words.at(1));
                    // A path is the Agent's, so one relative to where it runs
                    // is made whole here, before the browser sees it.
                    QJsonArray files;
                    for (const auto &file : words.mid(2)) {
                        files.append(QFileInfo(file).absoluteFilePath());
                    }
                    step.insert(QStringLiteral("files"), files);
                } else if (action == u"wait") {
                    const auto kind = words.value(1);
                    if ((kind != u"text" && kind != u"url") || words.size() < 3) {
                        error = QStringLiteral("Use `wait text <text>` or `wait url <address>`.");
                        return {};
                    }
                    step.insert(kind, rest(2));
                } else {
                    error = QStringLiteral("There is no step \"%1\".").arg(action);
                    return {};
                }
                steps.append(step);
            }
            if (!error.isEmpty()) {
                return {};
            }
        }
        return steps;
    }

    QString quoted(const QString &text) { return u'"' + text + u'"'; }

    // What `look` saw, as the Agent reads it: the page, its outline, then one
    // line per target.
    QString formatLook(const QJsonObject &look)
    {
        QString text = look.value(QStringLiteral("title")).toString() + u'\n'
            + look.value(QStringLiteral("url")).toString() + u'\n';
        const auto outline = look.value(QStringLiteral("outline")).toString();
        if (!outline.isEmpty()) {
            text += u'\n' + outline + u'\n';
        }
        const auto targets = look.value(QStringLiteral("targets")).toArray();
        if (!targets.isEmpty()) {
            text += u'\n';
        }
        for (const auto &value : targets) {
            const auto target = value.toObject();
            QString line = u'[' + target.value(QStringLiteral("label")).toString() + u"] "
                + target.value(QStringLiteral("kind")).toString();
            const auto name = target.value(QStringLiteral("name")).toString();
            if (!name.isEmpty()) {
                line += u' ' + quoted(name);
            }
            if (target.contains(QStringLiteral("value"))) {
                line += u" = " + quoted(target.value(QStringLiteral("value")).toString());
            }
            if (target.value(QStringLiteral("checked")).toBool()) {
                line += QStringLiteral(" (checked)");
            }
            if (target.value(QStringLiteral("disabled")).toBool()) {
                line += QStringLiteral(" (disabled)");
            }
            text += line + u'\n';
        }
        if (const auto dialog = look.value(QStringLiteral("dialog")).toObject();
            !dialog.isEmpty()) {
            QString line = QStringLiteral("Dialog (%1) ")
                               .arg(dialog.value(QStringLiteral("kind")).toString())
                + quoted(dialog.value(QStringLiteral("message")).toString());
            if (dialog.contains(QStringLiteral("defaultText"))) {
                line += u" = " + quoted(dialog.value(QStringLiteral("defaultText")).toString());
            }
            text += u'\n' + line
                + QStringLiteral("\nThe page waits for `dialog accept` or `dialog dismiss`.\n");
        }
        for (const auto &value : look.value(QStringLiteral("downloads")).toArray()) {
            const auto download = value.toObject();
            if (download.value(QStringLiteral("held")).toBool()) {
                text += QStringLiteral("Download held for the reader: %1 (%2)\n")
                            .arg(download.value(QStringLiteral("fileName")).toString(),
                                download.value(QStringLiteral("risk")).toString());
            } else {
                text += QStringLiteral("Download %1: %2\n")
                            .arg(download.value(QStringLiteral("state")).toString(),
                                download.value(QStringLiteral("path")).toString());
            }
        }
        const auto above = look.value(QStringLiteral("above")).toInt();
        const auto below = look.value(QStringLiteral("below")).toInt();
        if (above > 0) {
            text += QStringLiteral("%1 more above\n").arg(above);
        }
        if (below > 0) {
            text += QStringLiteral("%1 more below\n").arg(below);
        }
        return text;
    }

    QString formatSteps(const QJsonObject &answer)
    {
        QString text;
        for (const auto &value : answer.value(QStringLiteral("steps")).toArray()) {
            const auto step = value.toObject();
            QString line = step.value(QStringLiteral("ok")).toBool() ? QStringLiteral("ok")
                                                                     : QStringLiteral("failed");
            line += u' ' + step.value(QStringLiteral("step")).toString();
            const auto error = step.value(QStringLiteral("error")).toString();
            if (!error.isEmpty()) {
                line += QStringLiteral(": ") + error;
            } else if (step.contains(QStringLiteral("settled"))
                && !step.value(QStringLiteral("settled")).toBool()) {
                line += QStringLiteral(" (still changing)");
            }
            text += line + u'\n';
        }
        return text;
    }

    int answerTimeoutFor(const QJsonObject &request)
    {
        const auto verb = request.value(QStringLiteral("verb")).toString();
        if (verb == u"shot" && request.value(QStringLiteral("full")).toBool()) {
            return fullShotAnswerTimeoutMs;
        }
        if (verb == u"do") {
            const auto steps = request.value(QStringLiteral("steps")).toArray().size();
            const auto perStep = request.value(QStringLiteral("timeout")).toInt(10000)
                + request.value(QStringLiteral("settle")).toInt(300) + 2000;
            return static_cast<int>(std::min<qint64>(
                16 * 60 * 1000, static_cast<qint64>(steps) * perStep + pageAnswerTimeoutMs));
        }
        if (verb == u"look" || verb == u"read" || verb == u"shot" || verb == u"eval") {
            return pageAnswerTimeoutMs;
        }
        return answerTimeoutMs;
    }

} // namespace

QJsonArray readAgentSteps(const QStringList &arguments, QString &error)
{
    return readSteps(arguments, error);
}

QString agentConnectionName(const QString &name)
{
    const auto cleaned = cleanName(name);
    return cleaned.isEmpty() ? fallbackName : cleaned;
}

int agentAnswerTimeoutMs(const QJsonObject &request) { return answerTimeoutFor(request); }

bool isAgentCommand(const QStringList &arguments)
{
    static const QSet<QString> verbs {QStringLiteral("spaces"), QStringLiteral("tabs"),
        QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space"),
        QStringLiteral("look"), QStringLiteral("read"), QStringLiteral("do"),
        QStringLiteral("shot"), QStringLiteral("eval"), QStringLiteral("console"),
        QStringLiteral("commands"), QStringLiteral("run"), QStringLiteral("focus")};
    return arguments.size() > 1 && verbs.contains(arguments.at(1));
}

AgentCommand readAgentCommand(const QStringList &arguments, const QString &defaultName)
{
    AgentCommand command;
    if (!isAgentCommand(arguments)) {
        command.error = QStringLiteral("Not an Agent command.");
        return command;
    }
    auto verb = arguments.at(1);
    qsizetype next = 2;
    // `space <name>` switches to a Space, so a Space called "new" or "delete"
    // is switched to by its id.
    if (verb == u"space") {
        const auto action = arguments.value(2);
        if (action == u"new" || action == u"delete") {
            verb += u' ' + action;
            next = 3;
        }
    }

    const auto grammar = grammarFor(verb);
    QJsonObject request {{QStringLiteral("verb"), verb}};
    QString name = defaultName;
    QStringList positionals;
    // `--` ends the options, so an expression or an address that starts with
    // two dashes can still be given.
    auto optionsEnded = false;
    for (auto index = next; index < arguments.size(); ++index) {
        const auto &argument = arguments.at(index);
        if (!optionsEnded && argument == u"--") {
            optionsEnded = true;
            continue;
        }
        if (optionsEnded || !argument.startsWith(u"--")) {
            positionals.append(argument);
            continue;
        }
        auto option = argument.mid(2);
        QString value;
        const auto equals = option.indexOf(u'=');
        const auto hasInlineValue = equals >= 0;
        if (hasInlineValue) {
            value = option.mid(equals + 1);
            option = option.left(equals);
        }
        if (option == u"json" && !hasInlineValue) {
            command.json = true;
            continue;
        }
        if (grammar.flags.contains(option) && !hasInlineValue) {
            request.insert(option, true);
            continue;
        }
        const auto takesValue = option == u"name" || grammar.valued.contains(option);
        if (!takesValue) {
            command.error = QStringLiteral("`%1` takes no option --%2.").arg(verb, option);
            return command;
        }
        if (!hasInlineValue) {
            if (index + 1 >= arguments.size()) {
                command.error = QStringLiteral("--%1 needs a value.").arg(option);
                return command;
            }
            value = arguments.at(++index);
        }
        if (option == u"name") {
            name = value;
        } else if (option == u"settle" || option == u"timeout") {
            bool number = false;
            const auto milliseconds = value.toInt(&number);
            if (!number) {
                command.error = QStringLiteral("--%1 is a number of milliseconds.").arg(option);
                return command;
            }
            request.insert(option, milliseconds);
        } else if (option == u"since") {
            bool number = false;
            const auto cursor = value.toULongLong(&number);
            if (!number) {
                command.error = QStringLiteral("--since is the cursor a `console` answered.");
                return command;
            }
            request.insert(option, static_cast<double>(cursor));
        } else if (option == u"level" && value != u"error" && value != u"warning"
            && value != u"all") {
            command.error = QStringLiteral("--level is error, warning or all.");
            return command;
        } else {
            request.insert(option, value);
        }
    }

    if (verb == u"do") {
        QString error;
        const auto steps = readSteps(positionals, error);
        if (!error.isEmpty() || steps.isEmpty()) {
            command.error
                = error.isEmpty() ? QStringLiteral("`do` takes at least one step.") : error;
            return command;
        }
        request.insert(QStringLiteral("steps"), steps);
        positionals.clear();
    } else if (verb == u"eval") {
        if (positionals.isEmpty()) {
            command.error = QStringLiteral("`eval` takes an expression.");
            return command;
        }
        request.insert(QStringLiteral("expression"), positionals.join(u' '));
        positionals.clear();
    } else if (verb == u"run") {
        if (positionals.isEmpty() || positionals.size() > 2) {
            command.error = QStringLiteral(
                "`run` takes a command, and a position for select-tab and select-space.");
            return command;
        }
        if (positionals.size() == 2) {
            bool number = false;
            const auto position = positionals.at(1).toInt(&number);
            if (!number) {
                command.error = QStringLiteral("A position is a number, 1 for the first.");
                return command;
            }
            request.insert(QStringLiteral("argument"), position);
        }
        request.insert(QStringLiteral("command"), positionals.constFirst());
        positionals.clear();
    } else if (positionals.size() < grammar.minimumPositionals
        || (grammar.maximumPositionals >= 0 && positionals.size() > grammar.maximumPositionals)) {
        command.error = verb == u"open" ? QStringLiteral("`open` takes one address.")
            : verb == u"space delete" ? QStringLiteral("`space delete` takes the Space to delete.")
            : verb == u"space"
            ? QStringLiteral("Use `space <space>`, `space new [name]` or `space delete <space>`.")
            : verb == u"focus"
            ? QStringLiteral("`focus` takes a tab's id or a part of its address.")
            : QStringLiteral("`%1` takes no argument %2.")
                  .arg(verb, positionals.value(std::max<qsizetype>(grammar.maximumPositionals, 0)));
        return command;
    }
    if (!positionals.isEmpty()) {
        request.insert(grammar.positionalField, positionals.constFirst());
    }
    request.insert(QStringLiteral("name"), agentConnectionName(name));
    command.request = request;
    return command;
}

namespace {

    // One message a line: its level, where it was logged, and what it said,
    // with its own line breaks written out so each stays one line. The last
    // line is the cursor to pass as `--since` next time.
    QString formatConsole(const QJsonObject &answer)
    {
        QString text;
        if (answer.value(QStringLiteral("truncated")).toBool()) {
            text += QStringLiteral("truncated\tolder messages were dropped before this call\n");
        }
        for (const auto &value : answer.value(QStringLiteral("messages")).toArray()) {
            const auto message = value.toObject();
            auto said = message.value(QStringLiteral("message")).toString();
            said.replace(u'\\', QStringLiteral("\\\\")).replace(u'\n', QStringLiteral("\\n"));
            const auto line = message.value(QStringLiteral("line")).toInt();
            const auto source = message.value(QStringLiteral("source")).toString();
            text += message.value(QStringLiteral("level")).toString() + u'\t'
                + (line > 0 ? source + u':' + QString::number(line) : source) + u'\t' + said
                + u'\n';
        }
        text += QStringLiteral("cursor\t%1\n")
                    .arg(static_cast<quint64>(answer.value(QStringLiteral("cursor")).toDouble()));
        return text;
    }

} // namespace

QString formatAgentAnswer(const QString &verb, const QJsonObject &answer)
{
    const auto line = [](const QStringList &fields) { return fields.join(u'\t') + u'\n'; };
    QString text;
    if (verb == u"spaces") {
        for (const auto &value : answer.value(QStringLiteral("spaces")).toArray()) {
            const auto space = value.toObject();
            QStringList flags;
            if (space.value(QStringLiteral("onShow")).toBool()) {
                flags.append(QStringLiteral("on-show"));
            }
            if (space.value(QStringLiteral("agent")).toBool()) {
                flags.append(QStringLiteral("agent"));
            }
            text += line({space.value(QStringLiteral("id")).toString(),
                space.value(QStringLiteral("name")).toString(), flags.join(u',')});
        }
        return text;
    }
    if (verb == u"tabs") {
        for (const auto &value : answer.value(QStringLiteral("tabs")).toArray()) {
            const auto tab = value.toObject();
            QStringList flags;
            if (tab.value(QStringLiteral("current")).toBool()) {
                flags.append(QStringLiteral("current"));
            }
            if (tab.value(QStringLiteral("pinned")).toBool()) {
                flags.append(QStringLiteral("pinned"));
            }
            if (tab.value(QStringLiteral("window")).toBool()) {
                flags.append(
                    QStringLiteral("window of ") + tab.value(QStringLiteral("opener")).toString());
            }
            text += line({tab.value(QStringLiteral("id")).toString(),
                tab.value(QStringLiteral("url")).toString(),
                tab.value(QStringLiteral("title")).toString(), flags.join(u',')});
        }
        return text;
    }
    if (verb == u"open") {
        return line({answer.value(QStringLiteral("tab"))
                .toObject()
                .value(QStringLiteral("id"))
                .toString()});
    }
    if (verb == u"space new") {
        return line({answer.value(QStringLiteral("space"))
                .toObject()
                .value(QStringLiteral("id"))
                .toString()});
    }
    if (verb == u"close") {
        return line({answer.value(QStringLiteral("closed")).toString()});
    }
    if (verb == u"commands") {
        for (const auto &value : answer.value(QStringLiteral("commands")).toArray()) {
            const auto command = value.toObject();
            text += line({command.value(QStringLiteral("command")).toString(),
                command.value(QStringLiteral("title")).toString()});
        }
        return text;
    }
    // Whether it ran is the exit status, which is all a keybind reads.
    if (verb == u"run") {
        return text;
    }
    if (verb == u"space") {
        return line({answer.value(QStringLiteral("space")).toString()});
    }
    if (verb == u"focus") {
        return line({answer.value(QStringLiteral("tab")).toString()});
    }
    if (verb == u"look") {
        return formatLook(answer.value(QStringLiteral("look")).toObject());
    }
    if (verb == u"read") {
        auto markdown = answer.value(QStringLiteral("markdown")).toString();
        return markdown.endsWith(u'\n') ? markdown : markdown + u'\n';
    }
    if (verb == u"do") {
        QString opened;
        for (const auto &value : answer.value(QStringLiteral("opened")).toArray()) {
            opened += QStringLiteral("Opened %1\n").arg(value.toString());
        }
        return formatSteps(answer) + opened + u'\n'
            + formatLook(answer.value(QStringLiteral("look")).toObject());
    }
    if (verb == u"shot") {
        return line({answer.value(QStringLiteral("path")).toString()});
    }
    if (verb == u"console") {
        return formatConsole(answer);
    }
    if (verb == u"eval") {
        const auto value = answer.value(QStringLiteral("value"));
        if (value.isString()) {
            return value.toString() + u'\n';
        }
        // A bare value is not a JSON document, so it goes out inside an array
        // and the brackets come off.
        const auto wrapped
            = QString::fromUtf8(QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact));
        return wrapped.mid(1, wrapped.size() - 2) + u'\n';
    }
    return line({answer.value(QStringLiteral("deleted")).toString()});
}

QString parentProcessName()
{
    const auto parent = ::getppid();
#if defined(Q_OS_MACOS)
    char name[PROC_PIDPATHINFO_MAXSIZE] {};
    if (proc_name(parent, name, sizeof(name)) <= 0) {
        return {};
    }
    return QString::fromLocal8Bit(name);
#else
    QFile comm(QStringLiteral("/proc/%1/comm").arg(parent));
    if (!comm.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromLocal8Bit(comm.readAll()).trimmed();
#endif
}

int runAgentCommand(const QStringList &arguments, const QString &socketPath)
{
    const auto command = readAgentCommand(arguments, parentProcessName());
    if (!command.error.isEmpty()) {
        print(stderr, QStringLiteral("omaweb: %1\n").arg(command.error));
        return 2;
    }

    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (!socket.waitForConnected(answerTimeoutMs)) {
        print(stderr, QStringLiteral("omaweb: no Omaweb is running for this user.\n"));
        return 3;
    }
    socket.write(QJsonDocument(command.request).toJson(QJsonDocument::Compact) + '\n');
    socket.flush();
    const auto timeout = answerTimeoutFor(command.request);
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(timeout)) {
            print(stderr, QStringLiteral("omaweb: the browser did not answer.\n"));
            return 3;
        }
    }
    const auto answer = QJsonDocument::fromJson(socket.readLine()).object();
    const auto verb = command.request.value(QStringLiteral("verb")).toString();
    const auto ok = answer.value(QStringLiteral("ok")).toBool();
    if (command.json) {
        print(stdout,
            QString::fromUtf8(QJsonDocument(answer).toJson(QJsonDocument::Compact)) + u'\n');
    } else if (ok) {
        print(stdout, formatAgentAnswer(verb, answer));
    } else if (verb == u"do" && answer.contains(QStringLiteral("look"))) {
        // A batch that stopped still says what it did and what the page is
        // like now, which is what the Agent decides its next step from.
        print(stdout, formatAgentAnswer(verb, answer));
        print(stderr,
            QStringLiteral("omaweb: %1\n").arg(answer.value(QStringLiteral("error")).toString()));
    } else {
        print(stderr,
            QStringLiteral("omaweb: %1\n").arg(answer.value(QStringLiteral("error")).toString()));
    }
    if (ok && command.request.value(QStringLiteral("temporary")).toBool()
        && !holdConnection(socket)) {
        print(stderr,
            QStringLiteral("omaweb: could not stay to keep the temporary Space, so it is "
                           "deleted now.\n"));
        return 1;
    }
    return ok ? 0 : 1;
}

} // namespace omaweb
