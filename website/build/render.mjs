// Turns the published GitHub releases into the release pages: the list of
// every version, and the notes for the one being read. Pure string work, so it
// is testable without a network or a browser; `site.mjs` does the fetching and
// the writing.
//
// The notes are a release body written by `scripts/release_notes.sh` and
// rewritten by a model in `scripts/rewrite_release_notes.py`. Either way they
// are Markdown produced elsewhere, so nothing here trusts them. `marked`
// parses the CommonMark and the renderer below decides what the body is
// allowed to become, which is the part that matters: what a release may say is
// then a question of what CommonMark is, rather than of what this file happens
// to have a pattern for.
//
// That is also what keeps the generated pages inside `default-src 'self'`. No
// rule here emits a subresource, only links, which the policy governs as
// navigations. Embedded HTML is escaped back into the text it reads as, an
// image becomes the link that reaches it, and a link the browser would not
// follow is nothing but its own label.
//
// GitHub Flavored Markdown rather than bare CommonMark, because that is what
// the bodies are: they are written in a GitHub release and read there too, so
// a table or a task list should mean on the page what it means at the source.
//
// The packages are not offered here. Installing is one section on the landing
// page and the same two commands whichever release it is, so a download button
// per version would be a third place saying it. `This release on GitHub`
// reaches the assets for anyone who wants a particular one.

import { Marked } from "marked";

const REPOSITORY = "https://github.com/villekivela/omaweb";

const SAFE_URL = /^https?:\/\//i;

// A tag becomes a directory name, so it may be nothing but the characters a
// version tag is made of. A tag that is not is left out rather than sanitised
// into a URL that names a different release.
const TAG = /^[A-Za-z0-9][A-Za-z0-9._-]*$/;

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

// The one place the policy is kept: an address the browser may follow becomes
// a link, and anything else is the text it would have been. Both the link and
// the image rule end here, so there is a single answer to what may carry an
// href off this page.
function linkTo(href, label) {
  return SAFE_URL.test(href) ? `<a href="${escapeHtml(href)}">${label}</a>` : label;
}

// A fenced block is a command a reader is meant to type. An indented one is
// four spaces in a sentence as often as it is code, and dressing that as a
// command block tells a reader to type prose.
const FENCED = /^\s{0,3}(?:`{3,}|~{3,})/;

// How deep inside a link's label the renderer currently is. An image that
// became a link of its own would nest one anchor inside another, which no
// browser keeps and which a badge written as `[![alt](image)](page)` produces
// on the first try. Rendering is synchronous and single-pass, so a counter is
// the whole of the bookkeeping.
let labelDepth = 0;

// Only the rules that have to differ from CommonMark's own. Everything a body
// can write that is not named here renders as `marked` renders it, which is
// what lets a body this was not written for arrive whole rather than as its
// own punctuation.
const PARSER = new Marked({
  gfm: true,
  renderer: {
    // Every heading flattens to h3. The release page's own h1 names the
    // release and h2 heads the notes, so a body's depth is not the page's
    // outline and a body that starts at `##` does not skip a level.
    heading({ tokens }) {
      return `<h3>${this.parser.parseInline(tokens)}</h3>\n`;
    },
    // A fence holds what is inside it literally: an install command is read
    // and typed rather than parsed. The info string after the opening fence
    // names a language this has no use for, and is dropped rather than
    // rendered as content.
    code({ text, raw }) {
      const commands = FENCED.test(raw) ? ' class="t-prose__commands"' : "";
      return `<pre${commands}><code>${escapeHtml(text)}</code></pre>\n`;
    },
    // Markup embedded in a body is the text it reads as. A release body is
    // written elsewhere and published unattended, so the one thing it may
    // never do is become markup of the page's own.
    html({ text, block }) {
      const escaped = escapeHtml(text.trim());
      return block ? `<p>${escaped}</p>\n` : escaped;
    },
    // A link the browser would follow, or nothing but its label. The scheme
    // is checked here rather than trusted from the body: `marked` resolves the
    // URL but does not judge it, and `javascript:` is a URL like any other to
    // a parser.
    link({ href, text, tokens, autolink }) {
      labelDepth += 1;
      const label = autolink ? escapeHtml(text) : this.parser.parseInline(tokens);
      labelDepth -= 1;
      // `[](https://x.test)` is a link with nothing to click and nothing to
      // read out. The address is what it would have said anyway.
      return linkTo(href, label || escapeHtml(href));
    },
    // An image is a subresource, and the policy admits none from anywhere. It
    // becomes the link that reaches it instead, so a screenshot in a body is
    // still a click away rather than gone. Inside a link's label it is its alt
    // text, because the link around it already goes somewhere.
    image({ href, text }) {
      const label = escapeHtml(text || href);
      return labelDepth === 0 ? linkTo(href, label) : label;
    },
  },
});

