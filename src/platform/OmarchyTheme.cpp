#include "OmarchyTheme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

namespace omaweb {

namespace {

QString directoryFromEnvironment(const char *variable, const QString &fallback)
{
    const auto configured = qEnvironmentVariable(variable);
    return configured.isEmpty() ? QDir::home().filePath(fallback) : configured;
}

QByteArray contentsOf(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

// The template Omaweb ships can come out of the binary's own read-only
// resources, so it is written rather than copied: a copy would carry the
// resource's permissions onto disk and hand the reader a file they cannot edit.
bool write(const QString &path, const QByteArray &contents)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (file.write(contents) != contents.size() || !file.commit()) {
        return false;
    }
    // A saved file keeps the private permissions of the temporary it was
    // written through, and this one is the reader's to edit.
    return QFile::setPermissions(path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup
            | QFileDevice::ReadOther);
}

} // namespace

OmarchyThemePaths OmarchyThemePaths::fromEnvironment()
{
    OmarchyThemePaths paths;
    paths.configuration = QDir(directoryFromEnvironment("XDG_CONFIG_HOME",
        QStringLiteral(".config"))).filePath(QStringLiteral("omarchy"));
    paths.state = QDir(directoryFromEnvironment("XDG_STATE_HOME",
        QStringLiteral(".local/state"))).filePath(QStringLiteral("omarchy"));
    return paths;
}

QString OmarchyThemePaths::userTemplate() const
{
    return QDir(configuration).filePath(QStringLiteral("themed/omaweb.json.tpl"));
}

QString OmarchyThemePaths::renderedTheme() const
{
    return QDir(state).filePath(QStringLiteral("current/theme/omaweb.json"));
}

OmarchyTemplateOutcome followOmarchyTheme(
    const OmarchyThemePaths &paths, const QString &shippedTemplate)
{
    if (qEnvironmentVariableIsSet("OMAWEB_NO_OMARCHY_TEMPLATE")) {
        return OmarchyTemplateOutcome::Declined;
    }
    // The state directory is Omarchy's own output, so its absence is the one
    // reliable answer to "is this an Omarchy desktop". Everything below writes
    // into the reader's configuration, and none of it happens without that.
    if (!QFileInfo::exists(paths.state)) {
        return OmarchyTemplateOutcome::Absent;
    }

    const auto shipped = contentsOf(shippedTemplate);
    if (shipped.isEmpty()) {
        qWarning("Omaweb could not read its own Omarchy theme template at %s.",
            qPrintable(shippedTemplate));
        return OmarchyTemplateOutcome::Failed;
    }

    auto outcome = OmarchyTemplateOutcome::Kept;
    const auto installed = paths.userTemplate();
    if (!QFileInfo::exists(installed)) {
        if (!write(installed, shipped)) {
            qWarning("Omaweb could not install its Omarchy theme template at %s. "
                     "Following the desktop's theme needs it, or set "
                     "OMAWEB_NO_OMARCHY_TEMPLATE to stop asking.",
                qPrintable(installed));
            return OmarchyTemplateOutcome::Failed;
        }
        qInfo("Omaweb installed its Omarchy theme template at %s, so the browser follows the "
              "desktop's theme. Set OMAWEB_NO_OMARCHY_TEMPLATE to manage that directory "
              "yourself.",
            qPrintable(installed));
        outcome = OmarchyTemplateOutcome::Installed;
    } else if (contentsOf(installed) != shipped) {
        // Someone's customisation, or a theme's. It is the palette Omaweb is
        // asked to draw in, and an upgrade does not overrule it — but a reader
        // chasing a colour the new template names deserves to know why it is
        // not there.
        qInfo("Omaweb kept the Omarchy theme template already at %s, which differs from the "
              "one this version ships. Delete it to take the shipped template.",
            qPrintable(installed));
    }

    // Omarchy renders templates when a theme is set, and the reader has
    // already set theirs. Asking for the active theme again is what turns the
    // template into a palette. A template just installed always wants that,
    // even where an older one left a palette behind, because the palette on
    // disk was rendered from a template that is no longer there.
    const auto wantsRender = outcome == OmarchyTemplateOutcome::Installed
        || !QFileInfo::exists(paths.renderedTheme());
    const auto omarchy = QStandardPaths::findExecutable(QStringLiteral("omarchy"));
    if (wantsRender && !omarchy.isEmpty()) {
        // Detached, because Omarchy re-renders every template a desktop has
        // and Omaweb is not waiting for a window to appear. The palette is
        // watched for, so it is picked up whenever it lands. A shell because
        // the theme to set is the one Omarchy reports as current, and `$1` is
        // the executable already found rather than a second path lookup.
        QProcess::startDetached(QStringLiteral("/bin/sh"),
            {QStringLiteral("-c"), QStringLiteral(R"SH("$1" theme set "$("$1" theme current)")SH"),
                QStringLiteral("sh"), omarchy});
    }
    return outcome;
}

} // namespace omaweb
