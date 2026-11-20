#include "QtAgentNetwork.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QQmlEngine>

#if OMAWEB_AGENT_NETWORK
#include <QtWebEngineCore/qwebenginedevtoolssession.h>
#endif

namespace omaweb {
namespace {

#if OMAWEB_AGENT_NETWORK
    QWebEngineDevToolsSession *sessionOf(const std::unique_ptr<QObject> &session)
    {
        return static_cast<QWebEngineDevToolsSession *>(session.get());
    }
#endif

    QVariantMap refusal(const QString &code, const QString &error)
    {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("code"), code},
            {QStringLiteral("error"), error}};
    }

} // namespace

QtAgentNetwork::QtAgentNetwork(QObject *parent)
    : QObject(parent)
{
}

QtAgentNetwork::~QtAgentNetwork() = default;

bool QtAgentNetwork::sessionAvailable() { return OMAWEB_AGENT_NETWORK; }

bool QtAgentNetwork::available() const { return sessionAvailable(); }

QObject *QtAgentNetwork::view() const { return m_view; }

void QtAgentNetwork::setView(QObject *view)
{
    if (view == m_view) {
        return;
    }
    detach();
    m_view = view;
    emit viewChanged();
    attachIfReady();
}

bool QtAgentNetwork::active() const { return m_active; }

void QtAgentNetwork::setActive(bool active)
{
    if (active == m_active) {
        return;
    }
    m_active = active;
    emit activeChanged();
    if (m_active) {
        attachIfReady();
    } else {
        detach();
    }
}

bool QtAgentNetwork::attached() const
{
#if OMAWEB_AGENT_NETWORK
    return m_session && sessionOf(m_session)->isAttached();
#else
    return false;
#endif
}

void QtAgentNetwork::attachIfReady()
{
    if (m_active && m_view && !attached()) {
        attach();
    }
}

void QtAgentNetwork::attach()
{
#if OMAWEB_AGENT_NETWORK
    const auto id = m_view->property("devToolsId").toString();
    if (id.isEmpty()) {
        return;
    }
    auto session = std::make_unique<QWebEngineDevToolsSession>();
    connect(
        session.get(), &QWebEngineDevToolsSession::messageReceived, this, &QtAgentNetwork::read);
    if (!session->attach(id)) {
        return;
    }
    // The page closing ends the session, and what it held goes with it. The
    // ended session is kept until the next attach or detach deletes it.
    connect(session.get(), &QWebEngineDevToolsSession::detached, this, [this] {
        m_waiting.clear();
        emit attachedChanged();
    });
    m_session = std::move(session);
    m_events = {};
    send(QStringLiteral("Page.getFrameTree"), {}, {}, [this](const QJsonObject &answer) {
        m_events.setMainFrame(answer.value(QStringLiteral("result"))
                .toObject()
                .value(QStringLiteral("frameTree"))
                .toObject()
                .value(QStringLiteral("frame"))
                .toObject()
                .value(QStringLiteral("id"))
                .toString());
    });
    // A document shown without a request, from the back-forward cache, is
    // known by the main frame's navigation alone.
    send(QStringLiteral("Page.enable"), {});
    listen({});
    emit attachedChanged();
#endif
}

// The page's session or a child's: its requests, and the frames it brings, a
// frame from another site running in a process of its own among them.
void QtAgentNetwork::listen(const QString &session)
{
    send(QStringLiteral("Network.enable"),
        // The engine keeps a body whole or not at all, so one far past the
        // 10 MB the core writes is still kept to be cut, and a page's bodies
        // together are kept to the engine's own default.
        {{QStringLiteral("maxTotalBufferSize"), QtNetworkEvents::keptBodiesBytes},
            {QStringLiteral("maxResourceBufferSize"), QtNetworkEvents::keptBodyBytes}},
        session);
    // Frames alone. Auto-attach would also bring the service worker whose
    // scope holds the page, which serves the profile's other tabs of that
    // scope, and the session cannot let go of a child once it has it.
    send(QStringLiteral("Target.setAutoAttach"),
        {{QStringLiteral("autoAttach"), true}, {QStringLiteral("waitForDebuggerOnStart"), false},
            {QStringLiteral("flatten"), true},
            {QStringLiteral("filter"),
                QJsonArray {QJsonObject {{QStringLiteral("type"), QStringLiteral("iframe")}}}}},
        session);
}

