// Turns the published GitHub releases into the release pages: the list of
// every version, and the notes for the one being read. Pure string work, so it
// is testable without a network or a browser; `site.mjs` does the fetching and
// the writing.
//
// The notes are a release body written by `scripts/release_notes.sh` and
// rewritten by a model in `scripts/rewrite_release_notes.py`. Either way they
// are Markdown produced elsewhere, so nothing here trusts them: every
// character is escaped first and the markup comes only from the patterns
// below. That is also what keeps the generated pages inside `default-src
// 'self'`. No rule here can emit a subresource, only links, which the policy
// governs as navigations. A fence is the strictest case of the same rule: what
// is inside it is escaped and then shown literally, because it is a command to
// be typed rather than markup to read.
//
// The packages are not offered here. Installing is one section on the landing
// page and the same two commands whichever release it is, so a download button
// per version would be a third place saying it. `This release on GitHub`
// reaches the assets for anyone who wants a particular one.

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

// A bare address, taken to the first space, `<` or closing bracket. Where it
// really ends is decided by `trimAddress` rather than by the character class,
// because the two cases pull opposite ways.
const AUTOLINK = /(^|[\s(])(https?:\/\/[^\s<)]+)/g;

// Sentence punctuation at the end of a bare address belongs to the sentence,
// not to the address, and a reader reading aloud would drop it too. The
// exception is a semicolon closing an entity: this runs over escaped text, so
// the `&` of a query string arrives as `&amp;` and dropping that semicolon
// would leave a broken entity in the href.
function trimAddress(address) {
  let end = address.length;
  while (end > 0 && ".,;:!?".includes(address[end - 1])) {
    const closesEntity =
      address[end - 1] === ";" && /&(?:#\d+|[a-zA-Z][a-zA-Z0-9]*);$/.test(address.slice(0, end));
    if (closesEntity) break;
    end -= 1;
  }
  return address.slice(0, end);
}

// Runs over already-escaped text, so `<` cannot occur in it and a `<n>` marker
// cannot collide with anything the body wrote. Code spans and links are held
// behind such a marker as soon as they are rendered, which is what stops the
// bare-address pass from linking an address that is already inside a link's
// own label, or inside a code span that is meant to read literally.
function renderInline(escaped) {
  const held = [];
  const hold = (html) => `<${held.push(html) - 1}>`;

  const rendered = escaped
    .replace(/`([^`]+)`/g, (whole, code) => hold(`<code>${code}</code>`))
    .replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>")
    .replace(/\[([^\]]+)\]\(([^)\s]+)\)/g, (whole, label, href) =>
      SAFE_URL.test(href) ? hold(`<a href="${href}">${label}</a>`) : whole,
    )
    .replace(AUTOLINK, (whole, before, address) => {
      const href = trimAddress(address);
      return `${before}${hold(`<a href="${href}">${href}</a>`)}${address.slice(href.length)}`;
    });

  // Recursive, because held markup can hold a marker of its own: a code span
  // inside a link's label is held before the link that contains it.
  const restore = (text) =>
    text.replace(/<(\d+)>/g, (whole, index) => restore(held[Number(index)]));
  return restore(rendered);
}

/**
 * Renders the Markdown subset a release body uses: headings written either
 * way, dash lists, fenced commands, paragraphs, and inline bold, code and
 * links. Anything else
 * arrives as the paragraph text it reads as, which is what lets a body this
 * was not written for still render, just plainly.
 */
export function markdownToHtml(markdown) {
  const lines = escapeHtml(markdown).split("\n");
  const parts = [];
  let list = [];
  let paragraph = [];
  // A fence holds what is inside it literally: an install command is read and
  // typed rather than parsed, so nothing in here is marked up, only escaped.
  // The info string after the opening fence names a language this has no use
  // for, and is dropped rather than rendered as content.
  let fenced = null;

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
    const fence = /^(?:```|~~~)(.*)$/.exec(text);

    if (fenced !== null) {
      if (fence) {
        parts.push(`<pre class="t-prose__commands"><code>${fenced.join("\n")}</code></pre>`);
        fenced = null;
      } else {
        fenced.push(line);
      }
      continue;
    }
    if (fence) {
      closeParagraph();
      closeList();
      fenced = [];
      continue;
    }

    const heading = /^#{1,6}\s+(.*)$/.exec(text);
    const item = /^[-*]\s+(.*)$/.exec(text);
    // A line of nothing but `=` or `-` underlines the line above it into a
    // heading. Without this the underline renders as itself, which is what a
    // reader saw at the top of v0.2.0: a title followed by a row of `=`.
    // A rule with nothing above it underlines nothing and is decoration, so it
    // is dropped rather than shown as its own punctuation.
    const underline = /^(?:={2,}|-{2,})$/.test(text);

    if (underline) {
      const title = paragraph.pop();
      closeParagraph();
      closeList();
      if (title !== undefined) parts.push(`<h3>${title}</h3>`);
    } else if (!text) {
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

  // An unclosed fence is still the text someone wrote, so it renders as the
  // block it was opening rather than vanishing with the body after it.
  if (fenced !== null && fenced.length) {
    parts.push(`<pre class="t-prose__commands"><code>${fenced.join("\n")}</code></pre>`);
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
