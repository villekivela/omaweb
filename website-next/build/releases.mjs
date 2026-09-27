// Writes the release pages for the preview: `releases/index.html`, the newest release, and one
// page per release at `releases/<tag>/index.html`. Run it from anywhere:
//
//   node website-next/build/releases.mjs
//
// The releases are read from GitHub here, when the pages are built, rather than in the reader's
// browser, so the pages stay static, load nothing from another origin, and spend no API rate
// limit per reader. A release body is Markdown written elsewhere and published unattended, so it
// goes through the live site's own renderer, website/build/render.mjs, which decides what a body
// may become, rather than a second copy of those rules.
//
// Each page is the landing page with its <main> swapped out: the head, the header, the footer and
// the scripts are read from index.html, so the chrome has one copy. The pages sit one or two
// directories down, so the chrome's relative addresses are rewritten to reach back to the root.

import { mkdir, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { releasePath, renderRelease } from "../../website/build/render.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const SITE = resolve(HERE, "..");
const OUTPUT = join(SITE, "releases");

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

function kindOf(release) {
  return KINDS.find((kind) => kind.test.test(release.tag_name)) || OTHER;
}

async function fetchReleases() {
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
function chrome(landing, root, meta) {
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
    .replace(/(<a href="[^"]*\/releases\/")>/, '$1 aria-current="true">');
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
function versions(releases, currentTag, root) {
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
function kinds(releases, current) {
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

async function writePage(directory, html) {
  await mkdir(directory, { recursive: true });
  await writeFile(join(directory, "index.html"), html);
}

async function main() {
  const landing = await readFile(join(SITE, "index.html"), "utf8");
  const template = await readFile(join(HERE, "release.html"), "utf8");
  const releases = await fetchReleases();
  const browser = releases.filter((release) => kindOf(release).kind === "browser");
  if (!browser.length) throw new Error("GitHub listed no browser release");

  await rm(OUTPUT, { recursive: true, force: true });
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
  await writePage(OUTPUT, page(browser[0], ".."));
  for (const release of releases) {
    await writePage(join(OUTPUT, releasePath(release.tag_name)), page(release, "../.."));
  }
  console.log(`releases: wrote ${releases.length} release pages`);
}

await main();
