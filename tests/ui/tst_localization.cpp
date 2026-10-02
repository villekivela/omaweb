#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Launches the UI lab under a locale and reads one line it reports back, which
// is how a test sees what a surface says on the screen rather than what the
// catalogue holds.
QString labReport(const QString &locale, const QStringList &reportArguments, const QString &prefix,
    const QStringList &extraArguments = {})
{
    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        return {};
    }
    QProcess lab;
    lab.setProgram(QStringLiteral(OMAWEB_UI_LAB_EXECUTABLE));
    lab.setArguments(reportArguments + QStringList {QStringLiteral("--data-root"), dataRoot.path()}
        + extraArguments);
    auto environment = QProcessEnvironment::systemEnvironment();
    for (const auto &name : {"LC_ALL", "LC_MESSAGES", "LANGUAGE"}) {
        environment.remove(QString::fromLatin1(name));
    }
    environment.insert(QStringLiteral("LANG"), locale);
    lab.setProcessEnvironment(environment);
    lab.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    lab.start();
    if (!lab.waitForFinished(60000)) {
        lab.kill();
        return {};
    }
    const auto output = QString::fromUtf8(lab.readAllStandardOutput());
    for (const auto &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size());
        }
    }
    return {};
}

QString startPageHint(const QString &locale, const QStringList &extraArguments = {})
{
    return labReport(locale, {QStringLiteral("--report-start-page")},
        QStringLiteral("start_page_hint="), extraArguments);
}

// One field of the row the command panel lists for a command, asked for by its
// identifier: `title`, `command` or `keys`.
QString commandField(const QString &locale, const QString &command, const QString &field)
{
    return labReport(locale, {QStringLiteral("--report-command-title"), command},
        QStringLiteral("command_%1=").arg(field));
}

// One line of what the window says for a prompt and a context-menu entry: the
// `prompt_message` of an Agent's request for a Space, or the `menu_entry` that
// copies a link's address.
QString chromeText(const QString &locale, const QString &key)
{
    return labReport(locale, {QStringLiteral("--report-chrome")}, QStringLiteral("%1=").arg(key));
}

QString commandTitle(const QString &locale, const QString &command)
{
    return commandField(locale, command, QStringLiteral("title"));
}

// One line a lab report prints under a locale, for the surfaces whose report is a flag
// alone.
QString reportLine(const QString &locale, const QString &report, const QString &key)
{
    return labReport(locale, {report}, key + QLatin1Char('='));
}

} // namespace

class Localization final : public QObject {
    Q_OBJECT

private slots:
    void theStartPageSpeaksFinnishUnderAFinnishLocale();
    void theStartPageStaysEnglishUnderAnEnglishLocale();
    void aLocaleWithoutACatalogueFallsBackToEnglish();
    void theLabSwitchesLocaleOverTheEnvironment();
    void aPromptSpeaksFinnishUnderAFinnishLocale();
    void aContextMenuEntrySpeaksFinnishUnderAFinnishLocale();
    void promptAndMenuStayEnglishUnderAnEnglishLocale();
    void theCommandPanelNamesACommandInFinnish();
    void theCommandPanelNamesACommandInEnglish();
    void aCommandKeepsItsIdentifierAndKeysUnderFinnish();
    void settingsSpeaksFinnishUnderAFinnishLocale();
    void siteInformationSpeaksFinnishUnderAFinnishLocale();
    void settingsAndSiteInformationStayEnglishUnderAnEnglishLocale();
    void aSettingChangedUnderFinnishStoresItsEnglishKeyAndValue();
};

void Localization::theStartPageSpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(startPageHint(QStringLiteral("fi_FI.UTF-8")), QStringLiteral("pikanäppäimet"));
}

void Localization::theStartPageStaysEnglishUnderAnEnglishLocale()
{
    QCOMPARE(startPageHint(QStringLiteral("en_US.UTF-8")), QStringLiteral("shortcuts"));
}

void Localization::aLocaleWithoutACatalogueFallsBackToEnglish()
{
    QCOMPARE(startPageHint(QStringLiteral("de_DE.UTF-8")), QStringLiteral("shortcuts"));
}

