// The website's build step. Vercel runs it from `website/` through the `buildCommand` in
// `vercel.json`, installing `website/package.json` first. That is one dependency, `marked`, which
// has none of its own, and it is what renders a release body (see `render.mjs`). Nothing else here
// needs a toolchain: this is a plain Node script otherwise.
//
// The site is static files, served as they are, plus the one thing static files cannot do: the
// release pages, read from GitHub's releases here rather than in the reader's browser, so the site
// needs no request at load and spends no API rate limit per reader. A release's notes only change
// when a tag is pushed, which is a deploy anyway.
//
// It writes into `dist/` rather than over the sources, so a local run leaves the working tree as it
// found it.
//
//   node build/site.mjs            # from website/
//   node build/site.mjs --local    # the same, without asking GitHub
//
// The film is the one thing the build does fail on: it is read from the `film` release's assets
// (see `film.mjs`), and a landing page without it is broken. `--local` skips it and serves the
// copy in `assets/film/`, if there is one.
//
// The build never fails on the releases API. A rate limit or an outage, or `--local`, leaves a
// releases page that says where the releases are, so the site deploys a page that is thin rather
// than not deploying at all.

import { copyFile, cp, mkdir, readdir, readFile, rm } from "node:fs/promises";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { downloadFilm } from "./film.mjs";
import { fallbackPage, fetchReleases, releasePages, writePages } from "./releases.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const WEBSITE = resolve(HERE, "..");
const OUTPUT = join(WEBSITE, "dist");
const RELEASES = join(OUTPUT, "releases");

// Not the site: `build/` is this script, its tests and the release template, `tests/` holds the
// page's own tests, `dist/` is where it writes, `node_modules/` and the package files are the
// build's own dependency, `.vercel` is the CLI's state, and `vercel.json` is read from the project
// root rather than served.
const NOT_DEPLOYED = new Set([
  "build",
  "tests",
  "dist",
  "node_modules",
  ".vercel",
  ".gitignore",
  "vercel.json",
  "package.json",
  "package-lock.json",
]);

const LOCAL = process.argv.includes("--local");

// Files kept outside website/ so one copy serves both the site and something else: the CRT road's
// parameters, which the browser's own road reads too (#496), and the one-line install, which
// tests/scripts/tst_install.py runs from where it sits. Each is served at the address it is listed
// under; `vercel.json` types /install as text, which an extensionless file would not be.
export const SHARED = {
  "crt-road.json": new URL("../../share/scenes/crt-road.json", import.meta.url).href,
  install: new URL("../../scripts/install.sh", import.meta.url).href,
};

// Entry by entry rather than one `cp` of the whole directory: the output lives inside the input,
// and `cp` refuses that however the filter is written. The READMEs say how the files are made and
// are for this repository's readers, not the site's; a hidden file is some tool's own.
async function copyStaticFiles() {
  for (const entry of await readdir(WEBSITE)) {
    if (NOT_DEPLOYED.has(entry)) continue;
    await cp(join(WEBSITE, entry), join(OUTPUT, entry), {
      recursive: true,
      filter: (source) => basename(source) !== "README.md" && !basename(source).startsWith("."),
    });
  }
}

async function main() {
  await rm(OUTPUT, { recursive: true, force: true });
  await mkdir(OUTPUT, { recursive: true });
  await copyStaticFiles();
  for (const [address, source] of Object.entries(SHARED)) {
    await copyFile(fileURLToPath(source), join(OUTPUT, address));
  }

  const landing = await readFile(join(WEBSITE, "index.html"), "utf8");
  if (LOCAL) {
    console.log("website: the film is the local copy in assets/film/, if there is one");
    await writePages(RELEASES, [["", fallbackPage(landing)]]);
    console.log("website: wrote the site, with the releases page that points at GitHub");
    return;
  }

  await downloadFilm(OUTPUT);

  // The fetch and the rendering are guarded together. A release the API answers with is data
  // from elsewhere, so rendering it can fail on something no fixed input would have shown, and
  // that is still a reason to deploy the fallback rather than to fail the deploy.
  try {
    const template = await readFile(join(HERE, "release.html"), "utf8");
    const pages = releasePages(await fetchReleases(), landing, template);
    await writePages(RELEASES, pages);
    console.log(`website: wrote the site and ${pages.length - 1} release pages`);
  } catch (error) {
    console.warn(`website: the releases page points at GitHub instead: ${error.message}`);
    await writePages(RELEASES, [["", fallbackPage(landing)]]);
  }
}

// The build runs when this file is run, not when a test imports it for SHARED.
if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) await main();
