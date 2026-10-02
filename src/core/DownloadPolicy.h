#pragma once

#include <QString>

namespace omaweb::DownloadPolicy {

// Prefer the filename because that is what the desktop will interpret. Use the
// media type only when the filename has no recognized high-risk extension.
QString riskKind(const QString &fileName, const QString &mimeType);

// The kind as the reader reads it, in their language. Agents and tests use the English kind itself.
QString riskLabel(const QString &kind);

bool highRisk(const QString &fileName, const QString &mimeType);

} // namespace omaweb::DownloadPolicy
