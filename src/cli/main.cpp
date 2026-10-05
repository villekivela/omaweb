#include "AgentClient.h"
#include "AgentProtocol.h"

#include <QCoreApplication>

#include <vector>

int main(int argc, char *argv[])
{
    // As it came, before Qt reads its own options out of it: the browser is
    // handed the command line the desktop gave this.
    const std::vector<char *> commandLine(argv, argv + argc);
    QCoreApplication client(argc, argv);
    QCoreApplication::setApplicationVersion(QStringLiteral(OMAWEB_VERSION));
    const auto browser = omaweb::findAgentBrowser(
        QCoreApplication::applicationDirPath(), QStringLiteral(OMAWEB_BROWSER_FROM_CLIENT));
    return omaweb::runAgentClient(commandLine, omaweb::agentSocketPath(), browser);
}
