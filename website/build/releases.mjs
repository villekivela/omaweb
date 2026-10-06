// The release pages: `releases/index.html`, the newest browser release, and one page per release
// at `releases/<tag>/index.html`, written by `site.mjs` into the deployed site. Pure apart from
// the fetch and the writing, so the markup is testable without a network (`releases.test.mjs`).
//
// The releases are read from GitHub when the site is built, rather than in the reader's browser,
// so the pages stay static, load nothing from another origin, and spend no API rate limit per
// reader. A release body is Markdown written elsewhere and published unattended, so it goes
// through `render.mjs`, which decides what a body may become.
//
// Each page is the landing page with its <main> swapped out: the head, the header, the footer and
// the scripts are read from index.html, so the chrome has one copy. The pages sit one or two
// directories down, so the chrome's relative addresses are rewritten to reach back to the root.

import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";

import { releasePath, renderRelease } from "./render.mjs";

// Every published release, of every kind: the browser's versions, the engine builds and the
// pacman repository. The list is read page by page, since it only grows.
const RELEASES = "https://api.github.com/repos/villekivela/omaweb/releases?per_page=100";

// What a tag says a release is, and the part of its name left once the kind is said by a chip:
// "Engine 6.11.2-2" is 6.11.2-2 under Engine, "Package repository (aarch64)" is aarch64.
const KINDS = [
  { kind: "browser", label: "Browser", test: /^v\d/, short: (name) => name },
  {
    kind: "engine",
    label: "Engine",
    test: /^engine-/,
    short: (name) => name.replace(/^engine\s*/i, ""),
  },
  {
    kind: "repository",
    label: "Repository",
    test: /^repo-/,
    short: (name, tag) => (/\(([^)]+)\)/.exec(name) || [])[1] || tag.replace(/^repo-/, ""),
  },
];
const OTHER = { kind: "other", label: "Other", short: (name) => name };

export function kindOf(release) {
  return KINDS.find((kind) => kind.test.test(release.tag_name)) || OTHER;
}

export async function fetchReleases() {
  const headers = { accept: "application/vnd.github+json", "user-agent": "omaweb-website-build" };
  if (process.env.GITHUB_TOKEN) headers.authorization = `Bearer ${process.env.GITHUB_TOKEN}`;
  const releases = [];
  for (let next = RELEASES; next; ) {
    const response = await fetch(next, { headers });
    if (!response.ok) throw new Error(`GitHub answered ${response.status} ${response.statusText}`);
    releases.push(...(await response.json()));
    next = (/<([^>]+)>;\s*rel="next"/.exec(response.headers.get("link") || "") || [])[1];
  }
  return releases.filter((release) => !release.draft && releasePath(release.tag_name));
}

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

