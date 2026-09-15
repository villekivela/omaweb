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
// Only the release pages are generated. Everything else is a file in this
// directory, copied across as it is.
//
// It writes into `dist/` rather than over the source files, so a local run
// leaves the working tree as it found it.
//
//   node build/site.mjs            # from website/
//
// The build never fails on the API. A rate limit or an outage leaves the
// `releases/index.html` committed here, which says where the releases are, so
// the site deploys a page that is thin rather than not deploying at all.

import { cp, mkdir, readdir, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { releasePath, renderReleasePage } from "./render.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const WEBSITE = resolve(HERE, "..");
const OUTPUT = join(WEBSITE, "dist");

// Enough that a reader sees the history without the list becoming the page.
// It links to the rest on GitHub.
const LISTED = 12;

// Asked for one more than are listed, so a draft at the top of the list costs
// a row rather than the oldest release on the page.
const RELEASES = `https://api.github.com/repos/villekivela/omaweb/releases?per_page=${LISTED + 1}`;

// None of these belongs in the deployed site: `build/` is this script, its
// tests and the page template, `dist/` is where they put their output,
// `.vercel` is the CLI's own state, and `vercel.json` is read from the project
// root rather than from the output, so copying it would only serve the
// deployment's own configuration at `/vercel.json`.
const NOT_DEPLOYED = new Set(["build", "dist", "node_modules", ".vercel", "vercel.json"]);

// Entry by entry rather than one `cp` of the whole directory: the output lives
// inside the input, and `cp` refuses that however the filter is written.
async function copyStaticFiles() {
  for (const entry of await readdir(WEBSITE)) {
    if (NOT_DEPLOYED.has(entry)) continue;
    await cp(join(WEBSITE, entry), join(OUTPUT, entry), { recursive: true });
  }
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

async function writeReleasePages(releases) {
  const template = await readFile(join(HERE, "release.template.html"), "utf8");

  // `releases/` is the newest release rather than a page of its own. A reader
  // arriving without a version in mind wants the latest notes, and the list
  // beside them is the way to any other.
  await writeFile(
    join(OUTPUT, "releases", "index.html"),
    renderReleasePage(releases[0], releases, template, ".."),
  );

  for (const release of releases) {
    const directory = join(OUTPUT, "releases", releasePath(release.tag_name));
    await mkdir(directory, { recursive: true });
    await writeFile(
      join(directory, "index.html"),
      renderReleasePage(release, releases, template, "../.."),
    );
  }
}

async function main() {
  await rm(OUTPUT, { recursive: true, force: true });
  await mkdir(OUTPUT, { recursive: true });
  await copyStaticFiles();

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
    await writeReleasePages(releases);
    console.log(`website: wrote ${releases.length} release pages`);
  } catch (error) {
    console.warn(`website: keeping the committed releases page: ${error.message}`);
  }
}

await main();
