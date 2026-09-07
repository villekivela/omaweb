#include "InputMethod.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QQmlEngine>

namespace omaweb {
namespace {

    // Qt matches the environment's name against a plugin's keys without
    // regard to case, so the comparison here is the one Qt itself makes.
    bool names(const QStringList &modules, const QString &module)
    {
        return std::any_of(modules.cbegin(), modules.cend(), [&module](const QString &candidate) {
            return candidate.compare(module, Qt::CaseInsensitive) == 0;
        });
    }

    QStringList requestedFromEnvironment()
    {
        // The plural names a list to try in order and the singular one module,
        // and the plural outranks the singular, which is the order Qt reads
        // them in. A report that read only one would answer for a desktop that
        // had configured the other.
        auto named = qEnvironmentVariable("QT_IM_MODULES").trimmed();
        if (named.isEmpty()) {
            named = qEnvironmentVariable("QT_IM_MODULE").trimmed();
        }
        QStringList modules;
        for (const auto &part : named.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const auto module = part.trimmed();
            // `none` is a reader turning input methods off rather than a
            // plugin they expect to be there.
            if (!module.isEmpty()
                && module.compare(QLatin1String("none"), Qt::CaseInsensitive) != 0) {
                modules.append(module);
            }
        }
        return modules;
    }

    QStringList installedPlugins()
    {
        const QDir directory(QLibraryInfo::path(QLibraryInfo::PluginsPath)
            + QStringLiteral("/platforminputcontexts"));
        QStringList keys;
        const auto files = directory.entryInfoList(QDir::Files);
        for (const auto &file : files) {
            // The keys are the plugin's own declaration of what it answers to,
            // read without loading it: a plugin that has to be loaded to be
            // asked about is a plugin the browser has run.
            const QPluginLoader loader(file.absoluteFilePath());
            const auto declared = loader.metaData()
                                      .value(QStringLiteral("MetaData"))
                                      .toObject()
                                      .value(QStringLiteral("Keys"))
                                      .toArray();
            for (const auto &key : declared) {
                const auto name = key.toString().trimmed();
                if (!name.isEmpty()) {
                    keys.append(name);
                }
            }
        }
        return keys;
    }

} // namespace

InputMethodHost InputMethodHost::fromEnvironment()
{
    return InputMethodHost {requestedFromEnvironment(), installedPlugins()};
}

bool inputMethodAvailable(const InputMethodHost &host)
{
    if (host.requestedModules.isEmpty()) {
        return true;
    }
    return std::any_of(host.requestedModules.cbegin(), host.requestedModules.cend(),
        [&host](const QString &module) { return names(host.installedModules, module); });
}

QString inputMethodDiagnostic(const InputMethodHost &host)
{
    if (host.requestedModules.isEmpty()) {
        return {};
    }
    const auto asked = host.requestedModules.join(QStringLiteral(", "));
    for (const auto &module : host.requestedModules) {
        if (names(host.installedModules, module)) {
            return QStringLiteral("This desktop asks Qt applications for the %1 input method, and "
                                  "the plugin that answers to that name is installed.")
                .arg(module);
        }
    }
    return QStringLiteral("This desktop asks Qt applications for the %1 input method and installs "
                          "no plugin by that name. Qt loads no input context, Omaweb binds no "
                          "text-input protocol, and composing does nothing. Every Qt application "
                          "here is affected; installing the plugin package for %1 answers it.")
        .arg(asked);
}

InputMethodReport::InputMethodReport(InputMethodHost host, QObject *parent)
    : QObject(parent)
    , m_available(inputMethodAvailable(host))
    , m_diagnostic(inputMethodDiagnostic(host))
{
}

bool InputMethodReport::available() const { return m_available; }

QString InputMethodReport::diagnostic() const { return m_diagnostic; }

void registerInputMethodReport(InputMethodReport *report)
{
    qmlRegisterSingletonInstance("Omaweb", 1, 0, "InputMethodReport", report);
}

} // namespace omaweb
