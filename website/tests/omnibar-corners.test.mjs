// The website's Omnibar is the browser's, so its corners are the browser's: the radius the app's
// field and results panel are drawn with (src/ui/Omnibar.qml), on the dash itself and on the two
// rings of sunlight laid over its border.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const styles = readFileSync(new URL("../styles.css", import.meta.url), "utf8");
const app = readFileSync(new URL("../../src/ui/Omnibar.qml", import.meta.url), "utf8");

const rule = (selector) => {
  const found = styles.match(new RegExp(`^${selector} \\{([^}]*)\\}`, "m"));
  assert.ok(found, `styles.css has no rule for ${selector}`);
  return found[1];
};

test("omnibar corners: the dash takes the radius the app's panel is drawn with", () => {
  const panel = app.match(/id: panel\b[\s\S]*?\bradius: (\d+)/);
  assert.ok(panel, "Omnibar.qml's panel has no radius");
  assert.match(rule("\\.omnibar"), new RegExp(`border-radius: ${panel[1]}px;`));
});

test("omnibar corners: the sunlight on the border and its bloom follow them", () => {
  assert.match(rule("\\.omnibar::before,\\n\\.omnibar__glow::before"), /border-radius: inherit;/);
  assert.match(rule("\\.omnibar__glow"), /border-radius: inherit;/);
});
