// PROTOTYPE (#440). The landing page's words, shared by every variant so the
// variants disagree about structure and not about copy. Lifted from
// index.html and led by the Omnibar, as #440 asks; "command panel" is gone.

window.PV_CONTENT = {
  hero: {
    kicker: "Web navigation system",
    title: ["A browser", "for driving", "the web."],
    lede: ["Keyboard first. Private. Open.", "For people who know where they’re going."],
    cta: "Install Omaweb",
    platform: ["Made for Linux.", "Free and open source."],
  },

  // The walk: a real sequence, from the first new tab to a page of your own.
  steps: [
    {
      id: "start",
      label: "Start page",
      key: "Ctrl+T",
      title: "Every tab starts on the road.",
      body: "A new tab opens on the Start page: the Omnibar resting on the horizon of a night drive, lit in your theme’s accent. Nothing loads until you say where to.",
      shot: "start",
      alt: "The Start page: the Omnibar resting on the horizon of the night road",
    },
    {
      id: "omnibar",
      label: "Omnibar",
      key: "o",
      title: "One field for everything.",
      body: "Type a few letters and it finds the page: a web search with your engine’s suggestions, an open tab in this Space or another, a visit from history. The same field runs every browser command.",
      shot: "omnibar",
      alt: "The Omnibar listing a search, open tabs in two Spaces, history and engine suggestions",
    },
    {
      id: "spaces",
      label: "Spaces",
      key: "J K",
      title: "A sidebar for every Space.",
      body: "The sidebar lists a Space’s tabs: pinned ones at the top, the rest below. Each Space has its own logins, history and tabs, and its own colour in the footer.",
      shot: "space",
      alt: "Omaweb with its sidebar of Spaces and tabs beside a photo essay page",
    },
    {
      id: "agents",
      label: "Agents",
      key: "",
      title: "Room for an Agent.",
      body: "Hand a task to an Agent and it works in a Space of its own. Its mark sits in the footer beside your Spaces, and moves only while it is working.",
      shot: "agents",
      alt: "The sidebar footer with the reader's Spaces and an Agent's mark",
    },
    {
      id: "collapsed",
      label: "No sidebar",
      key: "Ctrl+B",
      title: "The page gets the whole window.",
      body: "Hide the sidebar and nothing is left but the page. J and K still move between tabs, and Ctrl+B brings the sidebar back.",
      shot: "collapsed",
      alt: "Omaweb with the sidebar hidden and the page filling the window",
    },
    {
      id: "blocking",
      label: "Content blocking",
      key: "",
      title: "Blocking, built in.",
      body: "EasyList and EasyPrivacy are on from the first run, with cosmetic rules and a count of what was blocked. Add lists, write your own rules, or turn blocking off for one site.",
      shot: "blocking",
      alt: "The Content Blocking settings, with filter lists and user rules",
    },
  ],

  features: [
    [
      "Omnibar",
      "One field for addresses, searches, open tabs in every Space, history and commands.",
    ],
    ["Keyboard first", "Link hints and Vim-style keys. The mouse is optional."],
    ["Spaces", "Work and personal in one window, each with its own logins, history and tabs."],
    [
      "For pros",
      "Split view, docked DevTools, an Agent’s own Space, and a Space for every client.",
    ],
    [
      "Private by default",
      "Content blocking built in. No account to make and no telemetry to turn off.",
    ],
  ],

  ticker: [
    "Omnibar",
    "Link hints",
    "Spaces",
    "Agents",
    "Split view",
    "History",
    "Content blocking",
    "And beyond",
  ],

  keys: [
    ["o", "Omnibar"],
    ["J", "Next tab"],
    ["K", "Previous tab"],
    ["f", "Links"],
    ["/", "Find"],
    ["?", "Shortcuts"],
  ],

  install: {
    title: ["Get on", "the road."],
    body: "Omaweb ships from its own pacman repository, so it installs and upgrades with the rest of your system. Two steps, once.",
    repo: "[omaweb]\nSigLevel = Required DatabaseRequired\nServer = https://github.com/villekivela/omaweb/releases/download/repo-$arch",
    commands: [
      "curl -fsSLO https://raw.githubusercontent.com/villekivela/omaweb/main/security/repo-signing-key.asc",
      "sudo pacman-key --add repo-signing-key.asc",
      "sudo pacman-key --lsign-key FDA535B2185755EA718BEA585DBF15FE484EFA64",
      "sudo pacman -Syu omaweb",
    ],
    after: "From then on, sudo pacman -Syu keeps it current.",
  },

  // The themes the page offers, as its swatches name them. "omaweb" is the
  // site's own palette, shown with Matte Black's captures.
  themes: [
    ["omaweb", "Omaweb"],
    ["matte-black", "Matte Black"],
    ["ristretto", "Ristretto"],
    ["retro-82", "Retro 82"],
    ["gruvbox", "Gruvbox"],
    ["everforest", "Everforest"],
    ["catppuccin", "Catppuccin"],
    ["tokyo-night", "Tokyo Night"],
    ["nord", "Nord"],
    ["hackerman", "Hackerman"],
  ],
};
