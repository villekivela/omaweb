// The release pages as releases.mjs writes them, against the shipped landing page and template, so
// a change to either that breaks the pages fails here rather than on the deployed site.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import {
  chrome,
  fallbackPage,
  kindOf,
  latestRelease,
  releasePages,
  versions,
} from "./releases.mjs";

const read = (path) => readFileSync(new URL(path, import.meta.url), "utf8");
const LANDING = read("../index.html");
const TEMPLATE = read("./release.html");

const release = (tag, name, published, body = "") => ({
  tag_name: tag,
  name,
  body,
  published_at: published,
  prerelease: tag.startsWith("v0."),
  html_url: `https://github.com/villekivela/omaweb/releases/tag/${tag}`,
});

const RELEASES = [
  release(
    "v0.7.3",
    "v0.7.3",
    "2026-09-25T03:27:41Z",
    "## Fixes\n\n- Extensions work in every Space.",
  ),
  release("engine-6.11.2-2", "Engine 6.11.2-2", "2026-09-24T05:42:57Z"),
  release("repo-aarch64", "Package repository (aarch64)", "2026-09-22T06:36:35Z"),
  release("v0.7.2", "v0.7.2", "2026-09-24T16:58:17Z"),
];

test("kinds: a tag says what a release is", () => {
  assert.equal(kindOf(RELEASES[0]).kind, "browser");
  assert.equal(kindOf(RELEASES[1]).kind, "engine");
  assert.equal(kindOf(RELEASES[2]).kind, "repository");
  assert.equal(kindOf(release("nightly", "Nightly", "2026-09-01T00:00:00Z")).kind, "other");
});

test("versions: a row names its kind once, so the name is what is left of it", () => {
  const list = versions(RELEASES, "v0.7.3", "..");
  assert.match(list, /<span class="log__kind">Engine<\/span><span class="log__name">6\.11\.2-2</);
  assert.match(list, /<span class="log__kind">Repository<\/span><span class="log__name">aarch64</);
  assert.match(list, /href="\.\.\/releases\/v0\.7\.3\/" aria-current="page"/);
  // The full name stays reachable where the short one is cut off.
  assert.match(list, /title="Package repository \(aarch64\)"/);
});

test("pages: releases/ is the newest browser release, and every release has its own", () => {
  const pages = releasePages(RELEASES, LANDING, TEMPLATE);
  assert.deepEqual(
    pages.map(([directory]) => directory),
    ["", "v0.7.3", "engine-6.11.2-2", "repo-aarch64", "v0.7.2"],
  );
  assert.match(pages[0][1], /<title>v0\.7\.3 · Omaweb<\/title>/);
});

test("pages: no placeholder is left, and the notes went through the renderer", () => {
  for (const [, html] of releasePages(RELEASES, LANDING, TEMPLATE)) {
    assert.equal(html.includes("{{"), false);
  }
  const [, newest] = releasePages(RELEASES, LANDING, TEMPLATE)[0];
  assert.match(newest, /<h3>Fixes<\/h3>/);
});

test("pages: the list opens on the browser, or on all when the release is not one", () => {
  const pages = Object.fromEntries(releasePages(RELEASES, LANDING, TEMPLATE));
  assert.match(pages["v0.7.3"], /data-kind-choice="browser" aria-pressed="true"/);
  assert.match(pages["engine-6.11.2-2"], /data-kind-choice="all" aria-pressed="true"/);
});

test("chrome: the landing page's addresses reach back to the root from a release page", () => {
  const html = chrome(LANDING, "../..", { title: "t", description: "d" })("<p>body</p>");
  assert.match(html, /href="\.\.\/\.\.\/styles\.css"/);
  assert.match(html, /src="\.\.\/\.\.\/script\.js"/);
  assert.match(html, /<a href="\.\.\/\.\.\/#features">/);
  assert.match(html, /<a href="\.\.\/\.\.\/releases\/" aria-current="true">/);
  // A symbol reference names this page's own sprite, so it stays as it is.
  assert.match(html, /<use href="#i-github" \/>/);
  // An address that leaves the site is left alone.
  assert.match(html, /href="https:\/\/github\.com\/villekivela\/omaweb"/);
  assert.match(html, /<main>\n<p>body<\/main>|<main>\n<p>body<\/p>\n {4}<\/main>/);
});

// A release page wears the theme the landing page does, Retro 82 or the reader's pick, and asks
// Omaweb for the reader's own palette, which themes.css lets win where Omaweb hands it over.
test("chrome: a release page keeps the landing page's theme and asks for the reader's palette", () => {
  assert.doesNotMatch(LANDING, /omaweb-palette/);
  const html = chrome(LANDING, "../..", { title: "t", description: "d" })("<p>body</p>");
  assert.match(html, /<html lang="en" data-theme="retro-82">/);
  assert.match(html, /<meta name="omaweb-palette" content="follow" \/>\n/);
});

test("fallback: a build that could not read the releases still says where they are", () => {
  const html = fallbackPage(LANDING);
  assert.match(html, /The releases are on GitHub\./);
  assert.match(html, /href="https:\/\/github\.com\/villekivela\/omaweb\/releases"/);
  assert.match(html, /href="\.\.\/styles\.css"/);
});

test("pages: a list with no browser release is an error, not an empty page", () => {
  assert.throws(() => releasePages(RELEASES.slice(1, 3), LANDING, TEMPLATE), /no browser release/);
});

// The landing page names the newest browser release beside its install link, written in when the
// site is built, so the page asks GitHub for nothing when it loads.
test("latest: the landing page names the newest browser release, with its date and notes", () => {
  const html = latestRelease(LANDING, RELEASES);
  assert.match(
    html,
    /<p class="start__release">Latest: v0\.7\.3 · <time datetime="2026-09-25">25 Sept 2026<\/time> · <a href="releases\/v0\.7\.3\/">release notes<\/a><\/p>/,
  );
  assert.doesNotMatch(html, /data-latest-release/);
});

test("latest: the line stays hidden in the source, for a build that could not read the releases", () => {
  assert.match(LANDING, /<p class="start__release" data-latest-release hidden><\/p>/);
});

test("latest: a landing page without the line's place is an error", () => {
  assert.throws(() => latestRelease("<main></main>", RELEASES), /no place for the latest release/);
});

// A release page shared elsewhere describes itself, not the landing page, and leaves its own
// address as its address.
test("chrome: a release page's Open Graph tags are its own", () => {
  const html = chrome(LANDING, "../..", { title: "v0.7.3 · Omaweb", description: "Notes" })("");
  assert.match(html, /<meta property="og:title" content="v0\.7\.3 · Omaweb" \/>/);
  assert.match(html, /property="og:description"\s+content="Notes"/);
  assert.doesNotMatch(html, /og:url/);
  assert.match(
    html,
    /<meta property="og:image" content="https:\/\/omaweb\.app\/assets\/og\.png" \/>/,
  );
});
