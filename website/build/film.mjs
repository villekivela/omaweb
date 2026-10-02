// The introductory film, which the site serves but the repository does not keep: `site.mjs` reads
// it from the `film` release's assets into `dist/assets/film/`, so Vercel serves it from the
// site's own address and the Content-Security-Policy stays `default-src 'self'`.
// `film/README.md` at the repository root has how a recording is made and uploaded there.
//
// The build fails when a file is missing, unlike the release pages: a landing page whose film
// does not play is a broken page, where a thin releases page is not.

import { mkdir, writeFile } from "node:fs/promises";
import { join } from "node:path";

const RELEASE = "https://github.com/villekivela/omaweb/releases/download/film";
export const FILM = ["omaweb.webm", "omaweb.mp4", "poster.webp"];

export async function downloadFilm(output, { fetch = globalThis.fetch } = {}) {
  const directory = join(output, "assets", "film");
  await mkdir(directory, { recursive: true });
  for (const name of FILM) {
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
  }
}
