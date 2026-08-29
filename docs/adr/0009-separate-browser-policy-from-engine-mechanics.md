# Separate browser policy from engine mechanics

Tanto core owns Spaces and tab identity, session restoration, Site-permission policy, download policy and records, history, content-blocking configuration, keyboard commands, and user-facing errors. Engine adapters own rendering, input delivery, profile mechanics, navigation execution, web storage, process lifecycle, and translation of engine events into the common contract.

Each Space shares its durable browser state across application variants but keeps separate QtWebEngine and Ladybird login state. Tanto does not translate cookies, storage databases, service workers, or credentials between engines. Site permissions belong to an origin within one Space and support allow-once, persistent allow, and block decisions. Private-window permissions expire with the shared private session.
