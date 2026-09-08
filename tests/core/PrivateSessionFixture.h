#pragma once

#include "BrowserController.h"
#include "PrivateSessionStore.h"
#include "SessionSiteState.h"

#include <QHash>
#include <QSharedPointer>
#include <QString>

#include <memory>
#include <utility>

namespace omaweb::test {

class PrivateSessionFixture final {
public:
    explicit PrivateSessionFixture(QString configRoot = {})
        : m_configRoot(std::move(configRoot))
        , m_permissionDecisions(QSharedPointer<QHash<QString, int>>::create())
        , m_store(std::make_shared<PrivateSessionStore>(m_permissionDecisions))
        , m_siteState(QSharedPointer<SessionSiteState>::create())
    {
    }

    std::unique_ptr<BrowserController> createController() const
    {
        return std::make_unique<BrowserController>(
            m_store, true, m_permissionDecisions, m_siteState, m_configRoot);
    }

private:
    QString m_configRoot;
    QSharedPointer<QHash<QString, int>> m_permissionDecisions;
    std::shared_ptr<PrivateSessionStore> m_store;
    QSharedPointer<SessionSiteState> m_siteState;
};

} // namespace omaweb::test
