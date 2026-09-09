#include "SessionFixture.h"

#include "SpaceListModel.h"
#include "TabListModel.h"

#include <QDirIterator>
#include <QTest>

#include <limits>
#include <utility>

using omaweb::SpaceListModel;
using omaweb::TabListModel;
using omaweb::test::SessionFixture;
using omaweb::test::SessionSpec;
using omaweb::test::SpaceSpec;
using omaweb::test::TabSpec;

Q_DECLARE_METATYPE(SessionSpec)

namespace {

SessionSpec validSession()
{
    return SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                .id = QStringLiteral("tab-one"),
                .url = QUrl(QStringLiteral("https://one.example")),
            }},
        }},
    };
}

} // namespace

class SessionFixtureTest final : public QObject {
    Q_OBJECT

private slots:
    void restoresDeclaredStateWithDomainDefaults();
    void restoresOrderedSpacesSelectionsAndSpaceAtRest();
    void restoresRecentClosesAndKeepsControllerChanges();
    void rejectsInvalidDeclarations();
    void rejectsInvalidDeclarations_data();
};

void SessionFixtureTest::restoresDeclaredStateWithDomainDefaults()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                .id = QStringLiteral("tab-one"),
                .url = QUrl(QStringLiteral("https://one.example/path")),
            }},
        }},
    });
    QVERIFY_SESSION_READY(fixture);

    const auto controller = fixture.createController();
    QVERIFY(controller);
    QVERIFY(controller->ready());
    QCOMPARE(controller->activeSpaceId(), QStringLiteral("personal"));
    QCOMPARE(controller->activeTabId(), QStringLiteral("tab-one"));
    QCOMPARE(controller->activeUrl(), QUrl(QStringLiteral("https://one.example/path")));
    QCOMPARE(controller->activeTitle(), QStringLiteral("https://one.example/path"));

    const auto space = controller->spaces()->index(0, 0);
    QCOMPARE(controller->spaces()->data(space, SpaceListModel::ColorRole).toString(),
        QStringLiteral("#7c6cff"));

    const auto tab = controller->tabs()->index(0, 0);
    QVERIFY(!controller->tabs()->data(tab, TabListModel::PinnedRole).toBool());
    QVERIFY(!controller->tabs()->data(tab, TabListModel::KeepActiveRole).toBool());
    QVERIFY(!controller->tabs()->data(tab, TabListModel::MutedRole).toBool());
    QCOMPARE(controller->tabs()->data(tab, TabListModel::ZoomRole).toDouble(), 1.0);
}

void SessionFixtureTest::restoresOrderedSpacesSelectionsAndSpaceAtRest()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {
            SpaceSpec {
                .id = QStringLiteral("personal"),
                .name = QStringLiteral("Personal"),
                .tabs = {
                    TabSpec {
                        .id = QStringLiteral("kept"),
                        .url = QUrl(QStringLiteral("https://kept.example")),
                        .pinned = true,
                        .keepActive = true,
                    },
                    TabSpec {
                        .id = QStringLiteral("resting"),
                    },
                },
                .activeTabId = QStringLiteral("resting"),
            },
            SpaceSpec {
                .id = QStringLiteral("work"),
                .name = QStringLiteral("Work"),
                .color = QStringLiteral("#123456"),
                .tabs = {
                    TabSpec {
                        .id = QStringLiteral("work-one"),
                        .url = QUrl(QStringLiteral("https://one.work.example")),
                    },
                    TabSpec {
                        .id = QStringLiteral("work-two"),
                        .url = QUrl(QStringLiteral("https://two.work.example")),
                        .title = QStringLiteral("Two"),
                        .zoom = 1.5,
                        .muted = true,
                    },
                },
                .activeTabId = QStringLiteral("work-two"),
            },
        },
        .activeSpaceId = QStringLiteral("work"),
    });
    QVERIFY_SESSION_READY(fixture);

    const auto controller = fixture.createController();
    QCOMPARE(controller->spaces()->rowCount(), 2);
    QCOMPARE(controller->spaces()->data(controller->spaces()->index(0, 0), SpaceListModel::IdRole),
        QStringLiteral("personal"));
    QCOMPARE(controller->spaces()->data(controller->spaces()->index(1, 0), SpaceListModel::IdRole),
        QStringLiteral("work"));
    QCOMPARE(
        controller->spaces()->data(controller->spaces()->index(1, 0), SpaceListModel::ColorRole),
        QStringLiteral("#123456"));
    QCOMPARE(controller->activeSpaceId(), QStringLiteral("work"));
    QCOMPARE(controller->activeTabId(), QStringLiteral("work-two"));
    QCOMPARE(controller->activeTitle(), QStringLiteral("Two"));
    QCOMPARE(controller->activeTabZoom(), 1.5);

    QVERIFY(controller->switchSpace(QStringLiteral("personal")));
    QVERIFY(controller->atRest());
    QCOMPARE(controller->activeTabId(), QStringLiteral("resting"));
    QCOMPARE(controller->activeUrl(), QUrl(QStringLiteral("about:blank")));
    QCOMPARE(controller->activeTitle(), QStringLiteral("New tab"));
    QCOMPARE(controller->tabs()->rowCount(), 2);
    QCOMPARE(controller->tabs()->data(controller->tabs()->index(0, 0), TabListModel::IdRole),
        QStringLiteral("kept"));
    QCOMPARE(controller->tabs()->data(controller->tabs()->index(1, 0), TabListModel::IdRole),
        QStringLiteral("resting"));
}

