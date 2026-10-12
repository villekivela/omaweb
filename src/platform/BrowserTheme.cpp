#include "BrowserTheme.h"

#include "OmarchyTheme.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>

namespace omaweb {

namespace {

    const QRegularExpression &hexColour()
    {
        static const QRegularExpression pattern(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
        return pattern;
    }

    QString rgbOf(const QString &hex)
    {
        const QColor colour(hex);
        return QStringLiteral("%1,%2,%3").arg(colour.red()).arg(colour.green()).arg(colour.blue());
    }

    // `mix_color` in Omarchy: an amount is a fraction, a percentage, or a
    // whole number read as one, and each channel rounds half up.
    QString mixed(const QString &start, const QString &end, QString amountText)
    {
        double amount = 0;
        if (amountText.endsWith(QLatin1Char('%'))) {
            amountText.chop(1);
            amount = amountText.toDouble() / 100;
        } else {
            amount = amountText.toDouble();
            if (amount > 1) {
                amount /= 100;
            }
        }
        amount = std::clamp(amount, 0.0, 1.0);
        const QColor from(start);
        const QColor to(end);
        const auto channel = [amount](int a, int b) {
            return static_cast<int>(std::floor(a * (1 - amount) + b * amount + 0.5));
        };
        return QColor(channel(from.red(), to.red()), channel(from.green(), to.green()),
            channel(from.blue(), to.blue()))
            .name();
    }

} // namespace

QByteArray renderOmarchyTemplate(const QByteArray &templateText, const OmarchyColours &colours)
{
    auto text = QString::fromUtf8(templateText);

    static const QRegularExpression mixToken(
        QStringLiteral(R"(\{\{\s*(mix|mix_strip|mix_rgb)\s+([A-Za-z0-9_]+)\s+([A-Za-z0-9_]+)\s+)"
                       R"(([0-9]+(?:\.[0-9]+)?%?)\s*\}\})"));
    // Omarchy substitutes `{{ key }}` with one space inside each brace, as sed
    // matches it literally, so that is the only spelling filled here too.
    static const QRegularExpression valueToken(QStringLiteral(R"(\{\{ ([A-Za-z0-9_]+) \}\})"));

    const auto fill =
        [&text](const QRegularExpression &pattern,
            const std::function<std::optional<QString>(const QRegularExpressionMatch &)> &valueOf) {
            QString out;
            qsizetype from = 0;
            auto matches = pattern.globalMatch(text);
            while (matches.hasNext()) {
                const auto match = matches.next();
                out.append(QStringView(text).mid(from, match.capturedStart() - from));
                const auto value = valueOf(match);
                out.append(value ? *value : match.captured());
                from = match.capturedEnd();
            }
            out.append(QStringView(text).mid(from));
            text = out;
        };

    fill(mixToken, [&colours](const QRegularExpressionMatch &match) -> std::optional<QString> {
        const auto start = colours.value(match.captured(2));
        const auto end = colours.value(match.captured(3));
        if (!hexColour().match(start).hasMatch() || !hexColour().match(end).hasMatch()) {
            return std::nullopt;
        }
        const auto value = mixed(start, end, match.captured(4));
        const auto form = match.captured(1);
        if (form == QLatin1String("mix_strip")) {
            return value.mid(1);
        }
        if (form == QLatin1String("mix_rgb")) {
            return rgbOf(value);
        }
        return value;
    });

    fill(valueToken, [&colours](const QRegularExpressionMatch &match) -> std::optional<QString> {
        const auto name = match.captured(1);
        if (const auto value = colours.constFind(name); value != colours.constEnd()) {
            return *value;
        }
        for (const auto &[suffix, strip] : {std::pair {QStringLiteral("_strip"), true},
                 std::pair {QStringLiteral("_rgb"), false}}) {
            if (!name.endsWith(suffix)) {
                continue;
            }
            const auto value = colours.value(name.chopped(suffix.size()));
            if (value.isEmpty()) {
                return std::nullopt;
            }
            if (strip) {
                return value.startsWith(QLatin1Char('#')) ? value.mid(1) : value;
            }
            return hexColour().match(value).hasMatch() ? std::optional(rgbOf(value)) : std::nullopt;
        }
        return std::nullopt;
    });

    return text.toUtf8();
}

OmarchyColours readOmarchyColours(const QString &coloursFile)
{
    const auto program = QStandardPaths::findExecutable(QStringLiteral("omarchy-theme-color"));
    if (program.isEmpty() || !QFileInfo::exists(coloursFile)) {
        return {};
    }
    QProcess process;
    process.start(program, {QStringLiteral("--file"), coloursFile, QStringLiteral("--all")});
    // A few tens of milliseconds on a laptop. The wait only bounds a stuck one.
    if (!process.waitForFinished(5000) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        process.kill();
        return {};
    }
    OmarchyColours colours;
    for (const auto &line : process.readAllStandardOutput().split('\n')) {
        const auto tab = line.indexOf('\t');
        if (tab > 0) {
            colours.insert(QString::fromUtf8(line.left(tab)), QString::fromUtf8(line.mid(tab + 1)));
        }
    }
    return colours;
}

BrowserThemePaths BrowserThemePaths::fromEnvironment(
    const QString &dataRoot, const QString &shippedTemplate)
{
    const auto omarchy = OmarchyThemePaths::fromEnvironment();
    // Where `omarchy` keeps itself, which its scripts read from this
    // variable and which the package installs here.
    auto omarchyPath = qEnvironmentVariable("OMARCHY_PATH");
    if (omarchyPath.isEmpty()) {
        omarchyPath = QStringLiteral("/usr/share/omarchy");
    }
    BrowserThemePaths paths;
    paths.userThemes = QDir(omarchy.configuration).filePath(QStringLiteral("themes"));
    paths.systemThemes = QDir(omarchyPath).filePath(QStringLiteral("themes"));
    paths.userTemplate = omarchy.userTemplate();
    paths.shippedTemplate = shippedTemplate;
    paths.desktopTheme = QDir(omarchy.state).filePath(QStringLiteral("current"));
    paths.rendered = QDir(dataRoot).filePath(QStringLiteral("browser-theme/omaweb.json"));
    return paths;
}

QStringList paletteSources(const QString &environmentOverride, const QString &readerTheme,
    const QString &pickedTheme, const QString &desktopTheme, const QString &builtInTheme)
{
    if (!environmentOverride.isEmpty()) {
        // An override names one file, and nothing overtakes it.
        return {environmentOverride};
    }
    QStringList sources;
    for (const auto &path : {readerTheme, pickedTheme, desktopTheme, builtInTheme}) {
        if (!path.isEmpty()) {
            sources.append(path);
        }
    }
    return sources;
}

BrowserTheme::BrowserTheme(BrowserThemePaths paths, ColourSource colours, QString readerTheme,
    QString environmentOverride, QObject *parent)
    : QObject(parent)
    , m_paths(std::move(paths))
    , m_colours(std::move(colours))
    , m_readerTheme(std::move(readerTheme))
    , m_environmentOverride(std::move(environmentOverride))
{
    // The theme controller watches the deepest folder of its sources that
    // exists, and while nothing is rendered that would be the data root,
    // which changes with every page. The render's own folder is quiet.
    QDir().mkpath(QFileInfo(m_paths.rendered).absolutePath());
    connect(
        &m_swatches, &QFutureWatcher<QVariantList>::finished, this, &BrowserTheme::applySwatches);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, &BrowserTheme::update);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &BrowserTheme::update);
    // A watch can miss a folder replaced under it, as a theme installed again
    // by `git clone` is, or a `theme.json` written into a configuration
    // directory that did not exist yet, so both are also polled.
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] {
        if (renderState() != m_renderState || readOverride() != m_override) {
            update();
        }
    });
    update();
}

