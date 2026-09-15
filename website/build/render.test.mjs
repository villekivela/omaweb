// The release pages are written at build time, so nothing here runs in a
// reader's browser and no test needs one. `node --test` is the whole harness;
// CI runs it in the job that already installs Node for the formatters.

import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import test from "node:test";

import { markdownToHtml, releasePath, renderReleaseNav, renderReleasePage } from "./render.mjs";

test("markdown: a heading of any depth becomes h3, under the page's own h1", () => {
  assert.equal(markdownToHtml("### Features"), "<h3>Features</h3>");
  assert.equal(markdownToHtml("## Fixes"), "<h3>Fixes</h3>");
});

test("markdown: consecutive dashes become one list", () => {
  assert.equal(markdownToHtml("- first\n- second"), "<ul><li>first</li><li>second</li></ul>");
});

test("markdown: a blank line separates paragraphs, and a wrapped line joins", () => {
  assert.equal(markdownToHtml("one\ntwo\n\nthree"), "<p>one two</p><p>three</p>");
});

test("markdown: bold, code and links render inline", () => {
  assert.equal(
    markdownToHtml("**loud** and `quiet`"),
    "<p><strong>loud</strong> and <code>quiet</code></p>",
  );
  assert.equal(
    markdownToHtml("[the diff](https://example.com/a)"),
    '<p><a href="https://example.com/a">the diff</a></p>',
  );
});

test("markdown: a bare https URL becomes a link, and trailing punctuation stays out of it", () => {
  assert.equal(
    markdownToHtml("see https://example.com/a."),
    '<p>see <a href="https://example.com/a">https://example.com/a</a>.</p>',
  );
});

test("markdown: a fenced block is a command to type, not markup to read", () => {
  assert.equal(
    markdownToHtml("```sh\nsudo pacman -U ./omaweb.pkg.tar.zst\n```"),
    '<pre class="t-prose__commands"><code>sudo pacman -U ./omaweb.pkg.tar.zst</code></pre>',
  );
});

test("markdown: nothing inside a fence is marked up", () => {
  // `**` and a bare address are literal in a command, and a fenced `<script>`
  // is the same escaped text it is anywhere else in a body.
  assert.equal(
    markdownToHtml("```\n**not bold** https://example.com\n```"),
    '<pre class="t-prose__commands"><code>**not bold** https://example.com</code></pre>',
  );
  assert.match(markdownToHtml("```\n<script>x</script>\n```"), /&lt;script&gt;/);
});

test("markdown: a fence nobody closed still renders what it opened", () => {
  assert.equal(
    markdownToHtml("```\nomaweb --version"),
    '<pre class="t-prose__commands"><code>omaweb --version</code></pre>',
  );
});

test("markdown: a release body cannot inject markup into the page", () => {
  assert.equal(
    markdownToHtml('<img src="https://evil.test/x" onerror="steal()">'),
    "<p>&lt;img src=&quot;https://evil.test/x&quot; onerror=&quot;steal()&quot;&gt;</p>",
  );
});

test("markdown: a link to anything but http or https stays text", () => {
  assert.equal(markdownToHtml("[run](javascript:alert(1))"), "<p>[run](javascript:alert(1))</p>");
});

test("markdown: a URL inside a link's own label does not become a second link", () => {
  assert.equal(
    markdownToHtml("[see https://x.test/a](https://y.test/b)"),
    '<p><a href="https://y.test/b">see https://x.test/a</a></p>',
  );
});

test("markdown: a link inside a code span stays code", () => {
  assert.equal(markdownToHtml("`[a](https://x.test)`"), "<p><code>[a](https://x.test)</code></p>");
  assert.equal(markdownToHtml("`https://x.test/a`"), "<p><code>https://x.test/a</code></p>");
});

test("markdown: a bare URL keeps the semicolon that closes an escaped entity", () => {
  assert.equal(
    markdownToHtml("https://x.test/?a=1&b=2"),
    '<p><a href="https://x.test/?a=1&amp;b=2">https://x.test/?a=1&amp;b=2</a></p>',
  );
});

test("markdown: bold inside a link's label renders", () => {
  assert.equal(
    markdownToHtml("[**loud**](https://x.test)"),
    '<p><a href="https://x.test"><strong>loud</strong></a></p>',
  );
});

const RELEASE = {
  tag_name: "v0.3.0",
  name: "v0.3.0",
  published_at: "2026-09-13T20:07:10Z",
  prerelease: true,
  draft: false,
  html_url: "https://github.com/villekivela/omaweb/releases/tag/v0.3.0",
  body: "Changes since v0.2.1.\n\n### Features\n\n- close things the way they came (24401e9)\n",
  assets: [
    {
      name: "omaweb-git-0.3.0.r0.ge69fdb2-1-x86_64.pkg.tar.zst",
      browser_download_url: "https://github.com/villekivela/omaweb/releases/download/v0.3.0/pkg",
      size: 10686839,
    },
  ],
};

const OLDER = {
  ...RELEASE,
  tag_name: "v0.2.1",
  name: "v0.2.1",
  published_at: "2026-09-09T12:20:29Z",
  html_url: "https://github.com/villekivela/omaweb/releases/tag/v0.2.1",
  body: "Changes since v0.2.0.\n",
};

