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

const CLIPS = { omnibar: { length: 9.5 }, theme: { length: 16.9, themes: [[0, "nebula"]] } };
const CLIP_FILES = ["omnibar", "theme"].flatMap((card) =>
  ["webm", "mp4", "webp"].map((kind) => `clip-${card}.${kind}`),
);
const FILES = ["omaweb.webm", "omaweb.mp4", "poster.webp", "clips.json", ...CLIP_FILES];

// GitHub as the build sees it: the asset's address answers with its bytes, `clips.json` with
// `clips`, and an asset the release does not hold answers 404.
function github(held, clips = CLIPS) {
  const asked = [];
  const fetch = async (address) => {
    asked.push(address);
    const name = address.split("/").pop();
    if (!held.includes(name)) return new Response("Not Found", { status: 404 });
    if (name === "clips.json") return new Response(JSON.stringify(clips), { status: 200 });
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
    const release = "https://github.com/villekivela/omaweb/releases/download/film/";
    assert.deepEqual(
      asked,
      FILES.map((name) => release + name),
    );
    const film = join(output, "assets", "film");
    assert.deepEqual((await readdir(film)).sort(), [...FILES].sort());
    assert.equal(await readFile(join(film, "poster.webp"), "utf8"), "bytes of poster.webp");
    assert.deepEqual(JSON.parse(await readFile(join(film, "clips.json"), "utf8")), CLIPS);
  });
});

test("film: a clip that clips.json names and the release does not hold fails the build", async () => {
  await inTemporaryDirectory(async (output) => {
    const { fetch } = github(FILES.filter((name) => name !== "clip-theme.mp4"));
    await assert.rejects(downloadFilm(output, { fetch }), /clip-theme\.mp4.*404/);
  });
});

test("film: clips.json naming a card that is not one word fails the build", async () => {
  await inTemporaryDirectory(async (output) => {
    for (const card of ["../index", "Omnibar", ""]) {
      const { fetch } = github(FILES, { [card]: { length: 1 } });
      await assert.rejects(downloadFilm(output, { fetch }), /clips\.json names a card/);
    }
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
