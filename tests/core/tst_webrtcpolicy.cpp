#include "GlobalPrivacyControl.h"
#include "WebRtcPolicy.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::GlobalPrivacyControl;
using omaweb::WebRtcPolicy;

class WebRtcPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void isOnWithoutBeingSetUp();
    void keepsTheReadersChoiceAcrossARestart();
    void readsAFileItCannotUseAsTheDefault_data();
    void readsAFileItCannotUseAsTheDefault();
    void writesNothingForAChoiceAlreadyMade();
    void sharesTheFileWithTheOtherPrivacyDecisions();
};

// The safe answer is the one nobody set up: a first run offers a page's
// calls the public route and nothing else.
void WebRtcPolicyTest::isOnWithoutBeingSetUp()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const WebRtcPolicy policy(root.filePath(QStringLiteral("config")));
    QVERIFY(policy.publicInterfacesOnly());
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/privacy.json"))));
}

// Off is the reader's decision, so it is written where the reader's other
// decisions live (ADR 0016) and a second instance reads it back.
void WebRtcPolicyTest::keepsTheReadersChoiceAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        WebRtcPolicy policy(configRoot);
        QSignalSpy changed(&policy, &WebRtcPolicy::publicInterfacesOnlyChanged);
        policy.setPublicInterfacesOnly(false);
        QVERIFY(!policy.publicInterfacesOnly());
        QCOMPARE(changed.count(), 1);
    }
    {
        WebRtcPolicy restarted(configRoot);
        QVERIFY(!restarted.publicInterfacesOnly());
        restarted.setPublicInterfacesOnly(true);
    }
    const WebRtcPolicy again(configRoot);
    QVERIFY(again.publicInterfacesOnly());
}

void WebRtcPolicyTest::readsAFileItCannotUseAsTheDefault_data()
{
    QTest::addColumn<QByteArray>("contents");
    QTest::newRow("not json") << QByteArray("webrtc-public-interfaces-only = false");
    QTest::newRow("not an object") << QByteArray("[false]");
    QTest::newRow("another key") << QByteArray(R"({"global-privacy-control": false})");
    QTest::newRow("not a bool") << QByteArray(R"({"webrtc-public-interfaces-only": "off"})");
}

// A file the reader edited by hand and got wrong opens the reader's other
// interfaces to nobody: only an explicit `false` does.
void WebRtcPolicyTest::readsAFileItCannotUseAsTheDefault()
{
    QFETCH(QByteArray, contents);
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir(root.path()).mkpath(QStringLiteral("config")));
    QFile file(root.filePath(QStringLiteral("config/privacy.json")));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();
    const WebRtcPolicy policy(root.filePath(QStringLiteral("config")));
    QVERIFY(policy.publicInterfacesOnly());
}

void WebRtcPolicyTest::writesNothingForAChoiceAlreadyMade()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    WebRtcPolicy policy(root.filePath(QStringLiteral("config")));
    QSignalSpy changed(&policy, &WebRtcPolicy::publicInterfacesOnlyChanged);
    policy.setPublicInterfacesOnly(true);
    QCOMPARE(changed.count(), 0);
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/privacy.json"))));
}

// The privacy section keeps one file, so each decision written there leaves
// the others as they were, whichever order the reader flips them in.
void WebRtcPolicyTest::sharesTheFileWithTheOtherPrivacyDecisions()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    GlobalPrivacyControl control(configRoot);
    WebRtcPolicy policy(configRoot);
    control.setEnabled(false);
    policy.setPublicInterfacesOnly(false);
    control.setEnabled(true);

    QFile file(root.filePath(QStringLiteral("config/privacy.json")));
    QVERIFY(file.open(QIODevice::ReadOnly));
    const auto written = QJsonDocument::fromJson(file.readAll()).object();
    QCOMPARE(written.value(QLatin1String("global-privacy-control")), QJsonValue(true));
    QCOMPARE(written.value(QLatin1String("webrtc-public-interfaces-only")), QJsonValue(false));

    const GlobalPrivacyControl controlAgain(configRoot);
    const WebRtcPolicy policyAgain(configRoot);
    QVERIFY(controlAgain.enabled());
    QVERIFY(!policyAgain.publicInterfacesOnly());
}

QTEST_GUILESS_MAIN(WebRtcPolicyTest)

#include "tst_webrtcpolicy.moc"
