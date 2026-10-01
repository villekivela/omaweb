// The themes the site offers, as scripts/build_website_themes.py writes them into themes.css. A
// release page asks Omaweb for the reader's own palette, so under any theme the page wears,
// Omaweb's colours win where Omaweb hands them over and the theme's own stand in everywhere else.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const themes = readFileSync(new URL("../assets/shots/themes.css", import.meta.url), "utf8");

test("themes: every role defers to Omaweb's palette, with the theme's own colour behind it", () => {
  const blocks = [...themes.matchAll(/\[data-theme="([^"]+)"\] \{([^}]*)\}/g)];
  assert.ok(blocks.length >= 9, "themes.css names fewer themes than the page offers");
  for (const [, name, body] of blocks) {
    for (const role of ["bg", "sidebar", "fg", "accent", "urgent", "muted"]) {
      assert.match(
        body,
        new RegExp(`--${role}: var\\(--omaweb-${role}, #[0-9a-f]{6}\\);`),
        `${name} --${role}`,
      );
    }
  }
});
