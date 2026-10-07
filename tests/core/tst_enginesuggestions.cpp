#include "EngineSuggestions.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using omaweb::EngineSuggestions;

class EngineSuggestionsTest final : public QObject {
    Q_OBJECT

private slots:
    void isOffWithoutBeingSetUp();
    void keepsTheReadersChoiceAcrossARestart();
    void readsOpenSearchSuggestions();
    void readsNoOtherShape_data();
    void readsNoOtherShape();
};

// Nothing typed leaves the machine until the reader asks for it to.
void EngineSuggestionsTest::isOffWithoutBeingSetUp()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const EngineSuggestions suggestions(root.filePath(QStringLiteral("config")));
    QVERIFY(!suggestions.enabled());
    QVERIFY(!QFile::exists(root.filePath(QStringLiteral("config/settings.json"))));
}

void EngineSuggestionsTest::keepsTheReadersChoiceAcrossARestart()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const auto configRoot = root.filePath(QStringLiteral("config"));
    {
        EngineSuggestions suggestions(configRoot);
        QSignalSpy changed(&suggestions, &EngineSuggestions::enabledChanged);
        suggestions.setEnabled(true);
        QVERIFY(suggestions.enabled());
        QCOMPARE(changed.count(), 1);
        suggestions.setEnabled(true);
        QCOMPARE(changed.count(), 1);
    }
    {
        EngineSuggestions restarted(configRoot);
        QVERIFY(restarted.enabled());
        restarted.setEnabled(false);
    }
    const EngineSuggestions again(configRoot);
    QVERIFY(!again.enabled());
}

// The typed text, then the engine's proposals. Google adds more after them,
// which is still the shape.
void EngineSuggestionsTest::readsOpenSearchSuggestions()
{
    QCOMPARE(EngineSuggestions::parse(R"(["weath", ["weather", "weather radar"]])"),
        (QStringList {QStringLiteral("weather"), QStringLiteral("weather radar")}));
    QCOMPARE(
        EngineSuggestions::parse(
            R"(["münch", ["münchen", "münchen airport"], [], {"google:suggestsubtypes": []}])"),
        (QStringList {QStringLiteral("münchen"), QStringLiteral("münchen airport")}));
    QCOMPARE(EngineSuggestions::parse(R"(["weath", []])"), QStringList {});
}

void EngineSuggestionsTest::readsNoOtherShape_data()
{
    QTest::addColumn<QByteArray>("body");
    QTest::newRow("empty") << QByteArray();
    QTest::newRow("not json") << QByteArray("weather\nweather radar");
    QTest::newRow("an object") << QByteArray(R"({"suggestions": ["weather"]})");
    QTest::newRow("no list") << QByteArray(R"(["weath"])");
    QTest::newRow("list first") << QByteArray(R"([["weather"], "weath"])");
    QTest::newRow("objects in the list")
        << QByteArray(R"(["weath", [{"phrase": "weather"}, {"phrase": "weather radar"}]])");
    QTest::newRow("a number in the list") << QByteArray(R"(["weath", ["weather", 7]])");
}

// A rich answer is not a list of terms to search for, so it lists nothing
// rather than whatever strings could be picked out of it.
void EngineSuggestionsTest::readsNoOtherShape()
{
    QFETCH(QByteArray, body);
    QCOMPARE(EngineSuggestions::parse(body), QStringList {});
}

QTEST_GUILESS_MAIN(EngineSuggestionsTest)
#include "tst_enginesuggestions.moc"