BrowserTheme::~BrowserTheme()
{
    // The colour source may hold what its owner frees next.
    m_swatches.waitForFinished();
}

QString BrowserTheme::choice() const { return m_choice; }
QString BrowserTheme::choiceName() const { return omarchyThemeName(m_choice); }
bool BrowserTheme::missing() const { return m_missing; }
QString BrowserTheme::desktopName() const { return m_desktopName; }
QVariantList BrowserTheme::themes() const { return m_themes; }
QVariantMap BrowserTheme::desktopColours() const { return m_desktopColours; }
QString BrowserTheme::overrideSource() const { return m_override; }
bool BrowserTheme::drawn() const
{
    return !m_choice.isEmpty() && !m_missing && m_override.isEmpty();
}
QString BrowserTheme::renderedPath() const { return m_paths.rendered; }

void BrowserTheme::setChoice(const QString &folder)
{
    m_choice = folder;
    update();
}

QString BrowserTheme::themeDirectory(const QString &folder) const
{
    // A folder's own name, never a path: the value is the reader's to edit
    // and Sync's to carry.
    if (folder.isEmpty() || folder.contains(QLatin1Char('/'))
        || folder.startsWith(QLatin1Char('.'))) {
        return {};
    }
    // `omarchy theme dir`: the reader's copy of a theme outranks the system's.
    const auto user = QDir(m_paths.userThemes).filePath(folder);
    return QFileInfo(user).isDir() ? user : QDir(m_paths.systemThemes).filePath(folder);
}

