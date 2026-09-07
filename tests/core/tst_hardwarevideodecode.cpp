#include "DevelopmentLaunch.h"
#include "HardwareVideoDecode.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using omaweb::hardwareVideoDecodeFlags;
using omaweb::readDevelopmentLaunch;

namespace {

const auto decodeFeature = QStringLiteral("--enable-features=VaapiVideoDecodeLinuxGL");

} // namespace

class HardwareVideoDecodeTest final : public QObject {
    Q_OBJECT

private slots:
    void asksTheEngineForVaApiDecodeOnAnOrdinaryLaunch();
    void carriesTheReadersOwnFeaturesForward();
    void addsNothingWhereThereIsNoGpuProcessToDecodeIn();
    void standsAsideWhereTheReaderTurnedTheFeatureOff();
    void addsNothingTheLaunchAuditRefuses();
    void namesTheFeatureTheApprovedEngineReads();
};

void HardwareVideoDecodeTest::asksTheEngineForVaApiDecodeOnAnOrdinaryLaunch()
{
    QCOMPARE(hardwareVideoDecodeFlags({}), QStringList {decodeFeature});

    // Flags that say nothing about the GPU or about features leave the ask
    // exactly as it is.
    QCOMPARE(hardwareVideoDecodeFlags({QStringLiteral("--use-fake-device-for-media-stream")}),
        QStringList {decodeFeature});
}

// Chromium reads one `--enable-features` list, and the last one on the command
// line is the one it reads. Omaweb's goes last, so it has to carry the reader's
// own features with it: a host that had to name a companion feature to get its
// driver working keeps hardware decode rather than losing it to the naming.
void HardwareVideoDecodeTest::carriesTheReadersOwnFeaturesForward()
{
    QCOMPARE(
        hardwareVideoDecodeFlags({QStringLiteral("--enable-features=VaapiIgnoreDriverChecks")}),
        QStringList {
            QStringLiteral("--enable-features=VaapiIgnoreDriverChecks,VaapiVideoDecodeLinuxGL")});

    // The list Chromium would have read is the one carried forward.
    QCOMPARE(hardwareVideoDecodeFlags({QStringLiteral("--enable-features=First"),
                 QStringLiteral("--enable-features=Second")}),
        QStringList {QStringLiteral("--enable-features=Second,VaapiVideoDecodeLinuxGL")});

    // A reader who already asked for it is asked nothing of.
    QVERIFY(hardwareVideoDecodeFlags({decodeFeature}).isEmpty());
    QVERIFY(hardwareVideoDecodeFlags(
        {QStringLiteral("--enable-features=VaapiVideoDecodeLinuxGL,VaapiIgnoreDriverChecks")})
            .isEmpty());
}

// VA-API decoding happens in the GPU process. A host that has turned that
// process off, which is what a virtual machine with virtualized graphics needs,
// has nowhere to decode, and an ask it cannot serve would only sit unread in
// the command line.
void HardwareVideoDecodeTest::addsNothingWhereThereIsNoGpuProcessToDecodeIn()
{
    for (const auto *flag : {"--disable-gpu", "--disable-accelerated-video-decode",
             "--disable-accelerated-video-decode=1"}) {
        QVERIFY2(hardwareVideoDecodeFlags({QString::fromLatin1(flag)}).isEmpty(), flag);
    }
}

// Turning the feature off names it, which is the one way to say no to hardware
// decode without turning the whole GPU process off.
void HardwareVideoDecodeTest::standsAsideWhereTheReaderTurnedTheFeatureOff()
{
    QVERIFY(hardwareVideoDecodeFlags({QStringLiteral("--disable-features=VaapiVideoDecodeLinuxGL")})
            .isEmpty());
    QVERIFY(hardwareVideoDecodeFlags(
        {QStringLiteral("--disable-features=Something,VaapiVideoDecodeLinuxGL")})
            .isEmpty());

    // Turning off an unrelated feature says nothing about decoding.
    QCOMPARE(hardwareVideoDecodeFlags({QStringLiteral("--disable-features=Something")}),
        QStringList {decodeFeature});
}

// Omaweb's own flags reach the engine through the audit the environment's flags
// reach it through, so what a launch may carry has one statement rather than
// one per route in. Nothing added here may be a thing that audit refuses.
void HardwareVideoDecodeTest::addsNothingTheLaunchAuditRefuses()
{
    const auto added = hardwareVideoDecodeFlags({});
    QVERIFY(!added.isEmpty());
    const auto launch = readDevelopmentLaunch({QStringLiteral("omaweb")}, added);
    QVERIFY2(launch.refusal.isEmpty(), qPrintable(launch.refusal));
    QVERIFY(launch.privateWindowsAvailable);
    QVERIFY(!launch.remoteDebugging);
}

// The feature's name is Chromium's and has changed between versions, so it is
// a claim about one engine rather than a constant. This fails when the approved
// engine baseline moves, which is when the name has to be looked at again.
void HardwareVideoDecodeTest::namesTheFeatureTheApprovedEngineReads()
{
    QFile baseline(QStringLiteral(OMAWEB_SECURITY_BASELINE));
    QVERIFY2(baseline.open(QIODevice::ReadOnly), qPrintable(baseline.fileName()));
    const auto document = QJsonDocument::fromJson(baseline.readAll());
    const auto chromium = document.object().value(QStringLiteral("chromium")).toString();
    QVERIFY2(!chromium.isEmpty(), qPrintable(document.toJson()));
    QCOMPARE(chromium.section(u'.', 0, 0).toInt(), omaweb::checkedChromiumMajorVersion);
}

QTEST_GUILESS_MAIN(HardwareVideoDecodeTest)

#include "tst_hardwarevideodecode.moc"
