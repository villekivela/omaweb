// The film's download as film.mjs does it, against a stand-in for GitHub, so a build that would
// deploy the page without its film fails here rather than on the deployed site.

import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { downloadFilm } from "./film.mjs";

const FILES = ["omaweb.webm", "omaweb.mp4", "poster.webp"];

// GitHub as the build sees it: the asset's address answers with its bytes, and an asset the
// release does not hold answers 404.
function github(held) {
  const asked = [];
  const fetch = async (address) => {
    asked.push(address);
    const name = address.split("/").pop();
    if (!held.includes(name)) return new Response("Not Found", { status: 404 });
    return new Response(`bytes of ${name}`, { status: 200 });
  };
  return { asked, fetch };
}

async function inTemporaryDirectory(run) {
  const directory = await mkdtemp(join(tmpdir(), "omaweb-film-"));
  try {
    await run(directory);
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
}

test("film: every file is read from the film release into assets/film", async () => {
  await inTemporaryDirectory(async (output) => {
    const { asked, fetch } = github(FILES);
    await downloadFilm(output, { fetch });
    assert.deepEqual(asked, [
      "https://github.com/villekivela/omaweb/releases/download/film/omaweb.webm",
      "https://github.com/villekivela/omaweb/releases/download/film/omaweb.mp4",
      "https://github.com/villekivela/omaweb/releases/download/film/poster.webp",
    ]);
    const film = join(output, "assets", "film");
    assert.deepEqual((await readdir(film)).sort(), [...FILES].sort());
    assert.equal(await readFile(join(film, "poster.webp"), "utf8"), "bytes of poster.webp");
  });
});

test("film: a file the release does not hold fails the build and names it", async () => {
  await inTemporaryDirectory(async (output) => {
    const { fetch } = github(["omaweb.webm", "poster.webp"]);
    await assert.rejects(downloadFilm(output, { fetch }), /omaweb\.mp4.*404/);
  });
});

test("film: an empty answer fails the build rather than deploying an empty film", async () => {
  await inTemporaryDirectory(async (output) => {
    const fetch = async () => new Response("", { status: 200 });
    await assert.rejects(downloadFilm(output, { fetch }), /omaweb\.webm.*empty/);
  });
});

test("film: the site's build stops when the film release does not hold the film", async () => {
  await inTemporaryDirectory(async (directory) => {
    // GitHub, as a build sees it before the first upload: every address answers 404.
    const stub = join(directory, "github.mjs");
    await writeFile(
      stub,
      'globalThis.fetch = async () => new Response("Not Found", { status: 404 });\n',
    );
    const website = fileURLToPath(new URL("..", import.meta.url));
    const build = spawnSync(process.execPath, ["--import", stub, "build/site.mjs"], {
      cwd: website,
      encoding: "utf8",
    });
    assert.notEqual(build.status, 0);
    assert.match(build.stderr, /the film release has no omaweb\.webm/);
  });
});
