// The website's build step. Vercel runs it from `website/` through the
// `buildCommand` in `vercel.json`, installing `website/package.json` first.
// That is one dependency, `marked`, which has none of its own, and it is what
// renders a release body (see `render.mjs`). Nothing else here needs a
// toolchain: this is a plain Node script otherwise.
//
// It does the one thing static files cannot. The releases are published on
// GitHub, and they are pulled here rather than in the reader's browser, so the
// site stays static, needs no request at load, and spends no API rate limit
// per reader. A release's notes only change when a tag is pushed, which is a
// deploy anyway.
//
// Every page is written through `build/shell.html`, which holds the head,
// the header and its nav, the footer and the script once: a page under
// `pages/` is what goes between the header and the footer. `pages/index.html`
// becomes `dist/index.html` and any other `pages/<name>.html` becomes
// `dist/<name>/index.html`, so each page is a directory and its address ends
// in a slash. Everything else in this directory is copied across as it is.
//
// It writes into `dist/` rather than over the source files, so a local run
// leaves the working tree as it found it.
//
//   node build/site.mjs            # from website/
//   node build/site.mjs --local    # the same, without asking GitHub
//
// The build never fails on the API. A rate limit or an outage leaves the
// `releases/index.html` committed here, which says where the releases are, so
// the site deploys a page that is thin rather than not deploying at all.

import { cp, mkdir, readdir, readFile, rm, writeFile } from "node:fs/promises";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { releasePath, renderRelease } from "./render.mjs";
import { parsePage, renderShell } from "./shell.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const WEBSITE = resolve(HERE, "..");
const OUTPUT = join(WEBSITE, "dist");

// Enough that a reader sees the history without the list becoming the page.
// It links to the rest on GitHub.
const LISTED = 12;

// Asked for one more than are listed, so a draft at the top of the list costs
// a row rather than the oldest release on the page.
const RELEASES = `https://api.github.com/repos/villekivela/omaweb/releases?per_page=${LISTED + 1}`;

// None of these belongs in the deployed site as it is: `build/` is this
// script, its tests and the templates, `pages/` is what the script writes
// into the shell, `dist/` is where they put their output, `.vercel` is the
// CLI's own state, and `vercel.json` is read from the project root rather than
// from the output, so copying it would only serve the deployment's own
// configuration at `/vercel.json`.
const NOT_DEPLOYED = new Set(["build", "pages", "dist", "node_modules", ".vercel", "vercel.json"]);

const PAGES = join(WEBSITE, "pages");

// Asked for without the network: the pages are written and the releases page
// is the committed one, which is what a local review of the pages wants.
const LOCAL = process.argv.includes("--local");

// Entry by entry rather than one `cp` of the whole directory: the output lives
// inside the input, and `cp` refuses that however the filter is written.
async function copyStaticFiles() {
  for (const entry of await readdir(WEBSITE)) {
    if (NOT_DEPLOYED.has(entry)) continue;
    await cp(join(WEBSITE, entry), join(OUTPUT, entry), { recursive: true });
  }
}

async function readShell() {
  return readFile(join(HERE, "shell.html"), "utf8");
}

async function writePage(shell, directory, root, page) {
  await mkdir(directory, { recursive: true });
  await writeFile(join(directory, "index.html"), renderShell(shell, { root, ...page }));
}

// Every page under `pages/`, into the shell. The landing page is the root;
// any other is a directory named after its file, one level down.
async function writePages(shell) {
  const names = (await readdir(PAGES)).filter((name) => name.endsWith(".html")).sort();
  for (const name of names) {
    const page = parsePage(await readFile(join(PAGES, name), "utf8"));
    const stem = basename(name, ".html");
    if (stem === "index") await writePage(shell, OUTPUT, ".", page);
    else await writePage(shell, join(OUTPUT, stem), "..", page);
  }
  return names.length;
}

async function fetchReleases() {
  const headers = { accept: "application/vnd.github+json", "user-agent": "omaweb-website-build" };
  // Vercel's builders share an address, so the unauthenticated limit is shared
  // with whoever else is building from it. A token raises it and is optional.
  if (process.env.GITHUB_TOKEN) headers.authorization = `Bearer ${process.env.GITHUB_TOKEN}`;

  const response = await fetch(RELEASES, { headers });
  if (!response.ok) {
    throw new Error(`GitHub answered ${response.status} ${response.statusText}`);
  }
  const releases = await response.json();
  // Every `v0.*` tag ships as a prerelease (ADR 0028), so prereleases are the
  // releases. A draft is not published and is nobody's to read yet. A tag a URL
  // cannot name is dropped here rather than deeper, so the version list and the
  // pages that exist cannot disagree.
  return releases
    .filter((release) => !release.draft && releasePath(release.tag_name))
    .slice(0, LISTED);
}

async function writeReleasePages(shell, releases) {
  const template = await readFile(join(HERE, "release.html"), "utf8");

  // `releases/` is the newest release rather than a page of its own. A reader
  // arriving without a version in mind wants the latest notes, and the list
  // beside them is the way to any other. It replaces the page `writePages`
  // wrote there, which only says where the releases are.
  const newest = renderRelease(releases[0], releases, template, "..");
  await writePage(shell, join(OUTPUT, "releases"), "..", newest);

  for (const release of releases) {
    const directory = join(OUTPUT, "releases", releasePath(release.tag_name));
    await writePage(shell, directory, "../..", renderRelease(release, releases, template, "../.."));
  }
}

async function main() {
  await rm(OUTPUT, { recursive: true, force: true });
  await mkdir(OUTPUT, { recursive: true });
  await copyStaticFiles();
  const shell = await readShell();
  console.log(`website: wrote ${await writePages(shell)} pages`);
  if (LOCAL) return;

  // The fetch and the rendering are guarded together. A release the API
  // answers with is data from elsewhere, so rendering it can fail on
  // something no fixed input would have shown, such as a date that will not
  // parse, and that is still a reason to keep the committed page rather than
  // to fail the deploy.
  try {
    const releases = await fetchReleases();
    if (!releases.length) {
      console.warn("website: keeping the committed releases page: no release the site can name");
      return;
    }
    await writeReleasePages(shell, releases);
    console.log(`website: wrote ${releases.length} release pages`);
  } catch (error) {
    console.warn(`website: keeping the committed releases page: ${error.message}`);
  }
}

await main();