void Localization::theLabSwitchesLocaleOverTheEnvironment()
{
    QCOMPARE(startPageHint(
                 QStringLiteral("en_US.UTF-8"), {QStringLiteral("--locale"), QStringLiteral("fi")}),
        QStringLiteral("pikanäppäimet"));
}

void Localization::aPromptSpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(chromeText(QStringLiteral("fi_FI.UTF-8"), QStringLiteral("prompt_message")),
        QStringLiteral("Agentti Forge haluaa käyttää tilaa Work"));
}

void Localization::aContextMenuEntrySpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(chromeText(QStringLiteral("fi_FI.UTF-8"), QStringLiteral("menu_entry")),
        QStringLiteral("Kopioi linkin osoite"));
}

void Localization::promptAndMenuStayEnglishUnderAnEnglishLocale()
{
    const auto locale = QStringLiteral("en_US.UTF-8");
    QCOMPARE(chromeText(locale, QStringLiteral("prompt_message")),
        QStringLiteral("An Agent named Forge wants to use Space Work"));
    QCOMPARE(chromeText(locale, QStringLiteral("menu_entry")), QStringLiteral("Copy link address"));
}

void Localization::theCommandPanelNamesACommandInFinnish()
{
    QCOMPARE(commandTitle(QStringLiteral("fi_FI.UTF-8"), QStringLiteral("new-tab")),
        QStringLiteral("Uusi välilehti"));
}

void Localization::theCommandPanelNamesACommandInEnglish()
{
    QCOMPARE(commandTitle(QStringLiteral("en_US.UTF-8"), QStringLiteral("new-tab")),
        QStringLiteral("New tab"));
}

// Sync projects keybindings.json, which names a command by its identifier and a
// key as the keyboard prints it. The panel's row for a command carries both, so
// a Finnish reader's panel must still say `new-tab` and `Ctrl+T`, whatever the
// title beside them says.
void Localization::aCommandKeepsItsIdentifierAndKeysUnderFinnish()
{
    const auto locale = QStringLiteral("fi_FI.UTF-8");
    QCOMPARE(commandField(locale, QStringLiteral("new-tab"), QStringLiteral("command")),
        QStringLiteral("new-tab"));
    QVERIFY(commandField(locale, QStringLiteral("new-tab"), QStringLiteral("keys"))
            .startsWith(QStringLiteral("Ctrl+T")));
    QCOMPARE(commandField(locale, QStringLiteral("new-tab"), QStringLiteral("title")),
        QStringLiteral("Uusi välilehti"));
}

void Localization::settingsSpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(reportLine(QStringLiteral("fi_FI.UTF-8"), QStringLiteral("--report-settings"),
                 QStringLiteral("settings_heading")),
        QStringLiteral("Asetukset"));
}

void Localization::siteInformationSpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(reportLine(QStringLiteral("fi_FI.UTF-8"), QStringLiteral("--report-settings"),
                 QStringLiteral("site_information_state")),
        QStringLiteral("· mitään sivua ei ole ladattu"));
}

void Localization::settingsAndSiteInformationStayEnglishUnderAnEnglishLocale()
{
    const auto locale = QStringLiteral("en_US.UTF-8");
    QCOMPARE(
        reportLine(locale, QStringLiteral("--report-settings"), QStringLiteral("settings_heading")),
        QStringLiteral("Settings"));
    QCOMPARE(reportLine(locale, QStringLiteral("--report-settings"),
                 QStringLiteral("site_information_state")),
        QStringLiteral("· no page is loaded"));
}

void Localization::aSettingChangedUnderFinnishStoresItsEnglishKeyAndValue()
{
    const auto locale = QStringLiteral("fi_FI.UTF-8");
    // The run is Finnish, or the stored value proves nothing.
    QCOMPARE(reportLine(locale, QStringLiteral("--report-setting-change"),
                 QStringLiteral("settings_heading")),
        QStringLiteral("Asetukset"));
    QCOMPARE(reportLine(locale, QStringLiteral("--report-setting-change"),
                 QStringLiteral("stored_floating_controls")),
        QStringLiteral("false"));
}

QTEST_GUILESS_MAIN(Localization)
#include "tst_localization.moc"
