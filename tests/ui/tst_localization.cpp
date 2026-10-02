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

// The name the command panel lists for a command, asked for by its identifier.
QString commandTitle(const QString &locale, const QString &command)
{
    return labReport(locale, {QStringLiteral("--report-command-title"), command},
        QStringLiteral("command_title="));
}

} // namespace

class Localization final : public QObject {
    Q_OBJECT

private slots:
    void theStartPageSpeaksFinnishUnderAFinnishLocale();
    void theStartPageStaysEnglishUnderAnEnglishLocale();
    void aLocaleWithoutACatalogueFallsBackToEnglish();
    void theLabSwitchesLocaleOverTheEnvironment();
    void theCommandPanelNamesACommandInFinnish();
    void theCommandPanelNamesACommandInEnglish();
    void aCommandKeepsItsIdentifierWhateverTheLocale();
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

// Sync projects keybindings by command identifier, so the panel is asked by it
// in both locales and finds the same command.
void Localization::aCommandKeepsItsIdentifierWhateverTheLocale()
{
    QVERIFY(!commandTitle(QStringLiteral("fi_FI.UTF-8"), QStringLiteral("close-tab")).isEmpty());
    QVERIFY(!commandTitle(QStringLiteral("en_US.UTF-8"), QStringLiteral("close-tab")).isEmpty());
}

QTEST_GUILESS_MAIN(Localization)
#include "tst_localization.moc"
