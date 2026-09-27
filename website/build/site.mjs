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
// The build never fails on the API. A rate limit or an outage, or `--local`, leaves a releases
// page that says where the releases are, so the site deploys a page that is thin rather than not
// deploying at all.

import { cp, mkdir, readdir, readFile, rm } from "node:fs/promises";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { fallbackPage, fetchReleases, releasePages, writePages } from "./releases.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const WEBSITE = resolve(HERE, "..");
const OUTPUT = join(WEBSITE, "dist");
const RELEASES = join(OUTPUT, "releases");

// Not the site: `build/` is this script, its tests and the release template, `dist/` is where it
// writes, `node_modules/` and the package files are the build's own dependency, `.vercel` is the
// CLI's state, and `vercel.json` is read from the project root rather than served.
const NOT_DEPLOYED = new Set([
  "build",
  "dist",
  "node_modules",
  ".vercel",
  ".gitignore",
  "vercel.json",
  "package.json",
  "package-lock.json",
]);

const LOCAL = process.argv.includes("--local");

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

  const landing = await readFile(join(WEBSITE, "index.html"), "utf8");
  if (LOCAL) {
    await writePages(RELEASES, [["", fallbackPage(landing)]]);
    console.log("website: wrote the site, with the releases page that points at GitHub");
    return;
  }

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

await main();