void SessionFixtureTest::rejectsInvalidDeclarations_data()
{
    QTest::addColumn<SessionSpec>("spec");
    QTest::addColumn<QString>("error");

    QTest::newRow("no Spaces") << SessionSpec {}
                               << QStringLiteral("spaces: at least one Space is required");

    auto spec = validSession();
    spec.spaces[0].id.clear();
    QTest::newRow("empty Space id") << spec << QStringLiteral("spaces[0].id: id is required");

    spec = validSession();
    spec.spaces[0].name = QStringLiteral("  ");
    QTest::newRow("empty Space name") << spec << QStringLiteral("spaces[0].name: name is required");

    spec = validSession();
    auto duplicateSpace = spec.spaces.constFirst();
    duplicateSpace.tabs[0].id = QStringLiteral("tab-two");
    spec.spaces.append(duplicateSpace);
    spec.activeSpaceId = QStringLiteral("personal");
    QTest::newRow("duplicate Space id")
        << spec << QStringLiteral("spaces[1].id: duplicate Space id 'personal'");

    spec = validSession();
    spec.spaces[0].tabs.clear();
    QTest::newRow("no open tabs") << spec
                                  << QStringLiteral(
                                         "spaces[0].tabs: at least one open tab is required");

    spec = validSession();
    spec.spaces[0].tabs[0].id.clear();
    QTest::newRow("empty tab id") << spec << QStringLiteral("spaces[0].tabs[0].id: id is required");

    spec = validSession();
    auto secondSpace = spec.spaces.constFirst();
    secondSpace.id = QStringLiteral("work");
    secondSpace.name = QStringLiteral("Work");
    spec.spaces.append(secondSpace);
    spec.activeSpaceId = QStringLiteral("personal");
    QTest::newRow("duplicate open-tab id")
        << spec << QStringLiteral("spaces[1].tabs[0].id: duplicate open-tab id 'tab-one'");

    spec = validSession();
    SpaceSpec work {
        .id = QStringLiteral("work"),
        .name = QStringLiteral("Work"),
        .tabs = {TabSpec {
            .id = QStringLiteral("work-tab"),
            .url = QUrl(QStringLiteral("https://work.example")),
        }},
    };
    spec.spaces.append(work);
    QTest::newRow("ambiguous active Space")
        << spec << QStringLiteral("activeSpaceId: required when several Spaces are declared");

    spec = validSession();
    spec.activeSpaceId = QStringLiteral("missing");
    QTest::newRow("unknown active Space")
        << spec << QStringLiteral("activeSpaceId: unknown Space id 'missing'");

    spec = validSession();
    spec.spaces[0].tabs.append(TabSpec {
        .id = QStringLiteral("tab-two"),
        .url = QUrl(QStringLiteral("https://two.example")),
    });
    QTest::newRow("ambiguous active tab")
        << spec << QStringLiteral("spaces[0].activeTabId: required when several tabs are declared");

    spec = validSession();
    spec.spaces[0].activeTabId = QStringLiteral("missing");
    QTest::newRow("unknown active tab")
        << spec << QStringLiteral("spaces[0].activeTabId: unknown tab id 'missing'");

    spec = validSession();
    spec.spaces[0].tabs[0].keepActive = true;
    QTest::newRow("ordinary tab kept active")
        << spec << QStringLiteral("spaces[0].tabs[0].keepActive: requires a Pinned tab");

    spec = validSession();
    spec.spaces[0].tabs[0].zoom = 0.0;
    QTest::newRow("invalid tab zoom")
        << spec << QStringLiteral("spaces[0].tabs[0].zoom: must be positive and finite");

    spec = validSession();
    spec.spaces[0].tabs[0].zoom = std::numeric_limits<double>::infinity();
    QTest::newRow("non-finite tab zoom")
        << spec << QStringLiteral("spaces[0].tabs[0].zoom: must be positive and finite");

    spec = validSession();
    spec.spaces[0].tabs.prepend(TabSpec {
        .id = QStringLiteral("ordinary"),
        .url = QUrl(QStringLiteral("https://ordinary.example")),
    });
    spec.spaces[0].tabs[1].pinned = true;
    spec.spaces[0].activeTabId = QStringLiteral("ordinary");
    QTest::newRow("Pinned tab after ordinary tab")
        << spec << QStringLiteral("spaces[0].tabs[1].pinned: Pinned tabs must come first");

    spec = validSession();
    spec.spaces[0].tabs[0].pinned = true;
    spec.spaces[0].tabs[0].url = QUrl(QStringLiteral("about:blank"));
    QTest::newRow("Pinned blank tab")
        << spec << QStringLiteral("spaces[0].tabs[0].url: a Pinned tab cannot be blank");

    spec = validSession();
    spec.spaces[0].tabs.append(TabSpec {
        .id = QStringLiteral("blank"),
    });
    spec.spaces[0].activeTabId = QStringLiteral("blank");
    QTest::newRow("blank beside ordinary tab")
        << spec << QStringLiteral("spaces[0].tabs: a blank tab must be the only ordinary tab");

    spec = validSession();
    spec.spaces[0].recentCloses = {TabSpec {}};
    QTest::newRow("blank recent close")
        << spec << QStringLiteral("spaces[0].recentCloses[0].url: address is required");

    spec = validSession();
    spec.spaces[0].recentCloses = {TabSpec {
        .url = QUrl(QStringLiteral("https://closed.example")),
        .keepActive = true,
    }};
    QTest::newRow("ordinary recent close kept active")
        << spec << QStringLiteral("spaces[0].recentCloses[0].keepActive: requires a Pinned tab");

    spec = validSession();
    spec.spaces[0].recentCloses = {TabSpec {
        .id = QStringLiteral("closed-tab"),
        .url = QUrl(QStringLiteral("https://closed.example")),
    }};
    QTest::newRow("recent close id")
        << spec
        << QStringLiteral("spaces[0].recentCloses[0].id: recent closes do not declare an id");
}

