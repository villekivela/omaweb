#pragma once

#include "QtNetworkEvents.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <functional>
#include <memory>

namespace omaweb {

// An Agent tab's network, read through a debugging session of its page alone
// (ADR 0051). The session is the engine patch's `QWebEngineDevToolsSession`,
// whose client Chromium does not trust, so it reaches the page and its frames
// and no other page, worker or the browser. An engine without the patch has
// no session, and this reports nothing.
//
// The session is open while `active` is true and the view has a page: the
// page area sets it while an Agent holds the tab, so capture starts when an
// Agent attaches and the session, with every body the engine kept for it,
// goes when it detaches, when Allow agents goes off and when the view goes.
class QtAgentNetwork : public QObject {
    Q_OBJECT
    // Whether this build's engine offers a page's own debugging session, read
    // from the engine's headers when Omaweb is built.
    Q_PROPERTY(bool available READ available CONSTANT)
    // The WebEngineView whose page is read, by its `devToolsId`.
    Q_PROPERTY(QObject *view READ view WRITE setView NOTIFY viewChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool attached READ attached NOTIFY attachedChanged)

public:
    explicit QtAgentNetwork(QObject *parent = nullptr);
    ~QtAgentNetwork() override;

    static bool sessionAvailable();
    bool available() const;
    QObject *view() const;
    void setView(QObject *view);
    bool active() const;
    void setActive(bool active);
    bool attached() const;

    // Opens the session if it should be open and is not, such as once the
    // view has made its page.
    Q_INVOKABLE void attachIfReady();
    // Asks the engine for a request's body, `part` "response" or "sent", by
    // the `body` a report carried. The answer comes as `bodyFetched` under
    // `ticket`: `{ok: true, body: <base64>}` or a refusal with `code` and
    // `error`.
    Q_INVOKABLE void fetchBody(int ticket, const QVariantMap &body, const QString &part);

signals:
    void viewChanged();
    void activeChanged();
    void attachedChanged();
    // What the page's session learnt of one request, as
    // `AgentControl::recordNetworkRequest` takes it, with `document` the
    // adapter's own count.
    void requestReported(const QVariantMap &report);
    void bodyFetched(int ticket, const QVariantMap &answer);

private:
    void attach();
    void detach();
    void read(const QByteArray &message);
    void send(const QString &method, const QJsonObject &params, const QString &session = {},
        std::function<void(const QJsonObject &)> answered = {});

    QPointer<QObject> m_view;
    bool m_active = false;
    // A `QWebEngineDevToolsSession`, which a stock engine does not declare.
    std::unique_ptr<QObject> m_session;
    QtNetworkEvents m_events;
    int m_nextCommand = 0;
    QHash<int, std::function<void(const QJsonObject &)>> m_waiting;
};

// Makes `QtAgentNetwork` available to QML as `import Omaweb.Engine`. Call once
// per process, before loading QML that uses it.
void registerQtAgentNetwork();

} // namespace omaweb
