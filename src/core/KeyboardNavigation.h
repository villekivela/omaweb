#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>

namespace omaweb {

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
    KeyboardNavigation(
        QString configurationPath, QString pageScriptPath, QObject *parent = nullptr);

    bool enabled() const;
    bool valid() const;
    QVariantMap bindings() const;
    QVariantMap browserBindings() const;
    QString errorMessage() const;
    QString pageScript() const;

    Q_INVOKABLE bool setEnabled(bool enabled);
    bool reload();

    // Carries a configuration file written by an earlier version forward onto
    // the shipped defaults. Owned here rather than by the startup path,
    // because this class already owns what the file means.
    static bool adoptDefaults(const QString &configurationPath, const QString &defaultsPath);
    // Seeds the file from the defaults on the first run. A copy out of a Qt
    // resource is read-only, and Sync and the Settings page write the file
    // later, so the seeded file is the reader's to write.
    static bool seedDefaults(const QString &configurationPath, const QString &defaultsPath);
    // An earlier release left the seeded file read-only. Runs at every start:
    // makes such a file writable by its owner again, and does nothing to a
    // file the owner can already write.
    static void restoreOwnerWrite(const QString &configurationPath);
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

} // namespace omaweb
