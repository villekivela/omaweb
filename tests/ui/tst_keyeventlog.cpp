#include "KeyEventLog.h"

#include <QGuiApplication>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QTest>
#include <QWindow>

namespace {

QStringList logged;

void collect(QtMsgType, const QMessageLogContext &context, const QString &message)
{
    if (QLatin1StringView(context.category).startsWith(QLatin1StringView("omaweb.")))
        logged.append(QStringLiteral("%1: %2").arg(QLatin1StringView(context.category), message));
}

} // namespace

class KeyEventLogTest final : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void saysNothingUnlessAsked();
    void namesTheKeyWhereItWentAndTheDialog();
    void namesWhatTookTheFocus();

private:
    QtMessageHandler m_previous = nullptr;
    QObject *m_log = nullptr;
};

void KeyEventLogTest::init()
{
    logged.clear();
    m_previous = qInstallMessageHandler(collect);
}

void KeyEventLogTest::cleanup()
{
    delete m_log;
    m_log = nullptr;
    qInstallMessageHandler(m_previous);
    QLoggingCategory::setFilterRules({});
}

// A browser nobody is diagnosing pays for no filter on every event.
void KeyEventLogTest::saysNothingUnlessAsked()
{
    m_log = omaweb::installKeyEventLog(qApp);
    QVERIFY(m_log == nullptr);
    QWindow window;
    QKeyEvent press(QEvent::KeyPress, Qt::Key_N, Qt::ControlModifier | Qt::AltModifier);
    QCoreApplication::sendEvent(&window, &press);
    QVERIFY(logged.isEmpty());
}

// `scripts/benchmark_runtime.py` prints these when a key it sent did nothing, so a line says which
// key arrived, what it would type, which object was handed it, what held the focus, and which
// dialog the window had open.
void KeyEventLogTest::namesTheKeyWhereItWentAndTheDialog()
{
    QLoggingCategory::setFilterRules(QStringLiteral("omaweb.keys.info=true"));
    m_log = omaweb::installKeyEventLog(qApp);
    QWindow window;
    window.setObjectName(QStringLiteral("shell"));
    window.setProperty("dialogMode", QStringLiteral("new"));

    QKeyEvent press(
        QEvent::KeyPress, Qt::Key_N, Qt::ControlModifier | Qt::AltModifier, QStringLiteral("\x0e"));
    QCoreApplication::sendEvent(&window, &press);
    QKeyEvent letter(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier, QStringLiteral("s"));
    QCoreApplication::sendEvent(&window, &letter);

    QCOMPARE(logged,
        QStringList({
            QStringLiteral(R"(omaweb.keys: press Ctrl+Alt+N text "\x0e" to QWindow "shell")"
                           R"(, focus none, dialog "new")"),
            QStringLiteral(R"(omaweb.keys: press S text "s" to QWindow "shell")"
                           R"(, focus none, dialog "new")"),
        }));
}

// A dialog that opens takes the focus for its field, so the focus moving says it opened.
void KeyEventLogTest::namesWhatTookTheFocus()
{
    QLoggingCategory::setFilterRules(QStringLiteral("omaweb.keys.info=true"));
    m_log = omaweb::installKeyEventLog(qApp);
    QWindow window;
    window.setObjectName(QStringLiteral("shell"));
    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&window));

    QVERIFY2(logged.contains(QStringLiteral(R"(omaweb.keys: focus moved to QWindow "shell")")),
        qPrintable(logged.join(u'\n')));
}

QTEST_MAIN(KeyEventLogTest)
#include "tst_keyeventlog.moc"
