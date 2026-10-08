#include "AgentControl.h"
#include "AgentSetup.h"
#include "AgentsFile.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>

#include <atomic>

#include <memory>

using omaweb::AgentControl;
using omaweb::AgentSetup;
namespace AgentsFile = omaweb::AgentsFile;

namespace {

QString linkTarget(const QString &path)
{
    const QFileInfo entry(path);
    return entry.isSymLink() ? entry.readSymLink() : QString();
}

bool present(const QString &path)
{
    const QFileInfo entry(path);
    return entry.isSymLink() || entry.exists();
}

// A program on `PATH` that writes each command line it is given to `log` and
// exits with `status`, or with `existingStatus` when asked whether it has a
// server already.
void writeAgent(const QString &directory, const QString &name, const QString &log,
    int existingStatus, int status = 0)
{
    QFile script(QDir(directory).filePath(name));
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write(QStringLiteral("#!/bin/sh\n"
                                "printf '%s|%s\\n' \"$PWD\" \"$*\" >> '%1'\n"
                                "[ \"$2\" = get ] && exit %2\n"
                                "exit %3\n")
            .arg(log)
            .arg(existingStatus)
            .arg(status)
            .toUtf8());
    script.close();
    QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
}

QStringList lines(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
}

} // namespace

class AgentSetupTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();

    void linksTheSkillForEachAgentThatIsInstalled();
    void findsTheSkillAlreadyLinkedTheSecondTime();
    void leavesTheReadersOwnSkillEntriesAlone();
    void unlinksOnlyLinksToThePackagedSkill();
    void addsTheMcpServerThroughEachAgentsOwnCommand();
    void leavesAnExistingMcpServerAlone();
    void reportsAnAgentCommandThatFails();
    void findsAnMcpAgentOnlyOnPath();

    void turningAllowAgentsOnLinksTheSkillAndOffUnlinksIt();
    void linksOnceForAReaderWhoAlreadyAllowsAgents();
    void leavesTheSkillForAReaderWhoHasNotAllowedAgents();
    void saysWhenAddSkillFindsEverythingInPlace();
    void saysWhatAddMcpServerAddedAndLeftAlone();
    void linksNothingWithoutASetup();
    void turningAgentsOffInTheFileUnlinksTheSkill();
    void linksNothingWhenTheFileAllowsAgents();
    void recordsTheOneTimeLinkEvenWhenItFails();
    void findsPiByItsAgentDirectory();
    void unlinksADanglingLinkToThePackagedSkill();
    void saysWhatItLeftAlone();
    void dropsAnMcpServerRunWhenAgentsAreTurnedOff();
    void stopsAnAgentCommandWhenCancelled();
    void quitsWithoutWaitingOnAnAgentCommand();
    void saysItIsAddingTheMcpServer();
    void asksForMcpAgentsAgainWithASetup();

private:
    std::unique_ptr<QTemporaryDir> m_home;
    std::unique_ptr<QTemporaryDir> m_bin;
    QString m_skill;
    QByteArray m_path;

    QString home(const QString &path) const { return QDir(m_home->path()).filePath(path); }
    AgentSetup setup() const { return AgentSetup(m_home->path(), m_skill); }
    QString config() const { return home(QStringLiteral(".config/omaweb")); }
    std::unique_ptr<AgentControl> control() const
    {
        auto made = std::make_unique<AgentControl>(nullptr, config());
        made->setAgentSetup(setup());
        return made;
    }
};

// The caption lists agents the way the reader's language does.
void AgentSetupTest::initTestCase()
{
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
}

void AgentSetupTest::init()
{
    m_home = std::make_unique<QTemporaryDir>();
    m_bin = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid() && m_bin->isValid());
    // The packaged skill stands outside the home, as /usr/share does.
    m_skill = QDir(m_bin->path()).filePath(QStringLiteral("share/omaweb/skills/omaweb"));
    QVERIFY(QDir().mkpath(m_skill));
    m_path = qgetenv("PATH");
    qputenv("PATH", m_bin->path().toUtf8());
}

void AgentSetupTest::cleanup() { qputenv("PATH", m_path); }

