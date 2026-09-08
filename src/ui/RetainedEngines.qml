import QtQuick
import Omaweb

// The pages the reader is answered for while their Space is away, and what they
// cost.
//
// A Space that is away keeps its pages frozen, so existing is not what makes a
// page interesting here. These are the named ones: the tabs the core retains,
// which are started even in a Space that has never been visited, are listed
// with what they hold, and lose their engine when the core stops retaining
// them. They freeze like every other page the reader cannot see; what retention
// decides is that they are answered for. It is kept apart from the engine of the
// tab on show because the two are asked different questions — one is about the
// page the reader is looking at, the other about pages they cannot see.
//
// The engines themselves belong to the host, which is the only thing that can
// build and destroy them; this object decides which of them should exist.
QtObject {
    id: root

    required property var host
    required property var browser
    required property var spaceProfiles
    // The Space on show. A retained tab of that Space is simply one of its
    // tabs again, so it stops being this object's business.
    required property string visibleSpaceId

    // The Space each retained tab belongs to, by tab id.
    readonly property var tabs: ({})

    function keeps(tabId) {
        return root.tabs[tabId] !== undefined;
    }

    // A tab of a Space being put away that keeps its engine anyway: hidden,
    // still running, and still identified.
    function keep(tabId, spaceId) {
        root.tabs[tabId] = spaceId;
    }

    function forget(tabId) {
        delete root.tabs[tabId];
    }

    // The Space on show has its pages back, so nothing in it is being retained
    // for it any more.
    function releaseVisibleSpace() {
        for (const tabId in root.tabs) {
            if (root.tabs[tabId] === root.visibleSpaceId)
                delete root.tabs[tabId];
        }
    }

    // A Pinned tab marked Keep active is running before its Space has ever been
    // selected, so a restart — or a session that outlived a crash — has to
    // start it rather than wait for the Space to be visited. A tab the core no
    // longer retains has no reason to keep a renderer, and loses it here.
    function reconcile() {
        if (!root.browser)
            return;
        const wanted = root.browser.retainedTabs;
        for (let index = 0; index < wanted.length; ++index) {
            const kept = wanted[index];
            if (root.host.engines[kept.tabId])
                continue;
            if (root.host.blankAddress(kept.url))
                continue;
            const profile = root.spaceProfiles.hostFor(kept.spaceId);
            if (!profile)
                continue;
            const engine = root.host.createEngine(kept.tabId, kept.url, kept.spaceId, root.browser.prepareProfileForSpace(
                                                      kept.spaceId), profile.profile);
            if (!engine)
                continue;
            root.host.setEngineVisible(kept.tabId, false);
            engine.audioMuted = kept.muted === true;
            engine.setZoomFactor(kept.zoom !== undefined ? kept.zoom : 1.0);
            root.tabs[kept.tabId] = kept.spaceId;
        }
        for (const tabId in root.tabs) {
            if (root.tabs[tabId] === root.visibleSpaceId)
                continue;
            let stillWanted = false;
            for (let index = 0; index < wanted.length; ++index)
                stillWanted = stillWanted || wanted[index].tabId === tabId;
            if (stillWanted)
                continue;
            delete root.tabs[tabId];
            root.host.discardEngine(tabId);
        }
    }

    // Naming the retained tabs is not enough: each is a renderer process the
    // reader cannot see, so what it holds is asked of the operating system
    // rather than estimated.
    function report() {
        const report = [];
        if (!root.browser)
            return report;
        const wanted = root.browser.retainedTabs;
        for (let index = 0; index < wanted.length; ++index) {
            const kept = wanted[index];
            const engine = root.host.engines[kept.tabId];
            report.push({
                            "tabId": kept.tabId,
                            "spaceId": kept.spaceId,
                            "spaceName": kept.spaceName,
                            "title": kept.title,
                            "url": kept.url,
                            "inspected": kept.inspected === true,
                            "running": engine !== undefined && engine !== null,
                            "residentBytes": engine ? ProcessResources.residentBytes(
                                                          engine.renderProcessPid) : 0
                        });
        }
        return report;
    }
}
