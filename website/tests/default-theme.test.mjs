// The theme the site wears when the reader has picked none: the desktop film's, as film/README.md
// records. The markup names it, the picker offers it, themes.css draws it, and theme.js leaves it
// alone when storage is empty or blocked.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";
import vm from "node:vm";

const DEFAULT = "ethereal-nebula-silver";
const read = (path) => readFileSync(new URL(path, import.meta.url), "utf8");
const html = read("../index.html");

test("default theme: the landing page's markup names the film's theme", () => {
  assert.match(html, new RegExp(`<html lang="en" data-theme="${DEFAULT}">`));
});

test("default theme: the picker offers it, pressed, and names it", () => {
  assert.match(
    html,
    new RegExp(
      `data-theme-choice="${DEFAULT}"[^>]*data-name="Ethereal Nebula Silver"[^>]*aria-pressed="true"`,
    ),
  );
  assert.match(html, /class="themes__name"[^>]*>Ethereal Nebula Silver</);
});

test("default theme: themes.css draws its roles", () => {
  assert.match(read("../assets/shots/themes.css"), new RegExp(`\\[data-theme="${DEFAULT}"\\]`));
});

test("default theme: theme.js leaves the markup's theme when storage is empty or blocked", () => {
  for (const sessionStorage of [
    { getItem: () => null },
    {
      getItem: () => {
        throw new Error("blocked");
      },
    },
  ]) {
    const documentElement = { dataset: { theme: DEFAULT } };
    vm.runInNewContext(read("../theme.js"), { sessionStorage, document: { documentElement } });
    assert.equal(documentElement.dataset.theme, DEFAULT);
  }
});

test("default theme: a picked theme still wins", () => {
  const documentElement = { dataset: { theme: DEFAULT } };
  const sessionStorage = { getItem: () => "nord" };
  vm.runInNewContext(read("../theme.js"), { sessionStorage, document: { documentElement } });
  assert.equal(documentElement.dataset.theme, "nord");
});

// A change of theme changes the name's width, so the name comes after the swatches and leaves them
// where they stand.
test("theme strip: the name follows the swatches, so a new name does not move them", () => {
  const strip = html.slice(html.indexOf('class="themes"'));
  const at = (part) => strip.indexOf(part);
  assert.ok(at('class="themes__label') < at('class="themes__row"'));
  assert.ok(at('class="themes__row"') < at('class="themes__name"'));
});