void AgentSetupTest::linksTheSkillForEachAgentThatIsInstalled()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".claude"))));
    QVERIFY(QDir().mkpath(home(QStringLiteral(".pi/agent"))));

    const auto outcome = setup().linkSkill();

    QCOMPARE(linkTarget(home(QStringLiteral(".agents/skills/omaweb"))), m_skill);
    QCOMPARE(linkTarget(home(QStringLiteral(".claude/skills/omaweb"))), m_skill);
    QCOMPARE(linkTarget(home(QStringLiteral(".pi/agent/skills/omaweb"))), m_skill);
    QVERIFY(!present(home(QStringLiteral(".codex"))));
    QVERIFY(!present(home(QStringLiteral(".hermes"))));
    QCOMPARE(outcome.changed,
        (QStringList {QString(), QStringLiteral("Claude Code"), QStringLiteral("pi")}));
    QVERIFY(outcome.kept.isEmpty());
    QVERIFY(outcome.failed.isEmpty());
}

void AgentSetupTest::findsTheSkillAlreadyLinkedTheSecondTime()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".codex"))));
    setup().linkSkill();

    const auto outcome = setup().linkSkill();

    QVERIFY(outcome.changed.isEmpty());
    QCOMPARE(outcome.kept, (QStringList {QString(), QStringLiteral("Codex")}));
    QCOMPARE(linkTarget(home(QStringLiteral(".codex/skills/omaweb"))), m_skill);
}

