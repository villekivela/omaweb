#pragma once

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QSet>

namespace omaweb {

class BrowserController;
class ContentBlocker;
class KeyboardNavigation;

struct BrowserStateSelection {
    QSet<QString> preferenceNames;
    bool keybindings = false;
    bool filterSubscriptions = false;
};

struct BrowserStateImage {
    QVector<SpaceState> spaces;
    QHash<QString, QVector<TabState>> tabsBySpace;
    QHash<QString, QString> preferences;
    QByteArray keybindings;
    QJsonArray filterSubscriptions;
    QString activeSpaceId;
    QHash<QString, QString> activeTabIds;
    QString activeTabId;
    bool pristine = false;
};

enum class BrowserStateSection {
    Browser = 0x1,
    Keybindings = 0x2,
    FilterSubscriptions = 0x4,
};
Q_DECLARE_FLAGS(BrowserStateSections, BrowserStateSection)

class BrowserStateExchange : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~BrowserStateExchange() override = default;

    virtual bool eligible() const = 0;
    virtual BrowserStateImage capture(const BrowserStateSelection &selection) const = 0;
    virtual void refresh(BrowserStateSections sections) = 0;

signals:
    void possiblyChanged();
};

class BrowserStateExchangeAdapter final : public BrowserStateExchange {
    Q_OBJECT

public:
    BrowserStateExchangeAdapter(BrowserController *browser, ContentBlocker *blocker,
        KeyboardNavigation *keyboardNavigation, QString dataRoot, QString configRoot,
        QObject *parent = nullptr);

    bool eligible() const override;
    BrowserStateImage capture(const BrowserStateSelection &selection) const override;
    void refresh(BrowserStateSections sections) override;

private:
    BrowserController *m_browser = nullptr;
    ContentBlocker *m_blocker = nullptr;
    KeyboardNavigation *m_keyboardNavigation = nullptr;
    QString m_dataRoot;
    QString m_configRoot;
};

} // namespace omaweb

Q_DECLARE_OPERATORS_FOR_FLAGS(omaweb::BrowserStateSections)