QString BrowserTheme::renderState() const
{
    // What a render is made from: the choice, its colours, a palette the theme
    // ships and the reader's template. The same state renders the same file.
    const auto directory = themeDirectory(m_choice);
    const auto stamp = [](const QFileInfo &file) {
        return QStringLiteral("%1 %2 %3")
            .arg(file.canonicalFilePath(), QString::number(file.size()),
                QString::number(file.lastModified().toMSecsSinceEpoch()));
    };
    const auto inTheme = [&directory](const QString &name) {
        return QFileInfo(directory.isEmpty() ? QString() : QDir(directory).filePath(name));
    };
    return QStringList {m_choice, stamp(inTheme(QStringLiteral("colors.toml"))),
        stamp(inTheme(QStringLiteral("omaweb.json"))), stamp(QFileInfo(m_paths.userTemplate))}
        .join(QLatin1Char('\n'));
}

void BrowserTheme::watch()
{
    QStringList paths {m_paths.userThemes, m_paths.systemThemes, m_paths.desktopTheme,
        QFileInfo(m_paths.userTemplate).absolutePath(), m_paths.userTemplate};
    if (!m_readerTheme.isEmpty()) {
        paths << QFileInfo(m_readerTheme).absolutePath();
    }
    if (const auto directory = themeDirectory(m_choice); !directory.isEmpty()) {
        paths << directory << QDir(directory).filePath(QStringLiteral("colors.toml"));
    }
    paths.removeIf([](const QString &path) { return !QFileInfo::exists(path); });
    // A file replaced under its watch is dropped from it, so the watch is
    // brought to the paths wanted now rather than added to once.
    auto stale = m_watcher.files() + m_watcher.directories();
    stale.removeIf([&paths](const QString &path) { return paths.contains(path); });
    if (!stale.isEmpty()) {
        m_watcher.removePaths(stale);
    }
    for (const auto &path : std::as_const(paths)) {
        if (!m_watcher.files().contains(path) && !m_watcher.directories().contains(path)) {
            m_watcher.addPath(path);
        }
    }
    if (!m_poll.isActive()) {
        m_poll.start();
    }
}

void BrowserTheme::update()
{
    const bool wasMissing = m_missing;
    const auto previousDesktop = m_desktopName;
    const auto previousOverride = m_override;
    const auto previousChoice = m_announcedChoice;
    m_announcedChoice = m_choice;
    m_desktopName = omarchyThemeName(QString::fromUtf8(
        readFile(QDir(m_paths.desktopTheme).filePath(QStringLiteral("theme.name"))).trimmed()));
    m_override = readOverride();
    // Anything in the folders watched wakes this, and a render runs a process,
    // so only a change to what the render is made from renders again.
    if (const auto state = renderState(); state != m_renderState) {
        m_renderState = state;
        render();
    }
    watch();
    if (m_missing != wasMissing || m_desktopName != previousDesktop
        || m_override != previousOverride || m_choice != previousChoice) {
        emit changed();
    }
}

