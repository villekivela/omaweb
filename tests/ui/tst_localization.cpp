#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Launches the UI lab under a locale and reads one reported line back, so a
// test reads the catalogue from the screen's own text.
QString labReport(const QString &report, const QString &key, const QString &locale,
    const QStringList &extraArguments = {})
{
    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        return {};
    }
    QProcess lab;
    lab.setProgram(QStringLiteral(OMAWEB_UI_LAB_EXECUTABLE));
    lab.setArguments(
        QStringList {report, QStringLiteral("--data-root"), dataRoot.path()} + extraArguments);
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
        const auto prefix = key + QLatin1Char('=');
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size());
        }
    }
    return {};
}

QString startPageHint(const QString &locale, const QStringList &extraArguments = {})
{
    return labReport(QStringLiteral("--report-start-page"), QStringLiteral("start_page_hint"),
        locale, extraArguments);
}

QString settingsReport(const QString &key, const QString &locale)
{
    return labReport(QStringLiteral("--report-settings"), key, locale);
}

} // namespace

class Localization final : public QObject {
    Q_OBJECT

private slots:
    void theStartPageSpeaksFinnishUnderAFinnishLocale();
    void theStartPageStaysEnglishUnderAnEnglishLocale();
    void aLocaleWithoutACatalogueFallsBackToEnglish();
    void theLabSwitchesLocaleOverTheEnvironment();
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

void Localization::settingsSpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(settingsReport(QStringLiteral("settings_heading"), QStringLiteral("fi_FI.UTF-8")),
        QStringLiteral("Asetukset"));
}

void Localization::siteInformationSpeaksFinnishUnderAFinnishLocale()
{
    QCOMPARE(
        settingsReport(QStringLiteral("site_information_state"), QStringLiteral("fi_FI.UTF-8")),
        QStringLiteral("· mitään sivua ei ole ladattu"));
}

void Localization::settingsAndSiteInformationStayEnglishUnderAnEnglishLocale()
{
    QCOMPARE(settingsReport(QStringLiteral("settings_heading"), QStringLiteral("en_US.UTF-8")),
        QStringLiteral("Settings"));
    QCOMPARE(
        settingsReport(QStringLiteral("site_information_state"), QStringLiteral("en_US.UTF-8")),
        QStringLiteral("· no page is loaded"));
}

void Localization::aSettingChangedUnderFinnishStoresItsEnglishKeyAndValue()
{
    const auto locale = QStringLiteral("fi_FI.UTF-8");
    // The run is Finnish, or the stored value proves nothing.
    QCOMPARE(labReport(QStringLiteral("--report-setting-change"),
                 QStringLiteral("settings_heading"), locale),
        QStringLiteral("Asetukset"));
    QCOMPARE(labReport(QStringLiteral("--report-setting-change"),
                 QStringLiteral("stored_floating_controls"), locale),
        QStringLiteral("false"));
}

QTEST_GUILESS_MAIN(Localization)
#include "tst_localization.moc"
