#include "ContentMatcher.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using tanto::ContentMatcher;

class ContentMatcherTest final : public QObject {
    Q_OBJECT

private slots:
    void sharedConformanceFixtures();
    void reportsUnsupportedCategories();
};

void ContentMatcherTest::sharedConformanceFixtures()
{
    QFile fixture(QStringLiteral(TANTO_CONTENT_BLOCKER_FIXTURES));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto document = QJsonDocument::fromJson(fixture.readAll());
    QVERIFY(document.isObject());
    const auto root = document.object();
    QStringList rules;
    for (const auto &rule : root.value(QStringLiteral("rules")).toArray()) {
        rules.append(rule.toString());
    }
    const auto compilation = ContentMatcher::compile(rules.join(QLatin1Char('\n')));
    QVERIFY(compilation.matcher);

    for (const auto &value : root.value(QStringLiteral("network")).toArray()) {
        const auto fixture = value.toObject();
        const auto blocked = compilation.matcher->shouldBlock(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            QUrl(fixture.value(QStringLiteral("source")).toString()),
            fixture.value(QStringLiteral("type")).toString());
        QCOMPARE(blocked, fixture.value(QStringLiteral("blocked")).toBool());
    }
    for (const auto &value : root.value(QStringLiteral("cosmetic")).toArray()) {
        const auto fixture = value.toObject();
        const auto css = compilation.matcher->cosmeticStyleSheet(
            QUrl(fixture.value(QStringLiteral("url")).toString()));
        QCOMPARE(css.contains(fixture.value(QStringLiteral("contains")).toString()),
            fixture.value(QStringLiteral("hidden")).toBool());
    }
}

void ContentMatcherTest::reportsUnsupportedCategories()
{
    const auto compilation = ContentMatcher::compile(QStringLiteral(
        "example.com##+js(abort-on-property-read, ad)\n"
        "example.com#?#div:has(.ad)\n"
        "||example.com^$redirect=noopjs\n"
        "||safe.example^"));
    QVERIFY(compilation.matcher);
    QCOMPARE(compilation.report.value(QStringLiteral("acceptedRuleCount")).toInt(), 1);
    const auto unsupported = compilation.report.value(QStringLiteral("unsupported")).toObject();
    QCOMPARE(unsupported.value(QStringLiteral("scriptlets")).toInt(), 1);
    QCOMPARE(unsupported.value(QStringLiteral("procedural selectors")).toInt(), 1);
    QCOMPARE(unsupported.value(QStringLiteral("redirects or resource replacement")).toInt(), 1);
}

QTEST_APPLESS_MAIN(ContentMatcherTest)

#include "tst_contentmatcher.moc"