test("path: a tag becomes a directory of its own, and an unusable one becomes nothing", () => {
  assert.equal(releasePath("v0.3.0"), "v0.3.0");
  assert.equal(releasePath("../etc/passwd"), null);
  assert.equal(releasePath("a tag"), null);
  assert.equal(releasePath(""), null);
});

test("nav: every release is a row, addressed from the page's own depth", () => {
  const nav = renderReleaseNav([RELEASE, OLDER], "v0.3.0", "../..");
  assert.match(nav, /href="\.\.\/\.\.\/releases\/v0\.3\.0\/"/);
  assert.match(nav, /href="\.\.\/\.\.\/releases\/v0\.2\.1\/"/);
  assert.equal((nav.match(/<li>/g) || []).length, 2);
});

test("nav: the release being read is marked as the current page", () => {
  const nav = renderReleaseNav([RELEASE, OLDER], "v0.2.1", "..");
  assert.match(nav, /href="\.\.\/releases\/v0\.2\.1\/" aria-current="page"/);
  assert.equal(nav.includes('href="../releases/v0.3.0/" aria-current'), false);
});

test("nav: a row carries the tag and a short date, in that order, for the columns", () => {
  const nav = renderReleaseNav([RELEASE], "v0.3.0", "..");
  assert.match(
    nav,
    /<span class="t-versions__tag">v0\.3\.0<\/span><time datetime="2026-09-13">13 Sept 2026<\/time>/,
  );
});

test("nav: the list keeps its semantics where the bullets are styled away", () => {
  assert.match(renderReleaseNav([RELEASE], "v0.3.0", ".."), /<ol class="t-versions" role="list">/);
});

test("nav: a release whose tag cannot be a path is left out rather than guessed at", () => {
  assert.equal(renderReleaseNav([{ ...RELEASE, tag_name: "../x" }], "v0.3.0", ".."), "");
});

test("nav: the site offers no package, so no asset reaches the markup", () => {
  const nav = renderReleaseNav([RELEASE], "v0.3.0", "..");
  assert.equal(nav.includes("pkg.tar.zst"), false);
  assert.equal(nav.includes("/download/"), false);
});

const TEMPLATE = [
  "<title>{{title}}</title>",
  '<meta name="description" content="{{description}}" />',
  '<link rel="stylesheet" href="{{root}}/styles.css" />',
  "<nav>{{nav}}</nav>",
  "<h1>{{tag}}</h1>",
  "<p>{{marks}}</p>",
  "<div>{{notes}}</div>",
  "<p>{{github}}</p>",
].join("\n");

test("page: the template is filled with the release's notes, its siblings and its depth", () => {
  const html = renderReleasePage(RELEASE, [RELEASE, OLDER], TEMPLATE, "../..");
  assert.match(html, /<title>Omaweb v0\.3\.0 release notes<\/title>/);
  assert.match(html, /<h1>v0\.3\.0<\/h1>/);
  assert.match(html, /<h3>Features<\/h3>/);
  assert.match(html, /close things the way they came/);
  assert.match(html, /href="\.\.\/\.\.\/styles\.css"/);
  assert.match(html, /href="\.\.\/\.\.\/releases\/v0\.2\.1\/"/);
  assert.match(html, /releases\/tag\/v0\.3\.0/);
  assert.match(html, /<time datetime="2026-09-13">13 September 2026<\/time>/);
  assert.match(html, /Prerelease/);
});

test("page: no placeholder is left unfilled", () => {
  assert.equal(renderReleasePage(RELEASE, [RELEASE], TEMPLATE, "..").includes("{{"), false);
});

test("page: the themed favicon is reached from the page's own depth", () => {
  // Against the shipped template rather than the fixture above: a release page
  // sits two levels down and the landing page none, so the path the theme
  // script writes has to come from the page rather than from the script.
  const shipped = readFileSync(
    fileURLToPath(new URL("./release.template.html", import.meta.url)),
    "utf8",
  );
  const html = renderReleasePage(RELEASE, [RELEASE], shipped, "../..");
  assert.match(html, /data-themed="\.\.\/\.\.\/assets\/icons\/favicon-\{theme\}\.svg"/);
});

test("page: a release with no body still renders, pointing at GitHub", () => {
  const html = renderReleasePage({ ...RELEASE, body: "" }, [RELEASE], TEMPLATE, "..");
  assert.match(html, /releases\/tag\/v0\.3\.0/);
  assert.match(html, /published no notes/);
});

test("page: the description is attribute-safe whatever the release is called", () => {
  const odd = { ...RELEASE, name: 'v1 "beta"' };
  const html = renderReleasePage(odd, [odd], TEMPLATE, "..");
  assert.match(html, /content="[^"]*&quot;beta&quot;[^"]*"/);
});

test("page: the site offers no package, so no asset reaches the markup", () => {
  const html = renderReleasePage(RELEASE, [RELEASE], TEMPLATE, "..");
  assert.equal(html.includes("pkg.tar.zst"), false);
  assert.equal(html.includes("/download/"), false);
});
