#include "StartupPhases.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QTest>

namespace {

QStringList logged;

void collect(QtMsgType, const QMessageLogContext &context, const QString &message)
{
    logged.append(QStringLiteral("%1: %2").arg(QLatin1StringView(context.category), message));
}

} // namespace

class StartupPhasesTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void saysNothingUnlessAsked();
    void namesThePhaseAndTheWallClock();

private:
    QtMessageHandler m_previous = nullptr;
};

void StartupPhasesTest::init()
{
    logged.clear();
    m_previous = qInstallMessageHandler(collect);
}

void StartupPhasesTest::cleanup()
{
    qInstallMessageHandler(m_previous);
    QLoggingCategory::setFilterRules({});
}

// A launch nobody is measuring writes nothing.
void StartupPhasesTest::saysNothingUnlessAsked()
{
    omaweb::markStartupPhase("qml-loaded");
    QVERIFY(logged.isEmpty());
}

// `scripts/benchmark_runtime.py` reads these lines from the browser's stderr and sets each
// against its own clock, so the time is the wall clock's, in milliseconds since the epoch.
void StartupPhasesTest::namesThePhaseAndTheWallClock()
{
    QLoggingCategory::setFilterRules(QStringLiteral("omaweb.startup.info=true"));
    const auto before = QDateTime::currentMSecsSinceEpoch();
    omaweb::markStartupPhase("qml-loaded");
    const auto after = QDateTime::currentMSecsSinceEpoch();

    QCOMPARE(logged.size(), 1);
    const auto match
        = QRegularExpression(QStringLiteral(R"(^omaweb\.startup: phase (\S+) at (\d+)$)"))
              .match(logged.constFirst());
    QVERIFY2(match.hasMatch(), qPrintable(logged.constFirst()));
    QCOMPARE(match.captured(1), QStringLiteral("qml-loaded"));
    const auto at = match.captured(2).toLongLong();
    QVERIFY(at >= before && at <= after);
}

QTEST_GUILESS_MAIN(StartupPhasesTest)
#include "tst_startupphases.moc"
