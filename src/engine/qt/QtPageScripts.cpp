#include "QtPageScripts.h"

#include <QFile>
#include <QtQml>

namespace omaweb {

QtPageScripts::QtPageScripts(QObject *parent)
    : QObject(parent)
{
    QFile picker(QStringLiteral(":/omaweb/engine-qt/site-style-picker.js"));
    if (picker.open(QIODevice::ReadOnly)) {
        m_siteStylePicker = QString::fromUtf8(picker.readAll());
    }
}

QString QtPageScripts::siteStylePicker() const { return m_siteStylePicker; }

void registerQtPageScripts()
{
    qmlRegisterSingletonType<QtPageScripts>("Omaweb.Engine", 1, 0, "QtPageScripts",
        [](QQmlEngine *, QJSEngine *) -> QObject * { return new QtPageScripts; });
}

} // namespace omaweb
