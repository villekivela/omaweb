#include "InputMethod.h"

#include <QTest>

using omaweb::inputMethodAvailable;
using omaweb::inputMethodDiagnostic;
using omaweb::InputMethodHost;

class InputMethodTest final : public QObject {
    Q_OBJECT

private slots:
    void saysNothingAboutADesktopThatAskedForNoInputMethod();
    void answersForTheModuleThePluginIsInstalledFor();
    void namesTheModuleNoPluginAnswersTo();
    void matchesTheNameTheWayQtMatchesIt();
    void takesAnyOneOfTheModulesNamedInOrder();
};

// A reader who is not using an input method has nothing to be told, and a
// browser that reported on one anyway would be reporting on every desktop.
void InputMethodTest::saysNothingAboutADesktopThatAskedForNoInputMethod()
{
    const InputMethodHost silent {{}, {QStringLiteral("compose")}};
    QVERIFY(inputMethodAvailable(silent));
    QCOMPARE(inputMethodDiagnostic(silent), QString());
}

void InputMethodTest::answersForTheModuleThePluginIsInstalledFor()
{
    const InputMethodHost host {
        {QStringLiteral("ibus")}, {QStringLiteral("compose"), QStringLiteral("ibus")}};
    QVERIFY(inputMethodAvailable(host));
    QVERIFY(inputMethodDiagnostic(host).contains(QStringLiteral("ibus")));
    QVERIFY(inputMethodDiagnostic(host).contains(QStringLiteral("installed")));
}

// The case this exists for: the desktop names a plugin it did not install, Qt
// loads no input context, and nothing anywhere says so.
void InputMethodTest::namesTheModuleNoPluginAnswersTo()
{
    const InputMethodHost host {
        {QStringLiteral("fcitx")}, {QStringLiteral("compose"), QStringLiteral("ibus")}};
    QVERIFY(!inputMethodAvailable(host));

    const auto diagnostic = inputMethodDiagnostic(host);
    QVERIFY(diagnostic.contains(QStringLiteral("fcitx")));
    QVERIFY(diagnostic.contains(QStringLiteral("no plugin by that name")));
}

// Qt looks a plugin's keys up without regard to case, so a desktop that shouts
// the name is configured rather than misconfigured.
void InputMethodTest::matchesTheNameTheWayQtMatchesIt()
{
    const InputMethodHost host {{QStringLiteral("FCITX")}, {QStringLiteral("fcitx")}};
    QVERIFY(inputMethodAvailable(host));
}

// `QT_IM_MODULES` names a list to try in order, and Qt uses the first that
// answers. One installed is enough, and none is the misconfiguration.
void InputMethodTest::takesAnyOneOfTheModulesNamedInOrder()
{
    const InputMethodHost some {
        {QStringLiteral("fcitx"), QStringLiteral("ibus")}, {QStringLiteral("ibus")}};
    QVERIFY(inputMethodAvailable(some));

    const InputMethodHost none {
        {QStringLiteral("fcitx"), QStringLiteral("ibus")}, {QStringLiteral("compose")}};
    QVERIFY(!inputMethodAvailable(none));
    QVERIFY(inputMethodDiagnostic(none).contains(QStringLiteral("fcitx, ibus")));
}

QTEST_MAIN(InputMethodTest)

#include "tst_inputmethod.moc"
