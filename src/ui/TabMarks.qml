import QtQuick

// The sidebar rows the reader has marked (#917): ordinary rows of the Space on
// show, never saved or synced. It holds tab ids and nothing about rows, because
// the list builds its rows again whenever it moves them. A row is a group of
// ids: one for a tab, two for a split, whose row counts as one and acts on both
// its tabs. The ids run in the order they were marked, so the last is the row
// a range starts from.
QtObject {
    id: root

    property var ids: []
    readonly property int count: ids.length
    readonly property string last: ids.length > 0 ? ids[ids.length - 1] : ""

    function has(tabId) {
        return root.ids.indexOf(tabId) >= 0;
    }

    function add(group) {
        const kept = root.ids.filter(function (id) {
            return group.indexOf(id) < 0;
        });
        root.ids = kept.concat(group);
    }

    function remove(group) {
        root.ids = root.ids.filter(function (id) {
            return group.indexOf(id) < 0;
        });
    }

    function toggle(group) {
        if (root.has(group[0]))
            root.remove(group);
        else
            root.add(group);
    }

    // Marks the rows between the last marked row and `group`, both ends
    // included. With nothing marked the range starts from the tab on show.
    // `rows` is every markable row in the order the sidebar lists them, each a
    // group of ids.
    function extendTo(group, rows, shown) {
        const from = root.count > 0 ? root.last : shown;
        const indexOf = function (tabId) {
            return rows.findIndex(function (row) {
                return row.indexOf(tabId) >= 0;
            });
        };
        const start = indexOf(from);
        const end = indexOf(group[0]);
        if (start < 0 || end < 0) {
            root.add(group);
            return;
        }
        const step = end >= start ? 1 : -1;
        for (let at = start; at !== end + step; at += step)
            root.add(rows[at]);
    }

    function clear() {
        if (root.ids.length > 0)
            root.ids = [];
    }

    // Drops what is no longer a markable row, as a tab closing or being pinned
    // or moved away leaves one.
    function keepOnly(tabIds) {
        const kept = root.ids.filter(function (id) {
            return tabIds.indexOf(id) >= 0;
        });
        if (kept.length !== root.ids.length)
            root.ids = kept;
    }

    // The order of a section after a marked row is dragged from `from` to
    // `destination`, both places in `order`: the rows in `moving` close up in
    // the order they had at the gap the dragged row would have taken.
    function dropOrder(order, moving, from, destination) {
        const staying = function (id) {
            return moving.indexOf(id) < 0;
        };
        const rest = order.filter(staying);
        const through = destination > from ? destination + 1 : destination;
        const before = order.slice(0, through).filter(staying).length;
        const group = order.filter(function (id) {
            return !staying(id);
        });
        return rest.slice(0, before).concat(group, rest.slice(before));
    }
}
