#pragma once

namespace omaweb {

// What a kind of window may do, stated once instead of at each place that
// would otherwise ask whether this window is private. A window is built with
// the table its kind answers from, and every refusal asks that table by name.
//
// A Private window keeps nothing, so there is nothing for a Space to hold, a
// Pinned tab to survive in, a search to read, or a clear command to remove
// (ADR 0012). Whether a window is private is a separate fact and stays where it
// is: this says what the window is entitled to do, not which kind it is.
//
// A capability is answered for by both kinds of window or by neither: the two
// tables are built through one constructor that takes every answer, so adding
// an answer obliges both to give it. Nothing here obliges a new call site to
// ask, and a name added to the enum with no answer behind it is a compiler
// warning from the switch below rather than an error. What catches either is
// the paired suite in tests/core/tst_browsercontroller.cpp.
class WindowCapabilities {
public:
    enum class Capability {
        // Creating, switching, renaming and deleting a Space, and moving a tab
        // between Spaces.
        Spaces,
        // Pinning a tab, and Keep active, which is a Pinned tab's setting.
        PinnedTabs,
        // The Omnibar's search of what has been visited.
        HistorySearch,
        // Removing what has been kept.
        ClearBrowsingData,
    };

    static constexpr WindowCapabilities mainWindow()
    {
        return WindowCapabilities(true, true, true, true);
    }

    static constexpr WindowCapabilities privateWindow()
    {
        return WindowCapabilities(false, false, false, false);
    }

    constexpr bool allows(Capability capability) const
    {
        switch (capability) {
        case Capability::Spaces:
            return m_spaces;
        case Capability::PinnedTabs:
            return m_pinnedTabs;
        case Capability::HistorySearch:
            return m_historySearch;
        case Capability::ClearBrowsingData:
            return m_clearBrowsingData;
        }
        return false;
    }

private:
    constexpr WindowCapabilities(
        bool spaces, bool pinnedTabs, bool historySearch, bool clearBrowsingData)
        : m_spaces(spaces)
        , m_pinnedTabs(pinnedTabs)
        , m_historySearch(historySearch)
        , m_clearBrowsingData(clearBrowsingData)
    {
    }

    bool m_spaces;
    bool m_pinnedTabs;
    bool m_historySearch;
    bool m_clearBrowsingData;
};

} // namespace omaweb