void AgentSetupTest::leavesTheReadersOwnSkillEntriesAlone()
{
    // A copy the reader keeps, and a link to their checkout.
    const auto own = home(QStringLiteral(".claude/skills/omaweb"));
    QVERIFY(QDir().mkpath(own));
    QVERIFY(QDir().mkpath(home(QStringLiteral(".codex/skills"))));
    const auto checkout = home(QStringLiteral("code/omaweb/integrations/agent/omaweb"));
    QVERIFY(QFile::link(checkout, home(QStringLiteral(".codex/skills/omaweb"))));
    // A file of the reader's in the skill's place.
    QVERIFY(QDir().mkpath(home(QStringLiteral(".hermes/skills"))));
    QFile file(home(QStringLiteral(".hermes/skills/omaweb")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("mine");
    file.close();

    const auto outcome = setup().linkSkill();

    QVERIFY(QFileInfo(own).isDir() && !QFileInfo(own).isSymLink());
    QCOMPARE(linkTarget(home(QStringLiteral(".codex/skills/omaweb"))), checkout);
    QVERIFY(QFileInfo(file.fileName()).isFile() && !QFileInfo(file.fileName()).isSymLink());
    QCOMPARE(outcome.changed, QStringList {QString()});
    QVERIFY(outcome.kept.isEmpty());
    QCOMPARE(outcome.leftAlone,
        (QStringList {
            QStringLiteral("Claude Code"), QStringLiteral("Codex"), QStringLiteral("Hermes")}));

    const auto off = setup().unlinkSkill();
    QVERIFY(QFileInfo(file.fileName()).isFile());
    QCOMPARE(off.changed, QStringList {QString()});
}

void AgentSetupTest::unlinksOnlyLinksToThePackagedSkill()
{
    const auto own = home(QStringLiteral(".claude/skills/omaweb"));
    QVERIFY(QDir().mkpath(own));
    QVERIFY(QDir().mkpath(home(QStringLiteral(".codex/skills"))));
    const auto checkout = home(QStringLiteral("code/omaweb/integrations/agent/omaweb"));
    QVERIFY(QFile::link(checkout, home(QStringLiteral(".codex/skills/omaweb"))));
    QVERIFY(QDir().mkpath(home(QStringLiteral(".hermes"))));
    setup().linkSkill();
    QCOMPARE(linkTarget(home(QStringLiteral(".hermes/skills/omaweb"))), m_skill);

    const auto outcome = setup().unlinkSkill();

    QVERIFY(!present(home(QStringLiteral(".agents/skills/omaweb"))));
    QVERIFY(!present(home(QStringLiteral(".hermes/skills/omaweb"))));
    QVERIFY(QFileInfo(own).isDir());
    QCOMPARE(linkTarget(home(QStringLiteral(".codex/skills/omaweb"))), checkout);
    QCOMPARE(outcome.changed, (QStringList {QString(), QStringLiteral("Hermes")}));
    // The directories the skill went into stay for whatever else is in them.
    QVERIFY(QFileInfo(home(QStringLiteral(".hermes/skills"))).isDir());
}

void AgentSetupTest::addsTheMcpServerThroughEachAgentsOwnCommand()
{
    const auto claudeLog = QDir(m_bin->path()).filePath(QStringLiteral("claude.log"));
    const auto codexLog = QDir(m_bin->path()).filePath(QStringLiteral("codex.log"));
    writeAgent(m_bin->path(), QStringLiteral("claude"), claudeLog, 1);
    writeAgent(m_bin->path(), QStringLiteral("codex"), codexLog, 1);

    const auto outcome = setup().addMcpServer();

    const auto at = m_home->path() + u'|';
    QCOMPARE(lines(claudeLog),
        (QStringList {at + QStringLiteral("mcp get omaweb"),
            at + QStringLiteral("mcp add -s user omaweb -- omaweb mcp")}));
    QCOMPARE(lines(codexLog),
        (QStringList {at + QStringLiteral("mcp get omaweb"),
            at + QStringLiteral("mcp add omaweb -- omaweb mcp")}));
    QCOMPARE(
        outcome.changed, (QStringList {QStringLiteral("Claude Code"), QStringLiteral("Codex")}));
    QVERIFY(outcome.kept.isEmpty());
    QVERIFY(outcome.failed.isEmpty());
}

void AgentSetupTest::leavesAnExistingMcpServerAlone()
{
    const auto claudeLog = QDir(m_bin->path()).filePath(QStringLiteral("claude.log"));
    writeAgent(m_bin->path(), QStringLiteral("claude"), claudeLog, 0);

    const auto outcome = setup().addMcpServer();

    QCOMPARE(lines(claudeLog), QStringList {m_home->path() + QStringLiteral("|mcp get omaweb")});
    QVERIFY(outcome.changed.isEmpty());
    QCOMPARE(outcome.kept, QStringList {QStringLiteral("Claude Code")});
}

void AgentSetupTest::reportsAnAgentCommandThatFails()
{
    const auto codexLog = QDir(m_bin->path()).filePath(QStringLiteral("codex.log"));
    writeAgent(m_bin->path(), QStringLiteral("codex"), codexLog, 1, 1);

    const auto outcome = setup().addMcpServer();

    QVERIFY(outcome.changed.isEmpty());
    QCOMPARE(outcome.failed, QStringList {QStringLiteral("Codex")});
}

void AgentSetupTest::findsAnMcpAgentOnlyOnPath()
{
    QVERIFY(!AgentSetup::mcpAgentPresent());
    writeAgent(m_bin->path(), QStringLiteral("codex"),
        QDir(m_bin->path()).filePath(QStringLiteral("codex.log")), 1);
    QVERIFY(AgentSetup::mcpAgentPresent());
}

void AgentSetupTest::turningAllowAgentsOnLinksTheSkillAndOffUnlinksIt()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".claude"))));
    const auto agents = control();
    QVERIFY(agents->hasAgentSetup());
    QVERIFY(!present(home(QStringLiteral(".agents/skills/omaweb"))));

    agents->setAllowAgents(true);

    QCOMPARE(linkTarget(home(QStringLiteral(".agents/skills/omaweb"))), m_skill);
    QCOMPARE(linkTarget(home(QStringLiteral(".claude/skills/omaweb"))), m_skill);
    QCOMPARE(
        agents->agentSetupNote(), QStringLiteral("Skill added for Claude Code and other agents."));

    agents->setAllowAgents(false);

    QVERIFY(!present(home(QStringLiteral(".agents/skills/omaweb"))));
    QVERIFY(!present(home(QStringLiteral(".claude/skills/omaweb"))));
    QVERIFY(agents->agentSetupNote().isEmpty());
}

void AgentSetupTest::linksOnceForAReaderWhoAlreadyAllowsAgents()
{
    QVERIFY(AgentsFile::write(config(), QLatin1StringView("allow-agents"), true));
    const auto shared = home(QStringLiteral(".agents/skills/omaweb"));

    control();

    QCOMPARE(linkTarget(shared), m_skill);

    // The reader took the link away, and a later start leaves it that way.
    QVERIFY(QFile::remove(shared));
    control();
    QVERIFY(!present(shared));
}

void AgentSetupTest::leavesTheSkillForAReaderWhoHasNotAllowedAgents()
{
    control();
    QVERIFY(!present(home(QStringLiteral(".agents"))));

    // Allowing them later links it through the switch, not at a start.
    QVERIFY(AgentsFile::write(config(), QLatin1StringView("allow-agents"), true));
    control();
    QVERIFY(!present(home(QStringLiteral(".agents"))));
}

