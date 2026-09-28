#include "AgentCommand.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QSet>

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

    // Long enough for a browser busy with a page to answer.
    constexpr int answerTimeoutMs = 10000;

    const auto fallbackName = QStringLiteral("agent");

    struct Grammar {
        // The options this verb takes a value for, and those it takes alone.
        QSet<QString> valued;
        QSet<QString> flags;
        qsizetype minimumPositionals = 0;
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

} // namespace

bool isAgentCommand(const QStringList &arguments)
{
    static const QSet<QString> verbs {QStringLiteral("spaces"), QStringLiteral("tabs"),
        QStringLiteral("open"), QStringLiteral("close"), QStringLiteral("space")};
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
    if (verb == u"space") {
        const auto action = arguments.value(2);
        if (action != u"new" && action != u"delete") {
            command.error = QStringLiteral("Use `space new [name]` or `space delete <space>`.");
            return command;
        }
        verb += u' ' + action;
        next = 3;
    }

    const auto grammar = grammarFor(verb);
    QJsonObject request {{QStringLiteral("verb"), verb}};
    QString name = defaultName;
    QStringList positionals;
    for (auto index = next; index < arguments.size(); ++index) {
        const auto &argument = arguments.at(index);
        if (!argument.startsWith(u"--") || argument == u"--") {
            positionals.append(argument);
            continue;
        }
        auto option = argument.mid(2);
        QString value;
        const auto equals = option.indexOf(u'=');
        const auto inline_ = equals >= 0;
        if (inline_) {
            value = option.mid(equals + 1);
            option = option.left(equals);
        }
        if (option == u"json" && !inline_) {
            command.json = true;
            continue;
        }
        if (grammar.flags.contains(option) && !inline_) {
            request.insert(option, true);
            continue;
        }
        const auto takesValue = option == u"name" || grammar.valued.contains(option);
        if (!takesValue) {
            command.error = QStringLiteral("`%1` takes no option --%2.").arg(verb, option);
            return command;
        }
        if (!inline_) {
            if (index + 1 >= arguments.size()) {
                command.error = QStringLiteral("--%1 needs a value.").arg(option);
                return command;
            }
            value = arguments.at(++index);
        }
        if (option == u"name") {
            name = value;
        } else {
            request.insert(option, value);
        }
    }

    if (positionals.size() < grammar.minimumPositionals
        || positionals.size() > grammar.maximumPositionals) {
        command.error = verb == u"open" ? QStringLiteral("`open` takes one address.")
            : verb == u"space delete"
            ? QStringLiteral("`space delete` takes the Space to delete.")
            : QStringLiteral("`%1` takes no argument %2.").arg(verb, positionals.value(0));
        return command;
    }
    if (!positionals.isEmpty()) {
        request.insert(grammar.positionalField, positionals.constFirst());
    }
    name = cleanName(name);
    request.insert(QStringLiteral("name"), name.isEmpty() ? fallbackName : name);
    command.request = request;
    return command;
}

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
    while (!socket.canReadLine()) {
        if (!socket.waitForReadyRead(answerTimeoutMs)) {
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
