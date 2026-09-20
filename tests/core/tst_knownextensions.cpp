#include "KnownExtensions.h"

#include <QTest>

using namespace omaweb;

// What Omaweb names, it has to be able to describe. A Known extension without a
// publisher or a licence is one a reader cannot judge before enabling it, and
// one without a store id cannot keep the identity its publisher's desktop
// application allows.
class KnownExtensionsTest : public QObject {
    Q_OBJECT

private slots:
    void theListNamesBitwardenAndOnePassword()
    {
        QStringList keys;
        for (const KnownExtension &extension : knownExtensions()) {
            keys.append(extension.key);
        }
        QCOMPARE(keys, QStringList({QStringLiteral("bitwarden"), QStringLiteral("1password")}));
    }

    void everyEntryCanBeJudgedBeforeItIsEnabled()
    {
        for (const KnownExtension &extension : knownExtensions()) {
            QVERIFY2(!extension.name.isEmpty(), qPrintable(extension.key));
            QVERIFY2(!extension.publisher.isEmpty(), qPrintable(extension.key));
            QVERIFY2(!extension.licence.isEmpty(), qPrintable(extension.key));
            QVERIFY2(!extension.homepage.isEmpty(), qPrintable(extension.key));
            QVERIFY2(!extension.summary.isEmpty(), qPrintable(extension.key));
        }
    }

    void everyEntryKeepsTheIdentityItsPublisherAllows()
    {
        for (const KnownExtension &extension : knownExtensions()) {
            // A Chromium extension id is sixteen bytes of a hash, written in
            // the first sixteen letters of the alphabet.
            QCOMPARE(extension.storeId.size(), 32);
            for (const QChar letter : extension.storeId) {
                QVERIFY2(letter >= u'a' && letter <= u'p', qPrintable(extension.storeId));
            }
        }
    }

    void anExtensionOmawebDoesNotNameIsNotAnError()
    {
        QVERIFY(knownExtension(QStringLiteral("something-else")).key.isEmpty());
        QCOMPARE(knownExtension(QStringLiteral("bitwarden")).publisher,
            QStringLiteral("Bitwarden, Inc."));
    }
};

QTEST_MAIN(KnownExtensionsTest)
#include "tst_knownextensions.moc"