void AgentSetupTest::saysWhenAddSkillFindsEverythingInPlace()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".codex"))));
    const auto agents = control();
    agents->setAllowAgents(true);
    QSignalSpy noted(agents.get(), &AgentControl::agentSetupNoteChanged);

    agents->addSkill();

    QCOMPARE(noted.count(), 1);
    QCOMPARE(
        agents->agentSetupNote(), QStringLiteral("Codex and other agents already had the skill."));
}

void AgentSetupTest::saysWhatAddMcpServerAddedAndLeftAlone()
{
    writeAgent(m_bin->path(), QStringLiteral("claude"),
        QDir(m_bin->path()).filePath(QStringLiteral("claude.log")), 1);
    writeAgent(m_bin->path(), QStringLiteral("codex"),
        QDir(m_bin->path()).filePath(QStringLiteral("codex.log")), 0);
    const auto agents = control();
    agents->setAllowAgents(true);
    QVERIFY(agents->mcpAgentPresent());

    agents->addMcpServer();

    QTRY_COMPARE(agents->agentSetupNote(),
        QStringLiteral("MCP server added for Claude Code. Codex already had one."));
}

void AgentSetupTest::linksNothingWithoutASetup()
{
    AgentControl agents(nullptr, config());
    QVERIFY(!agents.hasAgentSetup());
    agents.setAllowAgents(true);
    agents.addSkill();
    QVERIFY(!present(home(QStringLiteral(".agents"))));
}

// However Allow agents is turned off, the links go with it.
void AgentSetupTest::turningAgentsOffInTheFileUnlinksTheSkill()
{
    const auto agents = control();
    agents->setAllowAgents(true);
    const auto shared = home(QStringLiteral(".agents/skills/omaweb"));
    QCOMPARE(linkTarget(shared), m_skill);

    QVERIFY(AgentsFile::write(config(), QLatin1StringView("allow-agents"), false));

    QTRY_VERIFY(!agents->allowAgents());
    QVERIFY(!present(shared));
    QVERIFY(agents->agentSetupNote().isEmpty());
}

// Only the reader's switch links the skill; a file that turns Agents on is not
// the consent the decision waits for.
void AgentSetupTest::linksNothingWhenTheFileAllowsAgents()
{
    const auto agents = control();

    QVERIFY(AgentsFile::write(config(), QLatin1StringView("allow-agents"), true));

    QTRY_VERIFY(agents->allowAgents());
    QVERIFY(!present(home(QStringLiteral(".agents"))));
}

// The first start records that it ran, linked or not, and never runs again.
void AgentSetupTest::recordsTheOneTimeLinkEvenWhenItFails()
{
    QVERIFY(AgentsFile::write(config(), QLatin1StringView("allow-agents"), true));
    // A file where the shared skills directory goes.
    QFile blocking(home(QStringLiteral(".agents")));
    QVERIFY(blocking.open(QIODevice::WriteOnly));
    blocking.close();

    control();

    QVERIFY(blocking.remove());
    control();
    QVERIFY(!present(home(QStringLiteral(".agents"))));
}

// pi keeps its skills under ~/.pi/agent, which an installed pi has.
void AgentSetupTest::findsPiByItsAgentDirectory()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".pi"))));

    const auto outcome = setup().linkSkill();

    QVERIFY(!present(home(QStringLiteral(".pi/agent"))));
    QCOMPARE(outcome.changed, QStringList {QString()});
}

// The package was removed, and the link it leaves still goes when Agents do.
void AgentSetupTest::unlinksADanglingLinkToThePackagedSkill()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".claude"))));
    setup().linkSkill();
    QVERIFY(QDir(m_skill).removeRecursively());
    QVERIFY(!QFileInfo::exists(home(QStringLiteral(".claude/skills/omaweb"))));

    const auto outcome = setup().unlinkSkill();

    QVERIFY(!present(home(QStringLiteral(".claude/skills/omaweb"))));
    QVERIFY(!present(home(QStringLiteral(".agents/skills/omaweb"))));
    QCOMPARE(outcome.changed, (QStringList {QString(), QStringLiteral("Claude Code")}));
}

