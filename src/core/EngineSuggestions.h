#pragma once

#include "SettingsFile.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

class QNetworkReply;

namespace omaweb {

// Whether the Omnibar asks a search engine for Engine suggestions, and the
// client it asks through.
//
// One setting for the whole browser, off by default: turning it on sends what
// the reader types to a search engine, which nothing else Omaweb does before
// a commit. It lives in the configuration directory with the reader's other
// privacy decisions (ADR 0016), outside the Sync projection. A Private window
// never asks, whatever it says; that is the window's capability to refuse,
// not this setting's.
//
// The client is core's own, not a Space's Engine profile, so a request
// carries no cookies, no Space and nothing a page has stored.
class EngineSuggestions final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)

public:
    explicit EngineSuggestions(QString configRoot, QObject *parent = nullptr);
    ~EngineSuggestions() override;

    bool enabled() const;
    void setEnabled(bool enabled);

    // Sends the request for one suggest address. The reply belongs to the
    // caller, and aborts itself when the whole answer has not arrived within
    // a second.
    QNetworkReply *ask(const QUrl &address);

    // The proposals in an OpenSearch suggestions answer, `["typed", ["s1",
    // "s2"]]`, in the engine's order. Any other shape is no proposals.
    static QStringList parse(const QByteArray &body);

signals:
    void enabledChanged();

private:
    SettingsFile m_settings;
    bool m_enabled = false;
    // Built on the first ask: a browser whose reader never turns this on
    // never builds a network stack for it.
    std::unique_ptr<QNetworkAccessManager> m_network;
};

} // namespace omaweb