void BrowserTheme::render()
{
    const auto directory = themeDirectory(m_choice);
    const auto coloursFile = QDir(directory).filePath(QStringLiteral("colors.toml"));
    const bool installed = !directory.isEmpty() && QFileInfo::exists(coloursFile);

    QByteArray palette;
    if (installed) {
        // A theme may ship Omaweb's palette itself, which Omarchy copies in
        // place of rendering the template, and so does this.
        palette = readFile(QDir(directory).filePath(QStringLiteral("omaweb.json")));
        if (palette.isEmpty()) {
            const auto colours = m_colours(coloursFile);
            if (colours.isEmpty()) {
                qWarning("Omaweb could not read the colours of the Omarchy theme %s, so it "
                         "follows the desktop's theme.",
                    qPrintable(m_choice));
            } else {
                auto templateText = readFile(m_paths.userTemplate);
                if (templateText.isEmpty()) {
                    templateText = readFile(m_paths.shippedTemplate);
                }
                palette = renderOmarchyTemplate(templateText, colours);
            }
        }
    }

    if (palette.isEmpty()) {
        QFile::remove(m_paths.rendered);
    } else if (readFile(m_paths.rendered) != palette
        && !writeReaderFile(m_paths.rendered, palette)) {
        qWarning("Omaweb could not write the browser theme's palette to %s.",
            qPrintable(m_paths.rendered));
    }
    m_missing = !m_choice.isEmpty() && !installed;
}

QString BrowserTheme::readOverride() const
{
    // Named as the reader would look for it: the variable they set, or the
    // file with their home as `~`.
    if (!m_environmentOverride.isEmpty()) {
        return QStringLiteral("OMAWEB_THEME_FILE");
    }
    if (!QFileInfo::exists(m_readerTheme)) {
        return {};
    }
    const auto home = QDir::homePath();
    return m_readerTheme.startsWith(home + QLatin1Char('/'))
        ? QStringLiteral("~") + m_readerTheme.mid(home.size())
        : m_readerTheme;
}

void BrowserTheme::refreshThemes()
{
    // A read already on its way answers this ask too.
    if (m_swatches.isRunning()) {
        return;
    }
    // `omarchy theme list`: every folder in either place, once, in the order
    // of their folder names.
    QStringList folders;
    for (const auto &place : {m_paths.userThemes, m_paths.systemThemes}) {
        folders += QDir(place).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    }
    folders.sort();
    folders.removeDuplicates();

    QVariantList themes;
    QStringList coloursFiles;
    for (const auto &folder : std::as_const(folders)) {
        themes.append(QVariantMap {{QStringLiteral("folder"), folder},
            {QStringLiteral("name"), omarchyThemeName(folder)}});
        coloursFiles.append(QDir(themeDirectory(folder)).filePath(QStringLiteral("colors.toml")));
    }
    m_themes = themes;
    emit themesChanged();

    // The desktop's colours come last, beside the themes'. Each read is a
    // process, so the list is drawn first and coloured once they are back.
    coloursFiles.append(QDir(m_paths.desktopTheme).filePath(QStringLiteral("theme/colors.toml")));
    m_swatches.setFuture(QtConcurrent::run([colours = m_colours, coloursFiles] {
        QVariantList swatches;
        for (const auto &file : coloursFiles) {
            const auto resolved = colours(file);
            swatches.append(QVariantMap {
                {QStringLiteral("background"), resolved.value(QStringLiteral("background"))},
                {QStringLiteral("foreground"), resolved.value(QStringLiteral("foreground"))},
                {QStringLiteral("accent"), resolved.value(QStringLiteral("accent"))}});
        }
        return swatches;
    }));
}

void BrowserTheme::applySwatches()
{
    const auto swatches = m_swatches.result();
    if (swatches.size() != m_themes.size() + 1) {
        return;
    }
    for (qsizetype i = 0; i < m_themes.size(); ++i) {
        auto entry = m_themes.at(i).toMap();
        entry.insert(swatches.at(i).toMap());
        m_themes[i] = entry;
    }
    m_desktopColours = swatches.last().toMap();
    emit themesChanged();
}

QString omarchyThemeName(const QString &folder)
{
    // `omarchy theme list`: a lowercase letter that starts the name or follows
    // a hyphen is capitalised, then every hyphen is a space.
    auto name = folder;
    for (qsizetype i = 0; i < name.size(); ++i) {
        const bool starts = i == 0 || name.at(i - 1) == QLatin1Char('-');
        if (starts && name.at(i) >= QLatin1Char('a') && name.at(i) <= QLatin1Char('z')) {
            name[i] = name.at(i).toUpper();
        }
    }
    return name.replace(QLatin1Char('-'), QLatin1Char(' '));
}

} // namespace omaweb