void QtAgentNetwork::detach()
{
    if (!m_session) {
        return;
    }
    const auto wasAttached = attached();
    // Deleting the session ends it, and the engine lets go of the bodies it
    // kept for it. What was asked of it is dropped before attachedChanged,
    // which the page answers by refusing every body still waiting.
    m_session.reset();
    m_waiting.clear();
    if (wasAttached) {
        emit attachedChanged();
    }
}

void QtAgentNetwork::send(const QString &method, const QJsonObject &params, const QString &session,
    std::function<void(const QJsonObject &)> answered)
{
#if OMAWEB_AGENT_NETWORK
    if (!m_session) {
        if (answered) {
            answered({});
        }
        return;
    }
    const auto id = ++m_nextCommand;
    QJsonObject command {{QStringLiteral("id"), id}, {QStringLiteral("method"), method},
        {QStringLiteral("params"), params}};
    if (!session.isEmpty()) {
        command.insert(QStringLiteral("sessionId"), session);
    }
    if (answered) {
        m_waiting.insert(id, std::move(answered));
    }
    sessionOf(m_session)->sendMessage(QJsonDocument(command).toJson(QJsonDocument::Compact));
#else
    Q_UNUSED(method)
    Q_UNUSED(params)
    Q_UNUSED(session)
    if (answered) {
        answered({});
    }
#endif
}

void QtAgentNetwork::read(const QByteArray &message)
{
    const auto object = QJsonDocument::fromJson(message).object();
    if (object.contains(QStringLiteral("id"))) {
        if (const auto answered = m_waiting.take(object.value(QStringLiteral("id")).toInt())) {
            answered(object);
        }
        return;
    }
    if (object.value(QStringLiteral("method")).toString() == u"Target.attachedToTarget") {
        const auto params = object.value(QStringLiteral("params")).toObject();
        const auto type = params.value(QStringLiteral("targetInfo"))
                              .toObject()
                              .value(QStringLiteral("type"))
                              .toString();
        if (type == u"iframe") {
            listen(params.value(QStringLiteral("sessionId")).toString());
        }
        return;
    }
    for (const auto &report : m_events.read(object)) {
        emit requestReported(report);
    }
}

void QtAgentNetwork::fetchBody(
    int ticket, const QVariantMap &body, const QString &part, qint64 limit, qint64 size)
{
    const auto requestId = body.value(QStringLiteral("requestId")).toString();
    if (requestId.isEmpty()) {
        emit bodyFetched(ticket,
            refusal(QStringLiteral("not-found"),
                QStringLiteral("The engine keeps no body for this request: it was a redirect or a "
                               "WebSocket.")));
        return;
    }
    if (!attached()) {
        emit bodyFetched(ticket,
            refusal(QStringLiteral("not-found"),
                QStringLiteral("The page's session has ended, and its bodies with it.")));
        return;
    }
    send(part == u"sent" ? QStringLiteral("Network.getRequestPostData")
                         : QStringLiteral("Network.getResponseBody"),
        {{QStringLiteral("requestId"), requestId}},
        body.value(QStringLiteral("sessionId")).toString(),
        [this, ticket, part, limit, size](const QJsonObject &answer) {
            emit bodyFetched(ticket, QtNetworkEvents::bodyAnswer(answer, part, limit, size));
        });
}

void QtAgentNetwork::evaluate(int ticket, const QString &expression)
{
    attachIfReady();
    if (!attached()) {
        emit evaluated(ticket,
            refusal(QStringLiteral("failed"),
                QStringLiteral("The page's session is not open, so its own world cannot be "
                               "reached.")));
        return;
    }
    // Without a context, the main frame's main world. An expression that
    // never settles is answered by the page's own deadline.
    send(QStringLiteral("Runtime.evaluate"),
        {{QStringLiteral("expression"), QtNetworkEvents::evaluationSource(expression)},
            {QStringLiteral("returnByValue"), true}, {QStringLiteral("awaitPromise"), true}},
        {}, [this, ticket](const QJsonObject &answer) {
            emit evaluated(ticket, QtNetworkEvents::evaluationAnswer(answer));
        });
}

void registerQtAgentNetwork()
{
    qmlRegisterType<QtAgentNetwork>("Omaweb.Engine", 1, 0, "QtAgentNetwork");
}

} // namespace omaweb