void SessionFixtureTest::rejectsInvalidDeclarations()
{
    QFETCH(SessionSpec, spec);
    QFETCH(QString, error);

    SessionFixture fixture(std::move(spec));
    QVERIFY(!fixture.ready());
    QCOMPARE(fixture.errorMessage(), error);
    QDirIterator entries(fixture.dataRoot(), QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    QVERIFY(!entries.hasNext());
}

void SessionFixtureTest::restoresRecentClosesAndKeepsControllerChanges()
{
    SessionFixture fixture(SessionSpec {
        .spaces = {SpaceSpec {
            .id = QStringLiteral("personal"),
            .name = QStringLiteral("Personal"),
            .tabs = {TabSpec {
                .id = QStringLiteral("anchor"),
                .url = QUrl(QStringLiteral("https://anchor.example")),
            }},
            .recentCloses = {
                TabSpec {
                    .url = QUrl(QStringLiteral("https://two.example")),
                    .pinned = true,
                    .keepActive = true,
                    .zoom = 1.25,
                    .muted = true,
                },
                TabSpec {
                    .url = QUrl(QStringLiteral("https://one.example")),
                },
            },
        }},
    });
    QVERIFY_SESSION_READY(fixture);

    QString reopenedId;
    {
        const auto controller = fixture.createController();
        QCOMPARE(controller->closedTabCount(), 2);
        controller->reopenClosedTab();
        QCOMPARE(controller->activeUrl(), QUrl(QStringLiteral("https://two.example")));
        QVERIFY(controller->activeTabPinned());
        QVERIFY(controller->activeTabKeepActive());
        QCOMPARE(controller->activeTabZoom(), 1.25);
        const auto active = controller->tabs()->index(0, 0);
        QVERIFY(controller->tabs()->data(active, TabListModel::MutedRole).toBool());
        reopenedId = controller->activeTabId();
        QVERIFY(reopenedId != QStringLiteral("anchor"));
    }

    const auto restored = fixture.createController();
    QCOMPARE(restored->activeTabId(), reopenedId);
    QCOMPARE(restored->closedTabCount(), 1);
    restored->reopenClosedTab();
    QCOMPARE(restored->activeUrl(), QUrl(QStringLiteral("https://one.example")));
}

QTEST_MAIN(SessionFixtureTest)
#include "tst_sessionfixture.moc"