/**
 * Renders a release body as the notes on its page. CommonMark in, page markup
 * out, narrowed by the rules above: headings flatten to `h3`, fenced blocks
 * are commands to type, and nothing the browser would fetch comes out.
 */
export function markdownToHtml(markdown) {
  return PARSER.parse(markdown || "").trim();
}

/** The directory a release's page lives in, or null when its tag cannot be one. */
export function releasePath(tag) {
  return TAG.test(tag || "") ? tag : null;
}

function formatDate(published) {
  const date = new Date(published);
  const on = (month) =>
    date.toLocaleDateString("en-GB", { day: "numeric", month, year: "numeric", timeZone: "UTC" });
  return {
    machine: date.toISOString().slice(0, 10),
    // Two lengths for two places. The version list is a column of dates beside
    // a column of tags, and the long form makes that column wider than the
    // pane it sits in.
    reader: on("long"),
    brief: on("short"),
  };
}

/**
 * The list of every version, the master half of the release pages. `root` is
 * the way back to the site root from the page being rendered, so one function
 * serves `releases/` and `releases/<tag>/` alike.
 */
export function renderReleaseNav(releases, currentTag, root) {
  const rows = releases
    .map((release) => {
      const path = releasePath(release.tag_name);
      if (!path) return "";
      const date = formatDate(release.published_at);
      const tag = escapeHtml(release.name || release.tag_name);
      const current = release.tag_name === currentTag ? ' aria-current="page"' : "";
      return (
        `<li><a href="${root}/releases/${path}/"${current}>` +
        `<span class="t-versions__tag">${tag}</span>` +
        `<time datetime="${date.machine}">${date.brief}</time>` +
        `</a></li>`
      );
    })
    .join("");
  // `role="list"`, because the CSS takes the bullets off and Safari then stops
  // reporting the element as a list at all.
  return rows ? `<ol class="t-versions" role="list">${rows}</ol>` : "";
}

/**
 * One release's page, filled into `release.template.html`. The template carries
 * the site's chrome, so a release page is the same page as the rest of the site
 * and not a second design to keep in step.
 */
export function renderReleasePage(release, releases, template, root) {
  const tag = escapeHtml(release.name || release.tag_name);
  const date = formatDate(release.published_at);
  const notes = markdownToHtml(release.body || "");
  const page = release.html_url || `${REPOSITORY}/releases/tag/${release.tag_name}`;
  const state = release.prerelease ? "prerelease" : "release";

  const marks = [`<time datetime="${date.machine}">${date.reader}</time>`];
  if (release.prerelease) marks.push('<span class="t-release__state">Prerelease</span>');

  const fields = {
    root,
    title: `Omaweb ${tag} release notes`,
    // Built from the raw name rather than from `tag`, which is already
    // escaped: escaping it again would put a literal `&quot;` in the sentence.
    // `escapeHtml` covers the attribute here as it covers the element above.
    description: escapeHtml(
      `What changed in the Omaweb ${state} ${release.name || release.tag_name}.`,
    ),
    nav: renderReleaseNav(releases, release.tag_name, root),
    tag,
    marks: marks.join(""),
    notes: notes || "<p>This release published no notes.</p>",
    github: `<a class="btn" href="${escapeHtml(page)}">This release on GitHub</a>`,
  };

  return template.replace(/\{\{(\w+)\}\}/g, (whole, name) =>
    name in fields ? fields[name] : whole,
  );
}
