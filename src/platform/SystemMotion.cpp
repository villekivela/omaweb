#include "SystemMotion.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlEngine>

#include <algorithm>

namespace omaweb {

bool SystemMotion::reduced() const { return std::ranges::any_of(m_reducedBy, std::identity {}); }

void SystemMotion::setReducedBy(Source source, bool reduces)
{
    const auto wasReduced = reduced();
    m_reducedBy.at(static_cast<std::size_t>(source)) = reduces;
    if (reduced() != wasReduced) {
        emit reducedChanged();
    }
}

bool portalAsksToReduceMotion(const QVariant &reducedMotion)
{
    // The key is a `u`; a string that happens to read as one is not it.
    const auto type = reducedMotion.typeId();
    if (type != QMetaType::UInt && type != QMetaType::Int) {
        return false;
    }
    return reducedMotion.toUInt() == 1;
}

bool hyprlandAsksToReduceMotion(QByteArrayView getoptionJson)
{
    const auto answer = QJsonDocument::fromJson(getoptionJson.toByteArray()).object();
    // Hyprland 0.56 answers a boolean option as `bool`, and an older release
    // as `int`.
    const auto enabled = answer.value(QStringLiteral("bool"));
    if (enabled.isBool()) {
        return !enabled.toBool();
    }
    const auto value = answer.value(QStringLiteral("int"));
    return value.isDouble() && value.toInt() == 0;
}

bool hyprlandConfigurationReloaded(QByteArrayView events)
{
    constexpr QByteArrayView reload = "configreloaded>>";
    for (const auto &line : events.toByteArray().split('\n')) {
        if (line.startsWith(reload)) {
            return true;
        }
    }
    return false;
}

QString hyprlandSocketPath(const QProcessEnvironment &environment, HyprlandSocket socket)
{
    const auto runtime = environment.value(QStringLiteral("XDG_RUNTIME_DIR"));
    const auto signature = environment.value(QStringLiteral("HYPRLAND_INSTANCE_SIGNATURE"));
    // The signature names one directory and nothing else.
    if (runtime.isEmpty() || signature.isEmpty() || signature.contains(QLatin1Char('/'))
        || signature == QStringLiteral(".") || signature == QStringLiteral("..")) {
        return {};
    }
    return QDir(runtime).filePath(QStringLiteral("hypr/%1/%2")
            .arg(signature,
                socket == HyprlandSocket::Requests ? QStringLiteral(".socket.sock")
                                                   : QStringLiteral(".socket2.sock")));
}

bool gnomeAsksToReduceMotion(const QVariant &enableAnimations)
{
    return enableAnimations.typeId() == QMetaType::Bool && !enableAnimations.toBool();
}

void registerSystemMotion()
{
    qmlRegisterSingletonType<SystemMotion>("Omaweb", 1, 0, "SystemMotion",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new SystemMotion; });
}

} // namespace omaweb
