#include "AppIcon.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <utility>

namespace omaweb {

namespace {

    QByteArray contentsOf(const QString &path)
    {
        if (path.isEmpty()) {
            return {};
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    // The installed entry with `Icon=` naming `icon` and `TryExec=` naming the
    // browser, in the application's own group alone: an action's group keeps
    // the icon it names.
    QByteArray launcherEntry(const QByteArray &installed, const QString &icon)
    {
        QByteArrayList lines;
        bool application = false;
        for (const auto &line : installed.split('\n')) {
            if (line.startsWith('[')) {
                application = line.trimmed() == "[Desktop Entry]";
                lines.append(line);
                if (application) {
                    lines.append("TryExec=omaweb");
                }
            } else if (application && line.startsWith("Icon=")) {
                lines.append("Icon=" + icon.toUtf8());
            } else if (!application || !line.startsWith("TryExec=")) {
                lines.append(line);
            }
        }
        return lines.join('\n');
    }

} // namespace

AppIconPaths AppIconPaths::fromEnvironment(const QString &dataRoot)
{
    AppIconPaths paths;
    paths.omarchy = OmarchyThemePaths::fromEnvironment();
    const auto dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    paths.userEntry = QDir(
        dataHome.isEmpty() ? QDir::home().filePath(QStringLiteral(".local/share")) : dataHome)
                          .filePath(QStringLiteral("applications/omaweb.desktop"));
    auto dataDirs = qEnvironmentVariable("XDG_DATA_DIRS");
    if (dataDirs.isEmpty()) {
        dataDirs = QStringLiteral("/usr/local/share:/usr/share");
    }
    for (const auto &directory : dataDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        const auto entry = QDir(directory).filePath(QStringLiteral("applications/omaweb.desktop"));
        // A desktop that lists the reader's own data directory here as well
        // would otherwise have the user entry copied from itself.
        if (entry != paths.userEntry && QFileInfo::exists(entry)) {
            paths.installedEntry = entry;
            break;
        }
    }
    paths.iconDirectory = QDir(dataRoot).filePath(QStringLiteral("app-icon"));
    return paths;
}

QString AppIconPaths::iconTemplate() const
{
    return QDir(omarchy.configuration).filePath(QStringLiteral("themed/omaweb-icon.svg.tpl"));
}

QString AppIconPaths::renderedIcon() const
{
    return QDir(omarchy.state).filePath(QStringLiteral("current/theme/omaweb-icon.svg"));
}

AppIconLauncher::AppIconLauncher(AppIconPaths paths, QString shippedTemplate, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_shippedTemplate(std::move(shippedTemplate))
{
    // Omarchy builds the next theme beside the current one, removes the
    // current one and renames the next into its place, so the theme's own
    // directory is a new one after every switch. Its parent stays.
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        if (m_choice == AppIcon::Theme) {
            apply();
        }
    });
}

void AppIconLauncher::setChoice(AppIcon choice)
{
    m_choice = choice;
    // As with the palette's template: the reader who manages
    // `~/.config/omarchy` themselves said so, and a desktop without Omarchy
    // has no theme to render.
    const auto mayWriteTemplate = !qEnvironmentVariableIsSet("OMAWEB_NO_OMARCHY_TEMPLATE")
        && QFileInfo::exists(m_paths.omarchy.state);
    if (mayWriteTemplate && m_choice == AppIcon::Theme) {
        installTemplate();
    } else if (mayWriteTemplate) {
        removeTemplate();
    }
    apply();
}

// Under the rules `followOmarchyTheme` installs the palette's template by
// (ADR 0002): a template already there stands.
void AppIconLauncher::installTemplate()
{
    const auto shipped = contentsOf(m_shippedTemplate);
    if (shipped.isEmpty()) {
        qWarning("Omaweb could not read its own Omarchy icon template at %s.",
            qPrintable(m_shippedTemplate));
        return;
    }
    const auto installed = m_paths.iconTemplate();
    if (!QFileInfo::exists(installed)) {
        if (!writeReaderFile(installed, shipped)) {
            qWarning("Omaweb could not install its Omarchy icon template at %s, so the "
                     "launcher keeps the black and white icon.",
                qPrintable(installed));
            return;
        }
        qInfo("Omaweb installed its Omarchy icon template at %s, so the launcher shows the "
              "theme's icon.",
            qPrintable(installed));
        renderActiveOmarchyTheme();
    } else if (contentsOf(installed) != shipped) {
        qInfo("Omaweb kept the Omarchy icon template already at %s, which differs from the one "
              "this version ships. Delete it to take the shipped template.",
            qPrintable(installed));
    }
}

// Only the template Omaweb shipped is Omaweb's to remove. One the reader
// changed is a customisation they may want back with Theme.
void AppIconLauncher::removeTemplate()
{
    const auto installed = m_paths.iconTemplate();
    if (!QFileInfo::exists(installed)) {
        return;
    }
    if (contentsOf(installed) != contentsOf(m_shippedTemplate)) {
        qInfo("Omaweb left the Omarchy icon template at %s, which differs from the one this "
              "version ships. Delete it if nothing else uses it.",
            qPrintable(installed));
        return;
    }
    QFile::remove(installed);
}

// The entry Omaweb writes is the one whose icon is a copy of its own. Any
// other at that path is the reader's, perhaps with flags of theirs on `Exec=`.
bool AppIconLauncher::ownsUserEntry() const
{
    if (!QFileInfo::exists(m_paths.userEntry)) {
        return true;
    }
    const auto prefix = "Icon=" + QDir(m_paths.iconDirectory).absolutePath().toUtf8() + '/';
    for (const auto &line : contentsOf(m_paths.userEntry).split('\n')) {
        if (line.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

void AppIconLauncher::apply()
{
    watch();
    if (!ownsUserEntry()) {
        // Under Black and white the reader's entry is what they asked for, so
        // only Theme has something to say about it.
        if (m_choice == AppIcon::Theme) {
            qInfo("Omaweb left the desktop entry at %s, which it did not write. Delete it for "
                  "the launcher to show the Theme app icon.",
                qPrintable(m_paths.userEntry));
        }
        removeCopiesBut({});
        return;
    }
    const auto rendered = contentsOf(m_paths.renderedIcon());
    const auto installed = contentsOf(m_paths.installedEntry);
    if (m_choice != AppIcon::Theme || rendered.isEmpty() || installed.isEmpty()) {
        QFile::remove(m_paths.userEntry);
        removeCopiesBut({});
        return;
    }
    // Named by its contents, because Omarchy's menu caches an icon by its
    // path: a theme's colours under a path the menu has drawn before would
    // show the colours it drew then.
    const auto copy = QDir(m_paths.iconDirectory)
                          .filePath(QStringLiteral("omaweb-%1.svg")
                                  .arg(QString::fromLatin1(
                                      QCryptographicHash::hash(rendered, QCryptographicHash::Sha256)
                                          .toHex()
                                          .left(16))));
    if (!QFileInfo::exists(copy) && !writeReaderFile(copy, rendered)) {
        qWarning("Omaweb could not copy the theme's app icon to %s.", qPrintable(copy));
        return;
    }
    // Rebuilt from the installed entry every time, not edited in place: this
    // entry outranks the installed one, so a change the package makes to its
    // entry reaches the reader only through here, on the next start.
    const auto entry = launcherEntry(installed, copy);
    if (contentsOf(m_paths.userEntry) != entry) {
        writeReaderFile(m_paths.userEntry, entry);
    }
    removeCopiesBut(copy);
}

void AppIconLauncher::watch()
{
    const auto theme = QFileInfo(m_paths.renderedIcon()).absolutePath();
    for (const auto &directory : {QFileInfo(theme).absolutePath(), theme}) {
        if (QFileInfo::exists(directory) && !m_watcher.directories().contains(directory)) {
            m_watcher.addPath(directory);
        }
    }
}

void AppIconLauncher::removeCopiesBut(const QString &kept)
{
    const QDir copies(m_paths.iconDirectory);
    for (const auto &name : copies.entryList(QDir::Files)) {
        if (copies.filePath(name) != kept) {
            QFile::remove(copies.filePath(name));
        }
    }
}

} // namespace omaweb
