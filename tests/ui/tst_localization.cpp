#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Launches the UI lab under a locale and reads the Start page's hint back,
// which is the first surface wrapped for translation.
QString startPageHint(const QString &locale, const QStringList &extraArguments = {})
{
    QTemporaryDir dataRoot;
    if (!dataRoot.isValid()) {
        return {};
    }
    QProcess lab;
    lab.setProgram(QStringLiteral(OMAWEB_UI_LAB_EXECUTABLE));
    lab.setArguments(QStringList {QStringLiteral("--report-start-page"),
                         QStringLiteral("--data-root"), dataRoot.path()}
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
        const auto prefix = QStringLiteral("start_page_hint=");
        if (line.startsWith(prefix)) {
            return line.mid(prefix.size());
        }
    }
    return {};
}

} // namespace

class Localization final : public QObject {
    Q_OBJECT

private slots:
    void theStartPageSpeaksFinnishUnderAFinnishLocale();
    void theStartPageStaysEnglishUnderAnEnglishLocale();
    void aLocaleWithoutACatalogueFallsBackToEnglish();
    void theLabSwitchesLocaleOverTheEnvironment();
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
    QCOMPARE(startPageHint(QStringLiteral("en_US.UTF-8"), {QStringLiteral("--locale"), QStringLiteral("fi")}),
        QStringLiteral("pikanäppäimet"));
}

QTEST_GUILESS_MAIN(Localization)
#include "tst_localization.moc"
