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
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY configurationChanged)

public:
    explicit KeyboardNavigation(QString configurationPath, QObject *parent = nullptr);

    bool enabled() const;
    bool valid() const;
    QVariantMap bindings() const;
    QString errorMessage() const;

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
    QHash<QString, SiteRule> m_siteRules;
    QString m_errorMessage;
    bool m_enabled = false;
    bool m_valid = false;
};

} // namespace tanto
