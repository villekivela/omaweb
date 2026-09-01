#include "ContentBlocker.h"
#include "ContentMatcher.h"
#include "QtContentBlocker.h"

#include <QCoreApplication>
#include <QFile>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QWebEngineUrlRequestInfo>

namespace {

QJsonObject runFixtures(const QJsonObject &fixtures,
    const tanto::QtContentBlocker &adapter, const tanto::ContentBlocker &contentBlocker)
{
    const auto resourceType = [](const QString &name) {
        using Info = QWebEngineUrlRequestInfo;
        if (name == QStringLiteral("document")) return Info::ResourceTypeMainFrame;
        if (name == QStringLiteral("subdocument")) return Info::ResourceTypeSubFrame;
        if (name == QStringLiteral("stylesheet")) return Info::ResourceTypeStylesheet;
        if (name == QStringLiteral("script")) return Info::ResourceTypeScript;
        if (name == QStringLiteral("image")) return Info::ResourceTypeImage;
        if (name == QStringLiteral("font")) return Info::ResourceTypeFontResource;
        if (name == QStringLiteral("media")) return Info::ResourceTypeMedia;
        if (name == QStringLiteral("xmlhttprequest")) return Info::ResourceTypeXhr;
        return Info::ResourceTypeUnknown;
    };
    int passed = 0;
    int total = 0;
    for (const auto &value : fixtures.value(QStringLiteral("network")).toArray()) {
        const auto fixture = value.toObject();
        const auto decision = adapter.checkRequest(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            QUrl(fixture.value(QStringLiteral("source")).toString()),
            resourceType(fixture.value(QStringLiteral("type")).toString()));
        passed += decision.blocked == fixture.value(QStringLiteral("blocked")).toBool()
            && decision.substitute == fixture.value(QStringLiteral("substitute")).toString()
            && decision.rewrittenUrl
                == QUrl(fixture.value(QStringLiteral("rewritten")).toString());
        ++total;
    }
    // A refused window never reaches an engine's request interception, so the
    // popup fixtures ask the engine-neutral blocker rather than the adapter.
    for (const auto &value : fixtures.value(QStringLiteral("popup")).toArray()) {
        const auto fixture = value.toObject();
        const auto actual = contentBlocker.shouldBlockPopup(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            QUrl(fixture.value(QStringLiteral("opener")).toString()));
        passed += actual == fixture.value(QStringLiteral("blocked")).toBool();
        ++total;
    }
    // The scriptlet an engine injects is the same code whichever engine
    // injects it, so these fixtures ask the engine-neutral blocker too.
    for (const auto &value : fixtures.value(QStringLiteral("scriptlet")).toArray()) {
        const auto fixture = value.toObject();
        const auto actual = contentBlocker.scriptletSource(
            QUrl(fixture.value(QStringLiteral("url")).toString()))
                                .contains(fixture.value(QStringLiteral("contains")).toString());
        passed += actual == fixture.value(QStringLiteral("injected")).toBool();
        ++total;
    }
    for (const auto &value : fixtures.value(QStringLiteral("cosmetic")).toArray()) {
        const auto fixture = value.toObject();
        const auto actual = adapter.cosmeticStyleSheet(
            QUrl(fixture.value(QStringLiteral("url")).toString()))
                                .contains(fixture.value(QStringLiteral("contains")).toString());
        passed += actual == fixture.value(QStringLiteral("hidden")).toBool();
        ++total;
    }
    const auto stringList = [](const QJsonValue &value) {
        QStringList result;
        for (const auto &entry : value.toArray()) {
            result.append(entry.toString());
        }
        return result;
    };
    for (const auto &value : fixtures.value(QStringLiteral("cosmeticSurveyWanted")).toArray()) {
        const auto fixture = value.toObject();
        const auto actual = adapter.cosmeticSurveyWanted(
            QUrl(fixture.value(QStringLiteral("url")).toString()));
        passed += actual == fixture.value(QStringLiteral("wanted")).toBool();
        ++total;
    }
    for (const auto &value : fixtures.value(QStringLiteral("cosmeticSurvey")).toArray()) {
        const auto fixture = value.toObject();
        const auto actual = adapter.genericCosmeticStyleSheet(
            QUrl(fixture.value(QStringLiteral("url")).toString()),
            stringList(fixture.value(QStringLiteral("classes"))),
            stringList(fixture.value(QStringLiteral("ids"))))
                                .contains(fixture.value(QStringLiteral("contains")).toString());
        passed += actual == fixture.value(QStringLiteral("hidden")).toBool();
        ++total;
    }
    return {
        {QStringLiteral("passed"), passed},
        {QStringLiteral("total"), total},
        {QStringLiteral("status"), passed == total ? QStringLiteral("pass")
                                                   : QStringLiteral("fail")},
    };
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() != 2) {
        return 2;
    }
    QFile fixtureFile(QStringLiteral(TANTO_CONTENT_BLOCKER_FIXTURES));
    if (!fixtureFile.open(QIODevice::ReadOnly)) {
        return 2;
    }
    const auto fixtures = QJsonDocument::fromJson(fixtureFile.readAll()).object();
    QStringList rules;
    for (const auto &rule : fixtures.value(QStringLiteral("rules")).toArray()) {
        rules.append(rule.toString());
    }
    const auto compilation = tanto::ContentMatcher::compile(rules.join(QLatin1Char('\n')));
    if (!compilation.matcher) {
        return 1;
    }
    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        return 2;
    }
    tanto::ContentBlocker contentBlocker(
        dataRoot.path(), tanto::ContentBlocker::DefaultLists::None);
    QEventLoop compiled;
    QObject::connect(&contentBlocker, &tanto::ContentBlocker::rulesChanged,
        &compiled, &QEventLoop::quit);
    contentBlocker.setUserRules(rules.join(QLatin1Char('\n')));
    QTimer::singleShot(5000, &compiled, &QEventLoop::quit);
    compiled.exec();
    if (contentBlocker.compiling()) {
        return 1;
    }
    const tanto::QtContentBlocker qtAdapter(&contentBlocker);
    const auto result = runFixtures(fixtures, qtAdapter, contentBlocker);
    const auto report = QJsonObject{
        {QStringLiteral("contract"), QStringLiteral("Tanto content blocking v1")},
        {QStringLiteral("adblockRustVersion"), QStringLiteral("0.12.5")},
        {QStringLiteral("ladybirdRevision"), QStringLiteral(TANTO_LADYBIRD_REVISION)},
        {QStringLiteral("unsupportedRuleCategories"), QJsonArray{
            QStringLiteral("scriptlets requiring trust"),
            QStringLiteral("scriptlets this build does not carry"),
            QStringLiteral("procedural selectors"),
            QStringLiteral("response rewriting"),
            QStringLiteral("HTML filtering"),
            QStringLiteral("dynamic rules"),
            QStringLiteral("CNAME uncloaking"),
            QStringLiteral("content security policies"),
            QStringLiteral("substitutes this build does not carry"),
        }},
        {QStringLiteral("sharedPinnedParser"), QJsonObject{
            {QStringLiteral("status"), QStringLiteral("pass")},
            {QStringLiteral("acceptedRules"), compilation.report.value(
                QStringLiteral("acceptedRuleCount"))},
        }},
        {QStringLiteral("qtAdapter"), result},
        {QStringLiteral("ladybirdAdapter"), QJsonObject{
            {QStringLiteral("status"), QStringLiteral("not-run")},
            {QStringLiteral("reason"), QStringLiteral(
                "The Ladybird adapter is not present until issue #7")},
        }},
    };
    QSaveFile output(application.arguments().at(1));
    if (!output.open(QIODevice::WriteOnly)) {
        return 2;
    }
    output.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    return output.commit() && result.value(QStringLiteral("status")) == QStringLiteral("pass")
        ? 0 : 1;
}
