#include "LifeBoard.h"

#include <QImage>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

#include <algorithm>

namespace omaweb {

namespace {

    // FNV-1a over the indices of the live cells.
    constexpr quint32 firstHash = 2166136261U;
    constexpr quint32 hashPrime = 16777619U;

} // namespace

LifeBoard::LifeBoard(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void LifeBoard::setColumns(int columns) { resize(columns, m_rows); }

void LifeBoard::setRows(int rows) { resize(m_columns, rows); }

void LifeBoard::setTrail(int trail)
{
    trail = std::clamp(trail, 0, 255);
    if (trail == m_trail) {
        return;
    }
    m_trail = trail;
    for (auto &faded : m_faded) {
        if (faded > m_trail) {
            faded = 0;
        }
    }
    emit trailChanged();
    update();
}

void LifeBoard::setGround(const QColor &ground)
{
    if (ground == m_ground) {
        return;
    }
    m_ground = ground;
    emit coloursChanged();
    update();
}

void LifeBoard::setLive(const QColor &live)
{
    if (live == m_live) {
        return;
    }
    m_live = live;
    emit coloursChanged();
    update();
}

void LifeBoard::setTrailColours(const QVariantList &colours)
{
    if (colours == m_trailColours) {
        return;
    }
    m_trailColours = colours;
    emit coloursChanged();
    update();
}

void LifeBoard::resize(int columns, int rows)
{
    columns = std::max(0, columns);
    rows = std::max(0, rows);
    if (columns == m_columns && rows == m_rows) {
        return;
    }
    m_columns = columns;
    m_rows = rows;
    m_cells.assign(static_cast<size_t>(columns) * rows, 0);
    m_next.assign(m_cells.size(), 0);
    m_faded.assign(m_cells.size(), 0);
    emit sizeChanged();
    count();
}

int LifeBoard::index(int x, int y) const
{
    const auto wrapped = [](int value, int size) { return ((value % size) + size) % size; };
    return wrapped(y, m_rows) * m_columns + wrapped(x, m_columns);
}

void LifeBoard::clear()
{
    std::fill(m_cells.begin(), m_cells.end(), 0);
    std::fill(m_faded.begin(), m_faded.end(), 0);
    count();
}

void LifeBoard::place(const QStringList &pattern, int x, int y, bool flipX, bool flipY)
{
    if (m_cells.empty()) {
        return;
    }
    const auto height = static_cast<int>(pattern.size());
    for (int row = 0; row < height; ++row) {
        const auto &line = pattern.at(row);
        const auto width = static_cast<int>(line.size());
        for (int column = 0; column < width; ++column) {
            if (line.at(column) == QLatin1Char('O')) {
                const int across = flipX ? width - 1 - column : column;
                const int down = flipY ? height - 1 - row : row;
                m_cells[index(x + across, y + down)] = 1;
            }
        }
    }
    count();
}

void LifeBoard::soup(int x, int y, int size, double density, int seed)
{
    if (m_cells.empty()) {
        return;
    }
    // A linear congruential generator: the same seed gives the same soup on
    // every machine, as a test or a screenshot needs.
    auto state = static_cast<quint32>(seed) * 2654435761U + 1U;
    const auto threshold = static_cast<quint32>(std::clamp(density, 0.0, 1.0) * 65535.0);
    for (int row = 0; row < size; ++row) {
        for (int column = 0; column < size; ++column) {
            state = state * 1664525U + 1013904223U;
            if ((state >> 16) < threshold) {
                m_cells[index(x + column, y + row)] = 1;
            }
        }
    }
    count();
}

void LifeBoard::count()
{
    int population = 0;
    quint32 hash = firstHash;
    for (size_t cell = 0; cell < m_cells.size(); ++cell) {
        if (m_cells[cell]) {
            ++population;
            hash = (hash ^ static_cast<quint32>(cell)) * hashPrime;
        }
    }
    counted(population, hash);
}

void LifeBoard::counted(int population, quint32 hash)
{
    m_population = population;
    m_hash = hash;
    emit cellsChanged();
    update();
}

void LifeBoard::step()
{
    if (m_cells.empty()) {
        return;
    }
    const int columns = m_columns;
    const int rows = m_rows;
    const int trail = m_trail;
    const quint8 *cells = m_cells.data();
    quint8 *next = m_next.data();
    quint8 *faded = m_faded.data();
    // Counted as the board is moved on, rather than in a second pass, since
    // the Scene settles a new board for thirty generations at once.
    int population = 0;
    quint32 hash = firstHash;
    for (int y = 0; y < rows; ++y) {
        const int above = ((y + rows - 1) % rows) * columns;
        const int here = y * columns;
        const int below = ((y + 1) % rows) * columns;
        for (int x = 0; x < columns; ++x) {
            const int left = x == 0 ? columns - 1 : x - 1;
            const int right = x == columns - 1 ? 0 : x + 1;
            const int neighbours = cells[above + left] + cells[above + x] + cells[above + right]
                + cells[here + left] + cells[here + right] + cells[below + left] + cells[below + x]
                + cells[below + right];
            const quint8 alive = cells[here + x];
            const quint8 lives = neighbours == 3 || (neighbours == 2 && alive) ? 1 : 0;
            next[here + x] = lives;
            quint8 &since = faded[here + x];
            if (lives) {
                since = 0;
                ++population;
                hash = (hash ^ static_cast<quint32>(here + x)) * hashPrime;
            } else if (alive) {
                since = trail > 0 ? 1 : 0;
            } else if (since > 0) {
                since = since < trail ? since + 1 : 0;
            }
        }
    }
    m_cells.swap(m_next);
    counted(population, hash);
}

QStringList LifeBoard::picture() const
{
    QStringList lines;
    for (int y = 0; y < m_rows; ++y) {
        QString line(m_columns, QLatin1Char('.'));
        for (int x = 0; x < m_columns; ++x) {
            if (m_cells[index(x, y)]) {
                line[x] = QLatin1Char('O');
            } else if (const auto since = m_faded[index(x, y)]; since > 0) {
                line[x] = QString::number(since).at(0);
            }
        }
        lines.append(line);
    }
    return lines;
}

void LifeBoard::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

// The board as a texture, a texel to each cell, which the item stretches
// over itself without smoothing, so each cell is drawn as a sharp square.
QSGNode *LifeBoard::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    if (m_cells.empty() || width() <= 0 || height() <= 0) {
        delete old;
        return nullptr;
    }
    // A colour for each value of `m_faded`, the first unused: a trail past
    // the colours given is drawn as the ground.
    std::vector<QRgb> fades(static_cast<size_t>(m_trail) + 1, m_ground.rgba());
    for (int since = 1; since <= m_trail && since <= m_trailColours.size(); ++since) {
        fades[since] = m_trailColours.at(since - 1).value<QColor>().rgba();
    }
    const QRgb live = m_live.rgba();
    QImage image(m_columns, m_rows, QImage::Format_ARGB32);
    for (int y = 0; y < m_rows; ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        const size_t row = static_cast<size_t>(y) * m_columns;
        for (int x = 0; x < m_columns; ++x) {
            line[x] = m_cells[row + x] ? live : fades[std::min<int>(m_faded[row + x], m_trail)];
        }
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(old);
    if (node == nullptr) {
        node = new QSGSimpleTextureNode;
        node->setOwnsTexture(true);
        node->setFiltering(QSGTexture::Nearest);
    }
    auto *texture = window()->createTextureFromImage(image);
    texture->setFiltering(QSGTexture::Nearest);
    node->setTexture(texture);
    node->setRect(boundingRect());
    return node;
}

} // namespace omaweb
