#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

namespace tanto {

// Everything a tab carries has a value it starts at, so a place that builds
// one can name the fields it knows and leave the rest to the tab. Only the
// address, the title and the pinning are the session's to keep; the artwork,
// what the page is playing and why its renderer stopped belong to the page
// and start empty on the way out of the store.
struct TabState {
    QString id {};
    QString spaceId {};
    QUrl url {};
    QString title {};
    QUrl iconUrl {};
    bool pinned = false;
    bool active = false;
    bool loading = false;
    // What the page is doing, and the standing decision the reader made about
    // it. Only the first is the page's: a tab restored from the store is
    // silent until its page plays something. The muting is the reader's and,
    // like the zoom below, is the session's to keep — they silenced this tab,
    // not one document in it.
    bool audible = false;
    bool muted = false;
    // Whether this tab's sound is being held back until the reader has dealt
    // with its origin themselves. A page may start playing on its own — a
    // silent video costs nobody anything — but a page that starts making
    // sound on its own has not been asked for. This is not the reader's
    // muting: they never asked for it, it is never written to the session, and
    // it goes away the moment they touch the site.
    bool soundSuppressed = false;
    // How large the page is drawn in this tab. Unlike sound, zoom is the
    // session's to keep: a tab restored from the store comes back at the size
    // the reader left it. 1.0 is 100 percent, which is where a new tab starts.
    double zoom = 1.0;
    // Whether this Pinned tab's page keeps running while another Space is
    // active. Only a Pinned tab can carry it, a pin never implies it, and it
    // survives restart with the rest of the tab.
    bool keepActive = false;
    QString rendererFailureReason {};
};

class TabListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        SpaceIdRole,
        UrlRole,
        TitleRole,
        PinnedRole,
        ActiveRole,
        LoadingRole,
        // QML reads these positionally as Qt.UserRole + n, so new roles go on
        // the end. Inserting one renumbers every role after it.
        IconUrlRole,
        AudibleRole,
        MutedRole,
        ZoomRole,
        KeepActiveRole,
        SoundSuppressedRole,
    };
    Q_ENUM(Role)

    explicit TabListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    const QVector<TabState> &items() const;
    const TabState *find(const QString &id) const;
    TabState *find(const QString &id);
    void reset(QVector<TabState> tabs);
    void append(TabState tab);
    void insert(TabState tab, qsizetype row);
    bool remove(const QString &id);
    bool move(const QString &id, qsizetype destinationRow);
    void notifyChanged(const QString &id, const QList<int> &roles);

private:
    QVector<TabState> m_tabs;
};

} // namespace tanto
