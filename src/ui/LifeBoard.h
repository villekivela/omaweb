#pragma once

#include <QColor>
#include <QQuickItem>
#include <QStringList>
#include <QVariantList>

#include <vector>

namespace omaweb {

// The board the Game of Life Scene (GameOfLife.qml) evolves and draws: a
// cell to each of the Scene's pixels, `columns` by `rows`, wrapping at its
// edges, and all dead when it is given a new size. A generation is Conway's:
// a live cell with two or three live neighbours lives on, a dead cell with
// three comes alive, and every other cell is dead.
//
// The Scene decides when the board moves and what goes on it. The board
// keeps the rules, which run in C++ because a page area's board, some 70,000
// cells, moves thirty generations a second on commit.
class LifeBoard : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(int columns READ columns WRITE setColumns NOTIFY sizeChanged)
    Q_PROPERTY(int rows READ rows WRITE setRows NOTIFY sizeChanged)
    // How many generations a cell that has died is drawn as a trail.
    Q_PROPERTY(int trail READ trail WRITE setTrail NOTIFY trailChanged)
    // What it draws: a dead cell in `ground`, a live one in `live`, and a
    // trail in `trailColours`, the first for a cell that has just died.
    Q_PROPERTY(QColor ground READ ground WRITE setGround NOTIFY coloursChanged)
    Q_PROPERTY(QColor live READ live WRITE setLive NOTIFY coloursChanged)
    Q_PROPERTY(
        QVariantList trailColours READ trailColours WRITE setTrailColours NOTIFY coloursChanged)
    // How many cells are alive, and a hash of which, the same for the same
    // live cells.
    Q_PROPERTY(int population READ population NOTIFY cellsChanged)
    Q_PROPERTY(double hash READ hash NOTIFY cellsChanged)

public:
    explicit LifeBoard(QQuickItem *parent = nullptr);

    int columns() const { return m_columns; }
    int rows() const { return m_rows; }
    void setColumns(int columns);
    void setRows(int rows);
    int trail() const { return m_trail; }
    void setTrail(int trail);
    QColor ground() const { return m_ground; }
    void setGround(const QColor &ground);
    QColor live() const { return m_live; }
    void setLive(const QColor &live);
    QVariantList trailColours() const { return m_trailColours; }
    void setTrailColours(const QVariantList &colours);
    int population() const { return m_population; }
    double hash() const { return m_hash; }

    // Every cell dead, with no trail.
    Q_INVOKABLE void clear();
    // Sets the live cells of `pattern`, rows of `O` for a live cell and any
    // other character for a dead one, with its top left corner at `x`, `y`,
    // mirrored left to right with `flipX` and top to bottom with `flipY`. A
    // pattern over an edge wraps.
    Q_INVOKABLE void place(
        const QStringList &pattern, int x, int y, bool flipX = false, bool flipY = false);
    // Brings each cell of the `size` square at `x`, `y` alive at random, at
    // `density`, the same cells for the same `seed`.
    Q_INVOKABLE void soup(int x, int y, int size, double density, int seed);
    // Moves the board on a generation.
    Q_INVOKABLE void step();
    // The board as rows of `O` for a live cell, the generations since it died
    // for a cell drawn as a trail, and `.` for any other dead one.
    Q_INVOKABLE QStringList picture() const;

signals:
    void sizeChanged();
    void trailChanged();
    void coloursChanged();
    void cellsChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    void resize(int columns, int rows);
    int index(int x, int y) const;
    void count();
    void counted(int population, quint32 hash);

    int m_columns = 0;
    int m_rows = 0;
    int m_trail = 0;
    QColor m_ground = Qt::black;
    QColor m_live = Qt::white;
    QVariantList m_trailColours;
    int m_population = 0;
    double m_hash = 0;
    std::vector<quint8> m_cells;
    std::vector<quint8> m_next;
    // Generations since each cell died while it is drawn as a trail, and 0
    // for a live cell or one whose trail has gone.
    std::vector<quint8> m_faded;
};

} // namespace omaweb
