# Isolate private browsing by window

Private windows use one shared temporary profile until the last Private window closes. They have
vertical tabs but no Space switcher, pinned tabs, persistent history, or session restoration, and
they use a visibly distinct theme treatment. Closing the final private tab closes its window;
closing the final tab in the main window instead opens a new-tab Omnibar.

Sites may create Auxiliary windows for authentication, payment, and similar flows. An Auxiliary
window inherits its opener's Space or private identity, uses minimal chrome, and closes with the
flow. Other new-window requests become tabs in the current Space.
