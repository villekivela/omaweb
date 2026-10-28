#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace omaweb {

// The Diagnostic overrides one run was started with (ADR 0068): named changes
// to what Omaweb presents to sites, given as `--diagnose <name>=<value>`. They
// live in this process alone. Nothing here is written to a profile, the
// settings or Sync, so the next start without them presents the engine's own.
struct DiagnosticOverrides {
    // `report-chrome`: the Chrome major version the user agent and the client
    // hints report.
    std::optional<int> reportedChrome;
    // `brands`: the brand list as Chrome sends it, in place of the engine's.
    std::optional<QStringList> brands;
    // `language`: the one language every site is told the reader reads.
    std::optional<QString> language;
    // Each override as it was given, `<name>=<value>`, in that order: what the
    // banner and a Problem report name.
    QStringList named;
    // Non-empty when Omaweb must refuse to start, and why.
    QString refusal;

    bool empty() const { return named.isEmpty(); }
};

// Reads every `--diagnose <name>=<value>` and `--diagnose=<name>=<value>` on
// the command line. An unknown name, a malformed value or a name given twice
// is a refusal that lists the overrides there are.
DiagnosticOverrides readDiagnosticOverrides(const QStringList &arguments);

// The overrides there are, one `<name>=<value>` form each, for the refusal and
// for `omaweb help`.
const QStringList &diagnosticOverrideForms();

// What the engine presents, with the overrides applied. Each is the engine's
// value where no override changes it.
QString overriddenUserAgent(const QString &engine, const DiagnosticOverrides &overrides);
// The brands and their full versions, as `QWebEngineClientHints` holds them:
// a brand at the engine's Chromium version reports `reportedChrome`'s, the
// engine's other brands keep theirs, and a brand `brands` adds reports
// Chromium's.
QVariantMap overriddenFullVersionList(
    const QVariantMap &engine, const DiagnosticOverrides &overrides);
QString overriddenFullVersion(const QString &engine, const DiagnosticOverrides &overrides);
QString overriddenAcceptLanguage(const QString &engine, const DiagnosticOverrides &overrides);

} // namespace omaweb
