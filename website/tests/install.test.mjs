// The one-line install: the landing page tells a reader to pipe /install into sh, and the site
// serves scripts/install.sh there, as text, under the same headers as every other page.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import { SHARED } from "../build/site.mjs";

const LINE = "curl -fsSL https://omaweb.app/install | sh";
const vercel = JSON.parse(readFileSync(new URL("../vercel.json", import.meta.url), "utf8"));

// Vercel's `source` is a path pattern; these are the two forms vercel.json uses.
function matches(source, path) {
  return source === path || new RegExp(`^${source}$`).test(path);
}

function headersFor(path) {
  const sent = {};
  for (const rule of vercel.headers)
    if (matches(rule.source, path))
      for (const { key, value } of rule.headers) sent[key.toLowerCase()] = value;
  return sent;
}

test("build: the site ships scripts/install.sh at /install", () => {
  assert.equal(new URL(SHARED.install).pathname.endsWith("/scripts/install.sh"), true);
});

test("headers: /install is plain text, under the site's policy", () => {
  const sent = headersFor("/install");
  assert.equal(sent["content-type"], "text/plain; charset=utf-8");
  assert.equal(sent["x-content-type-options"], "nosniff");
  assert.match(sent["content-security-policy"], /default-src 'self'/);
});

// The one line, ahead of the manual block, in the part of a document from `start` on.
function leadsWithTheLine(text, start) {
  const install = text.slice(text.indexOf(start));
  const first = install.indexOf(LINE);
  assert.notEqual(first, -1);
  assert.ok(first < install.indexOf("[omaweb]"), "the one line comes before the manual steps");
}

test("page: the install leads with the one line", () => {
  leadsWithTheLine(readFileSync(new URL("../index.html", import.meta.url), "utf8"), 'id="install"');
});

test("readme: the install leads with the one line", () => {
  leadsWithTheLine(readFileSync(new URL("../../README.md", import.meta.url), "utf8"), "## Install");
});
