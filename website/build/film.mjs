// The introductory film and its clips, which the site serves but the repository does not keep:
// `site.mjs` reads them from the `film` release's assets into `dist/assets/film/`, so Vercel serves
// them from the site's own address and the Content-Security-Policy stays `default-src 'self'`.
// `film/README.md` at the repository root has how a recording is made and uploaded there.
//
// `clips.json` names the cards that play a clip of the film on the drive; each one's clip is
// `clip-<card>` in the film's two codecs, with a poster of its first frame.
//
// The build fails when a file is missing, unlike the release pages: a landing page whose film
// does not play is a broken page, where a thin releases page is not.

import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";

const RELEASE = "https://github.com/villekivela/omaweb/releases/download/film";
export const FILM = ["omaweb.webm", "omaweb.mp4", "poster.webp", "clips.json"];
const CLIP_KINDS = ["webm", "mp4", "webp"];

async function download(directory, name, fetch) {
  const response = await fetch(`${RELEASE}/${name}`);
  if (!response.ok) {
    throw new Error(
      `the film release has no ${name}: GitHub answered ${response.status}; ` +
        "film/README.md says how to upload it",
    );
  }
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (bytes.length === 0) throw new Error(`the film release's ${name} is empty`);
  await writeFile(join(directory, name), bytes);
  return bytes;
}

// The cards `clips.json` names, each a lowercase word, since each becomes a file name here and an
// address on the page.
function cardsOf(bytes) {
  const cards = Object.keys(JSON.parse(new TextDecoder().decode(bytes)));
  for (const card of cards) {
    if (!/^[a-z]+$/.test(card)) throw new Error(`clips.json names a card ${JSON.stringify(card)}`);
  }
  return cards;
}

export async function downloadFilm(output, { fetch = globalThis.fetch } = {}) {
  const directory = join(output, "assets", "film");
  await mkdir(directory, { recursive: true });
  let cards = [];
  for (const name of FILM) {
    const bytes = await download(directory, name, fetch);
    if (name === "clips.json") cards = cardsOf(bytes);
  }
  for (const card of cards) {
    for (const kind of CLIP_KINDS) await download(directory, `clip-${card}.${kind}`, fetch);
  }
}
