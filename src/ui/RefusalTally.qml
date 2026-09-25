import QtQuick

// What Content blocking refused for one page address, for whatever is showing
// that address.
//
// The tally is keyed by page address and Space (ADR 0037), so it is asked for
// rather than handed over, and asking is a call rather than a property to bind
// to. Reading the generation beside the call is what puts the question again
// whenever a tally moves. Three places state this number — Site information,
// Settings and the Space outline — and the generation read is the part that
// would go wrong quietly if each of them wrote it out.
QtObject {
    id: root

    // Content blocking. Null in a window that runs no engine, where nothing
    // has been refused because nothing has been requested.
    required property var blocker
    // The core, which answers for the Space this session's state keys on.
    required property var browser
    property url pageAddress

    readonly property int count: root.blocker && root.browser
                                 && root.blocker.refusalTallyGeneration >= 0
                                 ? root.blocker.refusalTally(root.browser.sessionSpaceId,
                                                             root.pageAddress) : 0
    // Which requests those were, each address once: `address`, and
    // `canonicalName` for one refused through its host's CNAME chain.
    readonly property var requests: root.blocker && root.browser
                                    && root.blocker.refusalTallyGeneration >= 0
                                    ? root.blocker.refusedRequests(root.browser.sessionSpaceId,
                                                                   root.pageAddress) : []
    // The tally as the chrome says it, in one place so that every surface
    // counts one refusal in the singular.
    readonly property string sentence: root.count + (root.count === 1 ? " request" : " requests")
                                       + " blocked on this page"
}
