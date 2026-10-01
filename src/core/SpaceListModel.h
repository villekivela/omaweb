#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVector>

namespace omaweb {

struct SpaceState {
    QString id;
    QString name;
    // A palette name from spaceColourNames(), which the theme resolves: the
    // Space is drawn in the theme's own value for it and follows a theme
    // change. A store from before Spaces had one holds a hex value here until
    // the controller replaces it at start.
    QString color;
    bool active = false;
};

// The palette names a Space may be drawn in, in the order a new Space is
// offered them. Red, magenta and cyan are not among them: they say urgent,
// Private and Agent.
const QStringList &spaceColourNames();
bool isSpaceColourName(const QString &name);
// The name fewest of these Spaces use, the earliest in spaceColourNames() on a
// tie. A Space that holds no palette name counts towards none.
QString leastUsedSpaceColour(const QVector<SpaceState> &spaces);

class SpaceListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ColorRole,
        ActiveRole,
    };
    Q_ENUM(Role)

    explicit SpaceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    const QVector<SpaceState> &items() const;
    void reset(QVector<SpaceState> spaces);
    // Where one Space sits in the list, or -1 for an id the list does not
    // hold. Asked by a command that has to work out where a Space is going
    // before it can ask for the move.
    qsizetype rowOf(const QString &id) const;
    // The order Spaces are listed in is the reader's, so a move reports itself
    // as a move rather than as a new list: a sidebar and a Settings row that
    // are watching this model keep their delegates and their focus across it.
    bool move(const QString &id, qsizetype destinationRow);

private:
    QVector<SpaceState> m_spaces;
};

} // namespace omaweb
