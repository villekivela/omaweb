#include "LocaleReport.h"

#include <QQmlEngine>

namespace omaweb {

LocaleReport::LocaleReport(const LocaleChoice &choice, QStringList shipped, QObject *parent)
    : QObject(parent)
    , m_title(localeTitle(choice.locale))
    , m_variable(choice.variable)
    , m_shipped(std::move(shipped))
{
}

QString LocaleReport::title() const { return m_title; }

QString LocaleReport::variable() const { return m_variable; }

QStringList LocaleReport::shipped() const { return m_shipped; }

void registerLocaleReport(LocaleReport *report)
{
    qmlRegisterSingletonInstance("Omaweb", 1, 0, "LocaleReport", report);
}

} // namespace omaweb