void AgentSetupTest::saysWhatItLeftAlone()
{
    QVERIFY(QDir().mkpath(home(QStringLiteral(".claude/skills/omaweb"))));
    const auto agents = control();

    agents->setAllowAgents(true);

    QCOMPARE(agents->agentSetupNote(),
        QStringLiteral(
            "Skill added for other agents. Your own omaweb skill for Claude Code was left alone."));
}

// A server still being added when Agents are turned off is stopped, and what
// it found does not replace the caption the switch wrote after it.
void AgentSetupTest::dropsAnMcpServerRunWhenAgentsAreTurnedOff()
{
    QFile script(QDir(m_bin->path()).filePath(QStringLiteral("claude")));
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nexec /bin/sleep 30\n");
    script.close();
    QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
    const auto agents = control();
    agents->setAllowAgents(true);
    agents->addMcpServer();
    QTest::qWait(200);

    QElapsedTimer clock;
    clock.start();
    agents->setAllowAgents(false);
    agents->setAllowAgents(true);
    QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
    QCoreApplication::processEvents();

    QVERIFY2(clock.elapsed() < 5000, qPrintable(QString::number(clock.elapsed())));
    QCOMPARE(agents->agentSetupNote(), QStringLiteral("Skill added for other agents."));

    // The next click runs again.
    writeAgent(m_bin->path(), QStringLiteral("claude"),
        QDir(m_bin->path()).filePath(QStringLiteral("claude.log")), 1);
    agents->addMcpServer();
    QTRY_COMPARE(agents->agentSetupNote(), QStringLiteral("MCP server added for Claude Code."));
}

// An agent that has stopped answering, whose command is still waited on when
// the browser quits.
void AgentSetupTest::stopsAnAgentCommandWhenCancelled()
{
    QFile script(QDir(m_bin->path()).filePath(QStringLiteral("claude")));
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nexec /bin/sleep 30\n");
    script.close();
    QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
    const std::atomic_bool cancelled = true;

    QElapsedTimer clock;
    clock.start();
    const auto outcome = setup().addMcpServer(AgentSetup::defaultMcpTimeoutMs, &cancelled);

    QVERIFY2(clock.elapsed() < 5000, qPrintable(QString::number(clock.elapsed())));
    QCOMPARE(outcome.failed, QStringList {QStringLiteral("Claude Code")});
}

void AgentSetupTest::quitsWithoutWaitingOnAnAgentCommand()
{
    QFile script(QDir(m_bin->path()).filePath(QStringLiteral("claude")));
    QVERIFY(script.open(QIODevice::WriteOnly));
    script.write("#!/bin/sh\nexec /bin/sleep 30\n");
    script.close();
    QVERIFY(script.setPermissions(script.permissions() | QFileDevice::ExeOwner));
    auto agents = control();
    agents->setAllowAgents(true);
    agents->addMcpServer();
    QTest::qWait(200);

    QElapsedTimer clock;
    clock.start();
    agents.reset();
    QThreadPool::globalInstance()->waitForDone();

    QVERIFY2(clock.elapsed() < 5000, qPrintable(QString::number(clock.elapsed())));
}

void AgentSetupTest::saysItIsAddingTheMcpServer()
{
    writeAgent(m_bin->path(), QStringLiteral("claude"),
        QDir(m_bin->path()).filePath(QStringLiteral("claude.log")), 1);
    const auto agents = control();
    agents->setAllowAgents(true);

    agents->addMcpServer();

    QCOMPARE(agents->agentSetupNote(), QStringLiteral("Adding the MCP server…"));
    QTRY_COMPARE(agents->agentSetupNote(), QStringLiteral("MCP server added for Claude Code."));
}

void AgentSetupTest::asksForMcpAgentsAgainWithASetup()
{
    writeAgent(m_bin->path(), QStringLiteral("codex"),
        QDir(m_bin->path()).filePath(QStringLiteral("codex.log")), 1);
    AgentControl agents(nullptr, config());
    QVERIFY(!agents.mcpAgentPresent());
    QSignalSpy asked(&agents, &AgentControl::mcpAgentPresentChanged);

    agents.setAgentSetup(setup());

    QCOMPARE(asked.count(), 1);
    QVERIFY(agents.mcpAgentPresent());
}

QTEST_GUILESS_MAIN(AgentSetupTest)
#include "tst_agentsetup.moc"
