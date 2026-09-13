#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QPluginLoader;

namespace omaweb {

class BrowserController;
class BrowserStateExchange;
class ContentBlocker;
class KeyboardNavigation;

class SyncLauncher final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *controller READ controller NOTIFY controllerChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool configured READ configured CONSTANT)

public:
    SyncLauncher(BrowserController *browser, ContentBlocker *blocker,
        KeyboardNavigation *keyboardNavigation, QString dataRoot, QString configRoot,
        QString modulePath, QObject *parent = nullptr);
    ~SyncLauncher() override;

    QObject *controller() const;
    QString errorMessage() const;
    bool configured() const;
    Q_INVOKABLE bool load();

signals:
    void controllerChanged();
    void errorMessageChanged();

private:
    BrowserController *m_browser = nullptr;
    ContentBlocker *m_blocker = nullptr;
    KeyboardNavigation *m_keyboardNavigation = nullptr;
    QString m_dataRoot;
    QString m_configRoot;
    QString m_modulePath;
    QString m_errorMessage;
    std::unique_ptr<QPluginLoader> m_loader;
    std::unique_ptr<BrowserStateExchange> m_stateExchange;
    QObject *m_controller = nullptr;
    bool m_configured = false;
};

} // namespace omaweb