// The landing page, split around its <main>, with every relative address made to reach back to
// the root from `root`, and the nav's Releases entry marked as the part of the site this is.
export function chrome(landing, root, meta) {
  const start = landing.indexOf("<main>");
  const end = landing.indexOf("</main>") + "</main>".length;
  if (start < 0 || end < start) throw new Error("index.html has no <main> to replace");
  const reroot = (html) =>
    html
      .replace(/(href|src)="(?![a-z]+:|\/\/|#)([^"]+)"/g, `$1="${root}/$2"`)
      // Only a link's own fragment goes back to the landing page; an icon's `<use href="#…">`
      // names a symbol in this page and stays as it is.
      .replace(/(<a\b[^>]*?\shref=")#([^"]*)"/g, `$1${root}/#$2"`);
  let before = reroot(landing.slice(0, start));
  const after = reroot(landing.slice(end));
  before = before
    .replace(/<title>[^<]*<\/title>/, `<title>${escapeHtml(meta.title)}</title>`)
    .replace(
      /(<meta\s+name="description"\s+content=")[^"]*(")/,
      `$1${escapeHtml(meta.description)}$2`,
    )
    .replace(/(<meta\s+property="og:title"\s+content=")[^"]*(")/, `$1${escapeHtml(meta.title)}$2`)
    .replace(
      /(<meta\s+property="og:description"\s+content=")[^"]*(")/,
      `$1${escapeHtml(meta.description)}$2`,
    )
    // The landing page's address is the landing page's: a release page has its own.
    .replace(/\n\s*<meta\s+property="og:url"[^>]*>/, "")
    .replace(/(<a href="[^"]*\/releases\/")>/, '$1 aria-current="true">')
    // A release page wears the landing page's theme, and asks Omaweb for the reader's own palette:
    // Omaweb hands a page that asks it as `--omaweb-*`, which themes.css prefers over every theme.
    .replace(
      /(\n\s*)(<meta name="viewport")/,
      '$1<meta name="omaweb-palette" content="follow" />$1$2',
    );
  return (body) => `${before}<main>\n${body}\n    </main>${after}`;
}

function dateOf(published) {
  const date = new Date(published);
  return {
    machine: date.toISOString().slice(0, 10),
    brief: date.toLocaleDateString("en-GB", {
      day: "numeric",
      month: "short",
      year: "numeric",
      timeZone: "UTC",
    }),
  };
}

// The version list: one line per release, its kind as a chip, a short name that is cut off
// rather than wrapped, and its date. The full name is the row's title for the cut-off case.
export function versions(releases, currentTag, root) {
  const rows = releases.map((release) => {
    const kind = kindOf(release);
    const name = release.name || release.tag_name;
    const date = dateOf(release.published_at);
    const current = release.tag_name === currentTag ? ' aria-current="page"' : "";
    return (
      `<li data-kind="${kind.kind}"><a href="${root}/releases/${releasePath(release.tag_name)}/"` +
      `${current} title="${escapeHtml(name)}">` +
      `<span class="log__kind">${kind.label}</span>` +
      `<span class="log__name">${escapeHtml(kind.short(name, release.tag_name))}</span>` +
      `<time datetime="${date.machine}">${date.brief}</time></a></li>`
    );
  });
  return `<ol class="log__list" role="list">${rows.join("")}</ol>`;
}

// The filter over the list: every kind that has a release, and all of them, with `current` the one
// it opens on.
export function kinds(releases, current) {
  const present = [...KINDS, OTHER].filter((kind) =>
    releases.some((release) => kindOf(release) === kind),
  );
  return [{ kind: "all", label: "All" }, ...present]
    .map(
      ({ kind, label }) =>
        `<button type="button" data-kind-choice="${kind}" aria-pressed="${kind === current}">${label}</button>`,
    )
    .join("");
}

// The place index.html keeps for the newest browser release, beside the Start page's install
// link. It is hidden there, so a build that could not read the releases leaves no empty line.
const LATEST = '<p class="start__release" data-latest-release hidden></p>';

/** The landing page with the newest browser release written into its place. */
export function latestRelease(landing, releases) {
  if (!landing.includes(LATEST)) throw new Error("index.html has no place for the latest release");
  const release = releases.find((candidate) => kindOf(candidate).kind === "browser");
  if (!release) throw new Error("GitHub listed no browser release");
  const date = dateOf(release.published_at);
  const line =
    `<p class="start__release">Latest: ${escapeHtml(release.tag_name)} · ` +
    `<time datetime="${date.machine}">${date.brief}</time> · ` +
    `<a href="releases/${releasePath(release.tag_name)}/">release notes</a></p>`;
  return landing.replace(LATEST, line);
}

/**
 * Every page `releases/` holds, as [directory under it, html]: the newest browser release at its
 * root, then each release at its tag.
 */
export function releasePages(releases, landing, template) {
  const browser = releases.filter((release) => kindOf(release).kind === "browser");
  if (!browser.length) throw new Error("GitHub listed no browser release");
  const count = `${releases.length} releases`;
  const page = (release, root) => {
    const rendered = renderRelease(release, releases, template, root);
    const body = rendered.body
      .replace("{{count}}", count)
      // The list opens on the browser's releases, which are what most readers came for; on an
      // engine or repository page it opens on all of them, so the release being read is in it.
      .replace("{{kinds}}", kinds(releases, kindOf(release).kind === "browser" ? "browser" : "all"))
      .replace("{{versions}}", versions(releases, release.tag_name, root));
    return chrome(landing, root, rendered.meta)(body);
  };
  // `releases/` is the newest browser release rather than a page of its own: a reader arriving
  // without a version in mind wants the latest notes, and the list beside them reaches the rest.
  return [
    ["", page(browser[0], "..")],
    ...releases.map((release) => [releasePath(release.tag_name), page(release, "../..")]),
  ];
}

/**
 * The page `releases/` holds when the build could not read the releases, or was asked not to:
 * where they are, rather than no page at all.
 */
export function fallbackPage(landing) {
  const body = `      <section class="log log--empty wrap">
        <article class="log__release">
          <p class="kicker">Release notes</p>
          <h1 class="display display--md">The releases are on GitHub.</h1>
          <p>This page lists every release and its notes when the site is built, and this build could not read them.</p>
          <p class="log__out">
            <a class="btn" href="https://github.com/villekivela/omaweb/releases">Every release on GitHub</a>
          </p>
        </article>
      </section>`;
  const meta = { title: "Releases · Omaweb", description: "Every published Omaweb release." };
  return chrome(landing, "..", meta)(body);
}

export async function writePages(output, pages) {
  for (const [directory, html] of pages) {
    await mkdir(join(output, directory), { recursive: true });
    await writeFile(join(output, directory, "index.html"), html);
  }
}
