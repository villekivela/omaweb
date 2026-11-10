#pragma once

#include <QObject>
#include <QString>

namespace omaweb {

// The page scripts the adapter runs that are files of their own, read from the
// resources they travel in with the adapter, whichever way its QML is shipped.
class QtPageScripts final : public QObject {
    Q_OBJECT
    // The Site style panel's picker, `site-style-picker.js`.
    Q_PROPERTY(QString siteStylePicker READ siteStylePicker CONSTANT)

public:
    explicit QtPageScripts(QObject *parent = nullptr);

    QString siteStylePicker() const;

private:
    QString m_siteStylePicker;
};

// Makes `QtPageScripts` available to QML as `import Omaweb.Engine`.
void registerQtPageScripts();

} // namespace omaweb
