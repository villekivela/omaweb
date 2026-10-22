#pragma once

#include "ProblemReport.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace omaweb {

class BrowserController;
class PageScale;
class ProblemRecording;

// Writes the Problem report (ADR 0068) of the browser it is given: what is the
// same on every site, and the tab it was started from. What it reads is only
// what a report holds: never history, another tab's address, a Space's name,
// another site's permissions, or anything a site stored.
class ProblemReporter final : public QObject {
    Q_OBJECT

public:
    ProblemReporter(BrowserController *browser, QString configRoot, QObject *parent = nullptr);

    // What the browser and its engine say they are, a line each.
    void setEngineVersions(QStringList lines);
    // What a page reads of the browser: `userAgent`, `brands`,
    // `fullVersionList`, `platform` and `language`, from the engine's profile.
    Q_INVOKABLE void setSites(const QVariantMap &sites);
    // The Sync module's controller, or null where Sync is not loaded.
    void setSync(QObject *controller);
    void setRecording(ProblemRecording *recording);
    void setPageScale(const PageScale *scale);
    void setRun(ProblemReportRun run);

    // Who asked: the reader, in the browser's own window, or an Agent, which
    // every request over the socket is, from the CLI as from MCP.
    enum class Requester { Reader, Agent };

    // The report, started from the tab `tabId`, or from no tab when it is
    // empty. The reader's takes the recording in, running or ended, and ends
    // it. One from a Private window says only that it was, and an Agent's says
    // only that a recording is there to take; both leave it alone.
    QList<ProblemReportSection> sections(
        const QString &tabId, bool privateWindow, Requester requester = Requester::Reader);
    Q_INVOKABLE QString text(const QString &tabId, bool privateWindow);
    QString text(const QString &tabId, bool privateWindow, Requester requester);
    // The bug form filled with the report, as `address`, with `cut` when the
    // link could not carry it all and `text`, the whole report.
    Q_INVOKABLE QVariantMap form(const QString &tabId, bool privateWindow);

private:
    QStringList tabLines(const QString &tabId, bool privateWindow) const;
    QStringList syncLines() const;
    QStringList extensionLines() const;
    QStringList sessionLines() const;

    BrowserController *m_browser = nullptr;
    QString m_configRoot;
    QStringList m_engineVersions;
    QVariantMap m_sites;
    QPointer<QObject> m_sync;
    QPointer<ProblemRecording> m_recording;
    const PageScale *m_pageScale = nullptr;
    ProblemReportRun m_run;
};

} // namespace omaweb
