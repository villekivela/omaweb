#include "DiagnosticOverrides.h"

#include <QRegularExpression>

namespace omaweb {
namespace {

    constexpr auto option = QLatin1StringView("--diagnose");

    // Chrome's own reduced form: the major version, the rest zeros.
    QString chromeVersion(int major) { return QStringLiteral("%1.0.0.0").arg(major); }

    QString refusalFor(const QString &problem)
    {
        return QStringLiteral("%1. The Diagnostic overrides are %2.")
            .arg(problem, diagnosticOverrideForms().join(QStringLiteral(", ")));
    }

    // Reads one `<name>=<value>` into `overrides`, or answers why it cannot.
    QString readOne(const QString &given, DiagnosticOverrides &overrides)
    {
        const auto equals = given.indexOf(u'=');
        const auto name = given.left(equals);
        const auto value = equals < 0 ? QString() : given.mid(equals + 1);
        if (name.isEmpty()) {
            return refusalFor(QStringLiteral("--diagnose names no override"));
        }
        const auto malformed = [&given](const QString &why) {
            return refusalFor(QStringLiteral("`%1` is malformed: %2").arg(given, why));
        };
        const auto twice
            = [&name] { return refusalFor(QStringLiteral("`%1` is given twice").arg(name)); };
        if (name == u"report-chrome") {
            static const QRegularExpression major(QStringLiteral("^[1-9][0-9]{0,3}$"));
            if (!major.match(value).hasMatch()) {
                return malformed(QStringLiteral("it takes a Chrome major version, such as 152"));
            }
            if (overrides.reportedChrome) {
                return twice();
            }
            overrides.reportedChrome = value.toInt();
            overrides.named.append(name + u'=' + value);
            return {};
        }
        if (name == u"brands") {
            QStringList brands;
            for (const auto &part : value.split(u',')) {
                const auto brand = part.trimmed();
                // Sent quoted in `Sec-CH-UA`, so a quote would end it early.
                if (brand.isEmpty() || brand.contains(u'"') || brand.contains(u'\\')
                    || brands.contains(brand)) {
                    return malformed(QStringLiteral(
                        "it takes brand names separated by commas, each once and without quotes"));
                }
                brands.append(brand);
            }
            if (overrides.brands) {
                return twice();
            }
            overrides.brands = brands;
            overrides.named.append(name + u'=' + brands.join(u','));
            return {};
        }
        if (name == u"language") {
            static const QRegularExpression tag(
                QStringLiteral("^[A-Za-z]{2,3}(-[A-Za-z0-9]{1,8})*$"));
            if (!tag.match(value).hasMatch()) {
                return malformed(QStringLiteral("it takes one language tag, such as fi-FI"));
            }
            if (overrides.language) {
                return twice();
            }
            overrides.language = value;
            overrides.named.append(name + u'=' + value);
            return {};
        }
        return refusalFor(QStringLiteral("`%1` is no Diagnostic override").arg(name));
    }

} // namespace

DiagnosticOverrides readDiagnosticOverrides(const QStringList &arguments)
{
    DiagnosticOverrides overrides;
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const auto &argument = arguments.at(index);
        QString given;
        if (argument == option) {
            if (index + 1 == arguments.size()) {
                overrides.refusal = refusalFor(QStringLiteral("--diagnose names no override"));
                return overrides;
            }
            given = arguments.at(++index);
        } else if (argument.startsWith(option + u'=')) {
            given = argument.mid(option.size() + 1);
        } else {
            continue;
        }
        overrides.refusal = readOne(given, overrides);
        if (!overrides.refusal.isEmpty()) {
            return overrides;
        }
    }
    return overrides;
}

const QStringList &diagnosticOverrideForms()
{
    static const QStringList forms {
        QStringLiteral("report-chrome=<major>"),
        QStringLiteral("brands=<list>"),
        QStringLiteral("language=<tag>"),
    };
    return forms;
}

QString overriddenUserAgent(const QString &engine, const DiagnosticOverrides &overrides)
{
    if (!overrides.reportedChrome) {
        return engine;
    }
    static const QRegularExpression version(QStringLiteral("(?<= Chrome/)[0-9.]+"));
    auto agent = engine;
    return agent.replace(version, chromeVersion(*overrides.reportedChrome));
}

QVariantMap overriddenFullVersionList(
    const QVariantMap &engine, const DiagnosticOverrides &overrides)
{
    const auto chromium = engine.value(QStringLiteral("Chromium")).toString();
    const auto major = chromium.section(u'.', 0, 0);
    QVariantMap versions = engine;
    if (overrides.reportedChrome) {
        for (auto brand = versions.begin(); brand != versions.end(); ++brand) {
            if (brand.value().toString().section(u'.', 0, 0) == major) {
                brand.value() = chromeVersion(*overrides.reportedChrome);
            }
        }
    }
    if (!overrides.brands) {
        return versions;
    }
    const auto reported
        = overrides.reportedChrome ? chromeVersion(*overrides.reportedChrome) : chromium;
    QVariantMap named;
    for (const auto &brand : *overrides.brands) {
        named.insert(brand, versions.value(brand, reported));
    }
    return named;
}

QString overriddenFullVersion(const QString &engine, const DiagnosticOverrides &overrides)
{
    return overrides.reportedChrome ? chromeVersion(*overrides.reportedChrome) : engine;
}

QString overriddenAcceptLanguage(const QString &engine, const DiagnosticOverrides &overrides)
{
    return overrides.language.value_or(engine);
}

} // namespace omaweb
