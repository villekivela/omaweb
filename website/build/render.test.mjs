// The release notes are written into the site at build time, so nothing here
// runs in a reader's browser and no test needs one. `node --test` is the whole
// harness; CI runs it in the job that already installs Node for the
// formatters.

import assert from "node:assert/strict";
import test from "node:test";

import {
  markdownToHtml,
  releasePath,
  renderReleaseList,
  renderReleasePage,
  writeReleases,
} from "./render.mjs";

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

test("markdown: a release body cannot inject markup into the page", () => {
  assert.equal(
    markdownToHtml('<img src="https://evil.test/x" onerror="steal()">'),
    "<p>&lt;img src=&quot;https://evil.test/x&quot; onerror=&quot;steal()&quot;&gt;</p>",
  );
});

test("markdown: a link to anything but http or https stays text", () => {
  assert.equal(markdownToHtml("[run](javascript:alert(1))"), "<p>[run](javascript:alert(1))</p>");
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
    {
      name: "omaweb-v0.3.0-sbom.json",
      browser_download_url: "https://github.com/villekivela/omaweb/releases/download/v0.3.0/sbom",
      size: 26925,
    },
  ],
};

test("path: a tag becomes a directory of its own, and an unusable one becomes nothing", () => {
  assert.equal(releasePath("v0.3.0"), "v0.3.0");
  assert.equal(releasePath("../etc/passwd"), null);
  assert.equal(releasePath("a tag"), null);
  assert.equal(releasePath(""), null);
});

test("list: each release is a row that opens its own page", () => {
  const html = renderReleaseList([RELEASE, { ...RELEASE, tag_name: "v0.2.1", name: "v0.2.1" }]);
  assert.match(html, /href="releases\/v0\.3\.0\/"/);
  assert.match(html, /href="releases\/v0\.2\.1\/"/);
  assert.equal((html.match(/<li class="t-release">/g) || []).length, 2);
});

test("list: a row names its tag, its date and its state", () => {
  const html = renderReleaseList([RELEASE]);
  assert.match(html, /v0\.3\.0/);
  assert.match(html, /<time datetime="2026-09-13">13 September 2026<\/time>/);
  assert.match(html, /Prerelease/);
});

test("list: the package is offered beside the row and the other assets are not", () => {
  const html = renderReleaseList([RELEASE]);
  assert.match(html, /releases\/download\/v0\.3\.0\/pkg/);
  assert.equal(html.includes("/sbom"), false);
  assert.match(html, /10\.2 MiB/);
});

test("list: a debug package is not the download", () => {
  const debugOnly = {
    ...RELEASE,
    assets: [
      {
        name: "omaweb-git-debug-0.3.0-1-x86_64.pkg.tar.zst",
        browser_download_url: "https://example.com/debug",
        size: 1,
      },
    ],
  };
  assert.equal(renderReleaseList([debugOnly]).includes("https://example.com/debug"), false);
});

test("list: a release whose tag cannot be a path is left out rather than guessed at", () => {
  assert.equal(renderReleaseList([{ ...RELEASE, tag_name: "../x", name: "../x" }]), "");
});

const TEMPLATE = [
  "<title>{{title}}</title>",
  '<meta name="description" content="{{description}}" />',
  "<h1>{{tag}}</h1>",
  "<p>{{marks}}</p>",
  "<div>{{notes}}</div>",
  "<p>{{links}}</p>",
].join("\n");

test("page: the template is filled with the release's own notes and links", () => {
  const html = renderReleasePage(RELEASE, TEMPLATE);
  assert.match(html, /<title>Omaweb v0\.3\.0 release notes<\/title>/);
  assert.match(html, /<h1>v0\.3\.0<\/h1>/);
  assert.match(html, /<h3>Features<\/h3>/);
  assert.match(html, /close things the way they came/);
  assert.match(html, /releases\/download\/v0\.3\.0\/pkg/);
  assert.match(html, /releases\/tag\/v0\.3\.0/);
  assert.match(html, /<time datetime="2026-09-13">/);
});

test("page: no placeholder is left unfilled", () => {
  assert.equal(renderReleasePage(RELEASE, TEMPLATE).includes("{{"), false);
});

test("page: a release with no body still renders, pointing at GitHub", () => {
  const html = renderReleasePage({ ...RELEASE, body: "" }, TEMPLATE);
  assert.match(html, /releases\/tag\/v0\.3\.0/);
});

test("page: the description is attribute-safe whatever the release is called", () => {
  const html = renderReleasePage({ ...RELEASE, name: 'v1 "beta"' }, TEMPLATE);
  assert.match(html, /content="[^"]*&quot;beta&quot;[^"]*"/);
});

const PAGE = [
  "<p>before</p>",
  "<!-- releases:start -->",
  "<p>the fallback</p>",
  "<!-- releases:end -->",
  "<p>after</p>",
].join("\n");

test("write: the block between the markers is replaced and the markers stay", () => {
  const written = writeReleases(PAGE, "<p>the list</p>");
  assert.match(written, /<!-- releases:start -->/);
  assert.match(written, /<!-- releases:end -->/);
  assert.match(written, /<p>the list<\/p>/);
  assert.equal(written.includes("the fallback"), false);
  assert.match(written, /<p>before<\/p>/);
  assert.match(written, /<p>after<\/p>/);
});

test("write: a page without the markers is a build error, not a silent no-op", () => {
  assert.throws(() => writeReleases("<p>nothing here</p>", "<p>x</p>"), /releases:start/);
});
