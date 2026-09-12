#pragma once

#include <QDebug>
#include <QString>

namespace omaweb::probe {

// A runtime measurement the tests keep, the way `scripts/benchmark_build.sh`
// keeps build times. The line printed here is what the development guide's
// performance section records, so it names the number, its unit and the
// threshold in one place; the message returned is what a failing probe says.
//
// The threshold is set at a margin over the first measurement rather than
// guessed ahead of it. A probe fails when its number crosses it.
inline QString report(const QString &name, double measured, const QString &unit, double threshold)
{
    const auto value = QString::number(measured, 'f', 1);
    const auto limit = QString::number(threshold, 'f', 1);
    qInfo("probe %s: %s %s (threshold %s %s)", qPrintable(name), qPrintable(value),
        qPrintable(unit), qPrintable(limit), qPrintable(unit));
    return QStringLiteral("%1 measured %2 %3, threshold %4 %3").arg(name, value, unit, limit);
}

} // namespace omaweb::probe
