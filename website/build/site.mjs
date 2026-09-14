// The website's build step. Vercel runs it from `website/` through the
// `buildCommand` in `vercel.json`, and it is a plain Node script with no
// dependencies: there is nothing to install and no toolchain to keep current.
//
// It does the one thing static files cannot. The releases are published on
// GitHub, and they are pulled here rather than in the reader's browser, so the
// site stays static, needs no request at load, and spends no API rate limit
// per reader. A release's notes only change when a tag is pushed, which is a
// deploy anyway.
//
// It writes into `dist/` rather than over the source files, so a local run
// leaves the working tree as it found it.
//
//   node build/site.mjs            # from website/
//
// The build never fails on the API. A rate limit or an outage leaves the
// fallback the committed page already carries, a link to the releases on
// GitHub, because a missing list is worth less than a deploy that does not
// happen.

import { cp, mkdir, readdir, readFile, rm, writeFile } from "node:fs/promises";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { releasePath, renderReleaseList, renderReleasePage, writeReleases } from "./render.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const WEBSITE = resolve(HERE, "..");
const OUTPUT = join(WEBSITE, "dist");

// Enough that a reader sees the recent history without the landing page
// carrying every release forever. The section links to the rest on GitHub.
const LISTED = 8;

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
  // releases. A draft is not published and is nobody's to read yet.
  return releases.filter((release) => !release.draft).slice(0, LISTED);
}

async function writeReleasePages(releases) {
  const template = await readFile(join(HERE, "release.template.html"), "utf8");
  for (const release of releases) {
    const path = releasePath(release.tag_name);
    if (!path) {
      console.warn(`website: skipping ${release.tag_name}: not a tag a URL can name`);
      continue;
    }
    const directory = join(OUTPUT, "releases", path);
    await mkdir(directory, { recursive: true });
    await writeFile(join(directory, "index.html"), renderReleasePage(release, template));
  }
}

async function main() {
  await rm(OUTPUT, { recursive: true, force: true });
  await mkdir(OUTPUT, { recursive: true });
  await copyStaticFiles();

  // The fetch and the rendering are guarded together. A release the API
  // answers with is data from elsewhere, so rendering it can fail on
  // something no fixed input would have shown, such as a date that will not
  // parse, and that is still a reason to keep the fallback rather than to
  // fail the deploy.
  let list = "";
  let releases = [];
  try {
    releases = await fetchReleases();
    list = renderReleaseList(releases);
  } catch (error) {
    console.warn(`website: keeping the fallback release list: ${error.message}`);
    return;
  }

  // An empty list is every bit as empty as a failed fetch, and writing it
  // would replace the fallback with nothing at all.
  if (!list) {
    console.warn("website: keeping the fallback release list: no release the site can name");
    return;
  }

  await writeReleasePages(releases);

  // Outside the guard above. The markers are ours, in a file in this
  // repository, so a page missing them is a mistake to fix rather than a
  // condition to degrade around.
  const page = await readFile(join(WEBSITE, "index.html"), "utf8");
  await writeFile(join(OUTPUT, "index.html"), writeReleases(page, list));
  console.log(`website: wrote ${releases.length} releases and their pages`);
}

await main();
