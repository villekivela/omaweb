#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>

namespace tanto {

class KeyboardNavigation final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY configurationChanged)
    Q_PROPERTY(bool valid READ valid NOTIFY configurationChanged)
    Q_PROPERTY(QVariantMap bindings READ bindings NOTIFY configurationChanged)
    Q_PROPERTY(QVariantMap browserBindings READ browserBindings NOTIFY configurationChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY configurationChanged)
    Q_PROPERTY(QString pageScript READ pageScript CONSTANT)

public:
    explicit KeyboardNavigation(QString configurationPath, QObject *parent = nullptr);
    KeyboardNavigation(QString configurationPath, QString pageScriptPath,
        QObject *parent = nullptr);

    bool enabled() const;
    bool valid() const;
    QVariantMap bindings() const;
    QVariantMap browserBindings() const;
    QString errorMessage() const;
    QString pageScript() const;

    Q_INVOKABLE bool setEnabled(bool enabled);
    Q_INVOKABLE QVariantMap configurationForUrl(const QUrl &url) const;

signals:
    void configurationChanged();

private:
    struct SiteRule {
        bool passthroughAll = false;
        QStringList passthroughKeys;
    };

    bool load();
    bool save() const;
    static bool hostMatches(const QString &host, const QString &ruleHost);

    QString m_configurationPath;
    QVariantMap m_bindings;
    QVariantMap m_browserBindings;
    QHash<QString, SiteRule> m_siteRules;
    QString m_errorMessage;
    QString m_pageScript;
    bool m_enabled = false;
    bool m_valid = false;
};

} // namespace tanto
