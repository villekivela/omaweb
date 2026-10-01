import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { test } from "node:test";

import { allKeyColours } from "./crt-road-key-colours.mjs";

// The browser's road is held to this file, so the file has to be what the website draws. A change
// to share/scenes/crt-road.json or website/crt-road.js that moves a key colour fails here until
// the file is written again, and then the browser's test says whether its road followed.
test("the key colours on file are the website's", () => {
  const onFile = JSON.parse(
    readFileSync(new URL("./crt-road-key-colours.json", import.meta.url), "utf8"),
  );
  assert.deepEqual(allKeyColours(), onFile);
});
