// Writes a page into the site's shell: the head, the header and its nav, the
// footer and the script live once in `shell.html`, and a page is what goes
// between the header and the footer plus the few things named at its top.
// Pure string work, like `render.mjs`, so it is testable without a browser;
// `site.mjs` reads the files and writes the output.
//
// A page under `pages/` opens with a comment naming its title, its
// description and, if one of the nav's entries is the page, which. Nothing
// else about the shell is the page's to set: a page that needs something the
// shell does not offer is a reason to change the shell for every page.

// The nav's entries, in order. `install` is a section of the landing page
// rather than a page, and is never current.
const NAV = [
  ["features", "features/", "Features"],
  ["docs", "docs/", "Docs"],
  ["releases", "releases/", "Releases"],
  ["install", "#install", "Install"],
];

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

/**
 * Splits a page file into what its opening comment names and the markup
 * after it. The comment is `key: value` lines; a page with no such comment
 * is a page with no title, which `renderShell` refuses.
 */
export function parsePage(source) {
  const match = /^<!--\n([\s\S]*?)\n-->\n?/.exec(source);
  const meta = {};
  if (match) {
    for (const line of match[1].split("\n")) {
      const field = /^\s*([a-z]+):\s*(.*)$/.exec(line);
      if (field) meta[field[1]] = field[2].trim();
    }
  }
  return { meta, body: match ? source.slice(match[0].length) : source };
}

/**
 * The nav with the current page marked. `current` names an entry; `mark`
 * is what `aria-current` says of it, `page` unless the caller says
 * otherwise. A release page passes `true`, since there the current page is
 * the release below the entry and the entry only says which part of the
 * site this is.
 */
export function renderNav(root, current, mark = "page") {
  return NAV.map(([name, path, label]) => {
    const here = name === current ? ` aria-current="${mark}"` : "";
    return `<a href="${root}/${path}"${here}>${label}</a>`;
  }).join("");
}

/**
 * One page, filled into the shell. `root` is the way back to the site root
 * from where the page is written, so one shell serves every depth.
 */
export function renderShell(shell, { root, meta, body }) {
  if (!meta.title) throw new Error("a page names its title in its opening comment");
  if (!meta.description) throw new Error(`${meta.title}: a page names its description`);
  const fields = {
    root,
    title: escapeHtml(meta.title),
    description: escapeHtml(meta.description),
    // What a link preview shows, where a shorter line than the search
    // description reads better. The description itself where none is given.
    summary: escapeHtml(meta.summary || meta.description),
    // `article` for a page about one thing, `website` for the landing page,
    // which is about the site.
    type: meta.type || "article",
    nav: renderNav(root, meta.current, meta.mark),
    body: body.replace(/\s+$/, ""),
  };
  return shell.replace(/\{\{(\w+)\}\}/g, (whole, name) => (name in fields ? fields[name] : whole));
}
