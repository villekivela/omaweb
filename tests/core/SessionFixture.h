#pragma once

#include "BrowserController.h"

#include <QTemporaryDir>
#include <QString>
#include <QUrl>
#include <QVector>

#include <memory>

namespace omaweb::test {

// Every member carries a default, so a spec names the fields the test is about
// and says nothing about the rest. That is the point of writing a session
// declaratively, and it is also what keeps the compilers quiet: a designated
// initialiser that leaves out a member with no default is one GCC reports as a
// missing initialiser and clang as a missing designated field, and both are
// errors here. A default turns each omission into a stated value.
struct TabSpec {
    QString id {};
    QUrl url {};
    QString title {};
    bool pinned = false;
    bool keepActive = false;
    double zoom = 1.0;
    bool muted = false;
};

struct SpaceSpec {
    QString id {};
    QString name {};
    QString color = QStringLiteral("#7c6cff");
    QVector<TabSpec> tabs {};
    QVector<TabSpec> recentCloses {};
    QString activeTabId {};
};

struct SessionSpec {
    QVector<SpaceSpec> spaces {};
    QString activeSpaceId {};
};

class SessionFixture final {
public:
    explicit SessionFixture(SessionSpec spec, QString configRoot = {});

    bool ready() const;
    QString errorMessage() const;
    QString dataRoot() const;
    std::unique_ptr<BrowserController> createController() const;

private:
    QTemporaryDir m_dataRoot;
    QString m_configRoot;
    QString m_errorMessage;
    bool m_ready = false;
};

} // namespace omaweb::test

#define QVERIFY_SESSION_READY(fixture)                                                             \
    do {                                                                                           \
        const auto &sessionFixture = (fixture);                                                    \
        QVERIFY2(sessionFixture.ready(), qPrintable(sessionFixture.errorMessage()));               \
    } while (false)
