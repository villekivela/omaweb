#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Launches the UI lab under a locale with a report flag and reads back the
// `name=value` lines it prints, the catalogue as the screen's own text.
QMap<QString, QString> labReport(
    const QString &flag, const QString &locale, const QStringList &extraArguments = {})
{
    QMap<QString, QString> report;
    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        return report;
    }
    QProcess lab;
    lab.setProgram(QStringLiteral(OMAWEB_UI_LAB_EXECUTABLE));
    lab.setArguments(
        QStringList {flag, QStringLiteral("--data-root"), dataRoot.path()} + extraArguments);
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
        return report;
    }
    const auto output = QString::fromUtf8(lab.readAllStandardOutput());
    for (const auto &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const auto separator = line.indexOf(QLatin1Char('='));
        if (separator > 0) {
            report.insert(line.left(separator), line.mid(separator + 1));
        }
    }
    return report;
}

QString startPageHint(const QString &locale, const QStringList &extraArguments = {})
{
    return labReport(QStringLiteral("--report-start-page"), locale, extraArguments)
        .value(QStringLiteral("start_page_hint"));
}

QString chromeText(const QString &key, const QString &locale)
{
    return labReport(QStringLiteral("--report-chrome"), locale).value(key);
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
    QCOMPARE(chromeText(QStringLiteral("prompt_message"), QStringLiteral("fi_FI.UTF-8")),
        QStringLiteral("Agentti Forge haluaa käyttää tilaa Work"));
}

void Localization::aContextMenuEntrySpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(chromeText(QStringLiteral("menu_entry"), QStringLiteral("fi_FI.UTF-8")),
        QStringLiteral("Kopioi linkin osoite"));
}

void Localization::promptAndMenuStayEnglishUnderAnEnglishLocale()
{
    const auto report = labReport(QStringLiteral("--report-chrome"), QStringLiteral("en_US.UTF-8"));
    QCOMPARE(report.value(QStringLiteral("prompt_message")),
        QStringLiteral("An Agent named Forge wants to use Space Work"));
    QCOMPARE(report.value(QStringLiteral("menu_entry")), QStringLiteral("Copy link address"));
}

QTEST_GUILESS_MAIN(Localization)
#include "tst_localization.moc"
