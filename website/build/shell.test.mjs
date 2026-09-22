// The shell is filled at build time, so nothing here runs in a reader's
// browser and no test needs one.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import test from "node:test";

import { parsePage, renderNav, renderShell } from "./shell.mjs";

const SHELL = [
  "<title>{{title}}</title>",
  '<meta name="description" content="{{description}}" />',
  '<meta property="og:description" content="{{summary}}" />',
  '<meta property="og:type" content="{{type}}" />',
  '<link rel="icon" href="{{root}}/favicon.svg" />',
  "<nav>{{nav}}</nav>",
  "<main>{{body}}</main>",
].join("\n");

const PAGE = [
  "<!--",
  "  title: What is in Omaweb",
  "  description: What Omaweb does.",
  "  current: features",
  "-->",
  "<section>the page</section>",
  "",
].join("\n");

test("page: the opening comment names the page and the rest is its markup", () => {
  const { meta, body } = parsePage(PAGE);
  assert.deepEqual(meta, {
    title: "What is in Omaweb",
    description: "What Omaweb does.",
    current: "features",
  });
  assert.equal(body, "<section>the page</section>\n");
});

test("page: a page with no comment is all markup and no title", () => {
  const { meta, body } = parsePage("<p>bare</p>");
  assert.deepEqual(meta, {});
  assert.equal(body, "<p>bare</p>");
});

test("nav: the current entry is marked, and a release page marks it as the part of the site", () => {
  assert.match(renderNav("..", "features"), /href="\.\.\/features\/" aria-current="page"/);
  assert.match(renderNav("../..", "releases", "true"), /releases\/" aria-current="true"/);
  assert.equal(renderNav(".", undefined).includes("aria-current"), false);
  assert.match(renderNav(".", undefined), /href="\.\/#install"/);
});

test("shell: the page is written in, with its head from the comment and its depth in every path", () => {
  const html = renderShell(SHELL, { root: "..", ...parsePage(PAGE) });
  assert.match(html, /<title>What is in Omaweb<\/title>/);
  assert.match(html, /content="What Omaweb does\." \/>/);
  // The summary is the description where none is given, and the type is
  // an article unless the page says otherwise.
  assert.match(html, /og:description" content="What Omaweb does\."/);
  assert.match(html, /og:type" content="article"/);
  assert.match(html, /href="\.\.\/favicon\.svg"/);
  assert.match(html, /<main><section>the page<\/section><\/main>/);
  assert.equal(html.includes("{{"), false);
});

test("shell: what the comment names is attribute-safe", () => {
  const page = parsePage('<!--\n  title: a "quoted" <title>\n  description: d\n-->\n');
  const html = renderShell(SHELL, { root: ".", ...page });
  assert.match(html, /<title>a &quot;quoted&quot; &lt;title&gt;<\/title>/);
});

test("shell: a page without a title or a description is refused", () => {
  assert.throws(() => renderShell(SHELL, { root: ".", ...parsePage("<p>x</p>") }), /title/);
  assert.throws(
    () => renderShell(SHELL, { root: ".", ...parsePage("<!--\n  title: t\n-->\n") }),
    /description/,
  );
});

test("shell: the shipped shell fills without a placeholder left, at every depth", () => {
  const shipped = readFileSync(fileURLToPath(new URL("./shell.html", import.meta.url)), "utf8");
  for (const root of [".", "..", "../.."]) {
    const html = renderShell(shipped, { root, ...parsePage(PAGE) });
    assert.equal(html.includes("{{"), false);
    assert.match(
      html,
      new RegExp(`<link rel="stylesheet" href="${root.replace(/\./g, "\\.")}/styles\\.css"`),
    );
  }
});
