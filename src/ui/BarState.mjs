// What the main window tells Omarchy's bar about itself (#906), one JSON
// object a line over the bar socket. The browser lists the Spaces; the window
// adds what only it knows: each Space's colour as the sidebar draws it, the
// Agent tabs, the page on show for the Location, and the Status island's marks.
// A mark that does not apply is null or false, as the island leaves it out.

function hostOf(address) {
  const text = String(address || "");
  const match = /^[a-z][a-z0-9+.-]*:\/\/(?:[^@/?#]*@)?(\[[^\]]*\]|[^:/?#]*)/i.exec(text);
  return match ? match[1].toLowerCase() : "";
}

function percent(fraction) {
  return Math.round(Math.max(0, Math.min(1, Number(fraction) || 0)) * 100);
}

function tabOf(tab, agentActivity) {
  const agent = agentActivity[tab.id];
  return {
    id: tab.id,
    title: tab.title,
    host: hostOf(tab.url),
    icon: String(tab.iconUrl || ""),
    sound: tab.sound || "",
    poppedOut: !!tab.poppedOut,
    agent: !!agent,
    agentAct: agent ? String(agent.act || "") : "",
  };
}

export function build(inputs) {
  const agents = inputs.agentActivity || {};
  const page = inputs.page;
  const downloads = inputs.downloads;
  const release = inputs.release;
  return {
    type: "state",
    version: 1,
    window: inputs.window,
    activeSpace: inputs.activeSpace,
    spaces: inputs.spaces.map((space) => ({
      id: space.id,
      name: space.name,
      colour: String(inputs.colourOf(space.colour)),
      active: space.active,
      count: space.pins.length + space.tabs.length,
      pins: space.pins.map((tab) => tabOf(tab, agents)),
      tabs: space.tabs.map((tab) => tabOf(tab, agents)),
    })),
    location: {
      host: hostOf(page.url),
      title: page.title,
      secure: !!page.secure,
      canBack: !!page.canGoBack,
      canForward: !!page.canGoForward,
    },
    status: {
      downloads: {
        running: downloads.running,
        progress: percent(downloads.fraction),
        files: downloads.files.map((file) => ({
          name: file.fileName,
          progress: percent(file.fraction),
        })),
      },
      release:
        release.announcing && release.release
          ? { version: release.release, instruction: release.instruction }
          : null,
      recording: !!inputs.recording,
    },
  };
}
