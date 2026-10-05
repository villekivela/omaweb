#include "AgentClient.h"

#include "AgentCommand.h"
#include "AgentMcp.h"
#include "LaunchRequest.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <functional>

#include <unistd.h>

namespace omaweb {

namespace {

    // Replaces this process with the browser, which reads `commandLine` as
    // though it had been run itself. Replaced rather than started beside, so
    // the process a desktop launched, and watches for its window, is the
    // browser's.
    int becomeBrowser(const std::vector<char *> &commandLine, const QString &browserPath)
    {
        const auto program = QFile::encodeName(browserPath);
        std::vector<char *> arguments(commandLine);
        arguments.front() = const_cast<char *>(program.constData());
        arguments.push_back(nullptr);
        ::execv(program.constData(), arguments.data());
        std::fprintf(stderr, "omaweb: could not run the browser at %s: %s\n", program.constData(),
            std::strerror(errno));
        return 1;
    }

} // namespace

// The browser outlives the MCP server that started it, and standard output is
// the MCP channel, so it gets none of this process's streams.
bool startAgentBrowser(const QString &path)
{
    QProcess browser;
    browser.setProgram(path);
    browser.setStandardInputFile(QProcess::nullDevice());
    browser.setStandardOutputFile(QProcess::nullDevice());
    browser.setStandardErrorFile(QProcess::nullDevice());
    return browser.startDetached();
}

QString findAgentBrowser(const QString &clientDirectory, const QString &installedFromClient)
{
    const QDir here(clientDirectory);
    const QStringList candidates {
        here.filePath(QStringLiteral("omaweb-browser")),
#if defined(Q_OS_MACOS)
        here.filePath(QStringLiteral("omaweb-browser.app/Contents/MacOS/omaweb-browser")),
#endif
        QDir::cleanPath(here.filePath(installedFromClient)),
    };
    for (const auto &candidate : candidates) {
        const QFileInfo browser(candidate);
        if (browser.isFile() && browser.isExecutable()) {
            return candidate;
        }
    }
    return {};
}

int runAgentClient(
    const std::vector<char *> &commandLine, const QString &socketPath, const QString &browserPath)
{
    QStringList arguments;
    for (const auto *argument : commandLine) {
        arguments.append(QString::fromLocal8Bit(argument));
    }
    if (isAgentMcpCommand(arguments)) {
        std::function<bool()> start;
        if (!browserPath.isEmpty()) {
            start = [browserPath] { return startAgentBrowser(browserPath); };
        }
        return runAgentMcp(arguments, socketPath, start);
    }
    if (isAgentCommand(arguments)) {
        return runAgentCommand(arguments, socketPath);
    }
    if (browserPath.isEmpty()) {
        // The browser reports the engine's versions too, and with none here
        // the client's own is all there is to report.
        if (readVersionRequest(arguments)) {
            std::printf("Omaweb %s\nThe client alone: no browser is installed here.\n",
                qPrintable(QCoreApplication::applicationVersion()));
            return 0;
        }
        std::fprintf(stderr,
            "omaweb: the Omaweb browser is not installed here. This machine reaches a browser "
            "only over its Agent socket, %s.\n",
            qPrintable(socketPath));
        return 1;
    }
    return becomeBrowser(commandLine, browserPath);
}

} // namespace omaweb
