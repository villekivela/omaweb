// Turns the published GitHub releases into the markup the site carries: a row
// per release for the landing page, and a page per release holding its notes.
// Pure string work, so it is testable without a network or a browser;
// `site.mjs` does the fetching and the writing.
//
// The notes are a release body written by `scripts/release_notes.sh` and,
// after #237, rewritten by a model. Either way they are Markdown produced
// elsewhere, so nothing here trusts them: every character is escaped first and
// the markup comes only from the patterns below. That is also what keeps the
// generated pages inside `default-src 'self'` -- no rule here can emit a
// subresource, only links, which the policy governs as navigations.

const REPOSITORY = "https://github.com/villekivela/omaweb";

// The release's own package for Arch, not the debug build beside it and not
// the SBOM. A reader looking at a release wants the thing they install.
const PACKAGE = /^omaweb-git-(?!debug-).*\.pkg\.tar\.zst$/;

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

// Runs over already-escaped text, so the entities it must not cut through are
// the ones it wrote itself: a `&quot;` inside a bare URL would end the link
// several characters early. Trailing sentence punctuation is left outside the
// link for the same reason a reader would leave it: it belongs to the
// sentence, not to the address.
function renderInline(escaped) {
  return escaped
    .replace(/`([^`]+)`/g, "<code>$1</code>")
    .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
    .replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, (whole, label, href) =>
      SAFE_URL.test(href) ? `<a href="${href}">${label}</a>` : whole,
    )
    .replace(/(^|[\s(])(https?:\/\/[^\s<)]*[^\s<).,;:!?])/g, '$1<a href="$2">$2</a>');
}

/**
 * Renders the Markdown subset a release body uses: headings, dash lists,
 * paragraphs, and inline bold, code and links. Anything else arrives as the
 * paragraph text it reads as, which is what lets a body this was not written
 * for still render, just plainly.
 */
export function markdownToHtml(markdown) {
  const lines = escapeHtml(markdown).split("\n");
  const parts = [];
  let list = [];
  let paragraph = [];

  const closeList = () => {
    if (list.length) parts.push(`<ul>${list.map((item) => `<li>${item}</li>`).join("")}</ul>`);
    list = [];
  };
  const closeParagraph = () => {
    if (paragraph.length) parts.push(`<p>${paragraph.join(" ")}</p>`);
    paragraph = [];
  };

  for (const line of lines) {
    const text = line.trim();
    const heading = /^#{1,6}\s+(.*)$/.exec(text);
    const item = /^[-*]\s+(.*)$/.exec(text);

    if (!text) {
      closeParagraph();
      closeList();
    } else if (heading) {
      closeParagraph();
      closeList();
      // Every heading flattens to h3. The release page's own h1 names the
      // release and h2 heads the notes, so a body's depth is not the page's
      // outline and a body that starts at `##` does not skip a level.
      parts.push(`<h3>${renderInline(heading[1])}</h3>`);
    } else if (item) {
      closeParagraph();
      list.push(renderInline(item[1]));
    } else {
      closeList();
      paragraph.push(renderInline(text));
    }
  }

  closeParagraph();
  closeList();
  return parts.join("");
}

/** The directory a release's page lives in, or null when its tag cannot be one. */
export function releasePath(tag) {
  return TAG.test(tag || "") ? tag : null;
}

function formatDate(published) {
  const date = new Date(published);
  return {
    machine: date.toISOString().slice(0, 10),
    reader: date.toLocaleDateString("en-GB", {
      day: "numeric",
      month: "long",
      year: "numeric",
      timeZone: "UTC",
    }),
  };
}

// MiB, because pacman reports the package in MiB and this is the same file.
function formatSize(bytes) {
  return `${(bytes / 1024 / 1024).toFixed(1)} MiB`;
}

function packageOf(release) {
  return (release.assets || []).find((candidate) => PACKAGE.test(candidate.name));
}

function marksOf(release) {
  const date = formatDate(release.published_at);
  const marks = [`<time datetime="${date.machine}">${date.reader}</time>`];
  if (release.prerelease) marks.push('<span class="t-release__state">Prerelease</span>');
  return marks.join("");
}

function downloadOf(release, classes) {
  const asset = packageOf(release);
  if (!asset) return "";
  return (
    `<a class="${classes}" href="${escapeHtml(asset.browser_download_url)}">` +
    `Package <span class="t-release__size">${formatSize(asset.size)}</span></a>`
  );
}

/**
 * The list on the landing page: one row per release, each opening the release's
 * own page, with the package beside it so a reader who only wants the download
 * never leaves the page.
 */
export function renderReleaseList(releases) {
  const rows = releases
    .map((release) => {
      const path = releasePath(release.tag_name);
      if (!path) return "";
      const tag = escapeHtml(release.name || release.tag_name);
      return [
        `<li class="t-release">`,
        `<a class="t-release__open" href="releases/${path}/">`,
        `<span class="t-release__tag">${tag}</span>`,
        `<span class="t-release__marks">${marksOf(release)}</span>`,
        `<span class="t-release__more">Release notes &rsaquo;</span>`,
        `</a>`,
        downloadOf(release, "btn t-release__get"),
        `</li>`,
      ].join("");
    })
    .join("");
  return rows ? `<ol class="t-releases">${rows}</ol>` : "";
}

/**
 * One release's page, filled into `release.template.html`. The template carries
 * the site's chrome so a release page is the same page as the rest of the site
 * and not a second design to keep in step.
 */
export function renderReleasePage(release, template) {
  const tag = escapeHtml(release.name || release.tag_name);
  const notes = markdownToHtml(release.body || "");
  const page = release.html_url || `${REPOSITORY}/releases/tag/${release.tag_name}`;
  const state = release.prerelease ? "prerelease" : "release";

  const links = [
    downloadOf(release, "btn btn--emphasis"),
    `<a class="btn" href="${escapeHtml(page)}">This release on GitHub</a>`,
  ].join("");

  const fields = {
    title: `Omaweb ${tag} release notes`,
    // Built from the raw name rather than from `tag`, which is already
    // escaped: escaping it again would put a literal `&quot;` in the sentence.
    // `escapeHtml` covers the attribute here as it covers the element above.
    description: escapeHtml(
      `What changed in the Omaweb ${state} ${release.name || release.tag_name}, ` +
        `and where to get it.`,
    ),
    tag,
    marks: marksOf(release),
    notes: notes || "<p>This release published no notes.</p>",
    links,
  };

  return template.replace(/\{\{(\w+)\}\}/g, (whole, name) =>
    name in fields ? fields[name] : whole,
  );
}

const START = "<!-- releases:start -->";
const END = "<!-- releases:end -->";

/**
 * Puts the rendered list between the markers in the landing page, keeping the
 * markers so the next build finds them. The committed page holds a fallback
 * there, which is what a reader gets when the build cannot reach the API.
 */
export function writeReleases(page, releases) {
  const from = page.indexOf(START);
  const to = page.indexOf(END);
  if (from === -1 || to === -1 || to < from) {
    throw new Error(`the page has no ${START} ... ${END} block to write the releases into`);
  }
  return page.slice(0, from + START.length) + "\n" + releases + "\n" + page.slice(to);
}
