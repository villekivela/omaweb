#pragma once

#include "Translations.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace omaweb {

// What Settings says about the locale in use, which is facts rather than wording: the
// wording is the page's, so it is translated like the rest of the chrome.
class LocaleReport final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString variable READ variable CONSTANT)
    Q_PROPERTY(QStringList shipped READ shipped CONSTANT)

public:
    LocaleReport(const LocaleChoice &choice, QStringList shipped, QObject *parent = nullptr);

    // The locale in its own language and code, `Suomi (fi_FI)`.
    QString title() const;
    // The environment variable that chose it, empty when the system's locale answered.
    QString variable() const;
    QStringList shipped() const;

private:
    QString m_title;
    QString m_variable;
    QStringList m_shipped;
};

// Makes one report available to QML as `import Omaweb`. Call once per process, before
// loading QML that reads it.
void registerLocaleReport(LocaleReport *report);

} // namespace omaweb
